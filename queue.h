#ifndef QUEUE
#define QUEUE

#include <pthread.h>

struct Queue_node {
    char* path;
    struct Queue_node* next;
};

struct Queue* Queue_create(void);
void Queue_free(struct Queue*);

struct Queue {
    struct Queue_node* front;
    struct Queue_node* rear;
    pthread_mutex_t mutex;
};

int dequeue(struct Queue*, char**);
int enqueue(struct Queue*, char*);

#endif
