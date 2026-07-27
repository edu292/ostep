#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int pid = fork();
  if (pid < 0) {
    fprintf(stderr, "fork failed\n");
    return 1;
  }

  if (pid == 0) {
    printf("hello\n");
  } else {
    struct timespec ts;
    nanosleep(&(struct timespec){.tv_nsec = 100000}, NULL);
    printf("goodbye\n");
  }

  return 0;
}

/*
Write another program using fork(). The child process should
print “hello”; the parent process should print “goodbye”. You should
try to ensure that the child process always prints first; can you do
this without calling wait() in the parent?
 */
