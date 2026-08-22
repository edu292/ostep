#include "common_threads.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>

sem_t s;

void *child(void *v) {
  printf("child\n");
  sleep(1);
  sem_post(&s);
  return NULL;
}

int main(void) {
  pthread_t p = 0;
  printf("parent: begin\n");
  sem_init(&s, 0, 0);
  Pthread_create(&p, NULL, child, NULL);
  sem_wait(&s);
  printf("parent: end\n");
  return 0;
}
