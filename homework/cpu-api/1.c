#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int x;

int main(void) {
  x = 100;
  printf("Setting the value of x to %d\n", x);
  int pid = fork();
  if (pid < 0) {
    fprintf(stderr, "fork failed\n");
    return 1;
  }

  if (pid == 0) {
    printf("The value of x was: %d (from child)\n", x);
    x = 200;
    printf("Setting it to: %d (from child)\n", x);
  }
  wait(NULL);
  printf("Value of x after infant death: %d\n", x);
  return 0;
}

/*
Write a program that calls fork(). Before calling fork(), have the
main process access a variable (e.g., x) and set its value to some-
thing (e.g., 100). What value is the variable in the child process?
What happens to the variable when both the child and parent change
the value of x?

R: The value of x is independendant on parent and children. The initial
value on forked process is the same it was before fork, but changes don't
affect parent.
*/
