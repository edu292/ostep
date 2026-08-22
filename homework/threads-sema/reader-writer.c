#include "common_threads.h"
#include <semaphore.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//
// Your code goes in the structure and functions below
//

typedef struct {
  sem_t struc_lock;
  sem_t write_lock;
  size_t readers;
} rwlock_t;

void rwlock_init(rwlock_t *rw) {
  sem_init(&rw->struc_lock, 0, 1);
  sem_init(&rw->write_lock, 0, 1);
  rw->readers = 0;
}

void rwlock_acquire_readlock(rwlock_t *rw) {
  sem_wait(&rw->struc_lock);
  rw->readers++;
  if (rw->readers == 1) {
    sem_wait(&rw->write_lock);
  }
  sem_post(&rw->struc_lock);
}

void rwlock_release_readlock(rwlock_t *rw) {
  sem_wait(&rw->struc_lock);
  rw->readers--;
  if (rw->readers == 0) {
    sem_post(&rw->write_lock);
  }
  sem_post(&rw->struc_lock);
}

void rwlock_acquire_writelock(rwlock_t *rw) { sem_wait(&rw->write_lock); }

void rwlock_release_writelock(rwlock_t *rw) { sem_post(&rw->write_lock); }

//
// Don't change the code below (just use it!)
//

int loops;
int value = 0;

rwlock_t lock;

void *reader(void *arg) {
  for (int i = 0; i < loops; i++) {
    rwlock_acquire_readlock(&lock);
    printf("read %d\n", value);
    rwlock_release_readlock(&lock);
  }
  return NULL;
}

void *writer(void *arg) {
  for (int i = 0; i < loops; i++) {
    rwlock_acquire_writelock(&lock);
    value++;
    printf("write %d\n", value);
    rwlock_release_writelock(&lock);
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  assert(argc == 4);
  int num_readers = atoi(argv[1]);
  int num_writers = atoi(argv[2]);
  loops = atoi(argv[3]);

  pthread_t pr[num_readers];
  pthread_t pw[num_writers];

  rwlock_init(&lock);

  printf("begin\n");

  for (int i = 0; i < num_readers; i++)
    Pthread_create(&pr[i], NULL, reader, NULL);
  for (int i = 0; i < num_writers; i++)
    Pthread_create(&pw[i], NULL, writer, NULL);

  for (int i = 0; i < num_readers; i++)
    Pthread_join(pr[i], NULL);
  for (int i = 0; i < num_writers; i++)
    Pthread_join(pw[i], NULL);

  printf("end: value %d\n", value);

  return 0;
}
