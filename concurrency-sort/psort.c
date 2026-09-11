#include <dlfcn.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
  uint32_t key;
  char data[96];
} Record;

void quicksort_(size_t low, size_t high, Record array[]) {
  if (high <= low) {
    return;
  }

  size_t mid_idx = low + ((high - low) / 2);
  uint32_t first = array[low].key;
  uint32_t mid = array[mid_idx].key;
  uint32_t last = array[high].key;

  uint32_t pivot = mid;
  if ((first <= last && first >= mid) || (first >= last && first <= mid)) {
    pivot = first;
  } else if ((last <= first && last >= mid) || (last >= first && last <= mid)) {
    pivot = last;
  }

  size_t i = low;
  size_t j = high;
  while (true) {
    while (array[i].key < pivot) {
      i++;
    }

    while (array[j].key > pivot) {
      j--;
    }

    if (i >= j) {
      break;
    }

    Record temp = array[i];
    array[i] = array[j];
    array[j] = temp;

    i++;
    j--;
  }

  quicksort_(low, j, array);
  quicksort_(j + 1, high, array);
}

void quicksort(size_t count, Record array[count]) {
  quicksort_(0, count - 1, array);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: psort input output\n");
    return EXIT_FAILURE;
  }

  char *in_filename = argv[1];
  int in_fd = open(in_filename, O_RDONLY);
  if (in_fd < 0) {
    fprintf(stderr, "psort: could not open file\n");
    return EXIT_FAILURE;
  }

  char *out_filename = argv[2];
  int out_fd = open(out_filename, O_WRONLY | O_TRUNC | O_CREAT, 0644);
  if (out_fd < 0) {
    fprintf(stderr, "psort: could not open file\n");
    return EXIT_FAILURE;
  }

  struct stat st;
  fstat(in_fd, &st);
  size_t data_size = (size_t)st.st_size;
  if (data_size == 0) {
    close(in_fd);
    close(out_fd);
    return EXIT_SUCCESS;
  }

  Record *data = mmap(nullptr, data_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON, -1, 0);
  if (data == MAP_FAILED) {
    perror("mmap failed");
    return EXIT_FAILURE;
  }
  read(in_fd, data, data_size);

  close(in_fd);
  madvise(data, data_size, MADV_SEQUENTIAL);

  size_t record_count = data_size / sizeof(Record);
  quicksort(record_count, data);

  write(out_fd, data, data_size);
  close(out_fd);
  munmap(data, data_size);

  return EXIT_SUCCESS;
}
