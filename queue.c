#include "queue.h"

int queue[NQUEUE][MAX_CUSTOMERS];
int front[NQUEUE];
int back[NQUEUE];
int queueLength[NQUEUE];

void initQueues(void) {
    for (int i = 0; i < NQUEUE; i++) {
        front[i] = 0;
        back[i] = 0;
        queueLength[i] = 0;
    }
}

void enqueue(int queueType, int customerIndex) {
    queue[queueType][back[queueType]] = customerIndex;
    back[queueType] = (back[queueType] + 1) % MAX_CUSTOMERS;
    queueLength[queueType]++;
}

int dequeue(int queueType) {
    int customerIndex = queue[queueType][front[queueType]];
    front[queueType] = (front[queueType] + 1) % MAX_CUSTOMERS;
    queueLength[queueType]--;
    return customerIndex;
}

int serveQueue(void) {
    if (queueLength[BUSINESS] > 0) return BUSINESS;
    if (queueLength[ECONOMY] > 0) return ECONOMY;
    return EMPTY;
}