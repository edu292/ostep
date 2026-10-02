#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <threads.h>
#include <unistd.h>

typedef struct {
  uint32_t key;
  char data[96];
} Record;

typedef struct {
  uint32_t key;
  uint32_t index;
} KeyIndex;

typedef struct {
  pthread_barrier_t *barrier;
  size_t thread_id;
  size_t thread_count;

  KeyIndex *keys[2];

  uint32_t *hist_table;

  size_t start;
  size_t end;

  const Record *in_records;
  Record *out_records;
} Context;

constexpr size_t KEY_BITS = sizeof(uint32_t) * 8;
constexpr size_t RADIX_BITS = 8;

constexpr size_t RADIX_BUCKETS = 1ULL << RADIX_BITS;
constexpr size_t RADIX_MASK = RADIX_BUCKETS - 1;

constexpr size_t NUM_PASSES = (KEY_BITS + RADIX_BITS - 1) / RADIX_BITS;

int worker(void *v) {
  Context *ctx = (Context *)v;
  const Record *in_records = ctx->in_records;

  KeyIndex *src = ctx->keys[0];
  KeyIndex *dst = ctx->keys[1];
  for (size_t i = ctx->start; i < ctx->end; i++) {
    src[i].key = in_records[i].key;
    src[i].index = (uint32_t)i;
  }

  uint32_t *my_hist = &ctx->hist_table[ctx->thread_id * RADIX_BUCKETS];
  for (size_t pass = 0; pass < NUM_PASSES; pass++) {
    memset(my_hist, 0, RADIX_BUCKETS * sizeof(uint32_t));
    for (size_t i = ctx->start; i < ctx->end; i++) {
      uint32_t bucket = (src[i].key >> (pass * RADIX_BITS)) & RADIX_MASK;
      my_hist[bucket]++;
    }

    pthread_barrier_wait(ctx->barrier);

    if (ctx->thread_id == 0) {
      uint32_t offset = 0;
      for (size_t b = 0; b < RADIX_BUCKETS; b++) {
        for (size_t t = 0; t < ctx->thread_count; t++) {
          size_t cell = (t * RADIX_BUCKETS) + b;
          uint32_t count = ctx->hist_table[cell];
          ctx->hist_table[cell] = offset;
          offset += count;
        }
      }
    }

    pthread_barrier_wait(ctx->barrier);

    for (size_t i = ctx->start; i < ctx->end; i++) {
      KeyIndex item = src[i];
      uint32_t bucket = (item.key >> (pass * RADIX_BITS)) & RADIX_MASK;
      uint32_t index = my_hist[bucket]++;
      dst[index] = item;
    }

    pthread_barrier_wait(ctx->barrier);

    KeyIndex *temp = src;
    src = dst;
    dst = temp;
  }

  Record *out_records = ctx->out_records;
  for (size_t i = ctx->start; i < ctx->end; i++) {
    uint32_t src_idx = src[i].index;
    out_records[i] = in_records[src_idx];
  }

  return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: psort input output\n");
    return EXIT_FAILURE;
  }

  int in_fd = open(argv[1], O_RDONLY);
  if (in_fd < 0) {
    fprintf(stderr, "psort: could not open file\n");
    return EXIT_FAILURE;
  }

  struct stat st;
  if (fstat(in_fd, &st) != 0) {
    fprintf(stderr, "psort: could not stat input file\n");
    close(in_fd);
    return EXIT_FAILURE;
  }

  size_t total_bytes = (size_t)st.st_size;
  if (total_bytes % sizeof(Record) != 0) {
    fprintf(stderr, "psort: invalid input file size\n");
    close(in_fd);
    return EXIT_FAILURE;
  }
  size_t num_records = total_bytes / sizeof(Record);

  int out_fd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (out_fd < 0) {
    fprintf(stderr, "psort: could not open file\n");
    close(in_fd);
    return EXIT_FAILURE;
  }

  if (total_bytes == 0) {
    close(out_fd);
    close(in_fd);
    return EXIT_SUCCESS;
  }

  Record *in_records = mmap(NULL, total_bytes, PROT_READ, MAP_SHARED, in_fd, 0);
  if (in_records == MAP_FAILED) {
    perror("in mmap failed");
    return EXIT_FAILURE;
  }

  close(in_fd);

  if (posix_fallocate(out_fd, 0, total_bytes) != 0) {
    perror("posix_fallocate failed");
    return EXIT_FAILURE;
  }

  Record *out_records =
      mmap(NULL, total_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, out_fd, 0);
  if (out_records == MAP_FAILED) {
    perror("out mmap failed");
    return EXIT_FAILURE;
  }

  close(out_fd);
  madvise(in_records, total_bytes, MADV_WILLNEED | MADV_SEQUENTIAL);
  madvise(out_records, total_bytes, MADV_WILLNEED | MADV_SEQUENTIAL);

  size_t thread_count = (size_t)get_nprocs();
  if (thread_count > num_records) {
    thread_count = num_records;
  }

  thrd_t workers[thread_count];
  size_t sz_barrier = sizeof(pthread_barrier_t);
  size_t sz_contexts = thread_count * sizeof(Context);
  size_t sz_keys = num_records * sizeof(KeyIndex);
  size_t sz_hist_table = thread_count * RADIX_BUCKETS * sizeof(uint32_t);

  char *arena =
      malloc(sz_barrier + sz_contexts + (2 * sz_keys) + sz_hist_table);

  char *ptr = arena;
  pthread_barrier_t *barrier = (pthread_barrier_t *)ptr;
  pthread_barrier_init(barrier, nullptr, thread_count);
  ptr += sz_barrier;
  Context *contexts = (Context *)ptr;
  ptr += sz_contexts;
  KeyIndex *keys0 = (KeyIndex *)ptr;
  ptr += sz_keys;
  KeyIndex *keys1 = (KeyIndex *)ptr;
  ptr += sz_keys;
  uint32_t *hist_table = (uint32_t *)ptr;

  size_t chunk_size = num_records / thread_count;
  size_t remainder = num_records % thread_count;
  size_t current = 0;
  for (size_t i = 0; i < thread_count; i++) {
    Context *ctx = &contexts[i];
    size_t count = chunk_size + (i < remainder ? 1 : 0);
    ctx->start = current;
    current += count;
    ctx->end = current;

    ctx->thread_id = i;
    ctx->thread_count = thread_count;
    ctx->barrier = barrier;

    ctx->keys[0] = keys0;
    ctx->keys[1] = keys1;
    ctx->hist_table = hist_table;

    ctx->in_records = in_records;
    ctx->out_records = out_records;

    thrd_create(&workers[i], worker, ctx);
  }

  for (size_t i = 0; i < thread_count; i++) {
    thrd_join(workers[i], nullptr);
  }

  munmap(out_records, total_bytes);
  munmap(in_records, total_bytes);
  pthread_barrier_destroy(barrier);
  free(arena);
  return EXIT_SUCCESS;
}
