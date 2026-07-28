#ifndef CLIENT_H
#define CLIENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define MAX_NICK 32
#define MAX_MSG 256
#define SERVER_PORT 12345

ssize_t read_line(int fd, char *buf, size_t size);
void send_line(int fd, const char *line);

int Socket(int domain, int type, int protocol);
void Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
void Inet_pton(int af, const char *src, void *dst);

void *serverDataProcessing(void *arg);

#endif
