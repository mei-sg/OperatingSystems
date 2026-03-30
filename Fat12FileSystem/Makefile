CC = gcc
CFLAGS = -Wall -Wextra -std=c11

# Build All Executables
all: diskinfo disklist diskget diskput

# Executables
diskinfo: diskinfo.o fat12.o
	$(CC) $(CFLAGS) diskinfo.o fat12.o -o diskinfo

disklist: disklist.o fat12.o
	$(CC) $(CFLAGS) disklist.o fat12.o -o disklist

diskget: diskget.o fat12.o
	$(CC) $(CFLAGS) diskget.o fat12.o -o diskget

diskput: diskput.o fat12.o
	$(CC) $(CFLAGS) diskput.o fat12.o -o diskput

# Object Files 
diskinfo.o: diskinfo.c fat12.h
	$(CC) $(CFLAGS) -c diskinfo.c

disklist.o: disklist.c fat12.h
	$(CC) $(CFLAGS) -c disklist.c

diskget.o: diskget.c fat12.h
	$(CC) $(CFLAGS) -c diskget.c

diskput.o: diskput.c fat12.h
	$(CC) $(CFLAGS) -c diskput.c

fat12.o: fat12.c fat12.h
	$(CC) $(CFLAGS) -c fat12.c

# Clean 
clean:
	rm -f diskinfo disklist diskget diskput *.o

.PHONY: all clean