#pragma once
#define HEADER_SIZE 32

bool relay(
	int sockfd,
	const void *buf,
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

bool relay_to(
	int sockfd,
	const void *buf,
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

size_t collect(
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
	size_t remaining, len = atoi(header);
	*content = realloc(*content, len + 1);
	for (size_t offset = 0; offset < len; offset += packet_size) {
		remaining = len - offset;
		size_t to_read = remaining < packet_size ? remaining : packet_size;
		numbytes = recv(sockfd, *content + offset, to_read, 0);
		if (numbytes == -1) {
			perror("collect: recv");
			return 0;
		}
	}
	(*content)[len] = '\0';
	return len;
}

size_t collect_from(
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
	size_t len = atoi(header);
	*content = realloc(*content, len + 1);
	for (size_t offset = 0; offset < len; offset += packet_size) {
		numbytes = recvfrom(sockfd, *content + offset, packet_size, 0, src_addr, addrlen);
		if (numbytes == -1) {
			perror("collect_from: recvfrom");
			return 0;
		}
	}
	(*content)[len] = '\0';
	return len;
}

bool readLine(char **line, size_t *size, size_t *length) {
	while (1) {
		printf("prompt> ");
		size_t len = getline(line, size, stdin);
		if (len == -1) return false;
		if (len == 1) continue;
		if ((*line)[len - 1] == '\n') {
			(*line)[--len] = '\0';
		}
		*length = len;
		return true;
	}
}

bool check_mode(char *mode) {
	if (!strcmp(mode, "-u")) {
		return false;
	} else if (!strcmp(mode, "-t")) {
		return true;
	} else {
		fprintf(stderr, "Invalid mode. Use -u for UDP or -t for TCP.\n");
		exit(1);
	}
}

bool set_server_info(struct addrinfo **servinfo, char* addr, char *port, bool is_tcp) {
	int rv;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = is_tcp ? SOCK_STREAM : SOCK_DGRAM;
	if (addr == NULL) hints.ai_flags = AI_PASSIVE; // use my IP
	if ((rv = getaddrinfo(addr, port, &hints, servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return false;
	}
	return true;
}