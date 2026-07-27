#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#define WRITE_COUNT 100000

int main(void) {
  FILE *file = fopen("file", "w+");
  int pid = fork();
  if (pid < 0) {
    fprintf(stderr, "fork failed\n");
    return 1;
  }

  if (pid == 0) {
    char *text = "hello, world from child\n";

    for (int i = 0; i < WRITE_COUNT; i++) {
      fwrite(text, strlen(text), 1, file);
    }
    printf("children wrote to file\n");
  } else {

    char *text = "hello, world from parent\n";
    for (int i = 0; i < WRITE_COUNT; i++) {
      fwrite(text, strlen(text), 1, file);
    }
    printf("parent wrote to file\n");
    wait(NULL);
    rewind(file);
    char buf[256];
    while (fgets(buf, sizeof(buf), file)) {
      fputs(buf, stdout);
    }

    fclose(file);
  }
  return 0;
}

/*
Write a program that opens a file (with the open() system call)
and then calls fork() to create a new process. Can both the child
and parent access the file descriptor returned by open()? What
happens when they are writing to the file concurrently, i.e., at the
same time?

R: The resulting number of lines ended up being corect, but there is no mechanism
to enforce the sincronization and they may write on top of each other. Buffering makes
the resulting file have alternating blocks of one process' writes.
*/
