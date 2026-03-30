#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fat12.h"

// Main function to read a disk image and print its information
int main(int argc, char *argv[]) {
    // Create a file pointer to read the disk image
    FILE *fp;

    // Check if the user provides a disk image as an argument
    if (argc != 2) {
        printf("Usage: %s disk.IMA\n", argv[0]);
        return 1;
    }

    // Open the disk image file in binary mode
    fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        // Raise error if the file cannot be opened
        printf("Error occurred while opening file\n");
        return 1;
    }

    // Print the disk information by calling the print_disk_info function
    print_disk_info(fp);

    // Close the file pointer and return 0 to end the program
    fclose(fp);
    return 0;
}