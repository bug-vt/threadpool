/**
 * threadpool.c
 *
 * Written by: Bug Lee, Dana altarace
 * Last modified : 10/20/21
 */

struct thread_pool {
};


struct future {
    fork_join_task_t task;
    void* args;
    void* result;
    // TO DO:
    // Synchronization primitives
};


struct thread_pool * 
thread_pool_new(int nthreads)
{
    return NULL;
}


void 
thread_pool_shutdown_and_destroy(struct thread_pool * pool)
{
}


struct future *
thread_pool_submit(struct thread_pool *pool, fork_join_task_t task, void *data)
{
    return NULL;
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
