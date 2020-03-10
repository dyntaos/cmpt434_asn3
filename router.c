/**********************************
 **           CMPT 434           **
 **  University of Saskatchewan  **
 **         Assignment 3         **
 **----------------------------- **
 **          Kale Yuzik          **
 **     kay851@mail.usask.ca     **
 **      kay851    11071571      **
 **********************************/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/epoll.h>
#include <time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdbool.h>

#include "tcp.h"



typedef uint32_t route_cost_t; // May be changed, but must be unsigned

#define EPOLL_EVENT_COUNT			2

#define MAX_ROUTING_TABLE_SIZE		26
#define ROUTE_COST_INFINITY			(~((route_cost_t) 0))


struct routing_entry {
	char         router_name;
	route_cost_t cost;
	char         next_hop_router;
};


struct routing_entry routing_table[MAX_ROUTING_TABLE_SIZE];

char local_name = 0;

char *local_port   = NULL,
	 *remote_port1 = NULL,
	 *remote_port2 = NULL;

int sock_remote1 = 0,
	sock_remote2 = 0;



void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


void initialize_routing_table(void) {
	for (uint32_t i = 0; i < MAX_ROUTING_TABLE_SIZE; i++) {
		routing_table[i].router_name = 0;             // 0 indicates unset (no router can have the name '\0')
		routing_table[i].cost = ROUTE_COST_INFINITY;
		routing_table[i].next_hop_router = 0;         // 0 indicates unset (no router can have the name '\0')
	}
}


int epoll_setup(void) {
	int epollfd;

	epollfd = epoll_create1(0);

	if (epollfd < 0) {
		perror("epoll_create1");
		return -1;
	}
	return epollfd;
}



int epoll_add(int epollfd, int fd) {
	struct epoll_event event;

	event.data.fd = fd;
	event.events = EPOLLIN;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event)) {
		perror("epoll_ctl");
		return -1;
	}
	return 0;
}



void validate_cli_args(int argc, char *argv[]) {

	if (argc != 3 && argc != 4) {
		printf(
			"Usage: %s ThisRouterLetterName LocalPort RemotePort [RemotePort2]\n\n",
			argv[0]);
		exit(EXIT_FAILURE);
	}

	local_name = argv[1][0];
	local_port = argv[2];
	remote_port1 = argv[3];
	if (argc == 4)
		remote_port2 = argv[4];

	if (strlen(argv[1]) > 1) {
		fprintf(
			stderr,
			"The name of this router may only be 1 letter long "
			"and should be unique (not case sensitive)\n");
		exit(EXIT_FAILURE);
	}


	for (size_t i = 0; i < strlen(local_port); i++) {
		if (!isdigit(local_port[i])) {
			fprintf(stderr, "The local port number provided must be numeric\n");
			exit(EXIT_FAILURE);
		}
	}

	for (size_t i = 0; i < strlen(remote_port1); i++) {
		if (!isdigit(remote_port1[i])) {
			fprintf(stderr, "The first remote port number provided must be numeric\n");
			exit(EXIT_FAILURE);
		}
	}

	for (size_t i = 0; remote_port2 != NULL && i < strlen(remote_port2); i++) {
		if (!isdigit(remote_port2[i])) {
			fprintf(stderr, "The second remote port number provided must be numeric\n");
			exit(EXIT_FAILURE);
		}
	}


	if (strtoul(local_port, NULL, 10) > 65535 || strtoul(local_port, NULL, 10) == 0) {
		fprintf(stderr, "Receiver port number must be between 1 to 65535\n");
		exit(EXIT_FAILURE);
	}

}



int main(int argc, char *argv[]) {
	int epollfd, epoll_count;
	struct epoll_event events[EPOLL_EVENT_COUNT];


	validate_cli_args(argc, argv);
	initialize_routing_table();

	// TODO: Open sockets


	epollfd = epoll_setup();
	if (epollfd < 0) {
		fprintf(
			stderr,
			"[%s : %d]: Failed to create epoll file descriptor\n",
			__FILE__,
			__LINE__);
		exit(EXIT_FAILURE);
	}

	epoll_add(epollfd, sock_rx_fd);
	epoll_add(epollfd, sock_tx_fd);


	for (;;) {

		epoll_count = epoll_wait(epollfd, events, EPOLL_EVENT_COUNT, get_timeout());

		if (epoll_count == -1) {
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		if (epoll_count == 0) {
			service_timeout(sock_tx_fd);
			continue;
		}

		for (int i = 0; i < epoll_count; i++) {

			if (events[i].data.fd == sock_tx_fd) {

			} else if (events[i].data.fd == sock_rx_fd) {


			} else {
				fprintf(
					stderr,
					"[%s : %d]: epoll_wait() returned an unknown file descriptor number!\n",
					__FILE__,
					__LINE__);
				exit(EXIT_FAILURE);
			}
		}
	}


	return EXIT_SUCCESS;
}
