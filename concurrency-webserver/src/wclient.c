//
// client.c: A very, very primitive HTTP client.
//
// To run, try:
//      client hostname portnumber filename
//
// Sends one HTTP request to the specified HTTP server.
// Prints out the HTTP response.
//
// For testing your server, you will want to modify this client.
// For example:
// You may want to make this multi-threaded so that you can
// send many requests simultaneously to the server.
//
// You may also want to be able to request different URIs;
// you may want to get more URIs from the command line
// or read the list from a file.
//
// When we test your server, we will be using modifications to this client.
//

#include "io_helper.h"
#include <stdlib.h>
#include <string.h>
#include <threads.h>

typedef struct {
  char *host;
  int port;
  char *filename;
} Context;

#define MAXBUF (8192)

//
// Send an HTTP request for the specified file
//
void client_send(int fd, char *filename) {
  char buf[MAXBUF];
  char hostname[MAXBUF];

  gethostname_or_die(hostname, MAXBUF);

  /* Form and send the HTTP request */
  sprintf(buf, "GET %s HTTP/1.1\n", filename);
  sprintf(buf, "%shost: %s\n\r\n", buf, hostname);
  write_or_die(fd, buf, strlen(buf));
}

//
// Read the HTTP response and print it out
//
void client_print(int fd) {
  char buf[MAXBUF];
  int n;

  // Read and display the HTTP Header
  n = readline_or_die(fd, buf, MAXBUF);
  while (strcmp(buf, "\r\n") && (n > 0)) {
    printf("Header: %s", buf);
    n = readline_or_die(fd, buf, MAXBUF);

    // If you want to look for certain HTTP tags...
    // int length = 0;
    // if (sscanf(buf, "Content-Length: %d ", &length) == 1) {
    //    printf("Length = %d\n", length);
    //}
  }

  // Read and display the HTTP Body
  n = readline_or_die(fd, buf, MAXBUF);
  while (n > 0) {
    printf("%s", buf);
    n = readline_or_die(fd, buf, MAXBUF);
  }
}

int worker(void *v) {
  Context *context = (Context *)v;
  int clientfd = open_client_fd_or_die(context->host, context->port);

  client_send(clientfd, context->filename);
  client_print(clientfd);

  close_or_die(clientfd);
  return 1;
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    fprintf(stderr, "Usage: %s <host> <port> <filename>...\n", argv[0]);
    exit(1);
  }

  Context context = {.host = argv[1], .port = atoi(argv[2])};
  int file_count = argc - 3;
  thrd_t threads[file_count];

  for (int i = 0; i < file_count; i++) {
    Context *worker_context = malloc(sizeof(Context));
    memcpy(worker_context, &context, sizeof(Context));
    worker_context->filename = argv[i + 3];
    thrd_create(&threads[i], worker, worker_context);
  }

  for (int i = 0; i < file_count; i++) {
    thrd_join(threads[i], nullptr);
  }

  exit(0);
}
