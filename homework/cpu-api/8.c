#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int fides[2];
  pipe(fides);

  int pid0 = fork();
  if (pid0 < 0) {
    fprintf(stderr, "fork failed\n");
    return 1;
  }

  if (pid0 == 0) {
    close(fides[0]);
    char *c = "hello, child 1\n";
    write(fides[1], c, strlen(c));
    return 0;
  }

  int pid1 = fork();
  if (pid1 < 0) {
    fprintf(stderr, "fork failed\n");
    return 1;
  }

  if (pid1 == 0) {
    close(fides[1]);
    char buf[512];
    read(fides[0], buf, 512);
    printf("child 0 said: %s", buf);
    return 0;
  }

  waitpid(pid1, NULL, 0);

  return 0;
}

/*
Write a program that creates two children, and connects the stan-
dard output of one to the standard input of the other, using the
pipe() system call.
*/
