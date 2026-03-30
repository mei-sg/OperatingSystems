#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fat12.h"

// Used to count the number of files in a directory and its subdirectories 
int main(int argc, char *argv[]) {
    // Create a file pointer to read the disk image
    FILE *fp;

    // Check if the user provides a disk image and a filename as arguments
    if (argc != 3) {
        printf("Usage: %s disk.IMA FILENAME.EXT\n", argv[0]);
        return 1;
    }

    // Open the disk image file in binary mode
    fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        printf("Error occurred while opening file\n");
        return 1;
    }

    // Create an array to hold the boot sector information and a pointer for the FAT
    unsigned char boot_sector[512];
    unsigned char *fat = NULL;

    // Load FAT12 metadata and FAT into memory
    if (load_fat12_metadata(fp, boot_sector, &fat) != 0) {
        fclose(fp);
        return 1;
    }

    // Read the FAT12 metadata from the boot sector to calculate the location of the root directory and data region
    int bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
    int sectors_per_fat = boot_sector[22] + (boot_sector[23] << 8);
    int num_fats = boot_sector[16];
    int root_dir_entries = boot_sector[17] + (boot_sector[18] << 8);
    int sectors_per_cluster = boot_sector[13];

    // Calculate the offset of the root directory and the first data sector
    int root_dir_offset = (reserved_sectors + (num_fats * sectors_per_fat)) * bytes_per_sector;
    int root_dir_sectors = (root_dir_entries * 32 + bytes_per_sector - 1) / bytes_per_sector;
    int first_data_sector = reserved_sectors + (num_fats * sectors_per_fat) + root_dir_sectors;

    // Raise error if seeking to the root directory fails
    if (fseek(fp, root_dir_offset, SEEK_SET) != 0) {
        printf("Error seeking to root directory\n");
        free(fat);
        fclose(fp);
        return 1;
    }

    // Initialize the number of files to 0 and an array for directory entry's 32 bytes
    unsigned char entry[32];
    int found = 0;
    int file_size = 0;
    int first_cluster = 0;

    // Loop through root directory entries to find the target file and get its size and starting cluster
    char target_name[13];
    strncpy(target_name, argv[2], 12);
    target_name[12] = '\0';

    // Loop through root directory entries to find the target file and get its size and starting cluster
    for (int i = 0; i < root_dir_entries; i++) {
        // Raise error if reading the entry fails
        if (fread(entry, 1, 32, fp) != 32) {
            break;
        }

        // Skip empty and deleted entries
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        unsigned char attr = entry[11];

        // Skip long filename entries, volume labels, and directories
        if (attr == 0x0F) continue;
        if (attr & 0x08) continue;
        if (attr & 0x10) continue;

        // Get the formatted filename for comparison
        char filename[20];
        get_formatted_filename(entry, filename);

        // If the formatted filename matches the target filename, then set found to 1 and get the file size and starting cluster of the file
        if (strcmp(filename, target_name) == 0) {
            found = 1;
            file_size = entry[28] |
                        (entry[29] << 8) |
                        (entry[30] << 16) |
                        (entry[31] << 24);
            first_cluster = entry[26] + (entry[27] << 8);
            break;
        }
    }

    // If the file is not found then exit the program and print an error message
    if (!found) {
        printf("File not found.\n");
        free(fat);
        fclose(fp);
        return 1;
    }

    // Create an output file to write the contents of the target file
    FILE *out = fopen(argv[2], "wb");
    if (out == NULL) {
        printf("Error creating output file\n");
        free(fat);
        fclose(fp);
        return 1;
    }

    // Loop through the cluster chain of the target file to read its contents and write to the output file
    int cluster = first_cluster;
    int remaining = file_size;
    int cluster_size = sectors_per_cluster * bytes_per_sector;

    // Loop through the cluster chain of the target file to read its contents and write to the output file
    while (cluster < 0xFF8 && remaining > 0) {
        // Calculate the offset of the current cluster
        int sector = first_data_sector + (cluster - 2) * sectors_per_cluster;
        int offset = sector * bytes_per_sector;

        // Raise error if seeking to the current cluster fails
        if (fseek(fp, offset, SEEK_SET) != 0) {
            printf("Error seeking to file data\n");
            fclose(out);
            free(fat);
            fclose(fp);
            return 1;
        }

        // Read the data of the current cluster and write to the output file
        int bytes_to_write = (remaining < cluster_size) ? remaining : cluster_size;
        unsigned char *buffer = malloc(cluster_size);
        if (buffer == NULL) {
            printf("Memory allocation failed\n");
            fclose(out);
            free(fat);
            fclose(fp);
            return 1;
        }

        // Raise error if reading the file data fails or if writing to the output file fails
        if (fread(buffer, 1, cluster_size, fp) != (size_t)cluster_size) {
            free(buffer);
            printf("Error reading file data\n");
            fclose(out);
            free(fat);
            fclose(fp);
            return 1;
        }

        // Raise error if writing to the output file fails
        if (fwrite(buffer, 1, bytes_to_write, out) != (size_t)bytes_to_write) {
            free(buffer);
            printf("Error writing output file\n");
            fclose(out);
            free(fat);
            fclose(fp);
            return 1;
        }

        // Free the buffer and update remaining bytes and move to the next cluster in the chain
        free(buffer);
        remaining -= bytes_to_write;
        cluster = get_fat_entry(fat, cluster);
    }

    // Close the output file, free the FAT memory, and close the disk image file
    fclose(out);
    free(fat);
    fclose(fp);
    return 0;
}