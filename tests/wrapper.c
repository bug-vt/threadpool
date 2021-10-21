/**
 * wrapper.c 
 * Author : Bug Lee (bug19)
 * Last modified: 10/20/2021
 */

#include "wrapper.h"

// wrapper for ptheread create
void Pthread_create(pthread_t *restrict thread,
                    void *(*start_routine)(void *),
                    void *restrict arg)
{
    int err = pthread_create(thread, NULL, start_routine, arg);
    if (err != 0) {
        perror("Pthread_craete error");
        exit(err);
    }
}

// wrapper for mutex init
void Pthread_mutex_init(pthread_mutex_t *mutex)
{
    int err = pthread_mutex_init(mutex, NULL);
    if (err != 0)
    {
        perror("Pthread_mutex_init error");
        exit(err);
    }
}

// wrapper for mutex lock
void Pthread_mutex_lock(pthread_mutex_t *mutex)
{
    int err = pthread_mutex_lock(mutex);
    if (err != 0) {
        perror("Pthread_mutex_lock error");
        exit(err);
    }
}

// wrapper for mutex unlock
void Pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    int err = pthread_mutex_unlock(mutex);
    if (err != 0) {
        perror("Pthread_mutex_unlock error");
        exit(err);
    }
}

// wrapper for cond init
void Pthread_cond_init(pthread_cond_t *cond)
{
    int err = pthread_cond_init(cond, NULL);
    if (err != 0)
    {
        perror("Pthread_mutex_init error");
        exit(err);
    }
}

// wrapper for conditional variable signal
void Pthread_cond_signal(pthread_cond_t *cond)
{
    int err = pthread_cond_signal(cond);
    if (err != 0) {
        perror("Pthread_cond_signal error");
        exit(err);
    }
}

// wrapper for conditional variable wait
void Pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    int err = pthread_cond_wait(cond, mutex);
    if (err != 0) {
        perror("Pthread_cond_wait error");
        exit(err);
    }
}

// wrapper for semaphore initialization
void Sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if (sem_init(sem, 0, value) < 0) {
        perror("Sem_init error");
        exit(errno);
    }
}

// wrapper for semaphore wait
void Sem_wait(sem_t *s)
{
    if (sem_wait(s) < 0) {
        perror("Sem_wait error");
        exit(errno);
    }
}

// wrapper for semaphore post
void Sem_post(sem_t *s) 
{
    if (sem_post(s) < 0)
    {
        perror("Sem_post erro");
        exit(errno);
    }
}

