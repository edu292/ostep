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

  int in_fd = open(argv[1], O_RDONLY);
  if (in_fd < 0) {
    perror("input open failed");
    return EXIT_FAILURE;
  }

  struct stat st;
  if (fstat(in_fd, &st) == -1) {
    perror("stat failed");
    return EXIT_FAILURE;
  }

  size_t in_size = (size_t)st.st_size;

  if (in_size == 0) {
    fprintf(stderr, "input file size is too small\n");
  }

  int out_fd = open(argv[2], O_TRUNC | O_RDWR | O_CREAT, 0644);
  if (out_fd < 0) {
    perror("out open failed");
    return EXIT_FAILURE;
  }

  const uint8_t *data =
      mmap(nullptr, in_size, PROT_READ, MAP_PRIVATE, in_fd, 0);

  if (close(in_fd) == -1) {
    perror("in close failed");
    return EXIT_FAILURE;
  };

  if (data == MAP_FAILED) {
    perror("mmap failed");
    return EXIT_FAILURE;
  }

  size_t out_size = (in_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
  if (ftruncate(out_fd, (off_t)out_size) == -1) {
    perror("truncate failed");
    return EXIT_FAILURE;
  }

  uint8_t *checksums =
      mmap(nullptr, out_size, PROT_WRITE, MAP_SHARED, out_fd, 0);

  if (close(out_fd)) {
    perror("out close failed");
    return EXIT_FAILURE;
  }

  if (checksums == MAP_FAILED) {
    perror("out mmap failed");
    return EXIT_FAILURE;
  }

  const uint8_t *ptr = data;
  size_t len = in_size;
  size_t block_idx = 0;
  while (len > 0) {
    uint8_t checksum = 0;
    size_t chunk_len = len >= CHUNK_SIZE ? CHUNK_SIZE : len;
    for (size_t i = 0; i < chunk_len; i++) {
      checksum ^= *ptr++;
    }
    len -= chunk_len;

    checksums[block_idx++] = checksum;
  }

  munmap((void *)data, in_size);
  munmap((void *)checksums, out_size);
}
