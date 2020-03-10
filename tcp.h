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


#define BACKLOG 20


int tcp_client_init(const char *host, const char *port);
int tcp_server_init(const char *port);
int tcp_accept(int sock_fd);
int tcp_receive(int socket, void *buffer, size_t buffer_len);
int tcp_send(int socket, void *buffer, size_t buffer_len);


#endif //_TCP_H