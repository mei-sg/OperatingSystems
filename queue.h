#ifndef QUEUE_H
#define QUEUE_H

#define EMPTY -1
#define BUSINESS 0
#define ECONOMY 1
#define NQUEUE 2
#define MAX_CUSTOMERS 100

extern int queue[NQUEUE][MAX_CUSTOMERS];
extern int front[NQUEUE];
extern int back[NQUEUE];
extern int queueLength[NQUEUE];

void initQueues(void);
void enqueue(int queueType, int customerIndex);
int dequeue(int queueType);
int serveQueue(void);

#endif