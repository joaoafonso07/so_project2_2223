#include<stdio.h>
#include"producer-consumer.h"
#include<pthread.h>
#include<stdlib.h>
#include"logging.h"

/*returns 0 if sucessful, -1 otherwise*/
int pcq_create(pc_queue_t *queue, size_t capacity){
    queue->pcq_buffer = malloc(capacity*sizeof(void*));
    if(queue->pcq_buffer == NULL)
        PANIC("failed to aloc pcq_buffer");
    queue->pcq_capacity = capacity;

    if(pthread_mutex_init(&(queue->pcq_current_size_lock), NULL))
        PANIC("failed to init the pcq_current_size_lock");
    if(pthread_mutex_init(&(queue->pcq_head_lock), NULL))
        PANIC("failed to init the pcq_head_lock");
    if(pthread_mutex_init(&(queue->pcq_tail_lock), NULL))
        PANIC("failed to init the pcq_tail_lock");
    if(pthread_mutex_init(&(queue->pcq_pusher_condvar_lock), NULL))
        PANIC("failed to init the pcq_pusher_condvar_lock");
    if(pthread_mutex_init(&(queue->pcq_popper_condvar_lock), NULL))
        PANIC("failed to init the pcq_popper_condvar_lock");

    queue->pcq_current_size = 0;
    queue->pcq_head = 0;
    queue->pcq_tail = 0;

    return 0;
}

int pcq_destroy(pc_queue_t *queue){
    free(queue->pcq_buffer);

    if(pthread_mutex_destroy(&(queue->pcq_current_size_lock)))
        PANIC("failed to destroy the pcq_current_size_lock");
    if(pthread_mutex_destroy(&(queue->pcq_head_lock)))
        PANIC("failed to destroy the pcq_head_lock");
    if(pthread_mutex_destroy(&(queue->pcq_tail_lock)))
        PANIC("failed to destroy the pcq_tail_lock");
    if(pthread_mutex_destroy(&(queue->pcq_pusher_condvar_lock)))
        PANIC("failed to destroy the pcq_pusher_condvar_lock");
    if(pthread_mutex_destroy(&(queue->pcq_popper_condvar_lock)))
        PANIC("failed to destroy the pcq_popper_condvar_lock");

    return 0;
}

int pcq_enqueue(pc_queue_t *queue, void *elem){
    if (pthread_mutex_lock(&(queue->pcq_current_size_lock)))
        PANIC("failed to lock pcq_current_size_lock");
    if (pthread_mutex_lock(&(queue->pcq_tail_lock)))
        PANIC("failed to lock pcq_tail_lock");
    queue->pcq_buffer[queue->pcq_tail] = elem;
    queue->pcq_tail = (queue->pcq_tail + 1) %(queue->pcq_capacity);
    queue->pcq_current_size ++;
    if (pthread_mutex_unlock(&(queue->pcq_current_size_lock)))
        PANIC("failed to unlock pcq_current_size_lock");
    if (pthread_mutex_unlock(&(queue->pcq_tail_lock)))
        PANIC("failed to unlock pcq_tail_lock");
    return 0;
}

void *pcq_dequeue(pc_queue_t *queue){
    if (pthread_mutex_lock(&(queue->pcq_current_size_lock)))
        PANIC("failed to lock pcq_current_size_lock");
    if (pthread_mutex_lock(&(queue->pcq_head_lock)))
        PANIC("failed to lock pcq_head_lock");
    void *elem = queue->pcq_buffer[queue->pcq_head];                       //doubt
    queue->pcq_head = (queue->pcq_head + 1) %(queue->pcq_capacity);
    queue->pcq_current_size --;
    return elem;
    if (pthread_mutex_unlock(&(queue->pcq_current_size_lock)))
        PANIC("failed to unlock pcq_current_size_lock");
    if (pthread_mutex_unlock(&(queue->pcq_head_lock)))
        PANIC("failed to unlock pcq_head_lock");
}

