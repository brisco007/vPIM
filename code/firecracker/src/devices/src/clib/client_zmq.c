#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zmq.h>

#define LINK "tcp://localhost:5555"
#define MSG_SIZE 100


typedef enum {
    ReqAlloc = 0,
    ReqFree = 1
} ReqType;

typedef enum {
    ResOk = 0,
    ResFailed = 1,
    ResUnavailable = 2
} ResponseStatus;

typedef struct {
    char rank_path[MSG_SIZE];
    char dax_path[MSG_SIZE];
    int is_owned;
    unsigned int vpim_id;
    int rank_id;
} Entry;

typedef struct {
    unsigned int req_type;
    unsigned int req_len;
    unsigned int vpim_id;
} Request;

typedef struct {
    unsigned int req_type;
    unsigned int status;
    unsigned int req_len;
    unsigned int vpim_id;
    char rank_path[MSG_SIZE];
    char dax_path[MSG_SIZE];
    unsigned int rank_id;
} Response;

int init_zmq_context(void** context);
int init_zmq_requester(void* context, void** requester);
int zmq_terminate(void* context, void* requester) ;
Response* send_rank_request(void* requester, Request request);
    
void serialize_response(Response *response, char *buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%u,%u,%u,%u,%s,%s,%u",
        response->req_type, response->status, response->req_len,
        response->vpim_id, response->rank_path, response->dax_path,
        response->rank_id);
}


int deserialize_response(char *buffer, Response *response) {
    char *token;
    const char delimiter[] = ",";
    
    token = strtok(buffer, delimiter);
    if (token == NULL) return 0;
    response->req_type = (unsigned int) atoi(token);
    
    token = strtok(NULL, delimiter);
    if (token == NULL) return 0;
    response->status = (unsigned int) atoi(token);
    
    token = strtok(NULL, delimiter);
    if (token == NULL) return 0;
    response->req_len = (unsigned int) atoi(token);
    
    token = strtok(NULL, delimiter);
    if (token == NULL) return 0;
    response->vpim_id = (unsigned int) atoi(token);
    
    token = strtok(NULL, delimiter);
    if (token == NULL) return 0;
    strncpy(response->rank_path, token, MSG_SIZE);
    
    token = strtok(NULL, delimiter);
    if (token == NULL) return 0;
    strncpy(response->dax_path, token, MSG_SIZE);
    
    token = strtok(NULL, delimiter);
    if (token == NULL) return 0;
    response->rank_id = (unsigned int) atoi(token);
    
    return 1;
}
    

int init_zmq_context(void** context) {
    *context = zmq_ctx_new();
    return 0;
}

int init_zmq_requester(void* context, void** requester) {
    *requester = zmq_socket(context, ZMQ_REQ);
    //if not zero, then the connexion failed
    return zmq_connect(*requester, LINK);
}

int zmq_terminate(void* context, void* requester) {
    zmq_close(requester);
    zmq_ctx_destroy(context);
    return 0;
}

char* serializeRequest( Request* request) {
    // Calculer la taille requise pour la chaîne de caractères
    size_t serializedSize = snprintf(NULL, 0, "%u,%u,%u",
                                     request->req_type, request->req_len,
                                     request->vpim_id);
    
    // Allouer la mémoire pour la chaîne de caractères
    char* serializedRequest = (char*)malloc(serializedSize + 1);
    if (serializedRequest == NULL) {
        // Gestion de l'erreur d'allocation mémoire
        return NULL;
    }
    
    // Sérialiser la structure Request dans la chaîne de caractères
    snprintf(serializedRequest, serializedSize + 1, "%u,%u,%u",
             request->req_type, request->req_len, request->vpim_id);
    
    return serializedRequest;
}

Response* send_rank_request(void* requester, Request request) {
    
    char* request_msg =  serializeRequest(&request);
    zmq_send(requester,request_msg, strlen(request_msg), 0);
    Response* response ;
    
    char response_msg[MSG_SIZE];
    int rc = zmq_recv(requester, response_msg, MSG_SIZE, 0);

    if (rc != -1) {
        response_msg[rc] = '\0';        
        response = (Response *) malloc(sizeof(Response));
        if (deserialize_response(response_msg, response)) {
            printf("Done \n");
        } else {
            printf("Failed to deserialize response\n");
        }
    } else {
        perror("Failed to receive response");
    }
    return response;
}

/* int main() {
    printf("Connecting to the ZMQ server\n");
    
    void* requester = malloc(sizeof(char*));
    void* context = malloc(sizeof(char*));
    init_zmq_context(&context);

    if (init_zmq_requester(context, &requester) != 0) {
        perror("Failed to connect to the ZMQ server");
        return EXIT_FAILURE;
    }
    
    int request_nr;

    for (request_nr = 0; request_nr < 3; request_nr++) {

        Request req = {
            .req_type = 0,
            .req_len = 0,
            .vpim_id=99
        };
        
        Response* response = send_rank_request(requester, req);
        if(response != NULL) {
             printf("Received World %d: req_type=%u, status=%u, req_len=%u, vpim_id=%u, rank_path=%s, dax_path=%s, rank_id=%u\n",
                0, response->req_type, response->status, response->req_len,
                response->vpim_id, response->rank_path, response->dax_path, response->rank_id);
        }
    }
    
    zmq_terminate(context, requester);
    return EXIT_SUCCESS;
}
 */