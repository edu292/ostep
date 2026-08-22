#include "common_threads.h"
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//
// Here, you have to write (almost) ALL the code. Oh no!
// How can you show that a thread does not starve
// when attempting to acquire this mutex you build?
//

typedef struct SemNode {
  sem_t sem;
  struct SemNode *next;
} SemNode;

typedef struct {
  SemNode *head;
  SemNode *tail;
  sem_t lock;
  bool first;
} ns_mutex_t;

void ns_mutex_init(ns_mutex_t *m) {
  m->head = NULL;
  m->tail = NULL;
  m->first = true;
  sem_init(&m->lock, 0, 1);
}

void ns_mutex_acquire(ns_mutex_t *m) {

  sem_wait(&m->lock);
  if (m->first) {
    m->first = false;
    sem_post(&m->lock);
    return;
  }

  SemNode node;
  sem_init(&node.sem, 0, 0);
  node.next = NULL;

  if (m->tail == NULL) {
    m->head = &node;
  } else {
    m->tail->next = &node;
  }

  m->tail = &node;
  sem_post(&m->lock);

  sem_wait(&node.sem);
  sem_destroy(&node.sem);
}

void ns_mutex_release(ns_mutex_t *m) {
  sem_wait(&m->lock);
  if (m->head == NULL) {
    m->first = true;
    sem_post(&m->lock);
    return;
  }

  SemNode *old_head = m->head;
  SemNode *new_head = old_head->next;
  if (new_head == NULL) {
    m->tail = NULL;
  }

  m->head = new_head;
  sem_post(&m->lock);
  sem_post(&old_head->sem);
}

#define NUM_GREEDY 10
#define ITERATIONS 100000

ns_mutex_t mtx;
volatile bool victim_done = false;
volatile size_t greedy_acquisitions = 0;

void *greedy_worker(void *arg) {
  (void)arg;
  for (int i = 0; i < ITERATIONS && !victim_done; i++) {
    ns_mutex_acquire(&mtx);
    greedy_acquisitions++;
    ns_mutex_release(&mtx);
  }
  return NULL;
}

void *victim_worker(void *arg) {
  (void)arg;
  usleep(1000);

  ns_mutex_acquire(&mtx);
  victim_done = true;
  printf("Victim acquired lock! Greedy acquisitions before entry: %zu\n",
         greedy_acquisitions);
  ns_mutex_release(&mtx);

  return NULL;
}

int main(void) {
  ns_mutex_init(&mtx);
  pthread_t greedy[NUM_GREEDY];
  pthread_t victim = 0;

  for (int i = 0; i < NUM_GREEDY; i++) {
    Pthread_create(&greedy[i], NULL, greedy_worker, NULL);
  }
  Pthread_create(&victim, NULL, victim_worker, NULL);

  Pthread_join(victim, NULL);
  for (int i = 0; i < NUM_GREEDY; i++) {
    Pthread_join(greedy[i], NULL);
  }

  printf("Test passed: Victim completed without infinite wait.\n");
  return 0;
}
