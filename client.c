#include "client.h"

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

void send_line(int fd, const char *line) {
	write(fd, line, strlen(line));
	write(fd, "\n", 1);
}

int Socket(int domain, int type, int protocol) {
	int res = socket(domain, type, protocol);
	if (res == -1) {
		perror("socket failure");
		exit(EXIT_FAILURE);
	}

	return res;
}

void Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	int res = connect(sockfd, addr, addrlen);
	if (res == -1) {
		perror("connect failure");
		exit(EXIT_FAILURE);
	}
}

void Inet_pton(int af, const char *src, void *dst) {
	int res = inet_pton(af, src, dst);
	if (res == -1) {
		perror("inet_pton failed");
		exit(EXIT_FAILURE);
	}
	if (res == 0) {
		perror("src does not contain string representing a valid network address");
		exit(EXIT_FAILURE);
	}
}

void *serverDataProcessing(void *arg) {
	int fd = *(int *)arg;
	char nick[MAX_NICK];
	char data[MAX_MSG];

	while (1) {
		if (read_line(fd, nick, sizeof nick) <= 0) {
			break;
		}
		if (read_line(fd, data, sizeof data) <= 0) {
			break;
		}

		printf("%s: %s\n", nick, data);
	}

	return NULL;
}

int main() {
	int client = Socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);

	Inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
	Connect(client, (struct sockaddr *)&addr, sizeof addr);

	printf("Connect established\n");
	printf("Available commands:\n");
	printf("/EXIT <-> leave the chat\n");
	printf("/USERS <-> list of active users\n");
	printf("/USER + nickname <-> private message\n");

	char buffer[MAX_MSG];
	if (read_line(client, buffer, sizeof buffer) <= 0) {
		close(client);
		return EXIT_FAILURE;
	}
	printf("%s\n", buffer);

	pthread_t serverData;
	if (pthread_create(&serverData, NULL, serverDataProcessing, (void *)&client) != 0) {
		perror("pthread_create failed\n");
		close(client);
		exit(EXIT_FAILURE);
	}
	pthread_detach(serverData);

	if (fgets(buffer, sizeof buffer, stdin) == NULL) {
		close(client);
		return EXIT_FAILURE;
	}
	buffer[strcspn(buffer, "\r\n")] = '\0';
	send_line(client, buffer);

	while (1) {
		if (fgets(buffer, sizeof buffer, stdin) == NULL) {
			break;
		}
		buffer[strcspn(buffer, "\r\n")] = '\0';

		if (strcmp(buffer, "/USERS") == 0) {
			send_line(client, buffer);
			continue;
		}

		if (strncmp(buffer, "/USER ", 6) == 0) {
			send_line(client, buffer);
			printf("Private message: ");
			if (fgets(buffer, sizeof buffer, stdin) == NULL) {
				break;
			}
			buffer[strcspn(buffer, "\r\n")] = '\0';
			send_line(client, buffer);
			continue;
		}

		send_line(client, buffer);

		if (strcmp(buffer, "/EXIT") == 0) {
			break;
		}
	}

	close(client);
	return 0;
}
