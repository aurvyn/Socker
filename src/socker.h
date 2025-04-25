#pragma once
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/sendfile.h>

#define HEADER_SIZE 32
#define HOSTNAME_SIZE 1024

// Sends a message with `sockfd` with a header containing the message length.
// Returns true on success, false on failure.
static inline bool relay(
	int sockfd,
	const char *buf,
	size_t len,
	size_t packet_size
) {
	char header[HEADER_SIZE];
	sprintf(header, "%zu", len);
	if (send(sockfd, header, HEADER_SIZE, 0) == -1) {
		perror("relay: send (header)");
		return false;
	}
	size_t remaining, numbytes;
	for (size_t offset = 0; offset < len; offset += packet_size) {
		remaining = len - offset;
		size_t to_send = remaining < packet_size ? remaining : packet_size;
		numbytes = send(sockfd, buf + offset, to_send, 0);
		if (numbytes == -1) {
			perror("relay: send");
			return false;
		}
	}
	return true;
}

static inline bool relay_file(
	int sockfd,
	int fd,
	int len,
	size_t packet_size
) {
	char header[HEADER_SIZE];
	sprintf(header, "%i", len);
	if (send(sockfd, header, HEADER_SIZE, 0) == -1) {
		perror("relay_file: send (header)");
		return false;
	}
	ssize_t sent = sendfile(sockfd, fd, 0, len);
	if (sent == -1) {
		perror("relay_file: sendfile");
		return false;
	}
	return true;
}

// Sends a message with `sockfd` to `dest_addr` with a header containing the message length.
// Returns true on success, false on failure.
static inline bool relay_to(
	int sockfd,
	const char *buf,
	size_t len,
	size_t packet_size,
	const struct sockaddr *dest_addr,
	socklen_t addrlen
) {
	char header[HEADER_SIZE];
	sprintf(header, "%zu", len);
	if (sendto(sockfd, header, HEADER_SIZE, 0, dest_addr, addrlen) == -1) {
		perror("relay_to: sendto (header)");
		return false;
	}
	int numbytes;
	for (size_t offset = 0; offset < len; offset += packet_size) {
		numbytes = sendto(sockfd, buf + offset, packet_size, 0, dest_addr, addrlen);
		if (numbytes == -1) {
			perror("relay_to: sendto");
			return false;
		}
	}
	return true;
}

// Receives a message with `sockfd` with a header containing the message length.
// Returns the length of the message on success, 0 on failure.
static inline size_t collect(
	int sockfd,
	char **content,
	size_t packet_size
) {
	char header[HEADER_SIZE];
	int numbytes = recv(sockfd, header, HEADER_SIZE, 0);
	if (numbytes == -1) {
		perror("collect: recv (header)");
		return 0;
	}
	header[HEADER_SIZE - 1] = '\0';
	size_t remaining, to_read, len = atoi(header);
	*content = (char*) realloc(*content, len + 1);
	for (size_t offset = 0; offset < len; offset += packet_size) {
		remaining = len - offset;
		to_read = remaining < packet_size ? remaining : packet_size;
		numbytes = recv(sockfd, *content + offset, to_read, 0);
		if (numbytes == -1) {
			perror("collect: recv");
			return 0;
		}
	}
	(*content)[len] = '\0';
	return len;
}

// Receives a message with `sockfd` from `src_addr` with a header containing the message length.
// Returns the length of the message on success, 0 on failure.
static inline size_t collect_from(
	int sockfd,
	char **content,
	size_t packet_size,
	struct sockaddr *src_addr,
	socklen_t *addrlen
) {
	char header[HEADER_SIZE];
	int numbytes = recvfrom(sockfd, header, HEADER_SIZE, 0, src_addr, addrlen);
	if (numbytes == -1) {
		perror("collect_from: recvfrom (header)");
		return 0;
	}
	header[HEADER_SIZE - 1] = '\0';
	size_t remaining, to_read, len = atoi(header);
	*content = (char*) realloc(*content, len + 1);
	for (size_t offset = 0; offset < len; offset += packet_size) {
		remaining = len - offset;
		to_read = remaining < packet_size ? remaining : packet_size;
		numbytes = recvfrom(sockfd, *content + offset, to_read, 0, src_addr, addrlen);
		if (numbytes == -1) {
			perror("collect_from: recvfrom");
			return 0;
		}
	}
	(*content)[len] = '\0';
	return len;
}

// Reads one line from the console, result is stored in `line` and its length in `length`.
// Returns true on success, false on failure.
static inline bool readLine(
	char **line,
	size_t *size,
	size_t *length
) {
	while (1) {
		size_t len = getline(line, size, stdin);
		if (len == -1) return false;
		if (len == 1) {
			printf("prompt> ");
			fflush(stdout);
			continue;
		}
		if ((*line)[len - 1] == '\n') {
			(*line)[--len] = '\0';
		}
		*length = len;
		return true;
	}
}

// Returns true if the mode is TCP, false if UDP.
// Exits with an error message if the mode is invalid.
static inline bool check_mode(
	char *mode
) {
	if (!strcmp(mode, "-u")) {
		return false;
	} else if (!strcmp(mode, "-t")) {
		return true;
	} else {
		fprintf(stderr, "Invalid mode. Use -u for UDP or -t for TCP.\n");
		exit(1);
	}
}

// Get sockaddr, IPv4 or IPv6:
static inline void *get_in_addr(
	struct sockaddr *sa
) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Prints server startup information.
static inline void display_server_info(
	struct sockaddr *ai_addr,
	char *port
) {
	char hostname[HOSTNAME_SIZE], addr[INET6_ADDRSTRLEN];
	inet_ntop(ai_addr->sa_family, get_in_addr(ai_addr), addr, sizeof addr);
	printf("Server on host %s/%s is listening on port %s\n\n", 
		gethostname(hostname, HOSTNAME_SIZE) ? "unknown" : hostname,
		addr[0] == '\0' ? "unknown" : addr,
		port
	);
	printf("Server starting, listening on port %s\n\n\n", port);
}

// Returns a listening socket for TCP or UDP.
// `servinfo` and `p` should be freed after use.
static inline int get_listening_socket(
	bool is_tcp,
	char *addr,
	char *port,
	struct addrinfo **servinfo,
	struct addrinfo **p
) {
	int sockfd, rv, yes = 1, no = 0;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = addr == NULL ? AF_INET6 : AF_UNSPEC;
	hints.ai_socktype = is_tcp ? SOCK_STREAM : SOCK_DGRAM;
	if (addr == NULL) hints.ai_flags = AI_PASSIVE; // use my IP
	if ((rv = getaddrinfo(addr, port, &hints, servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}
	for (*p = *servinfo; *p != NULL; *p = (*p)->ai_next) {
		if ((sockfd = socket((*p)->ai_family, (*p)->ai_socktype, (*p)->ai_protocol)) == -1) {
			perror("listening socket");
			continue;
		}
		if (addr == NULL) {
			if (is_tcp && setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
				perror("setsockopt SO_REUSEADDR");
				exit(1);
			}
			if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no)) == -1) {
				perror("setsockopt IPV6_V6ONLY");
				exit(1);
			}
			if (bind(sockfd, (*p)->ai_addr, (*p)->ai_addrlen) == -1) {
				perror("server: bind");
				close(sockfd);
				continue;
			}
		} else if (is_tcp && connect(sockfd, (*p)->ai_addr, (*p)->ai_addrlen)) {
			perror("client: connect");
			close(sockfd);
			continue;
		}
		break; // if we get here, we must have connected successfully
	}
	if (addr == NULL) {
		display_server_info((*p)->ai_addr, port);
		if (*p == NULL) {
			fprintf(stderr, "server: failed to bind\n");
			exit(1);
		}
		if (is_tcp && listen(sockfd, 10) == -1) {
			perror("listen");
			exit(1);
		}
	} else {
		if (*p == NULL) {
			fprintf(stderr, "client: failed to connect\n");
			exit(1);
		}
	}
	return sockfd;
}
