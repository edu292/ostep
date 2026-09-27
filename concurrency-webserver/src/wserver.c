#include "io_helper.h"
#include "request.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

typedef struct {
  cnd_t empty;
  cnd_t full;
  mtx_t lock;
  size_t capacity;
  size_t count;
  size_t head;
  size_t tail;
  int data[];
} Queue;

void queue_init(Queue *q, size_t capacity) {
  mtx_init(&q->lock, mtx_plain);
  cnd_init(&q->empty);
  cnd_init(&q->full);

  q->capacity = capacity;
  q->count = 0;
  q->head = 0;
  q->tail = 0;
}

void queue_push(Queue *q, int val) {
  mtx_lock(&q->lock);
  while (q->count == q->capacity) {
    cnd_wait(&q->full, &q->lock);
  }

  q->data[q->tail] = val;
  q->tail = q->tail + 1 < q->capacity ? q->tail + 1 : 0;
  q->count++;

  mtx_unlock(&q->lock);

  cnd_signal(&q->empty);
}

int queue_pop(Queue *q) {
  mtx_lock(&q->lock);
  while (q->count == 0) {
    cnd_wait(&q->empty, &q->lock);
  }

  int popped = q->data[q->head];
  q->head = q->head + 1 < q->capacity ? q->head + 1 : 0;
  q->count--;

  mtx_unlock(&q->lock);

  cnd_signal(&q->full);

  return popped;
}

void queue_destroy(Queue *q) {
  mtx_destroy(&q->lock);
  cnd_destroy(&q->empty);
  cnd_destroy(&q->full);
}

int worker(void *q) {
  Queue *queue = (Queue *)q;
  int conn_fd;
  while ((conn_fd = queue_pop(queue)) != -1) {
    request_handle(conn_fd);
    close_or_die(conn_fd);
  }

  return 0;
}

typedef enum { SCHED_FIFO, SCHED_SFF } SCHED_ALG;
char default_root[] = ".";

//
// ./wserver [-d <basedir>] [-p <portnum>] [-t <threadnum> ] [-b <bufnum> ] [-s
// <schedalg> ]
//
int main(int argc, char *argv[]) {
  int c;
  char *root_dir = default_root;
  int port = 10000;
  int threads = 1;
  int buffers = 1;
  [[maybe_unused]] SCHED_ALG schedalg = SCHED_FIFO;

  while ((c = getopt(argc, argv, "d:p:t:b:s:")) != -1) {
    switch (c) {
    case 'd':
      root_dir = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 't':
      threads = atoi(optarg);
      break;
    case 'b':
      buffers = atoi(optarg);
      break;
    case 's':
      if (strcmp(optarg, "FIFO") == 0) {
        schedalg = SCHED_FIFO;
      } else if (strcmp(optarg, "SFF") == 0) {
        schedalg = SCHED_SFF;
      } else {
        fprintf(stderr, "Invalid Scheduling Algorithm. Options: FIFO, SFF\n");
        return EXIT_FAILURE;
      }
      break;
    default:
      fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t threads] [-b "
                      "buffers] [-s schedalg]\n");
      exit(1);
    }
  }

  // run out of this directory
  chdir_or_die(root_dir);

  Queue queue;
  queue_init(&queue, (size_t)buffers);

  thrd_t workers[threads];
  for (int i = 0; i < threads; i++) {
    thrd_create(&workers[i], worker, &queue);
  }

  // now, get to work
  int listen_fd = open_listen_fd_or_die(port);
  while (1) {
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    int conn_fd = accept_or_die(listen_fd, (sockaddr_t *)&client_addr,
                                (socklen_t *)&client_len);
    queue_push(&queue, conn_fd);
  }
  return 0;
}
