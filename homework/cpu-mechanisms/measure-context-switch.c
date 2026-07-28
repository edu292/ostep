#define _GNU_SOURCE
#include <sched.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define ITERATIONS 1000000L

int main() {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(0, &mask);

  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
    perror("sched_setaffinity failed");
    return 1;
  }

  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
    perror("socket failed");
    return 1;
  }

  int child_pid = fork();
  if (child_pid == -1) {
    perror("fork failed");
    return 1;
  }

  if (child_pid == 0) {
    close(sv[0]);
    char *message = "pong";
    char response[5];
    for (size_t i = 0; i < ITERATIONS; i++) {
      read(sv[1], response, sizeof(response));
      write(sv[1], message, 5);
    }
    close(sv[1]);
    return 0;
  }

  close(sv[1]);
  char *message = "ping";
  char response[5];
  struct timespec before, after;
  clock_gettime(CLOCK_MONOTONIC, &before);
  for (size_t i = 0; i < ITERATIONS; i++) {
    write(sv[0], message, 5);
    read(sv[0], response, sizeof(response));
  }
  clock_gettime(CLOCK_MONOTONIC, &after);
  double total_sec =
      (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) / 1e9;
  double average_ns = (total_sec / ITERATIONS) * 1e9 / 2.0;
  printf("The cost of context switching is %f ns\n", average_ns);
}
