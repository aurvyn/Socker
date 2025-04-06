#pragma once

void print_breaker() {
	printf("*************************************************************\n\n");
}

void print_tcp_start(char *addr) {
	printf("Received connection request from /%s\n\n", addr);
	print_breaker();
}

void print_tcp_end() {
	printf("Client finished, now waiting to service another client...\n\n");
	print_breaker();
}

void print_udp_start(char *addr) {
	print_breaker();
	printf("Now listening for incoming messages...\n\n");
}