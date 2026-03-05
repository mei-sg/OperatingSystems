# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11

# Target executable
TARGET = ACS

# Source files
SRC = main.c

# Default rule: build the program
all: $(TARGET)

# Rule to create the executable from source files
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# Clean up generated files
clean:
	rm -f $(TARGET) *.o

# Phony targets (not real files)
.PHONY: all clean