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

#pragma pack(push, 1)
typedef struct {
  uint32_t length;
  char character;
} Run;
#pragma pack(pop)

typedef struct {
  char *data;
  size_t length;
  size_t run_count;
  Run dest[];
} Job;

int process_job(void *j) {
  Job *job = (Job *)j;
  char *end = job->data + job->length;
  Run *dest_ptr = job->dest;

  Run run = {.length = 1, .character = *job->data};
  for (char *current_ptr = job->data + 1; current_ptr <= end; current_ptr++) {
    char current_char = current_ptr < end ? *current_ptr : '\0';
    if (current_char == run.character) {
      run.length++;
      continue;
    }

    memcpy(dest_ptr, &run, sizeof(Run));
    dest_ptr++;

    job->run_count++;
    run.character = current_char;
    run.length = 1;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "pzip: file1 [file2 ...]\n");
    return 1;
  }

  size_t thread_count = (size_t)get_nprocs();
  thrd_t threads[thread_count];
  Run last_run = {};
  bool first_file = true;
  for (int file_index = 1; file_index < argc; file_index++) {
    int fd = open(argv[file_index], O_RDONLY);
    if (fd == -1) {
      fprintf(stderr, "pzip: cannot open file\n");
      return 1;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
      return 1;
    }

    size_t file_size = (size_t)st.st_size;
    if (file_size == 0) {
      close(fd);
      continue;
    }

    char *file = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == MAP_FAILED) {
      perror("");
      return 1;
    }

    size_t arena_size =
        (sizeof(Run) * file_size) + (sizeof(Job) * thread_count);
    char *job_arena = mmap(NULL, arena_size, PROT_WRITE | PROT_READ,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (job_arena == MAP_FAILED) {
      perror("");
      return 1;
    }

    size_t job_length = file_size / thread_count;
    char *job_data = file;
    char *job_cursor = job_arena;
    for (size_t i = 0; i < thread_count; i++) {
      Job *job = (Job *)job_cursor;
      job->length = i < thread_count - 1
                        ? job_length
                        : file_size - (job_length * (thread_count - 1));
      job->data = job_data;
      job_data += job_length;
      job_cursor += (job->length * sizeof(Run)) + sizeof(Job);

      if (thrd_create(&threads[i], process_job, (void *)job) != 0) {
        perror("");
        return 1;
      }
    };

    job_cursor = job_arena;
    for (size_t i = 0; i < thread_count; i++) {
      thrd_join(threads[i], NULL);
      Job *job = (Job *)job_cursor;

      if (job->run_count == 0) {
        job_cursor += (job->length * sizeof(Run)) + sizeof(Job);
        continue;
      }

      if (!first_file) {
        Run *first_run = &job->dest[0];
        if (first_run->character == last_run.character) {
          first_run->length += last_run.length;
        } else {
          fwrite(&last_run, sizeof(Run), 1, stdout);
        }
      }

      if (job->run_count > 1) {
        fwrite(&job->dest[0], sizeof(Run), job->run_count - 1, stdout);
      }

      memcpy(&last_run, &job->dest[job->run_count - 1], sizeof(Run));
      first_file = false;
      job_cursor += (job->length * sizeof(Run)) + sizeof(Job);
    }

    munmap(job_arena, arena_size);
    munmap(file, file_size);
    close(fd);
  }

  if (!first_file) {
    fwrite(&last_run, sizeof(Run), 1, stdout);
  }
}
