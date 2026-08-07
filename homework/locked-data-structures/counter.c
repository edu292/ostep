#include <errno.h>
#include <getopt.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

#define ADD 1000000UL
#define THRESHOLD 64

typedef void (*AddFunc)(void);

volatile atomic_size_t counter = 0;
size_t threshold = THRESHOLD;
mtx_t counter_mtx;
mtx_t init_mtx;
cnd_t init_cnd;
bool init = false;

void approximate_add() {
  size_t local_counter = 0;
  for (size_t i = 0; i < ADD; i++) {
    if (++local_counter == threshold) {
      mtx_lock(&counter_mtx);
      counter += local_counter;
      mtx_unlock(&counter_mtx);
      local_counter = 0;
    }
  }

  if (local_counter > 0) {
    mtx_lock(&counter_mtx);
    counter += local_counter;
    mtx_unlock(&counter_mtx);
  }
}

void precise_add() {
  for (size_t i = 0; i < ADD; i++) {
    mtx_lock(&counter_mtx);
    counter++;
    mtx_unlock(&counter_mtx);
  }
}

void atomic_add() {
  for (size_t i = 0; i < ADD; i++) {
    atomic_fetch_add(&counter, 1);
  }
}

int worker(void *f) {
  AddFunc add_func = (AddFunc)f;
  mtx_lock(&init_mtx);
  while (!init) {
    cnd_wait(&init_cnd, &init_mtx);
  }
  mtx_unlock(&init_mtx);

  add_func();

  return 0;
}

int main(int argc, char *argv[]) {
  AddFunc add_func = NULL;
  size_t thread_count = 0;
  char *endptr = NULL;

  int c = 0;
  while ((c = getopt(argc, argv, "a:t:")) != -1) {
    switch (c) {
    case 'a':
      if (strcmp(optarg, "precise") == 0) {
        add_func = precise_add;
      } else if (strcmp(optarg, "approximate") == 0) {
        add_func = approximate_add;
      } else if (strcmp(optarg, "atomic") == 0) {
        add_func = atomic_add;
      } else {
        fprintf(stderr,
                "Invalid add func type %s. Expected precise, approximate or "
                "atomic\n",
                optarg);
        return 1;
      }
      break;
    case 't':
      thread_count = strtoul(optarg, &endptr, 10);

      if (errno != 0) {
        perror("");
        return 1;
      }
      break;
    }
  }

  if (add_func == NULL || thread_count == 0) {
    fprintf(stderr, "Missing parameters\n");
    return 1;
  }

  mtx_init(&init_mtx, mtx_plain);
  mtx_init(&counter_mtx, mtx_plain);
  cnd_init(&init_cnd);

  thrd_t threads[thread_count];
  for (size_t i = 0; i < thread_count; i++) {
    thrd_create(&threads[i], worker, (void *)add_func);
  }

  struct timespec start;
  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  mtx_lock(&init_mtx);
  init = true;
  cnd_broadcast(&init_cnd);
  mtx_unlock(&init_mtx);

  for (size_t i = 0; i < thread_count; i++) {
    thrd_join(threads[i], NULL);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  double total_sec =
      (end.tv_sec - start.tv_sec) + ((end.tv_nsec - start.tv_nsec) / 1e9);
  double average_ns = total_sec * 1e9;
  printf("%lf", average_ns);

  mtx_destroy(&init_mtx);
  cnd_destroy(&init_cnd);
  return 0;
}
