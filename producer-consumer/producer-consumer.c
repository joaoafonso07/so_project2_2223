#include<stdio.h>
#include"producer-consumer.h"
#include<pthread.h>

int pcq_create(pc_queue_t *queue, size_t capacity){
    (void) queue;
    (void) capacity;
    return 0;
}