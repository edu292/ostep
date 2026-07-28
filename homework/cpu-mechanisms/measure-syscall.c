#include <bits/time.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#define ITERATIONS 10000000L

int main() {
  struct timespec before, after;

  for (size_t i = 0; i < 100; i++) {
    syscall(SYS_getpid);
  }

  clock_gettime(CLOCK_MONOTONIC, &before);
  for (size_t i = 0; i < ITERATIONS; i++) {
    syscall(SYS_getpid);
  }
  clock_gettime(CLOCK_MONOTONIC, &after);

  double total_sec =
      (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) / 1e9;
  double average_ns = (total_sec / ITERATIONS) * 1e9;
  printf("The cost of a syscall is %f ns\n", average_ns);
  return 0;
}
