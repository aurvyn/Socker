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

#include "../socker.h"
#include "notifier.h"

#define BACKLOG 10
#define PACKET_SIZE 1024
#define HOSTNAME_SIZE 1024

void sigchld_handler(int s) {
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;
	while (waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void display_server_info(struct sockaddr *ai_addr, char *port) {
	char hostname[HOSTNAME_SIZE], addr[INET6_ADDRSTRLEN];
	inet_ntop(ai_addr->sa_family, get_in_addr(ai_addr), addr, sizeof addr);
	printf("Server on host %s/%s is listening on port %s\n\n", 
		gethostname(hostname, HOSTNAME_SIZE) ? "unknown" : hostname,
		addr == NULL ? "unknown" : addr,
		port
	);
	printf("Server starting, listening on port %s\n\n", port);
}

int main(int argc, char *argv[]) {
	int sockfd, new_fd, numbytes, yes = 1;  // listen on sock_fd, new connection on new_fd
	struct addrinfo *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t addr_len;
	ssize_t total_sent, bytes_left;
	struct sigaction sa;
	char s[INET6_ADDRSTRLEN], *response = NULL;
	bool is_tcp;

	if (argc != 3) {
		fprintf(stderr,"usage: server -mode <port-number>\n");
		exit(1);
	}

	is_tcp = check_mode(argv[1]);

	if (!set_server_info(&servinfo, NULL, argv[2], is_tcp)) return 1;

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		if (is_tcp && setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break; // if we get here, we must have connected successfully
	}

	display_server_info(p->ai_addr, argv[2]);

	if (p == NULL) { // no address succeeded
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (!is_tcp) { // --- UDP ---
		print_udp_start(s);
		while (numbytes = collect_from(sockfd, &response, PACKET_SIZE, (struct sockaddr *)&their_addr, &addr_len)) {
			printf("Received the following message from client:\n\n\"%s\"\n\n", response);
		}
		freeaddrinfo(servinfo);
		close(sockfd);
		exit(0);
	}

	// --- TCP ---

	freeaddrinfo(servinfo);
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

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
			printf("Now listening for incoming messages...\n\n");
			while (numbytes = collect(new_fd, &response, PACKET_SIZE)) {
				printf("Received the following message from client:\n\n\"%s\"\n\n", response);
				if (!strcmp(response, ";;;")) break;
				printf("Now sending message back having changed the string to upper case...\n\n");
				for (int i = 0; i < numbytes; i++) {
					response[i] = toupper(response[i]);
				}
				relay(new_fd, response, numbytes, PACKET_SIZE);
			}
			print_tcp_end();
			close(new_fd);
			free(response);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}
}