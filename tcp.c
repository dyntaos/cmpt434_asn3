/**********************************
 **           CMPT 434           **
 **  University of Saskatchewan  **
 **         Assignment 3         **
 **----------------------------- **
 **          Kale Yuzik          **
 **     kay851@mail.usask.ca     **
 **      kay851    11071571      **
 **********************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>


#include "tcp.h"



int tcp_client_init(const char *host, const char *port) {
	int rv, fd;
	struct addrinfo hints, *servinfo, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			continue;
		}

		if (connect(fd, p->ai_addr, p->ai_addrlen) == -1) {
			close(fd);
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "Failed to connect to server\n");
		return -2;
	}

	printf("Connected...\n");

	freeaddrinfo(servinfo);
	return fd;
}


int tcp_server_init(const char *port) {
	struct addrinfo hints;
	struct addrinfo *servinfo, *p;
	int sockfd, rv, yes = 1;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(
				p->ai_family,
				p->ai_socktype,
				p->ai_protocol
		)) == -1) {
			perror("Server: socket");
			continue;
		}

		if (setsockopt(
				sockfd,
				SOL_SOCKET,
				SO_REUSEADDR,
				&yes,
				sizeof(int)
		) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("Server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo);

	if (p == NULL) {
		fprintf(stderr, "Server: Failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}
	return sockfd;
}


int tcp_accept(int sock_fd) {
	socklen_t sin_size;
	struct sockaddr_storage their_addr;
	int new_fd;

	sin_size = sizeof(their_addr);
	new_fd = accept(sock_fd, (struct sockaddr*) &their_addr, &sin_size);
	if (new_fd == -1) {
		perror("Failed to accept incoming connection");
		return -1;
	}

	return new_fd;
}


int tcp_receive(int socket, void *buffer, size_t buffer_len) {
	return recv(socket, buffer, buffer_len, 0);
}


int tcp_send(int socket, void *buffer, size_t buffer_len) {
	return send(socket, buffer, buffer_len, 0);
}