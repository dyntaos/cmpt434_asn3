/**********************************
 **           CMPT 434           **
 **  University of Saskatchewan  **
 **         Assignment 3         **
 **----------------------------- **
 **          Kale Yuzik          **
 **     kay851@mail.usask.ca     **
 **      kay851    11071571      **
 **********************************/

#ifndef _TCP_H
#define _TCP_H

#include <sys/socket.h>
#include <netdb.h>

#define BACKLOG 10


int tcp_client_init(char *host, char *port);
int tcp_server_init(char *port);
int tcp_accept(int sock_fd);
int tcp_receive(int socket, void *buffer, size_t buffer_len);
int tcp_send(int socket, void *buffer, size_t buffer_len);


#endif //_TCP_H