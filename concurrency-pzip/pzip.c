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

#ifndef CHUNK_SIZE
#define CHUNK_SIZE (512UL * 1024UL)
#endif

#ifndef CHUNK_FACTOR
#define CHUNK_FACTOR (2)
#endif

#pragma pack(push, 1)
typedef struct {
  uint32_t length;
  char character;
} Run;
#pragma pack(pop)

typedef enum { CHUNK_EMPTY, CHUNK_READY, CHUNK_WORKING, CHUNK_DONE } ChunkState;

typedef struct {
  cnd_t done;
  mtx_t lock;
  size_t raw_len;
  size_t run_count;
  _Atomic ChunkState state;
  char raw[CHUNK_SIZE];
  Run runs[CHUNK_SIZE];
} Chunk;

typedef struct {
  size_t count;
  bool filling;
  Chunk chunks[];
} ChunkArena;

typedef struct {
  char **filepaths;
  size_t count;
  int fd;
} Reader;

bool reader_init(Reader *r, char **filepaths, size_t count) {
  for (size_t i = 0; i < count; i++) {
    int fd = open(filepaths[i], O_RDONLY);
    if (fd < 0) {
      return false;
    }

    close(fd);
  }

  r->filepaths = filepaths;
  r->count = count;
  r->fd = -1;
  return true;
}

size_t reader_read_chunk(Reader *r, char *buf) {
  size_t total_read = 0;

  while (total_read < CHUNK_SIZE) {
    if (r->fd < 0) {
      if (r->count == 0) {
        break;
      }

      r->fd = open(r->filepaths[0], O_RDONLY);
      // posix_fadvise(r->fd, 0, 0, POSIX_FADV_SEQUENTIAL);
      r->filepaths++;
      r->count--;
    }

    ssize_t n = read(r->fd, buf + total_read, CHUNK_SIZE - total_read);

    if (n > 0) {
      total_read += (size_t)n;
    } else {
      close(r->fd);
      r->fd = -1;
    }
  }

  return total_read;
}

void chunk_arena_init(ChunkArena *arena, size_t count, Reader *reader) {
  arena->filling = true;
  arena->count = count;

  for (size_t i = 0; i < count; i++) {
    Chunk *chunk = &arena->chunks[i];
    mtx_init(&chunk->lock, mtx_plain);
    cnd_init(&chunk->done);

    size_t n = reader_read_chunk(reader, chunk->raw);
    if (n > 0) {
      chunk->raw_len = n;
      chunk->state = CHUNK_READY;
    } else {
      chunk->state = CHUNK_EMPTY;
      arena->filling = false;
    }
  }
}

void process_chunk(Chunk *chunk) {
  char *end = chunk->raw + chunk->raw_len;
  Run *runs_ptr = chunk->runs;
  chunk->run_count = 0;

  Run run = {.length = 1, .character = *chunk->raw};
  for (char *current_ptr = chunk->raw + 1; current_ptr < end; current_ptr++) {
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

int worker(void *a) {
  ChunkArena *arena = (ChunkArena *)a;
  size_t used_chunks = 0;
  size_t i = 0;
  while (true) {
    Chunk *chunk = &arena->chunks[i];
    ChunkState expected = CHUNK_READY;
    bool available =
        atomic_compare_exchange_strong(&chunk->state, &expected, CHUNK_WORKING);
    if (available) {
      used_chunks = 0;

      process_chunk(chunk);
      chunk->state = CHUNK_DONE;
      mtx_lock(&chunk->lock);

      cnd_broadcast(&chunk->done);
      mtx_unlock(&chunk->lock);
    } else {
      used_chunks++;
      if (used_chunks == arena->count) {
        if (!arena->filling) {
          return 0;
        }

        used_chunks = 0;
        thrd_yield();
      }
    }

    i = i + 1 < arena->count ? i + 1 : 0;
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "pzip: file1 [file2 ...]\n");
    return 1;
  }

  size_t thread_count = (size_t)get_nprocs();
  size_t chunk_count = CHUNK_FACTOR * thread_count;
  Reader reader;
  if (!reader_init(&reader, &argv[1], (size_t)(argc - 1))) {
    fprintf(stderr, "pzip: cannot open file\n");
    return 1;
  };

  ChunkArena *chunk_arena =
      mmap(NULL, sizeof(ChunkArena) + (chunk_count * sizeof(Chunk)),
           PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  chunk_arena_init(chunk_arena, chunk_count, &reader);

  thrd_t threads[thread_count];
  for (size_t i = 0; i < thread_count; i++) {
    thrd_create(&threads[i], worker, chunk_arena);
  }

  Run last_run = {};
  bool first_chunk = true;
  size_t i = 0;
  while (true) {
    Chunk *chunk = &chunk_arena->chunks[i];
    if (chunk->state == CHUNK_EMPTY) {
      break;
    }

    mtx_lock(&chunk->lock);
    while (chunk->state != CHUNK_DONE) {
      cnd_wait(&chunk->done, &chunk->lock);
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
    if (chunk_arena->filling) {
      size_t n = reader_read_chunk(&reader, chunk->raw);
      if (n == 0) {
        chunk->state = CHUNK_EMPTY;
      } else {
        chunk->raw_len = n;
        chunk->state = CHUNK_READY;
      }

      if (n < CHUNK_SIZE) {
        chunk_arena->filling = false;
      }
    } else {
      chunk->state = CHUNK_EMPTY;
    }

    i = i + 1 < chunk_arena->count ? i + 1 : 0;
  }

  if (!first_chunk) {
    fwrite(&last_run, sizeof(Run), 1, stdout);
  }

  for (size_t t = 0; t < thread_count; t++) {
    thrd_join(threads[t], NULL);
  }

  munmap(chunk_arena, sizeof(ChunkArena) + (chunk_count * sizeof(Chunk)));
}
