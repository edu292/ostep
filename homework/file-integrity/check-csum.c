#include <bits/time.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

constexpr size_t CHUNK_SIZE = 4UL * 1024;

int main(int argc, char *argv[]) {
  if (argc != 3) {
    return EXIT_FAILURE;
  }

  int data_fd = open(argv[1], O_RDONLY);
  if (data_fd < 0) {
    perror("data open failed");
    return EXIT_FAILURE;
  }

  struct stat data_st;
  if (fstat(data_fd, &data_st) == -1) {
    perror("stat failed");
    return EXIT_FAILURE;
  }

  size_t data_size = (size_t)data_st.st_size;

  if (data_size == 0) {
    fprintf(stderr, "input file size is too small\n");
  }

  int sum_fd = open(argv[2], O_RDONLY);
  if (sum_fd < 0) {
    perror("sum open failed");
    return EXIT_FAILURE;
  }

  struct stat sum_st;
  if (fstat(sum_fd, &sum_st) == -1) {
    perror("stat failed");
    return EXIT_FAILURE;
  }

  size_t sum_size = (size_t)sum_st.st_size;

  if (sum_size != (data_size + CHUNK_SIZE - 1) / CHUNK_SIZE) {
    fprintf(stderr, "sizes of data and checksum file don't match\n");
    return EXIT_FAILURE;
  }

  const uint8_t *data =
      mmap(nullptr, data_size, PROT_READ, MAP_PRIVATE, data_fd, 0);

  if (close(data_fd) == -1) {
    perror("in close failed");
    return EXIT_FAILURE;
  };

  if (data == MAP_FAILED) {
    perror("mmap failed");
    return EXIT_FAILURE;
  }

  uint8_t *checksums =
      mmap(nullptr, sum_size, PROT_READ, MAP_PRIVATE, sum_fd, 0);

  if (close(sum_fd) == -1) {
    perror("out close failed");
    return EXIT_FAILURE;
  }

  if (checksums == MAP_FAILED) {
    perror("out mmap failed");
    return EXIT_FAILURE;
  }

  const uint8_t *ptr = data;
  size_t len = data_size;
  size_t block_idx = 0;
  while (len > 0) {
    uint8_t checksum = 0;
    size_t chunk_len = len >= CHUNK_SIZE ? CHUNK_SIZE : len;
    for (size_t i = 0; i < chunk_len; i++) {
      checksum ^= *ptr++;
    }
    len -= chunk_len;

    if (checksum != checksums[block_idx]) {
      size_t offset = (size_t)(ptr - data) - chunk_len;
      fprintf(stderr,
              "Checksum mismatch on block %zu (offset %zu): expected 0x%02x, "
              "got 0x%02x\n",
              block_idx, offset, checksums[block_idx], checksum);
      return EXIT_FAILURE;
    }
    block_idx++;
  }
  munmap((void *)data, data_size);
  munmap((void *)checksums, sum_size);
}
