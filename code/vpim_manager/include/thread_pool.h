#include <assert.h>
#include <pthread.h>
#include <zmq.h>
#include <string.h>

#include "./utils.h"

#ifndef THREADPOOL_H
#define THREADPOOL_H

#define THREAD_POOL_SIZE 8

typedef struct _task {
    Request* request;
    int client_sock;
} Task;

typedef struct _thread_pool {
    Task* task_queue;
    int queue_size;
    int queue_front;
    int queue_rear;
    int shutdown;
    int queue_count;
    pthread_t* threads;
    int num_threads;
    pthread_cond_t condition;
    pthread_mutex_t mutex;
} ThreadPool;



ThreadPool* thread_pool_create(int thread_count, int queue_size);
void thread_pool_destroy(ThreadPool* pool);
void thread_pool_enqueue(ThreadPool* pool, Request* request, int client_socket);
Task thread_pool_dequeue(ThreadPool* pool);
void* thread_worker(void* arg);








#endif