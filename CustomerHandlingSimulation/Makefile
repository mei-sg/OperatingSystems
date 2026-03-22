CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread

ACS: main.o queue.o
	$(CC) $(CFLAGS) main.o queue.o -o ACS

main.o: main.c queue.h
	$(CC) $(CFLAGS) -c main.c

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) -c queue.c

clean:
	rm -f ACS *.o

# Phony targets (not real files)
.PHONY: all clean
