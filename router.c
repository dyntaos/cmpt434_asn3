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
#include <stdbool.h>

#include "tcp.h"



typedef uint32_t route_cost_t; // May be changed, but must be unsigned

#define LINK_WEIGHT_COST				1
#define MAX_ROUTING_TABLE_SIZE			26
#define ROUTING_BROADCAST_INTERVAL		2
#define ROUTE_COST_INFINITY				(~((route_cost_t) 0))

#define EPOLL_EVENT_COUNT				(3 + MAX_ROUTING_TABLE_SIZE)

const char localhost[] = "127.0.0.1";


struct router_interface {
	int socket;
	char name;
};

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

int sock_listen = 0;

struct router_interface remote1,
						remote2,
						accepted_connections[MAX_ROUTING_TABLE_SIZE];

int epollfd,
	epoll_count;

time_t last_broadcast = 0;




struct router_interface *get_available_accept_socket_interface(void) {
	for (uint32_t i = 0; i < MAX_ROUTING_TABLE_SIZE; i++) {
		if (accepted_connections[i].socket < 1) {
			accepted_connections[i].name = 0;
			return &accepted_connections[i];
		}
	}
	return NULL;
}


void initialize(void) {
	routing_table[0].router_name = local_name;
	routing_table[0].cost = 0;
	routing_table[0].next_hop_router = 0;

	for (uint32_t i = 1; i < MAX_ROUTING_TABLE_SIZE; i++) {
		routing_table[i].router_name = 0;             // 0 indicates unset (no router can have the name '\0')
		routing_table[i].cost = ROUTE_COST_INFINITY;
		routing_table[i].next_hop_router = 0;         // 0 indicates unset (no router can have the name '\0')
		accepted_connections[i].socket = -1;
		accepted_connections[i].name = 0;
	}
}



void print_routing_table(void) {
	printf(">> Routing Table for '%c' on port %s <<\n", local_name, local_port);
	printf("Destination\tCost\tNextHop\n");
	for (uint32_t i = 0; i < MAX_ROUTING_TABLE_SIZE; i++) {
		if (routing_table[i].router_name == 0) continue;

		printf("'%c'\t\t", routing_table[i].router_name);
		if (routing_table[i].cost == ROUTE_COST_INFINITY) {
			printf("INF\t\t");
		} else {
			printf("%u\t\t", routing_table[i].cost);
		}
		if (routing_table[i].next_hop_router == 0) {
			printf(" \n");
		} else {
			printf("'%c'\n", routing_table[i].next_hop_router);
		}
	}
	printf("\n");
}



char get_routing_table_owner(struct routing_entry table[]) {
	for (uint32_t i = 0; i < MAX_ROUTING_TABLE_SIZE; i++) {
		if (table[i].cost == 0 && table[i].router_name != 0) {
			return table[i].router_name;
		}
	}
	return 0;
}



struct routing_entry *find_router_entry(char router, struct routing_entry table[]) {
	for (uint32_t i = 0; i < MAX_ROUTING_TABLE_SIZE; i++) {
		if (table[i].router_name == router) {
			return &table[i];
		}
	}
	return NULL;
}



void add_to_route(char from_router, struct routing_entry *entry) {
	struct routing_entry *local_entry = NULL;

	if (from_router == 0) return;
	if (entry == NULL) return;
	if (entry->router_name == 0) return;

	local_entry = find_router_entry(entry->router_name, routing_table);
	if (local_entry == NULL) {
		// Don't have a record of this router name, try and create one
		local_entry = find_router_entry(0, routing_table);

		if (local_entry == NULL)
			return; // Routing table is full, abort

	} else {
		// Found an existing entry for this router in the table, check costs
		if (local_entry->cost <= entry->cost + LINK_WEIGHT_COST && local_entry->next_hop_router != from_router)
			return; // Don't save a new route if it isn't shorter than the existing route
	}

	local_entry->router_name = entry->router_name;
	local_entry->cost = (entry->cost == ROUTE_COST_INFINITY) ? ROUTE_COST_INFINITY : entry->cost + LINK_WEIGHT_COST;
	local_entry->next_hop_router = from_router;
}



void process_neighbour_routing_table(struct routing_entry table[]) {
	char origin = get_routing_table_owner(table);

	for (uint32_t i = 0; i < MAX_ROUTING_TABLE_SIZE; i++) {
		add_to_route(origin, &table[i]);
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

	if (argc < 3 || argc > 5) {
		printf(
			"Usage: %s LocalRouterLetterName LocalPort [RemotePort1] [RemotePort2]\n\n",
			argv[0]);
		exit(EXIT_FAILURE);
	}

	local_name = argv[1][0];
	local_port = argv[2];
	if (argc == 4) remote_port1 = argv[3];
	if (argc == 5) remote_port2 = argv[4];

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

	for (size_t i = 0; remote_port1 != NULL && i < strlen(remote_port1); i++) {
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



void send_routing_table(struct router_interface *router, char *socket_port, bool reopen_if_closed) {
	int result;

	if (router->socket > 0) {
		result = tcp_send(router->socket, routing_table, sizeof(routing_table));

		if (result < 0) {
			// Connection was closed
			close(router->socket); // Ensure it is fully closed

			if (reopen_if_closed) {
				// Try and reopen - send routing table next timeout
				// The socket will have already been deregistered from the epoll socket
				router->socket = tcp_client_init(localhost, socket_port);
				if (router->socket <= 0) {
					router->name = 0;
					router->socket = -1;
					return;
				}
				epoll_add(epollfd, router->socket);

			} else {
				router->socket = -1;
				router->name = 0;
			}
		}
	} else if (reopen_if_closed) {
		router->socket = tcp_client_init(localhost, socket_port);
		if (router->socket <= 0) {
			router->name = 0;
			router->socket = -1;
			return;
		}
		epoll_add(epollfd, router->socket);
	}
}



void broadcast_routing_table(void) {
	if (remote_port1 != NULL) send_routing_table(&remote1, remote_port1, true);
	if (remote_port2 != NULL) send_routing_table(&remote2, remote_port2, true);

	for (uint32_t i = 0; i < MAX_ROUTING_TABLE_SIZE; i++) {
		send_routing_table(&accepted_connections[i], NULL, false);
	}
}



void associate_socket_to_router_name(int sock_fd, struct routing_entry table[]) {
	struct router_interface *router = NULL;
	char origin;

	if (sock_fd <= 0) return;
	if (table == NULL) return;

	origin = get_routing_table_owner(table);
	if (origin == 0) return;

	if (sock_fd == remote1.socket) {
		router = &remote1;
	} else if (sock_fd == remote2.socket) {
		router = &remote2;
	} else {
		for (uint32_t i = 0; i < MAX_ROUTING_TABLE_SIZE; i++) {
			if (sock_fd == accepted_connections[i].socket) {
				router = &accepted_connections[i];
				break;
			}
		}
	}
	if (router != NULL)
		router->name = origin;
}



void prune_routing_table(void) {
	bool found;

	for (uint32_t i = 0; i < MAX_ROUTING_TABLE_SIZE; i++) {
		found = false;
		if (routing_table[i].router_name == 0) continue;
		if (routing_table[i].router_name == local_name) continue;

		if (routing_table[i].next_hop_router == remote1.name) continue;
		if (routing_table[i].next_hop_router == remote2.name) continue;

		for (uint32_t k = 0; k < MAX_ROUTING_TABLE_SIZE; k++) {
			if (accepted_connections[k].socket > 0 && routing_table[i].next_hop_router == accepted_connections[k].name) {
				found = true;
				break;
			}
		}
		if (found) continue;

		// If execution reaches here, there is no live socket
		// to the next hop of that route entry, so remove the entry
		routing_table[i].cost = ROUTE_COST_INFINITY;
		routing_table[i].next_hop_router = 0;
	}
}



int main(int argc, char *argv[]) {
	struct epoll_event events[EPOLL_EVENT_COUNT];
	struct routing_entry incoming_table[MAX_ROUTING_TABLE_SIZE];
	int recv_len;

	validate_cli_args(argc, argv);
	initialize();


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
			"[%s : %d]: Failed to open a server socket on port %s\n",
			__FILE__,
			__LINE__,
			local_port);
		exit(EXIT_FAILURE);
	}
	epoll_add(epollfd, sock_listen);

	if (remote_port1 != NULL) {
		remote1.socket = tcp_client_init(localhost, remote_port1);
		if (remote1.socket <= 0) {
			fprintf(
				stderr,
				"[%s : %d]: Failed to open a client socket to remote port 1: %s\n",
				__FILE__,
				__LINE__,
				remote_port1);
			exit(EXIT_FAILURE);
		}
		epoll_add(epollfd, remote1.socket);
	}


	if (remote_port2 != NULL) {
		remote2.socket = tcp_client_init(localhost, remote_port2);
		if (remote2.socket <= 0) {
			fprintf(
				stderr,
				"[%s : %d]: Failed to open a client socket to remote port 2: %s\n",
				__FILE__,
				__LINE__,
				remote_port2);
			exit(EXIT_FAILURE);
		}
		epoll_add(epollfd, remote2.socket);
	}

	last_broadcast = time(NULL);

	for (;;) {
		int timeout = (last_broadcast - time(NULL) + ROUTING_BROADCAST_INTERVAL) * 1000;
		if (timeout < 0) timeout = 0;

		epoll_count = epoll_wait(
			epollfd,
			events,
			EPOLL_EVENT_COUNT,
			timeout);

		if (epoll_count == -1) {
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		if (epoll_count == 0) {
			print_routing_table();
			broadcast_routing_table();
			last_broadcast = time(NULL);
			continue;
		}

		for (int i = 0; i < epoll_count; i++) {

			if (events[i].data.fd == sock_listen) {
				// New incoming connection - accept if possible and register with epoll
				int sock_temp;
				struct router_interface *router = get_available_accept_socket_interface();

				sock_temp = tcp_accept(sock_listen);

				if (router == NULL) {
					fprintf(
						stderr,
						"Received incomming connection request when all "
						"available sockets are already in use; rejecting!\n");
					close(sock_temp);
					continue;
				}

				router->socket = sock_temp;
				epoll_add(epollfd, router->socket);


			} else {
				// Data on an existing socket: sock_remote1, sock_remote2 or an element of sock_accepted[]
				// This is an incoming routing table
				recv_len = tcp_receive(events[i].data.fd, incoming_table, sizeof(incoming_table));

				if (recv_len == 0) {
					// Connection closed -- an attempt to open CLI provided ports on broadcast
					if (events[i].data.fd == remote1.socket) {
						close(remote1.socket);
						remote1.name = 0;
						remote1.socket = -1;

					} else if (events[i].data.fd == remote2.socket) {
						close(remote2.socket);
						remote2.name = 0;
						remote2.socket = -1;

					} else {
						for (uint32_t j = 0; j < MAX_ROUTING_TABLE_SIZE; j++) {
							if (events[i].data.fd == accepted_connections[j].socket) {
								close(accepted_connections[j].socket);
								accepted_connections[j].name = 0;
								accepted_connections[j].socket = -1;
								break;
							}
						}
					}

				} else if (recv_len != sizeof(incoming_table)) {
					fprintf(
						stderr,
						"Received unexpected message size (%d), discarding!\n",
						recv_len);
					continue;
				}

				associate_socket_to_router_name(events[i].data.fd, incoming_table);
				process_neighbour_routing_table(incoming_table);
			}
		}
		prune_routing_table();
	}


	return EXIT_SUCCESS;
}
