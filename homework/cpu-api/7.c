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
    fclose(stdout);
    printf("hello from child\n");
  } else {
    printf("hello from parent\n");
  }

  return 0;
}

/*
Write a program that creates a child process, and then in the child
closes standard output (STDOUT_FILENO). What happens if the child
calls printf() to print some output after closing the descriptor?

R: The output doesn't go anywhere and the call to printf has no visible
effect
*/
