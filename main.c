/* ============================ INCLUDE HEADER FILES ============================ */
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/types.h>
#include "queue.h"
#include <unistd.h>

/* ============================ CONSTANTS ============================ */
#define TRUE 1
#define FALSE 0

#define EMPTY -1
#define BUSINESS 0
#define ECONOMY 1
#define NQUEUE 2
#define NCLERKS 5
#define MAX_CUSTOMERS 100

/* ============================ DATA STRUCTURES ============================ */
struct customerInfo {
    int userID;
	int classType;
	int serviceTime;
	int arrivalTime;
};

/* ============================ CUSTOMER DATA ============================ */
// Customer list from file parsing
struct customerInfo *customerDetails; 
int totalCustomers;
int customersServed = 0;

/* ============================ QUEUE STATE ============================ */
// Which clerk is currently serving the queue (there could be no clerk serving the queue)
int queueStatus[NQUEUE];

// Record if the first customer in a queue has been selected and left the queue
int winnerSelected[NQUEUE] = {FALSE};

/* ============================ SIMULATION TIME ============================ */
struct timeval initTime; // Record the simulation start time
double overallWaitTime; //Overall waiting time for all customers

/* ============================ THREAD SYNCHRONIZATION ============================ */
// Mutexes and condition variables for BUSINESS and ECONOMY queues
pthread_mutex_t queueMutex[NQUEUE];
pthread_cond_t queueCond[NQUEUE];

// Mutexes and condition variables for clerks
pthread_t clerkThread[NCLERKS];
pthread_mutex_t clerkMutex[NCLERKS];
pthread_cond_t clerkCond[NCLERKS];

// Mutex for overall wait time
pthread_mutex_t overallWaitMutex;

// Mutex for number of customers served
pthread_mutex_t customersServedMutex;

/* ============================ FUNCTION PROTOTYPES ============================ */
struct customerInfo* readFile(char *filename, int *numCustomers);
void enqueue(int queueType, int customerIndex);
int dequeue(int queueType);
int serveQueue(void);
double getCurrentTime(void);
void updateWaitTime(double waitTime);
void* customerEntry(void *customerArg);
void* clerkEntry(void *clerkArg);


/* +++++++++++++++++++++++ MAIN FUNCTION +++++++++++++++++++++++ */

int main() {

/* ============================ INITIALIZATIONS ============================ */
	// Initialize the queues
	initQueues();
	for (int i = 0; i < NQUEUE; i++) {
    	queueStatus[i] = EMPTY;
	}

	// Initialize queue mutexes
	for(int i = 0; i < NQUEUE; i++) {
    	pthread_mutex_init(&queueMutex[i], NULL);
    	pthread_cond_init(&queueCond[i], NULL);
	}

	// Initialize clerk mutexes and condition variables
	for(int i = 0; i < NCLERKS; i++) {
    	pthread_mutex_init(&clerkMutex[i], NULL);
    	pthread_cond_init(&clerkCond[i], NULL);
	}

	// Initialize wait time mutex
	pthread_mutex_init(&overallWaitMutex, NULL);
	
	// Initialize customers served mutex
	pthread_mutex_init(&customersServedMutex, NULL);
	
	
	// Read customer information from a file and store the info in an array
	customerDetails = readFile("customers.txt", &totalCustomers);
	
	// Record the start time of the simulation
	gettimeofday(&initTime, NULL);
	
/* ============================ THREAD CREATION ============================ */
	pthread_t customerThread[MAX_CUSTOMERS];
    int clerkIndex[NCLERKS];
    int customerIndex[MAX_CUSTOMERS];

	// Clerk threads    
	for (int i = 0; i < NCLERKS; i++) {
        clerkIndex[i] = i;
        pthread_create(&clerkThread[i], NULL, clerkEntry, &clerkIndex[i]);
    }

    // Customer threads
    for (int i = 0; i < totalCustomers; i++) {
        customerIndex[i] = i;
        pthread_create(&customerThread[i], NULL, customerEntry, &customerIndex[i]);
    }

    // Wait for customers
    for (int i = 0; i < totalCustomers; i++) {
        pthread_join(customerThread[i], NULL);
    }

	// Wait for clerks
	for (int i = 0; i < NCLERKS; i++) {
    	pthread_join(clerkThread[i], NULL);
	}


	

	// destroy mutex & condition variable (optional)
	
	// calculate the average waiting time of all customers
	
	// Free memory
    free(customerDetails);

	return 0;
}

/* ==================================== HELPER FUNCTIONS ==================================== */


/* Read customer information from a file */
struct customerInfo* readFile(char *filename, int *numCustomers){

	// Open file and exit if empty
    FILE *fp = fopen(filename, "r");
    if(fp == NULL) {
        perror("Error opening customer information file");
        exit(1);
    }

	// Find number of cutomers and allocate memory
    fscanf(fp, "%d", numCustomers);
    struct customerInfo *customerList = malloc(sizeof(struct customerInfo) * (*numCustomers));
	
	// Account for allocation failure
    if(customerList == NULL) {
        perror("malloc failed");
        exit(1);
    }

	// Parse cutomer information into an arrray of customerInfo structs
    for(int i = 0; i < *numCustomers; i++) {
       fscanf(fp,"%d:%d,%d,%d",
            &customerList[i].userID,
            &customerList[i].classType,
            &customerList[i].arrivalTime,
            &customerList[i].serviceTime
        );
    }
	// Close file and return list
    fclose(fp);
    return customerList;
}


/* Get the current simulation time */
double getCurrentTime() {

	// Get the current time 
	struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

	// Calculate time passed since the start of simulation
    double seconds = (currentTime.tv_sec - initTime.tv_sec);
    double microseconds = (currentTime.tv_usec - initTime.tv_usec) / 1000000.0;
    return seconds + microseconds;
}


/* Add customer's wait time to overallWaitTime */
void updateWaitTime(double overallWaitTime) {

	// Lock mutex to update overall wait time
    pthread_mutex_lock(&overallWaitMutex);

	// Update overall wait time
    overallWaitTime += waitTime;

	// Unlock mutex after updating overall wait time
    pthread_mutex_unlock(&overallWaitMutex);
}


/* ==================================== CUSTOMER FUNCTION ==================================== */

/* Enter customer threads and print messages */
void* customerEntry(void *customerArg) {

	// Cast input to int from type void
	// this variable is the index of the current customer from the customerDetails list (from readfile())
	int customerDetailsIndex = *(int *)customerArg;

	// Sleep until arrival time
	usleep(customerDetails[customerDetailsIndex].arrivalTime * 100000);

	// Customer arrival
	int customerID = customerDetails[customerDetailsIndex].userID;
	int queueType = customerDetails[customerDetailsIndex].classType;
	int clerkID;

	printf("CUSTOMER ARRIVES: Unique customer ID is %d. \n", customerID);

	// Enter the queue
	pthread_mutex_lock(&queueMutex[queueType]);
	enqueue(queueType, customerDetailsIndex);
	printf("CUSTOMER ENTERS QUEUE: Successfully entered the %s queue and length is now %d customers. \n",
			queueType == BUSINESS ? "BUSINESS" : "ECONOMY",
			queueLength[queueType]);

	// Wait for clerk signal until customer is selected
	while (1) {
		pthread_cond_wait(&queueCond[queueType], &queueMutex[queueType]);

		if (queue[queueType][front[queueType]] == customerDetailsIndex && winnerSelected[queueType] == FALSE) {
			dequeue(queueType);
			winnerSelected[queueType] = TRUE;
			clerkID = queueStatus[queueType];
			break;
		}
	}

	pthread_mutex_unlock(&queueMutex[queueType]);

	double startTime = getCurrentTime();
	double arrivalTime = customerDetails[customerDetailsIndex].arrivalTime / 10.0;
	double waitTime = startTime - arrivalTime;
	updateWaitTime(waitTime);

	// Clerk starts serving customer
	printf("CLERK STARTS SERVING CUSTOMER: Clerk number %d is serving customer %d. | Start time: %2f. \n",
			clerkID, customerID, getCurrentTime());

	// Simulate time it takes to service a customer
    usleep(customerDetails[customerDetailsIndex].serviceTime * 100000);

    // Clerk finishes serving customer
    printf("CLERK FINISHES SERVING CUSTOMER: Clerk %d finishes serving customer %d. | End time: %.2f. \n",
            clerkID, customerID, getCurrentTime());

	// Increment number of customers served
	pthread_mutex_lock(&customersServedMutex);
	customersServed++;
	pthread_mutex_unlock(&customersServedMutex);


    // Notify the clerk that service is finished
    pthread_mutex_lock(&clerkMutex[clerkID]);
    pthread_cond_signal(&clerkCond[clerkID]);
    pthread_mutex_unlock(&clerkMutex[clerkID]);

    pthread_exit(NULL);
}


/* ==================================== CLERK FUNCTION ==================================== */

/* Enter clerk threads */
void *clerkEntry(void * clerkArg) {

	// Cast clerkID to int type from void
	int clerkID = *(int *)clerkArg;

	while(TRUE) {
		
		// Lock customer count and check if all customers have been served
		pthread_mutex_lock(&customersServedMutex);
		if (customersServed >= totalCustomers) {
			// Break loop and end thread
			pthread_mutex_unlock(&customersServedMutex);
			break;
		}
		// Unlock customer count
		pthread_mutex_unlock(&customersServedMutex);

		// Call serveQueue() to determine which queue to service next
		int serveNow = serveQueue();

		// Wait if both queues are empty
		if (serveNow == EMPTY) {
			usleep(1000);
			continue;
		}
		// lock the queue
		pthread_mutex_lock(&queueMutex[serveNow]);

		// Record the status of a queue and indicate that clerk X is now signaling the queue
		queueStatus[serveNow] = clerkID;

		// Reset winner flag so a new customer can be served
		winnerSelected[serveNow] = FALSE; 

		// Awake waiting customers from the queue
		pthread_cond_broadcast(&queueCond[serveNow]); 
		
		// Unlock the queue so other customers can enter
		pthread_mutex_unlock(&queueMutex[serveNow]);

		// Wait for clerk to finish with customer
        pthread_mutex_lock(&clerkMutex[clerkID]);
        pthread_cond_wait(&clerkCond[clerkID], &clerkMutex[clerkID]);

		// Unlock clerk
        pthread_mutex_unlock(&clerkMutex[clerkID]);
	}

	// Exit if all customers have been served
	pthread_exit(NULL);
}


