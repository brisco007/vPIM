// server.c
#include "../include/server.h"
#include "../include/transport.h"

#define PORT 8080
#define MAX_PENDING 10

/* int init_server() {
    int server_socket;
    struct sockaddr_in server_address;

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket to IP and port
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Socket binding failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, MAX_PENDING) == -1) {
        perror("Socket listening failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    return server_socket;
} */

int init_server() {
    int server_socket;
    struct sockaddr_un server_address;

    // Create socket
    if ((server_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket to IP and port
    server_address.sun_family = AF_UNIX;
    strcpy(server_address.sun_path, MANAGER_UNIX_SOCKET); 

    unlink(MANAGER_UNIX_SOCKET);
    
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Socket binding failed\n");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, MAX_PENDING) == -1) {
        perror("Socket listening failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    return server_socket;
}

void receive_request(int client_socket, char *buffer, size_t buffer_size) {
    ssize_t bytes_received = recv(client_socket, buffer, buffer_size - 1, 0);
    if (bytes_received <= 0) {
        printf("Client disconnected.\n");
        close(client_socket);
        return;
    }

    buffer[bytes_received] = '\0';  // Ensure null-terminated string
}

void send_response(int client_socket, const char *response) {
    size_t response_length = strlen(response);
    ssize_t bytes_sent = send(client_socket, response, response_length, 0);
    if (bytes_sent == -1) {
        perror("Error sending response");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
}
/* 
void handle_request(int client_socket) {
    while (1) {
        char buffer[MAX_DATA_SIZE];
        receive_request(client_socket, buffer, sizeof(buffer));
        if (buffer[0] == '\0') {
            buffer[0] = ' ';  // Add null character if missing
        }

        printf("Received request: %s\n", buffer);

        // Process the request

        const char *response = "Response from C server :)";
        send_response(client_socket, response);

        if (strcmp(buffer, "exit") == 0) {
            break;  // Exit the loop and close the connection
        }
    }

    close(client_socket);
} */
void handle_request(int client_socket, ThreadPool * pool) {
    while (1) {
        char buffer[MAX_DATA_SIZE];
        receive_request(client_socket, buffer, sizeof(buffer));
        if (buffer[0] == '\0') {
            buffer[0] = ' ';  // Add null character if missing
        }

        printf("Received request: %s\n", buffer);

        // Process the request
        //printf("Received message: %s\n", buffer);
        Request* req = deserializeRequest(buffer);

        //printf("Received message vPIM: %d\n", req->vpim_id);

        if (req == NULL) {
            printf("Error: Failed to deserialize request\n");
            continue;
        }
        thread_pool_enqueue(pool, req, client_socket);
        break;
    }
}

void cleanup_server(int server_socket) {
    close(server_socket);
    // Perform any additional cleanup tasks if needed
    // ...
}
