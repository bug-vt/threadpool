/**
 * threadpool.c
 *
 * Written by: Bug Lee, Dana altarace
 * Last modified : 10/21/21
 */

#include "threadpool.h"
#include "wrapper.h"
#include <stdbool.h>
#include "list.h"

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
    pthread_cond_t done;
};

struct thread_pool {
    pthread_t *workers;
    int workerCount;
    struct list globalDeque;
    pthread_mutex_t poolMutex;
    //struct list completed; 
    //pthread_mutex_t completed_mutex;
    pthread_cond_t workAvail; 
    bool shutDown;
};

struct worker {
    struct thread_pool * pool;  // path to access global queue
    int index;                  // position inside the thread pool
    struct list localDeque;
    pthread_t tid;
};

static _Thread_local struct worker *currentWorker; 


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
    currentWorker->tid = pthread_self();
     
    struct thread_pool *pool = currentWorker->pool;

    Pthread_mutex_lock(&pool->poolMutex);

    while (!pool->shutDown) {
        // wait for workAvail signal
        Pthread_cond_wait(&pool->workAvail, &pool->poolMutex);
        
        struct list_elem *elem = NULL;
        if (list_empty(&currentWorker->localDeque)) {
            // case 3
            if (list_empty(&pool->globalDeque)) { 
                // steal work from other workers
            }
            // case 2
            else { 
                elem = list_pop_front(&pool->globalDeque);
            }
        }
        // case 1
        else { 
            elem = list_pop_front(&currentWorker->localDeque);
        }

        // perfrom work
        if (elem != NULL) {
            struct future *fut = list_entry(elem, struct future, link);
            fut->result = fut->task(pool, fut->args);
            fut->status = COMPLETED;
            Pthread_cond_signal(&fut->done); 
        }
    }
    
    Pthread_mutex_unlock(&pool->poolMutex);
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
    Pthread_mutex_init(&pool->poolMutex);
    Pthread_cond_init(&pool->workAvail);
    pool->shutDown = false;

    // create n worker threads
    pthread_t *workers = malloc(nthreads * sizeof(pthread_t));
    for (int i = 0; i < nthreads; i++) {
        struct worker *newWorker = malloc(sizeof(struct worker)); 
        newWorker->pool = pool;
        list_init(&newWorker->localDeque);
        newWorker->index = i;

        Pthread_create(workers + i, worker_thread, newWorker);                       
    }

    pool->workers = workers;

    return pool;
}


void 
thread_pool_shutdown_and_destroy(struct thread_pool * pool)
{
    pool->shutDown = true;
    for (int i = 0; i < pool->workerCount; i++) {
        pthread_join(pool->workers[i], NULL);
    }
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
    Pthread_cond_init(&fut->done);

    // lock for share data (global, local deque)
    Pthread_mutex_lock(&pool->poolMutex); 

    // external submission from main thread
    if (currentWorker == NULL) { 
        list_push_front(&pool->globalDeque, &fut->link);
        // notify all workers that work is available
        Pthread_cond_signal(&pool->workAvail); 
    }
    else { //internal submision from worker thread
        list_push_front(&currentWorker->localDeque, &fut->link);
        // notify all workers that work is available
        Pthread_cond_signal(&pool->workAvail); 
    }

    Pthread_mutex_unlock(&pool->poolMutex);
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

    Pthread_mutex_lock(&pool->poolMutex);

    while (fut->status != COMPLETED) {
        if (currentWorker == NULL) {
            Pthread_cond_wait(&fut->done, &pool->poolMutex);
        }
        else {
            // TO DO: implement work helping
            Pthread_cond_wait(&fut->done, &pool->poolMutex); // place holder code
        }
    }

    Pthread_mutex_unlock(&pool->poolMutex);
    return fut->result;
}

void 
future_free(struct future * fut)
{
    free(fut);
}
