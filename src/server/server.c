#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/epoll.h>

#include "../socker.h"
#include "notifier.h"

#define BACKLOG 10
#define PACKET_SIZE 1024

void sigchld_handler(int s) {
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;
	while (waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}

void handle_dead_processes() {
	struct sigaction sa;
	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
}

int main(int argc, char *argv[]) {
	struct addrinfo *servinfo, *p;
	int sockfd, new_fd, epoll_fd, numbytes;  // listen on sock_fd, new connection on new_fd
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN], *message = NULL, *response = NULL;
	size_t len, size = 0;

	if (argc != 3) {
		fprintf(stderr,"usage: server -mode <port-number>\n");
		exit(1);
	}
	bool is_tcp = check_mode(argv[1]);
	sockfd = get_listening_socket(is_tcp, NULL, argv[2], &servinfo, &p);
	freeaddrinfo(servinfo);

	if (!is_tcp) { // --- UDP ---
		print_udp_start(s);
		while ((numbytes = collect_from(sockfd, &response, PACKET_SIZE, (struct sockaddr *)&their_addr, &addr_len))) {
			printf("Received the following message from client:\n\n\"%s\"\n\n", response);
		}
		close(sockfd);
		exit(0);
	}

	// --- TCP ---
	handle_dead_processes();

	while (1) {  // main accept() loop
		addr_len = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_len);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		print_tcp_start(s);

		if (!fork()) { // make child to handle client requests
			if ((epoll_fd = epoll_create1(0)) == -1) {
				perror("epoll_create1");
				exit(1);
			}
			// Add the client socket.
			struct epoll_event ev, events[2];
			ev.events = EPOLLIN;
			ev.data.fd = new_fd;
			if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &ev) == -1) {
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
			printf("Now listening for incoming messages...\n\n");
			bool connected = true;
			while (connected) {
				int n = epoll_wait(epoll_fd, events, 2, -1);
				if (n == -1) {
					perror("epoll_wait");
					exit(1);
				}
				for (int i = 0; i < n; i++) {
					if (events[i].data.fd == new_fd) {
						numbytes = collect(new_fd, &response, PACKET_SIZE);
						if (!numbytes) break;
						printf("Received the following message from client:\n\n\"%s\"\n\n", response);
						if (!strcmp(response, ";;;")) {
							connected = false;
							break;
						}
						printf("Now sending message back having changed the string to upper case...\n\n");
						for (int j = 0; j < numbytes; j++) {
							response[j] = toupper(response[j]);
						}
						relay(new_fd, response, numbytes, PACKET_SIZE);
					} else if (events[i].data.fd == STDIN_FILENO) {
						readLine(&message, &size, &len);
						relay(new_fd, message, len, PACKET_SIZE);
					}
				}
			}
			print_tcp_end();
			close(new_fd);
			free(response);
			free(message);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}
}