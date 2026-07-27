#include <stdio.h>
#include <unistd.h>

int main() {
  int pid = fork();
  if (pid < 0) {
    fprintf(stderr, "fork failed\n");
    return 1;
  }

  if (pid == 0) {
    execlp("ls", NULL);
    printf("hello from child\n");
  } else {
    printf("hello from parent\n");
  }
}

/*
Write a program that calls fork() and then calls some form of
exec() to run the program /bin/ls. See if you can try all of the
variants of exec(), including (on Linux) execl(), execle(),
execlp(), execv(), execvp(), and execvpe(). Why do
you think there are so many variants of the same basic call?

R: The variants exist to allow for different search strategies for the
program and how to pass it's arguments. All variants take a file to run.
The variants that start with l(ist) take individual strings as paramenters,
with the last one being a NULL. The ones startig wiht v take a single parameter
that is an array (vector) of strigs. Both of these become the arguments for the file.
Variations with e take an additional parameter envp that specifies the environment
variables of the child process as opposed to all other versions that take them from
a global variable called environ. The p variants search for the specified binary on
the path in a similar way to a shell.

They exist to accomodate the different conditions in which you might need to
use them, but ultimately are just wrappers on top a single implementation.
*/
