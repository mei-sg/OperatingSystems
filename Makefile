# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11

# Target executable
TARGET = ACS

# Source files (add more .c files here if needed)
SRC = main.c

# Default rule
all: $(TARGET)

# Build ACS from source files
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# Clean up generated files
clean:
	rm -f $(TARGET)
