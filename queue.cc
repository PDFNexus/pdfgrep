#include <malloc.h>

#include "queue.h"

struct Queue* Queue_create()
{
    struct Queue* queue = (struct Queue*)malloc(sizeof(struct Queue));
    if(queue == NULL)
        return NULL;
    queue->front = NULL;
    queue->rear = NULL;
    if (pthread_mutex_init(&(queue->mutex), NULL) != 0)
        return NULL;

    return queue;
}

void Queue_free(struct Queue* queue)
{
    pthread_mutex_destroy(&(queue->mutex));
    free( queue );
}

static struct Queue_node* Queue_node_create(char* path)
{
    struct Queue_node* new_node = (struct Queue_node*)malloc(sizeof(struct Queue_node));
    if (new_node == NULL)
        return 0;
    new_node->path = path;
    new_node->next = NULL;

    return new_node;
}

static void Queue_node_free(struct Queue_node* node)
{
    free( node );
}

int enqueue(struct Queue* queue, char* path)
{
    struct Queue_node* new_node = Queue_node_create(path);
    if (new_node == NULL )
        return 0;

    if (pthread_mutex_lock(&(queue->mutex)) != 0)
        perror("lock failed");

    if (queue->front == NULL)
        queue->front = new_node;
    else
        queue->rear->next = new_node;

    queue->rear = new_node;

    pthread_mutex_unlock(&(queue->mutex));
    return 1;
}

int dequeue(struct Queue* queue, char** path)
{
    if (queue->front == NULL)
        return 0;

    int error;
    pthread_mutex_lock(&(queue->mutex));

    struct Queue_node* tmp = queue->front;
    queue->front = queue->front->next;

    *path = tmp->path;

    Queue_node_free(tmp);

    pthread_mutex_unlock(&(queue->mutex));

    return 1;
}

