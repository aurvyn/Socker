#pragma once
#include <stdio.h>

static inline void print_breaker() {
	printf("*************************************************************\n\n");
}

static inline void print_shutdown() {
	printf("\nUser entered sentinel of \";;;\", now stopping client\n\n");
	print_breaker();
	printf("Attempting to shut down client sockets and other streams\n\n");
}

static inline void print_startup_tcp(char *host, char *port) {
	printf("Client has requested to start connection with host %s on port %s\n\n", host, port);
	print_breaker();
	printf("Connection established, now waiting for user input...\n\nprompt> ");
	fflush(stdout);
}

static inline void print_startup_udp(char *host, char *port) {
	printf("Client has opened UDP socket to start communication with host %s on port %s\n\n", host, port);
	print_breaker();
	printf("Now waiting for user input...\n\nprompt> ");
	fflush(stdout);
}