#include <stdio.h>
#include <stdlib.h>
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
	int sockfd, numbytes;
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
			printf("\nSending message to server...\n\n");
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
	while (readLine(&message, &size, &len)) {
		printf("\nSending message to server...\n\n");
		if (!relay(sockfd, message, len, PACKET_SIZE)) break;
		if (!strcmp(message, ";;;")) break;
		if (!(numbytes = collect(sockfd, &response, PACKET_SIZE))) break;
		printf("Received response from server of\n\n\"%s\"\n\n", response);
	}
	print_shutdown();
	close(sockfd);
	free(message);
	printf("Shut down successful... goodbye\n");
	return 0;
}
