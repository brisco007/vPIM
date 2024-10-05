// server.h
#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h> 
#include "common.h"
#include "thread_pool.h"
#include <sys/socket.h>
#include <sys/un.h>

int init_server();
void receive_request(int client_socket, char *buffer, size_t buffer_size);
void send_response(int client_socket, const char *response);
void handle_request(int client_socket, ThreadPool * pool);
void cleanup_server(int server_socket);

#endif  // SERVER_H
