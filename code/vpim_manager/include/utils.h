#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#ifndef UTILS_H
#define UTILS_H

#define MSG_REQ_SIZE 100
#define MAX_ID 99
#define DEV_PATH "/dev/"
#define MANAGER_UNIX_SOCKET "/tmp/pim_mgr.sock"
#define ALLOCATED 1
#define RESET_STATE 2
#define AVAILABLE 0
extern char** path_list;

enum ReqType {
    REQ_ALLOC=0,
    REQ_FREE=1
};
enum ResponseStatus {
    RES_OK=0, //everything is fine
    RES_FAILED=1, //there was an error
    RES_UNAVAILABLE=2 //no rank available
};
enum Bool {
    FALSE=0,
    TRUE=1
};

typedef struct _entry {
    char* rank_path;
    char* dax_path;
    int is_owned;
    unsigned int vpim_id;
    int rank_id;
} Entry;
extern pthread_mutex_t rank_table_mutex;

typedef struct _request {
    unsigned int req_type;
    unsigned int req_len;
    unsigned int vpim_id;
} Request;

typedef struct _response {
    unsigned int req_type;
    unsigned int status;
    unsigned int req_len;
    unsigned int vpim_id;
    char* rank_path;
    char* dax_path;
    unsigned int rank_id;
} Response;

typedef struct VPIM {
    unsigned int vpim_id;
    //char *unix_socket;
    struct VPIM *next;
    unsigned int rank_assigned;
} VPIM_t;


extern Entry* rank_table;
extern unsigned int vpim_id;
extern VPIM_t** head_vpim; 

#endif