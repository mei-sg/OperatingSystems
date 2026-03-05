#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

// The time calculation could be confusing, check the exmaple of gettimeofday on tutorial for more detail.

 /// Struct to record customer information read from customers.txt
struct customerInfo {
    int userID;
	int classType;
	int serviceTime;
	int arrivalTime;
};

// Global variables
 
//struct timeval initTime; // use this variable to record the simulation start time; No need to use mutex_lock when reading this variable since the value would not be changed by thread once the initial time was set.
//double overallWaitTime; //A global variable to add up the overall waiting time for all customers, every customer add their own waiting time to this variable, mutex_lock is necessary.
//int qLength[NQUEUE];// variable stores the real-time queue length information; mutex_lock needed

//int qStatus[NQUEUE]; // variable to record the status of a queue, the value could be idle (not using by any clerk) or the clerk id (1 ~ 4), indicating that the corresponding clerk is now signaling this queue.
//int selected[NQUEUE] = FALSE; // variable to record if the first customer in a queue has been successfully selected and left the queue.

/* Other global variable may include: 
 1. condition_variables (and the corresponding mutex_lock) to represent each queue; 
 2. condition_variables to represent clerks
 3. others.. depend on your design
 */

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



int main() {

	// initialize all the condition variable and thread lock will be used
	
	// Declare variables for file parsing 
	int numCustomers;
	struct customerInfo *customerDetails;
	
	// Read customer information from a file and store the info in an array
	customerDetails = readFile("customers.txt", &numCustomers);

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

	// Free memory
    free(customerDetails);

    return 0;



/*
	//create clerk thread (optional)
	for(i = 0, i < NClerks; i++){ // number of clerks
		pthread_create(&clerkId[i], NULL, clerk_entry, (void *)&clerk_info[i]); // clerk_info: passing the clerk information (e.g., clerk ID) to clerk thread
	}
	
	//create customer thread
	for(i = 0, i < NCustomers; i++){ // number of customers
		pthread_create(&customId[i], NULL, customer_entry, (void *)&custom_info[i]); //custom_info: passing the customer information (e.g., customer ID, arrival time, service time, etc.) to customer thread
	}
	// wait for all customer threads to terminate
	forEach customer thread{
		pthread_join(...);
	}
	// destroy mutex & condition variable (optional)
	
	// calculate the average waiting time of all customers
	return 0;
	*/
}
