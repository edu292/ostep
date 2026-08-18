#include <fcntl.h>
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

#define CHUNK_SIZE (1024UL * 1024UL)

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
  ChunkState state;
  char raw[CHUNK_SIZE];
  Run runs[CHUNK_SIZE];
} Chunk;

typedef struct {
  size_t count;
  Chunk chunks[];
} Context;

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

void process_chunk(Chunk *chunk) {
  char *end = chunk->raw + chunk->raw_len;
  Run *runs_ptr = chunk->runs;

  Run run = {.length = 1, .character = *chunk->raw};
  for (char *current_ptr = chunk->raw + 1; current_ptr <= end; current_ptr++) {
    char current_char = current_ptr < end ? *current_ptr : '\0';
    if (current_char == run.character) {
      run.length++;
      continue;
    }

    memcpy(runs_ptr, &run, sizeof(Run));
    runs_ptr++;

    chunk->run_count++;
    run.character = current_char;
    run.length = 1;
  }
}

int worker(void *c) {
  Context *context = (Context *)c;
  size_t used_chunks = 0;
  while (used_chunks != context->count) {
    for (size_t i = 0; i < context->count; i++) {
      Chunk *chunk = &context->chunks[i];
      mtx_lock(&chunk->lock);
      if (chunk->state == CHUNK_READY) {
        process_chunk(chunk);
        chunk->state = CHUNK_DONE;
        used_chunks = 0;
      } else {
        used_chunks++;
      }
      mtx_unlock(&chunk->lock);
    }
  }

  return 0;
}

void work_ring_init(Context *w, Reader *reader) {
  for (size_t i = 0; i < w->count; i++) {
    Chunk *chunk = &w->chunks[i];
    mtx_init(&chunk->lock, mtx_plain);
    cnd_init(&chunk->done);

    size_t n = reader_read_chunk(reader, chunk->raw);
    if (n > 0) {
      chunk->raw_len = n;
      chunk->state = CHUNK_READY;
    } else {
      chunk->state = CHUNK_EMPTY;
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "pzip: file1 [file2 ...]\n");
    return 1;
  }

  size_t thread_count = (size_t)get_nprocs();
  size_t chunk_count = 2 * thread_count;
  Reader reader;
  if (!reader_init(&reader, &argv[1], (size_t)(argc - 1))) {
    fprintf(stderr, "pzip: cannot open file\n");
    return 1;
  };

  Context *work_ring =
      mmap(NULL, sizeof(Context) + (chunk_count * sizeof(Chunk)),
           PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (work_ring == MAP_FAILED) {
    return 1;
  }
  work_ring->count = chunk_count;

  work_ring_init(work_ring, &reader);

  thrd_t threads[thread_count];
  for (size_t i = 0; i < thread_count; i++) {
    thrd_create(&threads[i], worker, work_ring);
  }

  Run last_run = {};
  bool first_chunk = true;
  bool running = true;
  while (running) {
    for (size_t index = 0; index < work_ring->count; index++) {
      Chunk *chunk = &work_ring->chunks[index];
      if (chunk->state == CHUNK_EMPTY) {
        running = false;
        break;
      }

      mtx_lock(&chunk->lock);
      while (chunk->state != CHUNK_DONE) {
        cnd_wait(&chunk->done, &chunk->lock);
      }

      if (chunk->run_count > 1) {
        if (!first_chunk) {
          Run *first_run = &chunk->runs[0];
          if (first_run->character == last_run.character) {
            first_run->length += last_run.length;
          } else {
            fwrite(&last_run, sizeof(Run), 1, stdout);
          }
        }

        fwrite(&chunk->runs[0], sizeof(Run), chunk->run_count - 1, stdout);
      }

      memcpy(&last_run, &chunk->runs[chunk->run_count - 1], sizeof(Run));
      first_chunk = false;
      size_t n = reader_read_chunk(&reader, chunk->raw);
      if (n > 0) {
        chunk->raw_len = n;
        chunk->state = CHUNK_READY;
      } else {
        chunk->state = CHUNK_EMPTY;
      }

      mtx_unlock(&chunk->lock);
    }
  }

  if (!first_chunk) {
    fwrite(&last_run, sizeof(Run), 1, stdout);
  }
}
