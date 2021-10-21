/**
 * threadpool.c
 *
 * Written by: Bug Lee, Dana altarace
 * Last modified : 10/20/21
 */

#include "threadpool.h"
#include "wrapper.h"
#include <stdbool.h>
#include "list.h"



struct future {
    fork_join_task_t task;
    void* args;
    void* result;
    struct list_elem link;
    // TO DO:
    // Synchronization primitives
};

struct thread_pool {
    pthread_t *workers;
    struct list globalDeque;
    pthread_mutex_t poolMutex;
    //struct list completed; 
    //pthread_mutex_t completed_mutex;
    pthread_cond_t workAvail; 
    bool shut_down;
};

struct worker {
    struct thread_pool * pool; // path to access global queue
    int index;
    struct list localDeque;
    pthread_t tid;
};

_Thread_local struct worker *currentWorker; 

static void *
worker_thread(void * no_arg)
{
    return NULL;
}

struct thread_pool * 
thread_pool_new(int nthreads)
{
    struct thread_pool *pool = malloc(sizeof(struct thread_pool)); 

    // create n worker threads
    pthread_t *workers = malloc(nthreads * sizeof(pthread_t));
    for (int i = 0; i < nthreads; i++) {
        Pthread_create(workers + i, worker_thread, NULL);                       
    }

    // set up thread pool
    pool->workers = workers;
    list_init(&pool->globalDeque);
    Pthread_mutex_init(&pool->poolMutex);
    Pthread_cond_init(&pool->workAvail);
    pool->shut_down = false;

    return pool;
}



void 
thread_pool_shutdown_and_destroy(struct thread_pool * pool)
{
}

/**
 * Submit new task to the thread pool.
 * If submission is from outside to main thread, then 
 * the taks will be added to global queue.
 * Otherwise, sub-task from the worker thread will be
 * added to worker's queue who created a sub-task. 
 */
struct future *
thread_pool_submit(struct thread_pool *pool, fork_join_task_t task, void *data)
{
    struct future *newTask = malloc(sizeof(struct future));
    newTask->task = task;
    newTask->args = data;

    // lock for share data (global, local deque)
    Pthread_mutex_lock(&pool->poolMutex); 

    // external submission from main thread
    if (currentWorker == NULL) { 
        list_push_front(&pool->globalDeque, &newTask->link);
        // notify all workers that work is available
        Pthread_cond_signal(&pool->workAvail); 
    }
    else { //internal submision from worker thread
        list_push_front(&currentWorker->localDeque, &newTask->link);
        // notify all workers that work is available
        Pthread_cond_signal(&pool->workAvail); 
    }

    Pthread_mutex_unlock(&pool->poolMutex);
    return newTask;
}


void *
future_get(struct future * task)
{
    return NULL;
}

void 
future_free(struct future * task)
{
}
