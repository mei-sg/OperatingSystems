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
#define ALL 3
#define NCLERKS 5
#define MAX_CUSTOMERS 100

/* ============================ DATA STRUCTURES ============================ */
// Struct to store customer information
struct customerInfo {
    int userID;
	int classType;
	int serviceTime;
	int arrivalTime;
};

/* ============================ CUSTOMER & CLERK DATA ============================ */
// Customer list from file parsing
struct customerInfo *customerDetails; 
int totalCustomers;
int customersServed;

// BUSINESS customer count
int totalBusinessCustomers;

// ECONOMY customer count
int totalEconomyCustomers;

// Flag to signal clerk is done
int clerkDone[NCLERKS];


/* ============================ QUEUE STATE ============================ */
// Which clerk is currently serving the queue (there could be no clerk serving the queue)
int queueStatus[NQUEUE];

// Record if the first customer in a queue has been selected and left the queue
int winnerSelected[NQUEUE] = {FALSE};

/* ============================ SIMULATION TIME ============================ */
// Record the simulation start time
struct timeval initTime;

// Overall waiting time for all customers
double overallWaitTime = 0.0; 

// Overall waiting time for BUSINESS customers
double businessWaitTime = 0.0; 

// Overall waiting time for ECONOMY customers
double economyWaitTime = 0.0; 


/* ============================ THREAD SYNCHRONIZATION ============================ */
// Mutexes and condition variables for BUSINESS and ECONOMY queues
pthread_mutex_t queueMutex[NQUEUE];
pthread_cond_t queueCond[NQUEUE];

// Mutexes and condition variables for clerks
pthread_t clerkThread[NCLERKS];
pthread_mutex_t clerkMutex[NCLERKS];
// Clerk condition variables used to signal when a clerk is done serving a customer
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
void calculateAverageTime(int classType, int totalClassCustomers, double classWaitTime);
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

	// Initialize clerk done flags
	for (int i = 0; i < NCLERKS; i++) {
    	clerkDone[i] = FALSE;
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


/* ============================ CLEANUP ============================ */
	// destroy queue mutex & condition variables
	for (int i = 0; i < NQUEUE; i++) {
    	pthread_mutex_destroy(&queueMutex[i]);
    	pthread_cond_destroy(&queueCond[i]);
	}

	// Destroy clerk mutexes and condition variables
	for (int i = 0; i < NCLERKS; i++) {
    	pthread_mutex_destroy(&clerkMutex[i]);
    	pthread_cond_destroy(&clerkCond[i]);
	}

	// Destroy other mutexes
	pthread_mutex_destroy(&overallWaitMutex);
	pthread_mutex_destroy(&customersServedMutex);

/* ============================ END MESSAGES ============================ */
	printf("\nAll customers have been served. Simulation ending.\n");
	printf("\n=======================================================================\n");
	printf("\nSummary of results:\n");

	// Calculate the average waiting time of all customers and print message
	calculateAverageTime(BUSINESS, totalBusinessCustomers, businessWaitTime);
	calculateAverageTime(ECONOMY, totalEconomyCustomers, economyWaitTime);
	calculateAverageTime(ALL, totalCustomers, overallWaitTime);

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
	int validCustomers = 0;

	// Parse cutomer information into an arrray of customerInfo structs
    for(int i = 0; i < *numCustomers; i++) {

		// Declare customer information variables to read from file
		int userID;
		int classType;
		int arrivalTime;
		int serviceTime;

		// Read customer information from file and store in variables
       	fscanf(fp,"%d:%d,%d,%d",
            &userID,
            &classType,
            &arrivalTime,
            &serviceTime
        );

		// Skip lines with negative arrival or service times and decrement total customer count
		if (arrivalTime < 0 || serviceTime < 0) {
			(*numCustomers)--;
			fprintf(stderr, "Invalid arrival or service time for customer %d. Times must be non-negative.\n", userID);
			continue;
		}
		// Store customer information in the customer list
		customerList[i].userID = userID;
		customerList[i].classType = classType;
		customerList[i].arrivalTime = arrivalTime;
		customerList[i].serviceTime = serviceTime;

		validCustomers++;
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
void updateWaitTime(double waitTime) {

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
	int currentLength = queueLength[queueType];
	pthread_mutex_unlock(&queueMutex[queueType]);
	
	// Record the time when customer enters the queue
	double queueEnterTime = getCurrentTime();

	printf("CUSTOMER ENTERS QUEUE: Successfully entered the %s queue and length is now %d customer(s). \n",
			queueType == BUSINESS ? "BUSINESS" : "ECONOMY",
			currentLength);
	
	// Lock the the queue 
	pthread_mutex_lock(&queueMutex[queueType]);
	
	/* Check the conditions for waiting:
		1) the queue is empty
		2) the customer is not at the front of the queue
		3) a winner has already been selected for the queue
		4) the queue status is empty (no clerk is currently serving the queue) */
	while (queueLength[queueType] == 0 ||
			queue[queueType][front[queueType]] != customerDetailsIndex ||
			winnerSelected[queueType] == TRUE ||
			queueStatus[queueType] == EMPTY
		) {
		//pthread_mutex_lock(&queueMutex[queueType]);
		pthread_cond_wait(&queueCond[queueType], &queueMutex[queueType]);
	}
	// Dequeue the customer and record the clerk that is serving the customer
	dequeue(queueType);
	winnerSelected[queueType] = TRUE;
	clerkID = queueStatus[queueType];
	queueStatus[queueType] = EMPTY;

	pthread_mutex_unlock(&queueMutex[queueType]);

	// Record the time when the customer starts service with a clerk
	double startTime = getCurrentTime();
	double waitTime = startTime - queueEnterTime;

	// Update overall wait time
	updateWaitTime(waitTime);

	// Update class wait times
	pthread_mutex_lock(&overallWaitMutex);

	if (queueType == BUSINESS) {
    	businessWaitTime += waitTime;
    	totalBusinessCustomers++;
	} else if (queueType == ECONOMY) {
    	economyWaitTime += waitTime;
    	totalEconomyCustomers++;
	}

	pthread_mutex_unlock(&overallWaitMutex);

	// Clerk starts serving customer
	printf("CLERK STARTS SERVING CUSTOMER: Clerk number %d is serving customer %d. | Start time: %.2f. \n",
			clerkID, customerID, getCurrentTime());

	// Simulate time it takes to service a customer
    usleep(customerDetails[customerDetailsIndex].serviceTime * 100000);

    // Clerk finishes serving customer
    printf("CLERK FINISHES SERVING CUSTOMER: Clerk %d finishes serving customer %d. | End time: %.2f. \n",
            clerkID, customerID, getCurrentTime());

	pthread_mutex_lock(&clerkMutex[clerkID]);
	clerkDone[clerkID] = TRUE;
	pthread_cond_signal(&clerkCond[clerkID]);
	pthread_mutex_unlock(&clerkMutex[clerkID]);

	// Increment number of customers served
	pthread_mutex_lock(&customersServedMutex);
	customersServed++;
	pthread_mutex_unlock(&customersServedMutex);

    //pthread_exit(NULL);
	return NULL;
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
	
		// Lock the queue
		pthread_mutex_lock(&queueMutex[serveNow]);

		if (queueLength[serveNow] == 0 || queueStatus[serveNow] != EMPTY) {
    		pthread_mutex_unlock(&queueMutex[serveNow]);
    		usleep(1000);
    		continue;
		}

		// Record the status of a queue and indicate that clerk X is now signaling the queue
		queueStatus[serveNow] = clerkID;

		// Reset winner flag so a new customer can be served
		winnerSelected[serveNow] = FALSE;

		// Lock the clerk
		pthread_mutex_lock(&clerkMutex[clerkID]);
		// Reset clerk done flag for the current clerk before waking customer
		clerkDone[clerkID] = FALSE;
		// Unlock the queue so other customers can enter
		pthread_mutex_unlock(&clerkMutex[clerkID]);

		
		// Awake waiting customers from the queue
		pthread_cond_broadcast(&queueCond[serveNow]);
		pthread_mutex_unlock(&queueMutex[serveNow]);

		// Lock the clerk and wait for the signal that the customer has been served
        pthread_mutex_lock(&clerkMutex[clerkID]);

		/* Check the conditions for waiting:
			1) the clerk is not done serving the customer */
		while (clerkDone[clerkID] == FALSE) {
			pthread_cond_wait(&clerkCond[clerkID], &clerkMutex[clerkID]);
		}

		// Unlock clerk
        pthread_mutex_unlock(&clerkMutex[clerkID]);
	}
	// Exit if all customers have been served
	pthread_exit(NULL);
}


/* ==================================== WAIT TIME CALCULATIONS ==================================== */

/* Calculate overall wait times for each class */
void calculateAverageTime(int classType, int totalClassCustomers, double classWaitTime) {

	// Assign class type integer to a string
	const char *className = "UNKNOWN";

	if (classType == BUSINESS) {
		className = "BUSINESS";
	} else if (classType == ECONOMY) {
		className = "ECONOMY";
	} else if (classType == ALL) {
		className = "OVERALL";
	}

	if (totalClassCustomers == 0) {

		// Rename class with integer 3 to ANY for readability
		if (classType == 3) {
		 className = "ANY";
		}
		printf("No customers in %s class. Cannot compute average wait time.\n", className);
		return;
	}

	// Compute average wait time
	double averageTime = 0.0;
	averageTime = classWaitTime / totalClassCustomers;

	printf("\n");
	printf("Total %s customers: %d.\n", className, totalClassCustomers);
	printf("Total %s wait time %.2f seconds.\n", className, classWaitTime);
	printf("Average %s wait time: %.2f seconds.\n", className, averageTime);
}