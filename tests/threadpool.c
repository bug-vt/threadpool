/**
 * threadpool.c
 *
 * Written by: Bug Lee, Dana altarace
 * Last modified : 10/21/21
 */

#include "threadpool.h"
//#include "wrapper.h"
#include <stdbool.h>
#include "list.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h> //free
#include <stdio.h> // perror
#include <unistd.h>

typedef enum {
    READY, RUNNING, COMPLETED
} Status;

struct future {
    fork_join_task_t task;
    void* args;
    void* result;
    struct list_elem link;
    struct thread_pool * pool;
    Status status;
    sem_t done;
};

struct worker {
    struct thread_pool * pool;  // path to access global queue
    struct list localDeque;
    pthread_t tid;
};

struct thread_pool {
    struct worker **workers;
    int workerCount;
    struct list globalDeque;
    pthread_mutex_t poolMutex;
    //struct list completed; 
    //pthread_mutex_t completed_mutex;
    pthread_cond_t workAvail; 
    bool shutDown;
};


static _Thread_local struct worker *currentWorker; 

static bool
have_work()
{
    struct thread_pool *pool = currentWorker->pool;
    if (!list_empty(&currentWorker->localDeque) || !list_empty(&pool->globalDeque)) {
        return true;
    }
    return false;
}

/**
 * Work stealing approach was used:
 * case 1. Perform task by dequeuing from worker's own deque
 * case 2. If empty, check global queue first and dequeue
 * case 3. Otherwise, try to steal task from other worker's deque
 * case 4. If all the above fail, idle
 */
static void *
worker_thread(void * newWorker)
{
    // set up current worker
    currentWorker = (struct worker*) newWorker;
     
    struct thread_pool *pool = currentWorker->pool;

    pthread_mutex_lock(&pool->poolMutex);

    while (!pool->shutDown) {
        // wait for workAvail signal
        pthread_cond_wait(&pool->workAvail, &pool->poolMutex);

        while (have_work()) {
            struct list_elem *elem = NULL;
            if (list_empty(&currentWorker->localDeque)) {
                // case 3
                if (list_empty(&pool->globalDeque)) { 
                    // steal work from other workers
                }
                // case 2
                else { 
                    elem = list_pop_back(&pool->globalDeque);
                }
            }
            // case 1
            else { 
                elem = list_pop_front(&currentWorker->localDeque);
            }
            // perfrom work
            if (elem != NULL) {
                struct future *fut = list_entry(elem, struct future, link);
                fut->status = RUNNING;

                if (fut->task == NULL) {
                    //printf("NULL task\n");
                }
                if (fut->task != NULL) {
                    // unlock to avoid recursive lock from calling fut->task function
                    pthread_mutex_unlock(&pool->poolMutex);
                    // generate sub-task and store result 
                    fut->result = fut->task(pool, fut->args);
                    pthread_mutex_lock(&pool->poolMutex);
                }

                fut->status = COMPLETED;
                sem_post(&fut->done); 
            }
        }
    }
    
    pthread_mutex_unlock(&pool->poolMutex);
    free(currentWorker);
    return NULL;
}


/**
 * Set up n worker thread inside the pool.
 */
struct thread_pool * 
thread_pool_new(int nthreads)
{
    // set up thread pool
    struct thread_pool *pool = malloc(sizeof(struct thread_pool)); 
    pool->workerCount = nthreads;
    list_init(&pool->globalDeque);
    pthread_mutex_init(&pool->poolMutex, NULL);
    pthread_cond_init(&pool->workAvail, NULL);
    pool->shutDown = false;

    pthread_mutex_lock(&pool->poolMutex); 

    // create n worker threads
    struct worker **workers = malloc(nthreads * sizeof(struct worker));
    for (int i = 0; i < nthreads; i++) {
        struct worker *newWorker = malloc(sizeof(struct worker)); 
        newWorker->pool = pool;
        list_init(&newWorker->localDeque);
        workers[i] = newWorker;

        int err = pthread_create(&newWorker->tid, NULL, worker_thread, newWorker);
        if (err != 0) {
            perror("pthread_create error");
            abort();
        }
    }

    pool->workers = workers;

    pthread_mutex_unlock(&pool->poolMutex); 

    return pool;
}


void 
thread_pool_shutdown_and_destroy(struct thread_pool * pool)
{
    pthread_mutex_lock(&pool->poolMutex); 

    pool->shutDown = true;
    // broadcast to all workers to wake up
    pthread_cond_broadcast(&pool->workAvail); 

    pthread_mutex_unlock(&pool->poolMutex); 
    // wait for all workers to come home
    for (int i = 0; i < pool->workerCount; i++) {
        pthread_join(pool->workers[i]->tid, NULL);
    }

    pthread_mutex_destroy(&pool->poolMutex);
    free(pool->workers); 
    free(pool);

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
    // lock for share data (global, local deque)
    pthread_mutex_lock(&pool->poolMutex); 

    struct future *fut = malloc(sizeof(struct future));
    fut->task = task;
    fut->args = data;
    fut->pool = pool;
    fut->status = READY;
    sem_init(&fut->done, 0, 0);


    // external submission from main thread
    if (currentWorker == NULL) { 
        list_push_front(&pool->globalDeque, &fut->link);
    }
    else { //internal submision from worker thread
        list_push_back(&currentWorker->localDeque, &fut->link);
    }
    // notify workers that work is available
    pthread_cond_signal(&pool->workAvail); 

    pthread_mutex_unlock(&pool->poolMutex);
    return fut;
}

/**
 * If the calling thread is the main thread (external thread), than
 * it will be blocked if the task have not been completed.
 * Otherwise, if the function was called by worker threads, than
 * it can help finishing task (Work helping).
 */
void *
future_get(struct future * fut)
{
    struct thread_pool * pool = fut->pool;

    // main thread
    if (currentWorker == NULL) {
        sem_wait(&fut->done);
    }
    // worker thread
    else {
        // TO DO: implement work helping
        if (fut->status == READY) {
            fut->result = fut->task(fut->pool, fut->args); 

            pthread_mutex_lock(&pool->poolMutex);
            fut->status = COMPLETED;
            sem_post(&fut->done); 
            pthread_mutex_unlock(&pool->poolMutex);
        }
        else if (fut->status != COMPLETED) {
            sem_wait(&fut->done);
        }
    }

    return fut->result;
}

void 
future_free(struct future * fut)
{
    free(fut);
}
