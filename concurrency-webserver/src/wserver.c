#include "io_helper.h"
#include "request.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

typedef enum { POLICY_FIFO, POLICY_SFF } Policy;

typedef struct {
  cnd_t not_empty;
  cnd_t not_full;
  mtx_t lock;
  size_t capacity;
  size_t count;
  Policy policy;
  Request **data;
} PriorityQueue;

void queue_init(PriorityQueue *pq, size_t capacity, Policy policy) {
  mtx_init(&pq->lock, mtx_plain);
  cnd_init(&pq->not_empty);
  cnd_init(&pq->not_full);
  pq->data = malloc(capacity * sizeof(char *));

  pq->policy = policy;
  pq->capacity = capacity;
  pq->count = 0;
}

void queue_push(PriorityQueue *pq, Request *val) {
  mtx_lock(&pq->lock);
  while (pq->count == pq->capacity) {
    cnd_wait(&pq->not_full, &pq->lock);
  }

  switch (pq->policy) {
  case POLICY_FIFO:
    pq->data[pq->count] = val;
    break;
  case POLICY_SFF:
    size_t index = pq->count;
    long insert_file_size = val->sbuf.st_size;
    while (index > 0) {
      size_t parent_index = (index - 1) / 2;
      Request *parent = pq->data[parent_index];
      if (parent->sbuf.st_size < insert_file_size) {
        break;
      }

      pq->data[index] = parent;
      index = parent_index;
    }
    pq->data[index] = val;
    break;
  }

  pq->count++;

  mtx_unlock(&pq->lock);

  cnd_signal(&pq->not_empty);
}

Request *queue_pop(PriorityQueue *pq) {
  mtx_lock(&pq->lock);
  while (pq->count == 0) {
    cnd_wait(&pq->not_empty, &pq->lock);
  }

  Request *popped = pq->data[0];
  pq->count--;
  switch (pq->policy) {
  case POLICY_FIFO:
    memmove(&pq->data[0], &pq->data[1], pq->count * sizeof(Request *));
    break;
  case POLICY_SFF:
    pq->data[0] = pq->data[pq->count];
    size_t index = 0;

    while (1) {
      size_t left = (2 * index) + 1;
      size_t right = (2 * index) + 2;
      size_t smallest = index;

      if (left < pq->count &&
          pq->data[left]->sbuf.st_size < pq->data[smallest]->sbuf.st_size) {
        smallest = left;
      }

      if (right < pq->count &&
          pq->data[right]->sbuf.st_size < pq->data[smallest]->sbuf.st_size) {
        smallest = right;
      }

      if (smallest == index) {
        break;
      }

      Request *tmp = pq->data[index];
      pq->data[index] = pq->data[smallest];
      pq->data[smallest] = tmp;

      index = smallest;
    }
  }

  mtx_unlock(&pq->lock);

  cnd_signal(&pq->not_full);

  return popped;
}

void queue_destroy(PriorityQueue *pq) {
  mtx_destroy(&pq->lock);
  cnd_destroy(&pq->not_empty);
  cnd_destroy(&pq->not_full);
}

int worker(void *q) {
  PriorityQueue *queue = (PriorityQueue *)q;
  Request *request;
  while ((request = queue_pop(queue)) != nullptr) {
    request_handle(request);
    close_or_die(request->conn_fd);
    free(request);
  }

  return 0;
}

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
  Policy schedalg = POLICY_FIFO;

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
        schedalg = POLICY_FIFO;
      } else if (strcmp(optarg, "SFF") == 0) {
        schedalg = POLICY_SFF;
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

  PriorityQueue queue;
  queue_init(&queue, (size_t)buffers, schedalg);

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
    Request *request = malloc(sizeof(Request));
    if (request_parse(conn_fd, request)) {
      queue_push(&queue, request);
    }
  }
  queue_destroy(&queue);
  return 0;
}
