#define _GNU_SOURCE
#include <getopt.h>
#include <sched.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define PAGESIZE 4096

int main(int argc, char *argv[]) {

  size_t number_of_pages = 0;
  size_t tries = 0;
  char *endptr = NULL;
  int c = 0;
  while ((c = getopt(argc, argv, "p:t:")) != -1) {
    switch (c) {
    case 'p':
      number_of_pages = strtoul(optarg, &endptr, 10);
      break;
    case 't':
      tries = strtoul(optarg, &endptr, 10);
      break;
    }
  }
  if (number_of_pages == 0 || tries == 0) {
    return 1;
  }

  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(0, &mask);
  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
    perror("sched_setaffinity failed");
    return 1;
  }

  int *a = malloc(number_of_pages * PAGESIZE);
  if (a == NULL) {
    perror("failed to alloate necessary buffer");
    return 1;
  }

  size_t jump = PAGESIZE / sizeof(int);
  size_t total_elements = number_of_pages * jump;
  for (size_t j = 0; j < total_elements; j += jump) {
    a[j] = 0;
  }

  struct timespec start;
  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  for (size_t i = 0; i < tries; i++) {
    for (size_t j = 0; j < total_elements; j += jump) {
      *((volatile int *)&a[j]) += 1;
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &end);

  double total_sec =
      (end.tv_sec - start.tv_sec) + ((end.tv_nsec - start.tv_nsec) / 1e9);
  double average_ns =
      (total_sec / (double)tries / (double)number_of_pages) * 1e9;

  free(a);
  printf("%lf", average_ns);
}
