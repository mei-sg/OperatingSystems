Intro to Operating Systems Assignment - Customer Airline Check-In Simulation

This project implements a multithreaded airline check-in system (ACS) in C using the POSIX pthread library. The program simulates customers arriving at an airport check-in area and being served by clerks according to queue priority rules. 

Customers are represented as threads with predefined arrival times, service times, and ticket class (business or economy). When a customer arrives, they enter the appropriate queue and wait to be served by one of five clerk threads. Business-class customers are given priority over economy-class customers, while each queue maintains FIFO ordering. 

The simulation uses mutexes and condition variables to coordinate thread synchronization, manage shared queues safely, and ensure that clerks serve customers according to the system’s scheduling rules. At the end of the simulation, the program reports statistics such as the average waiting time for all customers, business-class customers, and economy-class customers. 

This project demonstrates concepts from operating systems including thread creation, synchronization, mutual exclusion, and concurrent scheduling.



The program reads input from the file customers.txt in the p2 directory.
Run the following commands inside the p2 directory to compile and run the executable:

    make
    ./ACS


