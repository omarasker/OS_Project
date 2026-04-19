#ifndef PRIQUEUE_H
#define PRIQUEUE_H


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

struct pnode {
    struct Process* process;
    struct pnode* next;
};

struct PriQueue {
    struct pnode* front;
};




void initPriQueue(struct PriQueue* pq) {
    pq->front = NULL;
}

void enqueuePri(struct Process* process, struct PriQueue* pq) {
    struct pnode* new_node = (struct pnode*)malloc(sizeof(struct pnode));
    new_node->process = process;
    new_node->next = NULL;

    if (pq->front == NULL || process->priority < pq->front->process->priority) {
        new_node->next = pq->front;
        pq->front = new_node;
        return;
    }
    

    struct pnode* current = pq->front;
    

    while (current->next != NULL && 
           (current->next->process->priority < process->priority || 
            (current->next->process->priority == process->priority && 
             current->next->process->arrival_time <= process->arrival_time))) {
        current = current->next;
    }

    new_node->next = current->next;
    current->next = new_node;
}


struct Process* dequeuePri(struct PriQueue* pq) {
    if (pq->front == NULL) return NULL;
    struct pnode* temp = pq->front;
    struct Process* p = temp->process;
    pq->front = pq->front->next;
    free(temp);
    return p;
}

struct Process* peekPri(struct PriQueue* pq) {
    if (pq->front == NULL) return NULL;
    return pq->front->process;
}


int isEmptyPri(struct PriQueue* pq) {
    return pq->front == NULL;
}


int sizePri(struct PriQueue* pq) {
    int count = 0;
    struct pnode* temp = pq->front;
    while (temp != NULL) {
        count++;
        temp = temp->next;
    }
    return count;
}



void destroyPri(struct PriQueue* pq) {
    struct pnode* temp;
    while (pq->front != NULL) {
        temp = pq->front;
        pq->front = pq->front->next;
        free(temp->process);
        free(temp);
    }
}

void printPriQueue(struct PriQueue* pq) {
    struct pnode* temp = pq->front;
    while (temp != NULL) {
        printf("Process ID: %d, Arrival: %d, Run Time: %d, Priority: %d\n",
               temp->process->id,
               temp->process->arrival_time,
               temp->process->running_time,
               temp->process->priority);
        temp = temp->next;
    }
    printf("\n");
}

#endif