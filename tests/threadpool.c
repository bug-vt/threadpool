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
    struct thread_pool * pool;  // path to access global queue
    int index;                  // position inside the thread pool
    struct list localDeque;
    pthread_t tid;
};

_Thread_local struct worker *currentWorker; 


/**
 * Work stealing approach was used:
 * case 1. Perform task by dequeuing from worker's own deque
 * case 2. If empty, check global queue first and dequeue
 * case 3. Otherwise, try to steal task from other worker's deque
 * case 4. If all the above fail, idle
 */
static void *
worker_thread(void * no_arg)
{
    struct thread_pool *pool = currentWorker->pool;
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
    }
    
    return NULL;
}


/**
 * Set up n worker thread inside the pool.
 */
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
