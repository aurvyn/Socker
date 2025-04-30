#pragma once
#include <stdio.h>

static inline void print_breaker() {
	printf("*************************************************************\n\n");
}

static inline void print_tcp_start(char *addr) {
	printf("Received connection request from /%s\n\n", addr);
	print_breaker();
}

static inline void print_connection_start() {
	printf("Now listening for incoming messages...\n\nserver> ");
	fflush(stdout);
}

static inline void print_tcp_end() {
	printf("\rClient finished, now waiting to service another client...\n\n");
	print_breaker();
}

static inline void print_udp_start(char *addr) {
	print_breaker();
	printf("Now listening for incoming messages...\n\n");
}
