#include <alloca.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

volatile sig_atomic_t keep_running = 1;

void timer_handler(int signum) { keep_running = 0; }

void print_usage() {
  fprintf(stderr, "Usage: memory-user [MEMORY FLAG] [OPTIONAL TIME FLAG]\n\n"
                  "Memory Flags (exactly one required):\n"
                  "  -b <bytes>      Allocation size in bytes\n"
                  "  -k <kilobytes>  Allocation size in KB\n"
                  "  -m <megabytes>  Allocation size in MB\n"
                  "  -g <gigabytes>  Allocation size in GB\n\n"
                  "Time Flags (optional, defaults to infinite):\n"
                  "  -s <seconds>    Duration in seconds\n"
                  "  -M <minutes>    Duration in minutes\n"
                  "  -H <hours>      Duration in hours\n\n"
                  "Help:\n"
                  "  -h              Show this help message\n");
}

int main(int argc, char *argv[]) {
  size_t bytes = 0;
  long seconds = -1;
  bool mem_set = false;
  int opt;

  opterr = 0;

  while ((opt = getopt(argc, argv, "b:k:m:g:s:M:H:h")) != -1) {
    switch (opt) {
    // Memory Flags
    case 'b':
      if (mem_set)
        goto invalid;
      bytes = (size_t)atoll(optarg);
      mem_set = true;
      break;
    case 'k':
      if (mem_set)
        goto invalid;
      bytes = (size_t)atoll(optarg) * 1024ULL;
      mem_set = true;
      break;
    case 'm':
      if (mem_set)
        goto invalid;
      bytes = (size_t)atoll(optarg) * 1024ULL * 1024ULL;
      mem_set = true;
      break;
    case 'g':
      if (mem_set)
        goto invalid;
      bytes = (size_t)atoll(optarg) * 1024ULL * 1024ULL * 1024ULL;
      mem_set = true;
      break;

    // Time Flags
    case 's':
      seconds = atol(optarg);
      break;
    case 'M':
      seconds = atol(optarg) * 60L;
      break;
    case 'H':
      seconds = atol(optarg) * 3600L;
      break;

    case 'h':
      print_usage();
      return 0;
    default:
      goto invalid;
    }
  }

  if (!mem_set || bytes == 0 || optind < argc) {
    goto invalid;
  }

  printf("PID: %u\n", getpid());

  if (seconds != -1) {
    signal(SIGALRM, timer_handler);
    alarm(seconds);
  }

  char *aloc = malloc(bytes);
  if (aloc == NULL) {
    perror("failed to allocate memory");
    return 1;
  }

  while (keep_running) {
    for (size_t i = 0; i < bytes && keep_running; i++) {
      *(volatile char *)&aloc[i] = 'z';
    }
  }
  free(aloc);

  return 0;

invalid:
  print_usage();
  return 1;
}
