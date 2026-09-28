#include <fcntl.h>
#include <stddef.h>
#include <sys/types.h>
#ifndef __REQUEST_H__

#define MAXBUF (8192)
typedef enum { REQUEST_STATIC, REQUEST_CGI } RequestType;

typedef struct {
  char file_name[MAXBUF];
  char cgiargs[MAXBUF];
  int conn_fd;
  RequestType type;
  struct stat sbuf;
} Request;

bool request_parse(int conn_fd, Request *request);

void request_handle(Request *request);

#endif // __REQUEST_H__
