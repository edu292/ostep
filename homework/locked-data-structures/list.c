#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

#define DEFAULT_OPS 1000UL
#define PREPOP_SIZE 1000UL

typedef struct Node {
  struct Node *next;
  size_t value;
  mtx_t lock;
} Node;

typedef struct {
  Node *head;
  Node *tail;
  mtx_t head_lock;
  mtx_t tail_lock;
} List;

typedef void (*AppendFn)(List *, size_t);
typedef bool (*LookupFn)(List *, size_t);

static inline Node *create_node(size_t val) {
  Node *node = malloc(sizeof(Node));
  if (node == NULL) {
    return NULL;
  }
  node->value = val;
  node->next = NULL;
  mtx_init(&node->lock, mtx_plain);
  return node;
}

bool list_init(List *l) {
  if (mtx_init(&l->head_lock, mtx_plain) != 0) {
    goto failed_head_lock;
  }

  if (mtx_init(&l->tail_lock, mtx_plain) != 0) {
    goto failed_tail_lock;
  }

  Node *dummy = create_node(UINT32_MAX);
  if (dummy == NULL) {
    goto failed_dummy;
  }

  l->tail = dummy;
  l->head = dummy;
  return true;

failed_dummy:
  mtx_destroy(&l->tail_lock);
failed_tail_lock:
  mtx_destroy(&l->head_lock);
failed_head_lock:
  return false;
}

void single_lock_append(List *l, size_t val) {
  Node *node = create_node(val);
  if (node == NULL) {
    return;
  }

  mtx_lock(&l->head_lock);
  l->tail->next = node;
  l->tail = node;
  mtx_unlock(&l->head_lock);
}

bool single_lock_lookup(List *l, size_t val) {
  mtx_lock(&l->head_lock);
  for (Node *curr = l->head->next; curr != NULL; curr = curr->next) {
    if (curr->value == val) {
      mtx_unlock(&l->head_lock);
      return true;
    }
  }
  mtx_unlock(&l->head_lock);
  return false;
}

void double_lock_append(List *l, size_t val) {
  Node *node = create_node(val);
  if (node == NULL) {
    return;
  }

  mtx_lock(&l->tail_lock);
  l->tail->next = node;
  l->tail = node;
  mtx_unlock(&l->tail_lock);
}

bool double_lock_lookup(List *l, size_t val) {
  mtx_lock(&l->head_lock);
  for (Node *curr = l->head->next; curr != NULL; curr = curr->next) {
    if (curr->value == val) {
      mtx_unlock(&l->head_lock);
      return true;
    }
  }
  mtx_unlock(&l->head_lock);
  return false;
}

void hand_over_hand_append(List *l, size_t val) {
  Node *node = create_node(val);
  if (!node) {
    return;
  }

  mtx_lock(&l->head->lock);
  Node *curr = l->head;

  while (1) {
    Node *next = curr->next;
    if (next == NULL) {
      break;
    }
    mtx_lock(&next->lock);
    mtx_unlock(&curr->lock);
    curr = next;
  }

  curr->next = node;
  l->tail = node;
  mtx_unlock(&curr->lock);
}

bool hand_over_hand_lookup(List *l, size_t val) {
  mtx_lock(&l->head->lock);
  Node *curr = l->head;

  while (1) {
    Node *next = curr->next;
    if (next == NULL) {
      mtx_unlock(&curr->lock);
      return false;
    }
    mtx_lock(&next->lock);
    mtx_unlock(&curr->lock);
    curr = next;

    if (curr->value == val) {
      mtx_unlock(&curr->lock);
      return true;
    }
  }
}

typedef enum { STRAT_SINGLE, STRAT_DOUBLE, STRAT_HAND_OVER_HAND } Strategy;

typedef enum { MODE_APPEND, MODE_GET, MODE_COMBINED } WorkloadMode;

typedef struct {
  List *list;
  AppendFn append_fn;
  LookupFn get_fn;
  WorkloadMode mode;
  size_t thread_id;
  size_t ops;
} WorkerArg;

mtx_t init_mtx;
cnd_t init_cnd;
bool init = false;

int worker(void *arg) {
  WorkerArg *w = (WorkerArg *)arg;

  mtx_lock(&init_mtx);
  while (!init) {
    cnd_wait(&init_cnd, &init_mtx);
  }
  mtx_unlock(&init_mtx);

  if (w->mode == MODE_APPEND) {
    for (size_t i = 0; i < w->ops; i++) {
      w->append_fn(w->list, i);
    }
  } else if (w->mode == MODE_GET) {
    for (size_t i = 0; i < w->ops; i++) {
      w->get_fn(w->list, i % PREPOP_SIZE);
    }
  } else if (w->mode == MODE_COMBINED) {
    if (w->thread_id % 2 == 0) {
      for (size_t i = 0; i < w->ops; i++) {
        w->append_fn(w->list, i);
      }
    } else {
      for (size_t i = 0; i < w->ops; i++) {
        w->get_fn(w->list, i % PREPOP_SIZE);
      }
    }
  }

  return 0;
}

int main(int argc, char *argv[]) {
  Strategy strategy = STRAT_SINGLE;
  WorkloadMode mode = MODE_APPEND;
  size_t thread_count = 1;
  size_t ops_per_thread = DEFAULT_OPS;

  int c = 0;
  while ((c = getopt(argc, argv, "s:m:t:n:")) != -1) {
    switch (c) {
    case 's':
      if (strcmp(optarg, "single") == 0)
        strategy = STRAT_SINGLE;
      else if (strcmp(optarg, "double") == 0)
        strategy = STRAT_DOUBLE;
      else if (strcmp(optarg, "hand-over-hand") == 0)
        strategy = STRAT_HAND_OVER_HAND;
      else {
        fprintf(stderr, "Invalid strategy: %s\n", optarg);
        return 1;
      }
      break;
    case 'm':
      if (strcmp(optarg, "append") == 0)
        mode = MODE_APPEND;
      else if (strcmp(optarg, "get") == 0)
        mode = MODE_GET;
      else if (strcmp(optarg, "combined") == 0)
        mode = MODE_COMBINED;
      else {
        fprintf(stderr, "Invalid mode: %s\n", optarg);
        return 1;
      }
      break;
    case 't':
      thread_count = strtoul(optarg, NULL, 10);
      break;
    case 'n':
      ops_per_thread = strtoul(optarg, NULL, 10);
      break;
    default:
      return 1;
    }
  }

  AppendFn append_fn = NULL;
  LookupFn get_fn = NULL;

  switch (strategy) {
  case STRAT_SINGLE:
    append_fn = single_lock_append;
    get_fn = single_lock_lookup;
    break;
  case STRAT_DOUBLE:
    append_fn = double_lock_append;
    get_fn = double_lock_lookup;
    break;
  case STRAT_HAND_OVER_HAND:
    append_fn = hand_over_hand_append;
    get_fn = hand_over_hand_lookup;
    break;
  }

  List list;
  if (!list_init(&list)) {
    fprintf(stderr, "List initialization failed\n");
    return 1;
  }

  if (mode == MODE_GET || mode == MODE_COMBINED) {
    for (size_t i = 0; i < PREPOP_SIZE; i++) {
      append_fn(&list, i);
    }
  }

  mtx_init(&init_mtx, mtx_plain);
  cnd_init(&init_cnd);

  thrd_t threads[thread_count];
  WorkerArg args[thread_count];

  for (size_t i = 0; i < thread_count; i++) {
    args[i] = (WorkerArg){.list = &list,
                          .append_fn = append_fn,
                          .get_fn = get_fn,
                          .mode = mode,
                          .thread_id = i,
                          .ops = ops_per_thread};
    thrd_create(&threads[i], worker, &args[i]);
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
  printf("%lf\n", average_ns);

  mtx_destroy(&init_mtx);
  cnd_destroy(&init_cnd);
  return 0;
}
