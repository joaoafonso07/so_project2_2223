#include<stdio.h>
#include"producer-consumer.h"
#include<pthread.h>
#include<stdlib.h>

/*returns 0 if sucessful, -1 otherwise*/
int pcq_create(pc_queue_t *queue, size_t capacity){
    queue = (pc_queue_t*)realloc(queue, sizeof(void*)*capacity);
    if(queue == NULL)
        return -1;
    return 0;
}

int pcq_destroy(pc_queue_t *queue){
    (void) queue;
    return 0;
}