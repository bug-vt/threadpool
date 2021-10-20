/**
 * wrapper.h 
 * Author : Bug Lee (bug19)
 * Last modified: 10/20/2021
 *
 * This module contians wrapper for semaphore and ptherad functions.
 * This technique was adapted from coruse textbook.
 */


#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdio.h> // perror
#include <stdlib.h> // exit

void Pthread_mutex_lock(pthread_mutex_t *mutex);
void Pthread_mutex_unlock(pthread_mutex_t *mutex);
void Pthread_cond_signal(pthread_cond_t *cond);
void Pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
void Sem_init(sem_t *sem, int pshared, unsigned int value);
void Sem_wait(sem_t *s);
void Sem_post(sem_t *s);

