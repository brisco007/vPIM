#include "../include/thread_pool.h"
#include "../include/transport.h"
#include "../include/handlers.h"
#include "../include/server.h"

ThreadPool* thread_pool_create(int num_threads, int queue_size) {
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    pool->task_queue = (Task*)malloc(sizeof(Task) * queue_size);
    pool->queue_size = queue_size;
    pool->queue_front = 0;
    pool->queue_rear = -1;
    pool->queue_count = 0;
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
    pool->num_threads = num_threads;
    pthread_cond_init(&(pool->condition), NULL);
    pthread_mutex_init(&(pool->mutex), NULL);

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&(pool->threads[i]), NULL, thread_worker, pool);
    }

    return pool;
}


void thread_pool_destroy(ThreadPool* pool) {
    pthread_mutex_lock(&(pool->mutex));
    pool->shutdown = 1;
    pthread_mutex_unlock(&(pool->mutex));

    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    pthread_mutex_destroy(&(pool->mutex));
    pthread_cond_destroy(&(pool->condition));
    free(pool->task_queue);
    free(pool->threads);
    free(pool);
}

void thread_pool_enqueue(ThreadPool* pool, Request* request, int client_socket) {
    pthread_mutex_lock(&(pool->mutex));
    while (pool->queue_count >= pool->queue_size) {
        pthread_cond_wait(&(pool->condition), &(pool->mutex));
    }
    pool->queue_rear = (pool->queue_rear + 1) % pool->queue_size;
    Task task = {
        .request = request,
        .client_sock = client_socket
    };
    pool->task_queue[pool->queue_rear] = task;
    pool->queue_count++;

    pthread_cond_signal(&(pool->condition));
    pthread_mutex_unlock(&(pool->mutex));
    //printf("ENQUEUED : %d\n",request->vpim_id);
}

Task thread_pool_dequeue(ThreadPool* pool) {
    pthread_mutex_lock(&(pool->mutex));

    while (pool->queue_count <= 0 && !pool->shutdown) {
        pthread_cond_wait(&(pool->condition), &(pool->mutex));
    }

    Task task = { .request = NULL, .client_sock = -1 };
    if (pool->queue_count > 0) {
        task = pool->task_queue[pool->queue_front];
        //printf("DEQUEUED : %d\n",task.request->vpim_id);

        pool->queue_front = (pool->queue_front + 1) % pool->queue_size;
        pool->queue_count--;
        pthread_cond_signal(&(pool->condition));
    }

    pthread_mutex_unlock(&(pool->mutex));
    return task;
}

void* thread_worker(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    Response* res;
    char* serializedResponse;
    while (1) {
        Task task = thread_pool_dequeue(pool);

        if (task.client_sock == -1) {
            break;  // Terminates the thread when shutdown is initiated
        }

        //printf("Processing request in thread %lu\n", pthread_self());

        Request* req = task.request;
        int client_socket = task.client_sock;
        //printf("req tyoe %d\n", req->req_type);
        switch (req->req_type) {
            case REQ_ALLOC:
                res = handle_alloc_req(req);
                break;
            case REQ_FREE:
                res = handle_free_req(req);
                break;
            default:
                res = NULL;  // Requête non reconnue
                break;
        }

         if (res == NULL) {
            res = (Response*)malloc(sizeof(Response));
            res->req_type = req->req_type;
            res->status = RES_FAILED;
            res->req_len = 0;
            res->vpim_id = req->vpim_id;
            res->rank_path = NULL;
            res->dax_path = NULL;
            res->rank_id = 99;
        }
                        

        /* res = (Response) {
            .req_type = req->req_type,
            .req_len = 0,
            .vpim_id = req->vpim_id,
            .rank_path = "/dev/dpu_rank0",
            .dax_path = "/dev/dax0.0",
            .rank_id = 2
        }; */
        serializedResponse = serializeResponse(res);
        //printf("Serialized response: %s\n", serializedResponse);

        // Envoyer la réponse
        send_response(client_socket, serializedResponse);
        close(client_socket);
        //printf("Response sent\n");
        
        free(serializedResponse);
        
        free(req);
        free(res);
    }

    return NULL;
}