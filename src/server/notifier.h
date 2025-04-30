#pragma once
#include <stdio.h>

static inline void print_breaker() {
	printf("*************************************************************\n\n");
}

static inline void print_tcp_start(char *addr) {
	printf("Received connection request from /%s\n\n", addr);
	print_breaker();
}

static inline void print_tcp_end() {
	printf("Client finished, now waiting to service another client...\n\n");
	print_breaker();
}

static inline void print_udp_start(char *addr) {
	print_breaker();
	printf("Now listening for incoming messages...\n\n");
}

static inline void print_command_status(bool is_successful, const char *command) {
	if (is_successful) {
		printf("\rclient's %s executed successfully.", command);
	} else {
		printf("\rclient's %s command failed.", command);
	}
	printf("\n\nserver> ");
	fflush(stdout);
}

static inline void print_iWant_status(bool is_successful) {
	print_command_status(is_successful, "iWant");
}

static inline void print_uTake_status(bool is_successful) {
	print_command_status(is_successful, "uTake");
}
