/**
 * threadpool.c
 *
 * Written by: Bug Lee, Dana altarace
 * Last modified : 10/30/21
 */

#include "threadpool.h"
#include <stdbool.h>
#include "list.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h> // free
#include <stdio.h> // perror
#include <unistd.h>

typedef enum {
    READY, RUNNING, COMPLETED
} Status;

struct future {
    fork_join_task_t task;
    void * args;
    void * result;
    struct list_elem link;
    struct thread_pool * pool;
    Status status;
    pthread_cond_t done;
    struct worker * owner;
    int padding[4];
};

struct worker {
    struct thread_pool * pool;  // path to access global queue
    struct list localDeque;
    pthread_t tid;
    pthread_mutex_t mutex;
    int padding[10];
};

struct thread_pool {
    struct worker **workers;
    int workerCount; // workerCount is nthreads. 
    struct list globalDeque;
    pthread_mutex_t mutex;
    pthread_cond_t workAvail; 
    pthread_barrier_t barrier;
    bool shutDown;
    int padding[4];
};


static _Thread_local struct worker *currentWorker; 


/**
 * steal work from other workers.
 * @return true if the current worker steal work; false otherwise.
 */
static bool
steal_work()
{ 
    // 1. iterate through workers inside the pool
    // 2. find the first worker have work (non-empty local deque)
    // 3. steal that work (dequeue from the local deque)
    //
    // if all fail do nothing
    
    struct thread_pool *pool = currentWorker->pool;
    struct list_elem * elem;

    pthread_mutex_unlock(&pool->mutex);

    for (int i = 0; i < pool->workerCount; i++) {

        pthread_mutex_lock(&pool->workers[i]->mutex);

        if (pool->workers[i] != currentWorker && !list_empty(&pool->workers[i]->localDeque)) {
            elem = list_pop_back(&pool->workers[i]->localDeque);
            if (elem != NULL) {
                struct future * fut = list_entry(elem, struct future, link);
                fut->status = RUNNING;

                // perfrom work
                pthread_mutex_unlock(&pool->workers[i]->mutex);
                // generate sub-task and store result 
                fut->result = fut->task(pool, fut->args);
                pthread_mutex_lock(&pool->workers[i]->mutex);

                fut->status = COMPLETED;
                pthread_cond_signal(&fut->done);
            }
            pthread_mutex_unlock(&pool->workers[i]->mutex);
            pthread_mutex_lock(&pool->mutex);
            return true;
        }

        pthread_mutex_unlock(&pool->workers[i]->mutex);
    }

    pthread_mutex_lock(&pool->mutex);
    return false;

}


/**
 * In fully-strict workload model, worker will either 
 * 1. steal work from other workers
 * 2. fetch the work from the global queue
 */
static void *
worker_thread(void * newWorker)
{
    // set up current worker
    currentWorker = (struct worker*) newWorker;
     
    struct thread_pool *pool = currentWorker->pool;

    pthread_barrier_wait(&pool->barrier);
    pthread_mutex_lock(&pool->mutex);

    while (true) {

        if (pool->shutDown) {
            break;
        }
        while (!steal_work()) {
            if (pool->shutDown || !list_empty(&pool->globalDeque)) {
                break;
            }
            // wait for workAvail signal
            pthread_cond_wait(&pool->workAvail, &pool->mutex);
        }

        if (!list_empty(&pool->globalDeque)) { 
            struct list_elem * elem;
            elem = list_pop_back(&pool->globalDeque);
            if (elem != NULL) {
                struct future * fut = list_entry(elem, struct future, link);
                fut->status = RUNNING;

                // perfrom work
                pthread_mutex_unlock(&pool->mutex);
                // generate sub-task and store result 
                fut->result = fut->task(pool, fut->args);
                pthread_mutex_lock(&pool->mutex);

                fut->status = COMPLETED;
                pthread_cond_signal(&fut->done);
            }
        }
    }

    pthread_mutex_unlock(&pool->mutex);

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
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->workAvail, NULL);
    pthread_barrier_init(&pool->barrier, NULL, nthreads + 1);
    pool->shutDown = false;

    // create n worker threads
    struct worker **workers = malloc(nthreads * sizeof(struct worker));
    for (int i = 0; i < nthreads; i++) {
        struct worker *newWorker = malloc(sizeof(struct worker)); 
        newWorker->pool = pool;
        list_init(&newWorker->localDeque);
        pthread_mutex_init(&newWorker->mutex, NULL);
        workers[i] = newWorker;

        int err = pthread_create(&newWorker->tid, NULL, worker_thread, newWorker);
        if (err != 0) {
            perror("pthread_create error");
            abort();
        }
    }

    pool->workers = workers;

    // wait for all worker threads to be spawn before continuing
    pthread_barrier_wait(&pool->barrier);


    return pool;
}

/**
 * release all the resources held by thread pool
 */
void 
thread_pool_shutdown_and_destroy(struct thread_pool * pool)
{
    pthread_mutex_lock(&pool->mutex); 

    pool->shutDown = true;

    // broadcast to all workers to wake up
    pthread_cond_broadcast(&pool->workAvail); 

    pthread_mutex_unlock(&pool->mutex); 
    
    // wait for all workers to come home
    for (int i = 0; i < pool->workerCount; i++) {
        pthread_join(pool->workers[i]->tid, NULL);
    }

    for (int i = 0; i < pool->workerCount; i++) {
        pthread_mutex_destroy(&pool->workers[i]->mutex);
        free(pool->workers[i]);
    }

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->workAvail);
    pthread_barrier_destroy(&pool->barrier);
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

    struct future *fut = malloc(sizeof(struct future));
    fut->task = task;
    fut->args = data;
    fut->pool = pool;
    fut->status = READY;
    pthread_cond_init(&fut->done, NULL);


    // external submission from main thread
    if (currentWorker == NULL) {
        fut->owner = NULL;

        pthread_mutex_lock(&pool->mutex); 
        list_push_front(&pool->globalDeque, &fut->link);
        pthread_mutex_unlock(&pool->mutex); 
    }
    else { //internal submision from worker thread
        fut->owner = currentWorker;

        pthread_mutex_lock(&currentWorker->mutex); 
        list_push_front(&currentWorker->localDeque, &fut->link);
        pthread_mutex_unlock(&currentWorker->mutex); 
    }


    // notify workers that work is available
    pthread_cond_signal(&pool->workAvail); 
        
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
    if (fut->owner == NULL) {
       
        pthread_mutex_lock(&pool->mutex);
        while (fut->status != COMPLETED) {
            pthread_cond_wait(&fut->done, &pool->mutex);
        }
        pthread_mutex_unlock(&pool->mutex);
    }
    // worker thread
    else {
        pthread_mutex_lock(&fut->owner->mutex);
        // -- work helping --
        // 1. if the task is not yet started, then the worker would
        // executed it itself.
        if (fut->status == READY) { // inside local deque
            // future is READY only when it is inside the deque.
            // so it is safe and must be remove from the deque to run it.
            list_remove(&fut->link);
            fut->status = RUNNING;

            // unlock to avoid recursive lock from calling fut->task function
            pthread_mutex_unlock(&fut->owner->mutex);
            // Perform task
            fut->result = fut->task(fut->pool, fut->args); 
            pthread_mutex_lock(&fut->owner->mutex);
            
            fut->status = COMPLETED;
            pthread_cond_signal(&fut->done); 
        }
        // 2. otherwise, it could wait for it to finish
        else { // other worker is working on it
            while (fut->status != COMPLETED) {
                pthread_cond_wait(&fut->done, &fut->owner->mutex);
            }
        }
        pthread_mutex_unlock(&fut->owner->mutex);
    }

    return fut->result;
}

/**
 * release resources that held by future
 */
void 
future_free(struct future * fut)
{
    pthread_cond_destroy(&fut->done);
    free(fut);
}
