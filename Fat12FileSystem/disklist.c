#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fat12.h"

// Main to read a disk image and print information about all directories and subdirectories in the disk
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
        printf("Error occurred while opening file\n");
        return 1;
    }

    // Print all directory information
    print_all_directory_info(fp);

    // Close the file pointer and return 0
    fclose(fp);
    return 0;
}

// Print information about all directories and subdirectories in the disk
void print_all_directory_info(FILE *fp) {
    // Create an array to store the boot sector information
    unsigned char boot_sector[512];

   
    unsigned char *fat = NULL;

    if (load_fat12_metadata(fp, boot_sector, &fat) != 0) {
        return;
    }

    list_root_directory(fp, fat, boot_sector);

    free(fat);
}

// Helper that formats the printing 
void printing_format(char *name) {
    printf("%s\n", name);
    printf("==================\n");
}

// Find the root directory entries and print the information about all directories and subdirectories in the root directory
void list_root_directory(FILE *fp, unsigned char *fat, unsigned char *boot_sector) {
    int num_bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int num_reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
    int num_sectors_per_fat = boot_sector[22] + (boot_sector[23] << 8);
    int num_fats = boot_sector[16];
    int root_dir_entries = boot_sector[17] + (boot_sector[18] << 8);

    int root_dir_offset = (num_reserved_sectors + (num_fats * num_sectors_per_fat)) * num_bytes_per_sector;

    if (fseek(fp, root_dir_offset, SEEK_SET) != 0) {
        printf("Error seeking to root directory\n");
        return;
    }

    // Call printing format for the root directory printing
    printing_format("ROOT");

    // Create an array to hold entry information
    unsigned char entry[32];

    // Print file and subdirectory entries
    // Loop through root directory entries to print files and subdirectories
    for (int i = 0; i < root_dir_entries; i++) {
        if (fread(entry, 1, 32, fp) != 32) {
            break;
        }
        // Skip empty and deleted entries
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        // Assign entry[11] to a variable to check the attribute of the entry
        unsigned char attr = entry[11];

        // Skip long filename entries and volume label
        if ((attr == 0x0F)) continue;
        if (attr & 0x08) continue;
        
        // Ignore the reserved clusters for directory entries
        if (attr & 0x10) {
            int first_cluster = entry[26] + (entry[27] << 8);
            if (first_cluster <= 1) continue;
        }

        print_directory_entry(entry);
    }

    // Use recursion to get into subdirectories
    if (fseek(fp, root_dir_offset, SEEK_SET) != 0) {
        printf("Error seeking back to root directory\n");
        return;
    }

    // Loop through root directory entries again to find subdirectories and call list_subdirectory for each subdirectory
    for (int i = 0; i < root_dir_entries; i++) {
        if (fread(entry, 1, 32, fp) != 32) {
            break;
        }
        // Skip empty and deleted entries
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        // Assign entry[11] to a variable to check the attribute of the entry
        unsigned char attr = entry[11];

        // Skip long filename entries and volume label
        if ((attr == 0x0F)) continue;
        if (attr & 0x08) continue;

        // If the entry is a valid directory, then go through its contents
        if (attr & 0x10) {
            // skip . and ..
            if (entry[0] == '.') continue;   

            // If the entry is a subdirectory, then print its information and call list_subdirectory to print its subdirectories and
            int first_cluster = entry[26] + (entry[27] << 8);
            // Ignore the reserved clusters
            if (first_cluster <= 1) continue;

            // Get formatted directory name and recurse into it
            char dir_name[20];
            get_formatted_filename(entry, dir_name);
            list_subdirectory(fp, fat, boot_sector, first_cluster, dir_name);
        }
    }
}

// Find the subdirectory entries and print the information about all directories and subdirectories in the subdirectory
void list_subdirectory(FILE *fp, unsigned char *fat, unsigned char *boot_sector, int start_cluster, char *dir_name) {
    int bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
    int sectors_per_fat = boot_sector[22] + (boot_sector[23] << 8);
    int num_fats = boot_sector[16];
    int root_dir_entries = boot_sector[17] + (boot_sector[18] << 8);
    int sectors_per_cluster = boot_sector[13];

    int root_dir_sectors = (root_dir_entries * 32 + bytes_per_sector - 1) / bytes_per_sector;
    int first_data_sector = reserved_sectors + (num_fats * sectors_per_fat) + root_dir_sectors;

    // Call printing format for the subdirectory printing
    printing_format(dir_name);
    // Initialize the cluster variable to the starting cluster of the subdirectory
    int cluster = start_cluster;

    // Print entries in this subdirectory
    // Loop through the cluster chain of the subdirectory to print all entries in the subdirectory
    while (cluster < 0xFF8) {
        // Calculate the offset of the current cluster
        int sector = first_data_sector + (cluster - 2) * sectors_per_cluster;
        int offset = sector * bytes_per_sector;

        // Raise error if seeking to the current cluster fails
        if (fseek(fp, offset, SEEK_SET) != 0) {
            printf("Error seeking to subdirectory\n");
            return;
        }

        // Calculate the number of entries in this cluster
        int bytes_in_cluster = sectors_per_cluster * bytes_per_sector;
        int entries_per_cluster = bytes_in_cluster / 32;

        // Assign entry[32] to a variable to read the directory entry information
        unsigned char entry[32];

        // Loop through entries in the cluster to print files and subdirectories
        for (int i = 0; i < entries_per_cluster; i++) {
            // Raise error if reading the entry fails
            if (fread(entry, 1, 32, fp) != 32) return;

            // Skip empty and deleted entries
            if (entry[0] == 0x00) break;
            if (entry[0] == 0xE5) continue;

            // Assign attribute byte to a variable to check the type of entry
            unsigned char attr = entry[11];

            // Skip long filename entries, volume label, ".", and ".." entries
            if (attr == 0x0F) continue;
            if (attr & 0x08) continue;
            if (entry[0] == '.') continue;

            // Ignore the reserved clusters for directory entries
            if (attr & 0x10) {
                int first_cluster = entry[26] + (entry[27] << 8);
                if (first_cluster <= 1) continue;
            }

            // Print the entry information if it is a valid file or subdirectory
            print_directory_entry(entry);
        }

        // Move to the next cluster in the chain
        cluster = get_fat_entry(fat, cluster);
    }

    // Recurse into child subdirectories
    cluster = start_cluster;

    // Loop through the cluster chain of the subdirectory again to find child subdirectories and call list_subdirectory for each child subdirectory
    while (cluster < 0xFF8) {
        // Calculate the offset of the current cluster
        int sector = first_data_sector + (cluster - 2) * sectors_per_cluster;
        int offset = sector * bytes_per_sector;

        // Raise error if seeking to the current cluster fails
        if (fseek(fp, offset, SEEK_SET) != 0) {
            printf("Error seeking to subdirectory\n");
            return;
        }

        // Calculate the number of entries in this cluster
        int bytes_in_cluster = sectors_per_cluster * bytes_per_sector;
        int entries_per_cluster = bytes_in_cluster / 32;

        // Assign entry[32] to a variable to read the directory entry information
        unsigned char entry[32];

        // Loop through entries in the cluster to print files and subdirectories
        for (int i = 0; i < entries_per_cluster; i++) {
            if (fread(entry, 1, 32, fp) != 32) return;

            // Skip empty and deleted entries, long filename entries, volume label, ".", and ".." entries
            if (entry[0] == 0x00) break;
            if (entry[0] == 0xE5) continue;

            unsigned char attr = entry[11];

            if (attr == 0x0F) continue;
            if (attr & 0x08) continue;

            if (attr & 0x10) {
                if (entry[0] == '.') continue;

                // If the entry is a subdirectory, then print its information and call list_subdirectory to print its subdirectories and files
                int first_cluster = entry[26] + (entry[27] << 8);
                if (first_cluster <= 1) continue;

                char child_name[20];
                // Get formatted directory name and recurse into it
                get_formatted_filename(entry, child_name);
                list_subdirectory(fp, fat, boot_sector, first_cluster, child_name);
            }
        }
        // Move to the next cluster in the chain
        cluster = get_fat_entry(fat, cluster);
    }
}

// Helper to format FAT12 creation date and time for printing
void get_formatted_datetime(unsigned char *entry, char *datetime) {
    unsigned short time = entry[14] + (entry[15] << 8);
    unsigned short date = entry[16] + (entry[17] << 8);

    int year = ((date >> 9) & 0x7F) + 1980;
    int month = (date >> 5) & 0x0F;
    int day = date & 0x1F;

    int hour = (time >> 11) & 0x1F;
    int minute = (time >> 5) & 0x3F;

    sprintf(datetime, "%04d-%02d-%02d %02d:%02d", year, month, day, hour, minute);
}

// Print one directory entry in the required format
void print_directory_entry(unsigned char *entry) {
    unsigned char attr = entry[11];
    char type;
    int file_size;
    char filename[20];
    char datetime[20];

    if (attr & 0x10) {
        type = 'D';
    } else {
        type = 'F';
    }

    file_size = entry[28]
              + (entry[29] << 8)
              + (entry[30] << 16)
              + (entry[31] << 24);

    get_formatted_filename(entry, filename);
    get_formatted_datetime(entry, datetime);

    printf("%c %10d %-20s %s\n", type, file_size, filename, datetime);
}