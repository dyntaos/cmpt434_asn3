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

#define MAX_ROUTING_TABLE_SIZE			26
#define ROUTING_BROADCAST_INTERVAL_MS	2000
#define ROUTE_COST_INFINITY				(~((route_cost_t) 0))

#define EPOLL_EVENT_COUNT				(3 + MAX_ROUTING_TABLE_SIZE)



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

int sock_listen = 0,
	sock_accepted[MAX_ROUTING_TABLE_SIZE],
	sock_remote1 = 0,
	sock_remote2 = 0;



void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int *get_available_accept_socket_int(void) {
	for (uint32_t i = 0; i < MAX_ROUTING_TABLE_SIZE; i++) {
		if (sock_accepted[i] < 1) {
			return &sock_accepted[i];
		}
	}
	return -1;
}


void initialize_routing_table(void) {
	for (uint32_t i = 0; i < MAX_ROUTING_TABLE_SIZE; i++) {
		routing_table[i].router_name = 0;             // 0 indicates unset (no router can have the name '\0')
		routing_table[i].cost = ROUTE_COST_INFINITY;
		routing_table[i].next_hop_router = 0;         // 0 indicates unset (no router can have the name '\0')
		sock_accepted[i] = -1;
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



void service_timeout(void) {
	// TODO
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
	int epollfd,
		epoll_count;
	struct epoll_event events[EPOLL_EVENT_COUNT];
	struct routing_entry incoming_table[MAX_ROUTING_TABLE_SIZE];

	validate_cli_args(argc, argv);
	initialize_routing_table();


	epollfd = epoll_setup();
	if (epollfd < 0) {
		fprintf(
			stderr,
			"[%s : %d]: Failed to create epoll file descriptor\n",
			__FILE__,
			__LINE__);
		exit(EXIT_FAILURE);
	}


	sock_listen = tcp_server_init(local_port);
	if (sock_listen <= 0) {
		fprintf(
			stderr,
			"[%s : %d]: Failed to open a server socket on port %d\n",
			__FILE__,
			__LINE__,
			local_port);
		exit(EXIT_FAILURE);
	}
	epoll_add(epollfd, sock_listen);


	sock_remote1 = tcp_client_init("127.0.0.1", remote_port1);
	if (sock_remote1 <= 0) {
		fprintf(
			stderr,
			"[%s : %d]: Failed to open a client socket to remote port 1: %d\n",
			__FILE__,
			__LINE__,
			remote_port1);
		exit(EXIT_FAILURE);
	}
	epoll_add(epollfd, sock_remote1);


	if (remote_port2 != NULL) {
		sock_remote2 = tcp_client_init("127.0.0.1", remote_port2);
		if (sock_remote1 <= 0) {
			fprintf(
				stderr,
				"[%s : %d]: Failed to open a client socket to remote port 2: %d\n",
				__FILE__,
				__LINE__,
				remote_port2);
			exit(EXIT_FAILURE);
		}
		epoll_add(epollfd, sock_remote2);
	}


	for (;;) {

		epoll_count = epoll_wait(epollfd, events, EPOLL_EVENT_COUNT, ROUTING_BROADCAST_INTERVAL_MS);

		if (epoll_count == -1) {
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		if (epoll_count == 0) {
			service_timeout();
			continue;
		}

		for (int i = 0; i < epoll_count; i++) {

			if (events[i].data.fd == sock_listen) {
				// New incoming connection
				// TODO


			} else {
				// Data on an existing socket: sock_remote1, sock_remote2 or an element of sock_accepted[]
				// TODO


			}
		}
	}


	return EXIT_SUCCESS;
}
