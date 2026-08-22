#define _GNU_SOURCE
#include <arpa/inet.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

constexpr unsigned PORT = 8080;
constexpr unsigned MAX_CLIENTS = 127;
constexpr unsigned FILE_BUF_SIZE = 8 * 1024;
constexpr unsigned RX_BUF_SIZE = 128;

typedef struct {
  char file_buf[FILE_BUF_SIZE];
  char rx_buf[RX_BUF_SIZE];
  size_t rx_len;

  size_t file_len;
  size_t file_sent;

  int file_fd;
} ClientContext;

int main(void) {
  struct pollfd poll_fds[MAX_CLIENTS + 1];
  ClientContext clients[MAX_CLIENTS];

  int server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (server_fd < 0) {
    perror("socket failed");
    return EXIT_FAILURE;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  struct sockaddr_in address = {.sin_family = AF_INET,
                                .sin_addr.s_addr = INADDR_ANY,
                                .sin_port = htons(PORT)};
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    return EXIT_FAILURE;
  }

  if (listen(server_fd, 5) < 0) {
    perror("listen failed");
    return EXIT_FAILURE;
  }
  size_t active_fds = 1;

  poll_fds[0].fd = server_fd;
  poll_fds[0].events = POLLIN;

  for (size_t i = 1; i < MAX_CLIENTS; i++) {
    poll_fds[i].fd = -1;
  }

  printf("Server listening on port %d...\n", PORT);

  struct sockaddr_in client_addr = {};
  socklen_t addr_len = sizeof(client_addr);
  while (true) {
    int ready = poll(poll_fds, active_fds, -1);
    if (ready < 0) {
      perror("poll error");
    }

    if (poll_fds[0].revents & POLLIN) {
      int client_fd = accept4(server_fd, (struct sockaddr *)&client_addr,
                              &addr_len, SOCK_NONBLOCK);
      if (client_fd < 0) {
        perror("accept failed");
        close(server_fd);
        return EXIT_FAILURE;
      }

      poll_fds[active_fds].fd = client_fd;
      poll_fds[active_fds].events = POLLIN;
      clients[client_fd].file_fd = -1;
      active_fds++;
    }

    for (size_t i = 1; i < active_fds; i++) {
      struct pollfd *client = &poll_fds[i];
      if (!client->revents) {
        continue;
      }

      if (poll_fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
        close(client->fd);
        poll_fds[i] = poll_fds[active_fds - 1];
        poll_fds[active_fds - 1].fd = -1;
        active_fds--;
        i--;
        continue;
      }

      ClientContext *ctx = &clients[client->fd];

      switch (client->revents) {
      case POLLIN: {
        ssize_t read_bytes = read(client->fd, &ctx->rx_buf[ctx->rx_len],
                                  RX_BUF_SIZE - ctx->rx_len);
        ctx->rx_len += (size_t)read_bytes;
        if (ctx->rx_buf[ctx->rx_len - 1] != '\n') {
          continue;
        }

        ctx->rx_buf[ctx->rx_len - 1] = '\0';
        ctx->rx_len = 0;
        int file_fd = open(ctx->rx_buf, O_RDONLY | O_NONBLOCK);
        if (file_fd < 0) {
          write(client->fd, "failed to open\n", 15);
          continue;
        }

        ctx->file_fd = file_fd;
        ctx->file_len = 0;
        ctx->file_sent = 0;
        client->events = POLLOUT;
        break;
      }
      case POLLOUT: {
        if (ctx->file_sent >= ctx->file_len) {
          ssize_t read_bytes = read(ctx->file_fd, ctx->file_buf, FILE_BUF_SIZE);
          if (read_bytes < 0) {
            close(ctx->file_fd);
            ctx->file_fd = -1;
          }

          ctx->file_len = (size_t)read_bytes;
          ctx->file_sent = 0;
        }

        size_t to_write = ctx->file_len - ctx->file_sent;
        if (to_write == 0) {
          client->events = POLLIN;
          continue;
        }

        ssize_t written =
            write(client->fd, &ctx->file_buf[ctx->file_sent], to_write);
        ctx->file_sent += (size_t)written;
        break;
      }
      }
    }
  }
}
