#include <fcntl.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <threads.h>
#include <unistd.h>

constexpr size_t CHUNK_SIZE = 512UL * 1024UL;
constexpr size_t CHUNK_FACTOR = 2;

#pragma pack(push, 1)
typedef struct {
  uint32_t length;
  char character;
} Run;
#pragma pack(pop)

typedef struct {
  cnd_t done_cnd;
  mtx_t lock;
  char *data;
  size_t data_len;
  size_t run_count;
  bool done;
  Run runs[CHUNK_SIZE];
} Chunk;

void chunk_set(Chunk *c, char *data, size_t data_len) {
  c->data_len = data_len;
  c->run_count = 0;
  c->data = data;
  c->done = false;
}

typedef struct {
  cnd_t empty;
  cnd_t full;
  mtx_t lock;
  size_t capacity;
  size_t count;
  size_t head;
  size_t tail;
  Chunk chunks[];
} Queue;

void queue_init(Queue *q, size_t capacity) {
  mtx_init(&q->lock, mtx_plain);
  cnd_init(&q->empty);
  cnd_init(&q->full);

  q->capacity = capacity;
  q->count = 0;
  q->head = 0;
  q->tail = 0;

  for (size_t i = 0; i < capacity; i++) {
    Chunk *c = &q->chunks[i];
    mtx_init(&c->lock, mtx_plain);
    cnd_init(&c->done_cnd);
    chunk_set(c, nullptr, 0);
  }
}

void queue_push(Queue *q, char *data, size_t data_len) {
  mtx_lock(&q->lock);
  while (q->count == q->capacity) {
    cnd_wait(&q->full, &q->lock);
  }

  Chunk *c = &q->chunks[q->tail];
  chunk_set(c, data, data_len);
  q->tail = q->tail + 1 < q->capacity ? q->tail + 1 : 0;
  q->count++;

  mtx_unlock(&q->lock);

  cnd_signal(&q->empty);
}

Chunk *queue_pop(Queue *q) {
  mtx_lock(&q->lock);
  while (q->count == 0) {
    cnd_wait(&q->empty, &q->lock);
  }

  Chunk *popped = &q->chunks[q->head];
  q->head = q->head + 1 < q->capacity ? q->head + 1 : 0;
  q->count--;

  mtx_unlock(&q->lock);

  cnd_signal(&q->full);

  return popped;
}

void queue_destroy(Queue *q) {
  mtx_destroy(&q->lock);
  cnd_destroy(&q->empty);
  cnd_destroy(&q->full);

  for (size_t i = 0; i < q->capacity; i++) {
    Chunk *c = &q->chunks[i];
    mtx_destroy(&c->lock);
    cnd_destroy(&c->done_cnd);
  }
}

typedef struct {
  size_t chunk_size;
  char **filepaths;
  size_t count;
  char *map_ptr;
  size_t map_size;
  size_t map_offset;
} Reader;

bool reader_init(Reader *r, char **filepaths, size_t count, size_t chunk_size) {
  for (size_t i = 0; i < count; i++) {
    if (access(filepaths[i], F_OK) != 0) {
      return false;
    }
  }

  r->filepaths = filepaths;
  r->count = count;
  r->map_ptr = nullptr;
  r->map_size = 0;
  r->map_offset = 0;
  r->chunk_size = chunk_size;
  return true;
}

size_t reader_get_chunk(Reader *reader, char **data_ptr) {
  if (reader->map_ptr == nullptr) {
    if (reader->count == 0) {
      return 0;
    }

    int fd = open(reader->filepaths[0], O_RDONLY);
    reader->filepaths++;
    reader->count--;

    struct stat sb;
    fstat(fd, &sb);
    reader->map_size = (size_t)sb.st_size;
    reader->map_offset = 0;

    reader->map_ptr =
        mmap(nullptr, reader->map_size, PROT_READ, MAP_PRIVATE, fd, 0);
    madvise(reader->map_ptr, reader->map_size, MADV_SEQUENTIAL);
    close(fd);
  }

  *data_ptr = reader->map_ptr + reader->map_offset;

  size_t remaining = reader->map_size - reader->map_offset;
  size_t read = reader->chunk_size;

  if (remaining < reader->chunk_size) {
    read = remaining;
    reader->map_ptr = nullptr;
  }

  reader->map_offset += read;

  return read;
}

void process_chunk(Chunk *chunk) {
  char *end = chunk->data + chunk->data_len;
  Run *runs_ptr = chunk->runs;
  chunk->run_count = 0;

  Run run = {.length = 1, .character = *chunk->data};
  for (char *current_ptr = chunk->data + 1; current_ptr < end; current_ptr++) {
    char current_char = *current_ptr;
    if (current_char == run.character) {
      run.length++;
      continue;
    }

    *runs_ptr++ = run;
    chunk->run_count++;

    run.character = current_char;
    run.length = 1;
  }

  *runs_ptr++ = run;
  chunk->run_count++;
}

int worker(void *q) {
  Queue *queue = (Queue *)q;
  Chunk *chunk = nullptr;
  while (true) {
    chunk = queue_pop(queue);
    if (chunk->data == nullptr) {
      return 0;
    }

    process_chunk(chunk);
    mtx_lock(&chunk->lock);
    chunk->done = true;
    cnd_signal(&chunk->done_cnd);
    mtx_unlock(&chunk->lock);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "pzip: file1 [file2 ...]\n");
    return EXIT_FAILURE;
  }

  size_t thread_count = (size_t)get_nprocs();
  size_t chunk_count = CHUNK_FACTOR * thread_count;
  Reader reader;
  if (!reader_init(&reader, &argv[1], (size_t)(argc - 1), CHUNK_SIZE)) {
    fprintf(stderr, "pzip: cannot open file\n");
    return EXIT_FAILURE;
  };

  Queue *queue =
      mmap(nullptr, sizeof(Queue) + (chunk_count * sizeof(Chunk)),
           PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  queue_init(queue, chunk_count);
  size_t remaining_chunks = 0;
  for (size_t i = 0; i < chunk_count; i++) {
    char *chunk = nullptr;
    size_t read = reader_get_chunk(&reader, &chunk);
    if (read == 0) {
      break;
    }

    remaining_chunks++;
    queue_push(queue, chunk, read);
  }

  thrd_t threads[thread_count];
  for (size_t i = 0; i < thread_count; i++) {
    thrd_create(&threads[i], worker, queue);
  }

  Run last_run = {};
  bool first_chunk = true;
  size_t i = 0;
  while (remaining_chunks > 0) {
    Chunk *chunk = &queue->chunks[i];

    mtx_lock(&chunk->lock);
    while (!chunk->done) {
      cnd_wait(&chunk->done_cnd, &chunk->lock);
    }
    mtx_unlock(&chunk->lock);

    if (!first_chunk) {
      Run *first_run = &chunk->runs[0];
      if (first_run->character == last_run.character) {
        first_run->length += last_run.length;
      } else {
        fwrite(&last_run, sizeof(Run), 1, stdout);
      }
    }

    if (chunk->run_count > 1) {
      fwrite(&chunk->runs[0], sizeof(Run), chunk->run_count - 1, stdout);
    }

    first_chunk = false;
    memcpy(&last_run, &chunk->runs[chunk->run_count - 1], sizeof(Run));
    if (remaining_chunks == chunk_count) {
      char *data = nullptr;
      size_t len = reader_get_chunk(&reader, &data);

      if (len == 0) {
        remaining_chunks--;
      } else {
        queue_push(queue, data, len);
      }
    } else {
      remaining_chunks--;
    }

    i = i + 1 < chunk_count ? i + 1 : 0;
  }

  if (!first_chunk) {
    fwrite(&last_run, sizeof(Run), 1, stdout);
  }

  for (size_t t = 0; t < thread_count; t++) {
    queue_push(queue, nullptr, 0);
  }

  for (size_t t = 0; t < thread_count; t++) {
    thrd_join(threads[t], nullptr);
  }

  queue_destroy(queue);
  munmap(queue, sizeof(Queue) + (chunk_count * sizeof(Chunk)));
}
