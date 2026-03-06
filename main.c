#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include<pthread.h> //for multi-thread related system call
#include<sys/types.h> //thread id type “pthread 

// Global variables
#define EMPTY -1
#define BUSINESS 0
#define ECONOMY 1
#define NQUEUE 2
#define NCLERKS 5
#define MAX_CUSTOMERS 100
#define TRUE 1
#define FALSE 0

// Struct to record customer information read from customers.txt
struct customerInfo {
    int userID;
	int classType;
	int serviceTime;
	int arrivalTime;
};

// Customer list from file parsing
struct customerInfo *customerDetails; 
int totalCustomers;

// Queue variables
int queue[NQUEUE][MAX_CUSTOMERS]; // Make two queues
int front[NQUEUE]; // Keep track of queue front
int back[NQUEUE]; // Keep track of queue back
int queueLength[NQUEUE]; // Queue length for BUSINESS and ECONOMY
int queueStatus[NQUEUE]; // Variable to record the status of a queue, the value could be idle (not using by any clerk) or the clerk id (1 ~ 4), indicating that the corresponding clerk is now signaling this queue.
int winnerSelected[NQUEUE] = {FALSE}; // Variable to record if the first customer in a queue has been successfully selected and left the queue.


// Time variables 
struct timeval initTime; // Use this variable to record the simulation start time; No need to use mutex_lock when reading this variable since the value would not be changed by thread once the initial time was set.
double overallWaitTime; //A global variable to add up the overall waiting time for all customers, every customer add their own waiting time to this variable, mutex_lock is necessary.


// thread variables
pthread_mutex_t queueMutex[NQUEUE];
pthread_cond_t queueCond[NQUEUE];

pthread_t clerkThreads[NCLERKS];
pthread_mutex_t clerkMutex[NCLERKS];
pthread_cond_t clerkCond[NCLERKS];

pthread_mutex_t overallWaitMutex;







int main() {

	// Initialize the queues
	for (int i = 0; i < NQUEUE; i++) {
    	front[i] = 0;
    	back[i] = 0;
    	queueLength[i] = 0;
    	winnerSelected[i] = FALSE;
    	queueStatus[i] = 0;
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

	// Declare variables for file parsing (array of cutomer details)
	int numCustomers;
	
	struct customerInfo *customerDetails;
	
	// Read customer information from a file and store the info in an array
	customerDetails = readFile("customers.txt", &numCustomers);

	pthread_t customerThread[MAX_CUSTOMERS];
    int clerkIndex[NCLERKS];
    int customerIndex[MAX_CUSTOMERS];
	
	/*
	printf("===================== DEBUG =====================\n");
	printf("Parsed Customers START:\n");

	for(int i = 0; i < numCustomers; i++) {
    printf("Customer %d | class %d | arrival %d | service %d\n",
        customerDetails[i].userID,
        customerDetails[i].classType,
        customerDetails[i].arrivalTime,
        customerDetails[i].serviceTime);
	}
	printf("Parsed Customers DONE!\n");
	*/ 

	// Free memory
    // free(customerDetails);

	// Create clerks threads
    for (int i = 0; i < NCLERKS; i++) {
        clerkIndex[i] = i;
        pthread_create(&clerkThreads[i], NULL, clerkEntry, &clerkIndex[i]);
    }

    // Create customer threads
    for (int i = 0; i < totalCustomers; i++) {
        customerIndex[i] = i;
        pthread_create(&customerThread[i], NULL, customerEntry, &customerIndex[i]);
    }

    // Wait for customers
    for (int i = 0; i < totalCustomers; i++) {
        pthread_join(customerThread[i], NULL);
    }

	


	

	// destroy mutex & condition variable (optional)
	
	// calculate the average waiting time of all customers
	

	return 0;
}

/* ==================================== HELPER FUNCTIONS ==================================== */


// Read customer information from a file
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

// Enqueue the customer's index (from customerDetails array) to the BUSINESS/ECONOMY queue
void enqueue(int queueType, int customerIndex) {

    // Add customer index to queue and increment the length
    queue[queueType][back[queueType]] = customerIndex;
    back[queueType] = (back[queueType] + 1) % MAX_CUSTOMERS;
    queueLength[queueType]++;
}

// Dequeue the customer's index (from customerDetails array) from the BUSINESS/ECONOMY queue
int dequeue(int queueType) {

	int customerIndex = queue[queueType][front[queueType]];

    // Remove customer index from queue and decrement the length
    front[queueType] = (front[queueType] + 1) % MAX_CUSTOMERS;
    queueLength[queueType]--;

	// Return index of dequeued customer
	return cutomerIndex;
}

// Clerk chooses which queue to serve next
int serveQueue() {

	if (queueLength[BUSINESS] > 0) {
		return BUSINESS;
	}

	if (queueLength[ECONOMY] > 0) {
		return ECONOMY;
	}

	// Both queues are empty
	return EMPTY;
}


// ============================ TO IMPLEMENT ===========================

// Get the current simulation time
double getCurrentTime() {

	// returns the current simulation time in seconds 	
	return 0.0;
}

// Add customer's wait time to overallWaitTime
void updateWaitTime(double overallWaitTime) {

	// updates the time and returns nothing
}






// Enter customer threads and print messages
void* customerEntry(void *customerArg) {

	// Cast customerID to int type from void
	int customerID = *(int *)customerArg;

	// Sleep
	// Call enqueue()
	// Wait
	// Receive service from clerk
	// Record wait time
	//
}






// Enter clerk threads and print messages
void *clerkEntry(void * clerkArg) {

	// Cast clerkID to int type from void
	int clerkID = *(int *)clerkArg;

	while(TRUE) {
		
		// Call serveQueue() to determine which queue to service next
		int serveNow = serveQueue();

		// Wait if both queues are empty
		if (serveNow == -1) {
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

	pthread_exit(NULL);
}


