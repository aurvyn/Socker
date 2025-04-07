#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "../socker.h"
#include "notifier.h"

#define PACKET_SIZE 1024

int main(int argc, char *argv[]) {
	int sockfd, epoll_fd, numbytes;
	char *message = NULL, *response = NULL;
	struct addrinfo *servinfo, *p;
	size_t len, size = 0;

	if (argc != 4) {
		fprintf(stderr,"usage: client -mode <server-IP-address> <port-number>\n");
		exit(1);
	}
	bool is_tcp = check_mode(argv[1]);
	sockfd = get_listening_socket(is_tcp, argv[2], argv[3], &servinfo, &p);

	if (!is_tcp) { // --- UDP ---
		print_startup_udp(argv[2], argv[3]);
		while (readLine(&message, &size, &len)) {
			printf("\nSending message to server...\n\nprompt> ");
			relay_to(sockfd, message, len, PACKET_SIZE, p->ai_addr, p->ai_addrlen);
			if (!strcmp(message, ";;;")) break;
		}
		print_shutdown();
		freeaddrinfo(servinfo);
		close(sockfd);
		free(message);
		printf("Shut down successful... goodbye\n");
		return 0;
	}

	// --- TCP ---
	print_startup_tcp(argv[2], argv[3]);
	freeaddrinfo(servinfo);
	if ((epoll_fd = epoll_create1(0)) == -1) {
		perror("epoll_create1");
		exit(1);
	}
	// Add the client socket.
	struct epoll_event ev, events[2];
	ev.events = EPOLLIN;
	ev.data.fd = sockfd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
		perror("epoll_ctl: client_fd");
		exit(1);
	}
	// Add standard input.
	ev.events = EPOLLIN;
	ev.data.fd = STDIN_FILENO;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1) {
		perror("epoll_ctl: STDIN");
		exit(1);
	}
	bool connected = true;
	while (connected) {
		int n = epoll_wait(epoll_fd, events, 2, -1);
		if (n == -1) {
			perror("epoll_wait");
			exit(1);
		}
		for (int i = 0; i < n; i++) {
			if (events[i].data.fd == sockfd) {
				numbytes = collect(sockfd, &response, PACKET_SIZE);
				if (!numbytes) break;
				printf("\rReceived response from server of\n\n\"%s\"\n\nprompt> ", response);
				fflush(stdout);
			} else if (events[i].data.fd == STDIN_FILENO) {
				if (!readLine(&message, &size, &len)) break;
				printf("\nSending message to server...\n\n");
				if (!relay(sockfd, message, len, PACKET_SIZE)) break;
				if (!strcmp(message, ";;;")) {
					connected = false;
					break;
				}
				if (!(numbytes = collect(sockfd, &response, PACKET_SIZE))) break;
				printf("Received response from server of\n\n\"%s\"\n\nprompt> ", response);
				fflush(stdout);
			}
		}
	}
	print_shutdown();
	close(sockfd);
	free(message);
	printf("Shut down successful... goodbye\n");
	return 0;
}
