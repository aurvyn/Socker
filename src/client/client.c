#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "../socker.h"
#include "notifier.h"

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
	int sockfd, numbytes;
	char *message = NULL, *response = NULL;
	struct addrinfo *servinfo, *p;
	size_t len, size = 0;
	bool is_tcp;

	if (argc != 4) {
		fprintf(stderr,"usage: client -mode <server-IP-address> <port-number>\n");
		exit(1);
	}

	is_tcp = check_mode(argv[1]);

	if (!set_server_info(&servinfo, argv[2], argv[3], is_tcp)) return 1;

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}
		if (is_tcp && connect(sockfd, p->ai_addr, p->ai_addrlen)) {
			perror("client: connect");
			close(sockfd);
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	if (!is_tcp) { // --- UDP ---
		print_startup_udp(argv[2], argv[3]);
		while (readLine(&message, &size, &len)) {
			printf("\nSending message to server...\n\n");
			relay_to(sockfd, message, len, p->ai_addr, p->ai_addrlen);
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

	while (readLine(&message, &size, &len)) {
		printf("\nSending message to server...\n\n");
		if (!relay(sockfd, message, len)) break;
		if (!strcmp(message, ";;;")) break;
		if (!(numbytes = collect(sockfd, &response))) break;
		printf("Received response from server of\n\n\"%s\"\n\n", response);
	}
	print_shutdown();
	close(sockfd);
	free(message);
	printf("Shut down successful... goodbye\n");
	return 0;
}
