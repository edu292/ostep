#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int pid = fork();
  if (pid < 0) {
    fprintf(stderr, "fork failed\n");
    return 1;
  }

  if (pid == 0) {
    printf("hello from child\n");
    int x = wait(NULL);
    printf("return from wait was: %d\n", x);
    printf("hello from child after wait\n");
  } else {
    printf("hello from parent\n");
    int child = waitpid(pid, NULL, 0);
    printf("pid was: %d\n", child);
    printf("hello from parent after wait\n");
  }

  return 0;
}

/*
Write a slight modification of the previous program, this time us-
ing waitpid() instead of wait(). When would waitpid() be
useful?

R: waitpid lets you specify which specific child to wait for, instead
of just waiting for the first that finishes. It can be useful in any situation
where you might create multiple forks, but the parent has a dependency in just
one of them.
 */

