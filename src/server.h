#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>

#define MAX_NICK 32
#define MAX_MSG 256
#define SERVER_PORT 12345
#define SERVER_NICK "SERVER"

struct Client {
	int fd;
	char nickname[MAX_NICK];
	struct Client *next;
};

extern atomic_int g_stop;
extern struct Client *g_head;
extern int COUNT_ACTIVE_USERS;
extern int QUANTITY_USERS;
extern pthread_mutex_t clients_mutex;

ssize_t read_line(int fd, char *buf, size_t size);
void send_message(int fd, const char *nick, const char *data);

int Socket(int domain, int type, int protocol);
void Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
void Listen(int sockfd, int backlog);
int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

void addClient(struct Client *current);
void removeClient(int this_fd);

char *getAllNames();

void *serverThread(void *arg);
void *registerThread(void *arg);

#endif
