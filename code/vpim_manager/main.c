#include <zmq.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <dirent.h>
#include "./include/table_management.h"
#include "./include/transport.h"
#include "./include/thread_pool.h"
#include "./include/observer.h"
#include "./include/server.h"
Entry* rank_table;
int nb_ranks;
pthread_t* rank_observer;

int server_should_exit = 0;
void handle_signal(int signal) {
    // Indicate that the server should exit
    server_should_exit = 1;
}
int main() {
    int server_socket = init_server();
    int res;
    rank_observer = (pthread_t*)malloc(sizeof(pthread_t));
    //Init rank structure.
    res = init_rank_table();
    if (res == -1) {
        printf("Error : init rank list failed");
        exit(1);
    }
    fill_path_list(nb_ranks);
    pthread_create(rank_observer, NULL, observe_is_owned, &nb_ranks);
    init_list_head();
    
    ThreadPool* pool = thread_pool_create(THREAD_POOL_SIZE, 100);

    while (!server_should_exit) {
        struct sockaddr_un client_address;
        socklen_t address_length = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, (socklen_t *)&address_length);
        if (client_socket == -1) {
            printf("ACCEPT ERROR\n");
            close(server_socket);
            close(client_socket);
            exit(EXIT_FAILURE);
        }

        printf("Client connected: %d:%s\n", client_address.sun_family, client_address.sun_path);

        handle_request(client_socket, pool);
    }
    
    cleanup_server(server_socket);
    return 0;
}

/*

int main() {
    int server_socket = init_server();

    while (1) {
        struct sockaddr_in client_address;
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, (socklen_t *)&address_length);
        if (client_socket == -1) {
            perror("Socket accepting failed");
            exit(EXIT_FAILURE);
        }

        printf("Client connected: %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        handle_request(client_socket);
    }

    return 0;
}

*/
/*
HANDLE ALL ERRORS : 
EAGAIN
Non-blocking mode was requested and no messages are available at the moment.
ENOTSUP
The zmq_recv() operation is not supported by this socket type.
EFSM
The zmq_recv() operation cannot be performed on this socket at the moment due to the socket not being in the appropriate state. This error may occur with socket types that switch between several states, such as ZMQ_REP. See the messaging patterns section of zmq_socket(3) for more information.
ETERM
The ØMQ context associated with the specified socket was terminated.
ENOTSOCK
The provided socket was invalid.
EINTR
The operation was interrupted by delivery of a signal before a message was available.
EFAULT
The message passed to the function was invalid.

*/