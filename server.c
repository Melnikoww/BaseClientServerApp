#include "server.h"

atomic_int g_stop = 0;
struct Client* g_head = NULL;
int COUNT_ACTIVE_USERS = 0;
int QUANTITY_USERS = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

ssize_t read_line(int fd, char *buf, size_t size) {
	size_t i = 0;

	while (i < size - 1) {
		char c;
		ssize_t n = read(fd, &c, 1);
		if (n <= 0) {
			return n;
		}
		if (c == '\n') {
			break;
		}
		if (c != '\r') {
			buf[i++] = c;
		}
	}
	buf[i] = '\0';
	return (ssize_t)i;
}

void send_message(int fd, const char *nick, const char *data) {
	write(fd, nick, strlen(nick));
	write(fd, "\n", 1);
	write(fd, data, strlen(data));
	write(fd, "\n", 1);
}

int Socket(int domain, int type, int protocol) {
	int res = socket(domain, type, protocol);
	if (res == -1) {
		perror("socket error");
		exit(EXIT_FAILURE);
	}

	return res;
}

void Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	int res = bind(sockfd, addr, addrlen);
	if (res == -1) {
		perror("bind error");
		exit(EXIT_FAILURE);
	}
}

void Listen(int sockfd, int backlog) {
	int res = listen(sockfd, backlog);
	if (res == -1) {
		perror("listen error");
		exit(EXIT_FAILURE);
	}
}

int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	int res;

	do {
		res = accept(sockfd, addr, addrlen);
	} while (res == -1 && errno == EINTR);

	if (res == -1) {
		if (atomic_load(&g_stop)) {
			return -1;
		}
		perror("accept failed");
		exit(EXIT_FAILURE);
	}

	return res;
}

void addClient(struct Client *current) {
	if (current == NULL) {
		return;
	}

	current->next = NULL;

	if (g_head == NULL) {
		g_head = current;
		return;
	}

	struct Client *cur = g_head;
	while (cur->next != NULL) {
		cur = cur->next;
	}
	cur->next = current;
}

void removeClient(int this_fd) {
	if (g_head == NULL) {
		return;
	}

	if (g_head->fd == this_fd) {
		g_head = g_head->next;
		return;
	}

	struct Client *cur = g_head;
	while (cur->next != NULL && cur->next->fd != this_fd) {
		cur = cur->next;
	}
	if (cur->next != NULL) {
		cur->next = cur->next->next;
	}
}

char *getAllNames() {
	size_t total_len = 1;

	struct Client *cur = g_head;
	while (cur != NULL) {
		total_len += strlen(cur->nickname);
		total_len += 2;
		cur = cur->next;
	}

	char *res = (char *)malloc(total_len);
	if (res == NULL) {
		return NULL;
	}
	res[0] = '\0';

	int i = 0;
	cur = g_head;
	while (cur != NULL) {
		strcat(res, cur->nickname);
		if (i < COUNT_ACTIVE_USERS - 1) {
			strcat(res, ", ");
		}

		i++;
		cur = cur->next;
	}

	return res;
}

void *serverThread(void *arg) {
	int server = *(int *)arg;
	char buff[MAX_MSG];

	while (!atomic_load(&g_stop)) {
		if (fgets(buff, sizeof buff, stdin) == NULL) {
			break;
		}
		buff[strcspn(buff, "\r\n")] = '\0';

		if (strcmp(buff, "/STOP") == 0) {
			atomic_store(&g_stop, 1);
			shutdown(server, SHUT_RDWR);
			close(server);
			break;
		}
	}

	return NULL;
}

void *registerThread(void *arg) {
	struct Client *client = (struct Client *)arg;
	char nick[MAX_NICK] = SERVER_NICK;
	char data_buf[MAX_MSG];

	pthread_mutex_lock(&clients_mutex);
	if (COUNT_ACTIVE_USERS >= QUANTITY_USERS) {
		pthread_mutex_unlock(&clients_mutex);
		send_message(client->fd, nick, "Server is full already");
		close(client->fd);
		free(client);
		pthread_exit(NULL);
	}
	pthread_mutex_unlock(&clients_mutex);

	write(client->fd, "Your nickname? \n", 16);

	char buffer[MAX_MSG];
	if (read_line(client->fd, buffer, sizeof buffer) <= 0) {
		close(client->fd);
		free(client);
		pthread_exit(NULL);
	}

	pthread_mutex_lock(&clients_mutex);

	int taken = 0;
	struct Client *cur = g_head;
	while (cur != NULL) {
		if (strcmp(buffer, cur->nickname) == 0) {
			taken = 1;
			break;
		}
		cur = cur->next;
	}

	if (taken) {
		pthread_mutex_unlock(&clients_mutex);
		send_message(client->fd, nick, "This name is already taken");
		close(client->fd);
		free(client);
		pthread_exit(NULL);
	}

	strncpy(client->nickname, buffer, sizeof(client->nickname) - 1);
	client->nickname[sizeof(client->nickname) - 1] = '\0';

	printf("%s join to server!\n", client->nickname);
	snprintf(data_buf, sizeof data_buf, "%s join to server!", client->nickname);

	addClient(client);

	cur = g_head;
	while (cur != NULL) {
		if (cur->fd != client->fd) {
			send_message(cur->fd, nick, data_buf);
		}
		cur = cur->next;
	}
	COUNT_ACTIVE_USERS++;

	printf("Active users now: %d\n", COUNT_ACTIVE_USERS);
	pthread_mutex_unlock(&clients_mutex);

	while (1) {
		if (read_line(client->fd, buffer, sizeof buffer) <= 0) {
			break;
		}

		if (strcmp(buffer, "/EXIT") == 0) {
			break;
		}

		if (strcmp(buffer, "/USERS") == 0) {
			pthread_mutex_lock(&clients_mutex);
			char *AllUsers = getAllNames();
			pthread_mutex_unlock(&clients_mutex);

			send_message(client->fd, nick, AllUsers ? AllUsers : "");
			free(AllUsers);
			continue;
		}

		if (strncmp(buffer, "/USER ", 6) == 0) {
			char *p = buffer + 5;
			while (*p == ' ') {
				p++;
			}

			if (*p == '\0') {
				send_message(client->fd, nick, "Pls, try again!");
				continue;
			}

			char target_tmp[MAX_NICK];
			int j = 0;
			while (*p != '\0' && *p != ' ' && j < (int)sizeof(target_tmp) - 1) {
				target_tmp[j++] = *p++;
			}
			target_tmp[j] = '\0';

			if (strcmp(target_tmp, client->nickname) == 0) {
				send_message(client->fd, nick, "You cannot text yourself");
				continue;
			}

			int found = 0;
			int target_fd = -1;

			pthread_mutex_lock(&clients_mutex);
			cur = g_head;
			while (cur != NULL) {
				if (strcmp(target_tmp, cur->nickname) == 0) {
					found = 1;
					target_fd = cur->fd;
					break;
				}
				cur = cur->next;
			}
			pthread_mutex_unlock(&clients_mutex);

			if (!found) {
				send_message(client->fd, nick, "Unknown user");
				continue;
			}

			if (read_line(client->fd, buffer, sizeof buffer) <= 0) {
				break;
			}

			send_message(target_fd, client->nickname, buffer);
			continue;
		}

		if (strcmp(buffer, "") != 0) {
			pthread_mutex_lock(&clients_mutex);
			cur = g_head;
			while (cur != NULL) {
				if (cur->fd != client->fd) {
					send_message(cur->fd, client->nickname, buffer);
				}
				cur = cur->next;
			}
			pthread_mutex_unlock(&clients_mutex);
			printf("%s: %s\n", client->nickname, buffer);
		}
	}

	pthread_mutex_lock(&clients_mutex);
	removeClient(client->fd);
	COUNT_ACTIVE_USERS--;
	pthread_mutex_unlock(&clients_mutex);

	printf("client %s out, active users now: %d\n", client->nickname, COUNT_ACTIVE_USERS);
	close(client->fd);
	free(client);
	pthread_exit(NULL);
}

int main() {
	int server = Socket(AF_INET, SOCK_STREAM, 0);

	printf("Кол-во пользователей на сервере: \n");
	if (scanf("%d", &QUANTITY_USERS) != 1) {
		fprintf(stderr, "Failed to read QUANTITY_USERS\n");
		exit(EXIT_FAILURE);
	}
	if (QUANTITY_USERS < 1) {
		fprintf(stderr, "QUANTITY_USERS must be >= 1\n");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in adr = {0};
	adr.sin_family = AF_INET;
    adr.sin_port = htons(SERVER_PORT);
	Bind(server, (struct sockaddr *)&adr, sizeof adr);

	Listen(server, QUANTITY_USERS);

	printf("Server started now!!!\n");
	printf("Available commands:\n");
	printf("/STOP <-> CLOSE THE SERVER!\n");

	pthread_t serverMessageThread;

	if (pthread_create(&serverMessageThread, NULL, serverThread, (void *)&server) != 0) {
		perror("pthread_create failed\n");
		close(server);
		exit(EXIT_FAILURE);
	}

	pthread_detach(serverMessageThread);

	while (!atomic_load(&g_stop)) {
		struct sockaddr_in client_adr = {0};
		socklen_t clientAddrLen = sizeof client_adr;

		int client_fd = Accept(server, (struct sockaddr *)&client_adr, &clientAddrLen);
		if (client_fd == -1) {
			break;
		}

		struct Client *client = (struct Client *)calloc(1, sizeof(struct Client));
		if (client == NULL) {
			close(client_fd);
			continue;
		}
		client->fd = client_fd;

		pthread_t fdThread;
		if (pthread_create(&fdThread, NULL, registerThread, (void *)client) != 0) {
			perror("pthread_create failed\n");
			free(client);
			close(client_fd);
			continue;
		}

		pthread_detach(fdThread);
	}

	pthread_mutex_destroy(&clients_mutex);
	return 0;
}
