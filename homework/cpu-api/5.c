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
    int child = wait(NULL);
    printf("pid was: %d\n", child);
    printf("hello from parent after wait\n");
  }

  return 0;
}

/*
Now write a program that uses wait() to wait for the child process
to finish in the parent. What does wait() return? What happens if
you use wait() in the child?

R: wait returns the pid of the first child process that finished execution.
Calling wait on a child process or any process that does not not itself have
forks results in an error and it immeadiately returns -1, also setting errno
to specify the no children error
 */
