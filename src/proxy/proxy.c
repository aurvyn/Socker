#include <ctype.h>
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
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "../socker.h"

#define BACKLOG 10
#define PACKET_SIZE 1024
#define HEADER_CAP 8192

// input: buf (request headers), len (#bytes)
// output: host, port
int parse_host_port(
	char *buf,
	size_t len,
	char **host,
	char **port
) {
	printf("buf: %s\n", buf);
	char *end = memmem(buf, len, "\r\n\r\n", 4);
	if (!end) return -1;

	char *h = buf, host_len = 0;
	while ((h = memmem(h, end - h, "\nHost:", 6))) {
		h += 6;
		while (isspace((unsigned char)*h)) ++h;
		while (*(h + host_len) != '\n') ++host_len;

		char *colon = memchr(h, ':', host_len);
		char *eol = memchr(h, '\r', host_len);

		if (colon) {
            *colon = '\0';
            *port = "80";
            char *p = colon + 1;
            while (p < eol && isdigit((unsigned char)*p)) ++p;
            *p = '\0';
        } else {
            *eol  = '\0';
            *port = "80";
        }
        *host = h;
		return 0;
	}
	return -1;
}

static void make_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

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
	int clientfd, new_clientfd, serverfd, epoll_fd, numbytes;  // listen on sock_fd, new connection on new_fd
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN], buf[PACKET_SIZE], headers[HEADER_CAP], *server_host, *server_port;
	size_t len = 0, size = 0, nread;

	if (argc != 2) {
		fprintf(stderr,"usage: proxy <port-number>\n");
		exit(1);
	}
	clientfd = get_listening_socket(true, NULL, argv[1], &servinfo, &p);
	freeaddrinfo(servinfo);

	handle_dead_processes();

	while (1) {  // main accept() loop
		addr_len = sizeof their_addr;
		new_clientfd = accept(clientfd, (struct sockaddr *)&their_addr, &addr_len);
		if (new_clientfd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);

		if (!fork()) { // make child to handle client requests
			make_nonblocking(new_clientfd);
			size_t len = 0;
			collect_request(new_clientfd, headers, HEADER_CAP, &len);
			parse_host_port(headers, len, &server_host, &server_port);
			serverfd = get_listening_socket(true, server_host, server_port, &servinfo, &p);
			make_nonblocking(serverfd);
			freeaddrinfo(servinfo);

			if (send(serverfd, headers, len, 0) != len) {
				perror("send initial request");
				exit(1);
			}
			if ((epoll_fd = epoll_create1(0)) == -1) {
				perror("epoll_create1");
				exit(1);
			}
			// Add the client socket.
			struct epoll_event ev, events[2];
			ev.events = EPOLLIN;
			ev.data.fd = new_clientfd;
			if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_clientfd, &ev) == -1) {
				perror("epoll_ctl: new_clientfd");
				exit(1);
			}
			// Add standard input.
			ev.events = EPOLLIN;
			ev.data.fd = serverfd;
			if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serverfd, &ev) == -1) {
				perror("epoll_ctl: serverfd");
				exit(1);
			}
			bool connected = true;
			while (connected) {
				int n = epoll_wait(epoll_fd, events, 2, -1);
				if (n == -1) {
					perror("epoll_wait");
					exit(1);
				}
				for (int i = 0; i < n; i++) {
					if (events[i].events & EPOLLIN) {
						if (events[i].data.fd == new_clientfd) {
							while ((nread = recv(new_clientfd, buf, PACKET_SIZE, 0)) > 0) {
								if (send(serverfd, buf, nread, 0) == -1) {
									connected = false;
									break;
								}
							}
						} else {
							while ((nread = recv(serverfd, buf, PACKET_SIZE, 0)) > 0) {
								if (send(new_clientfd, buf, nread, 0) == -1) {
									connected = false;
									break;
								}
							}
						}
						if (nread == 0 || (nread < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
							connected = false;
						}
					}
					if (events[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP))
						connected = false;
				}
			}
			close(new_clientfd);
			close(serverfd);
			exit(0);
		}
		close(new_clientfd);  // parent doesn't need this
	}
}
