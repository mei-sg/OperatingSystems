#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fat12.h"

void print_disk_info(FILE *fp) {
    // Read the boot sector for disk information
    unsigned char boot_sector[512]; 

    // Raise error if the boot sector cannot be read
    if (fread(boot_sector, 1, 512, fp) != 512) {
        printf("Error occurred whilereading boot sector\n");
        return;
    }

    // Find OS name from the boot sector and store in a string
    char os[9]; 
    memcpy(os, boot_sector + 3, 8); 
    os[8] = '\0';

    // Use helpers to find the disk information
    char disk_label[12];
    get_disk_label(fp, boot_sector, disk_label);
    int total_disk_size = get_total_size(boot_sector);
    int free_size = get_free_size(fp, boot_sector);
    int num_files = get_num_files(fp, boot_sector);
    int num_fat_copies = get_num_fat_copies(boot_sector);
    int num_sectors_per_fat = get_sectors_per_fat(boot_sector);
    

    // Print the disk information 
    printf("OS Name: %s\n", os);
    printf("Disk label: %s\n", disk_label);
    printf("Total size of the disk: %d %s\n", total_disk_size, "bytes");
    printf("Free size of the disk: %d %s\n\n", free_size, "bytes");
    printf("==============\n");
    printf("Number of files in the disk (including all files in the root directory and files in all subdirectories): %d\n", num_files);
    printf("==============\n\n");
    printf("Number of FAT copies: %d\n", num_fat_copies);
    printf("Sectors per FAT: %d\n\n\n", num_sectors_per_fat);
}

// Helper to calculate the FAT12 metadata from the boot sector and load the FAT into memory for use in other functions.
// Returns 0 on success and -1 on failure.
int load_fat12_metadata(FILE *fp, unsigned char *boot_sector, unsigned char **fat) {
    int num_bytes_per_sector;
    int num_reserved_sectors;
    int num_sectors_per_fat;
    int fat_offset;
    int fat_size;

    // Go to the start of the disk image
    if (fseek(fp, 0, SEEK_SET) != 0) {
        printf("Error seeking to boot sector\n");
        return -1;
    }

    // Read boot sector
    if (fread(boot_sector, 1, 512, fp) != 512) {
        printf("Error reading boot sector\n");
        return -1;
    }

    // Decode FAT12 metadata from boot sector
    num_bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    num_reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
    num_sectors_per_fat = boot_sector[22] + (boot_sector[23] << 8);

    // Compute FAT location and size
    fat_offset = num_reserved_sectors * num_bytes_per_sector;
    fat_size = num_sectors_per_fat * num_bytes_per_sector;

    // Allocate memory for FAT
    *fat = malloc(fat_size);
    if (*fat == NULL) {
        printf("Memory allocation failed\n");
        return -1;
    }

    // Seek to FAT
    if (fseek(fp, fat_offset, SEEK_SET) != 0) {
        printf("Error seeking to FAT\n");
        free(*fat);
        *fat = NULL;
        return -1;
    }

    // Read FAT
    if (fread(*fat, 1, fat_size, fp) != (size_t)fat_size) {
        printf("Error reading FAT\n");
        free(*fat);
        *fat = NULL;
        return -1;
    }

    return 0;
}


// Get the disk label from the root directory entry with attribute 0x08
void get_disk_label(FILE *fp, unsigned char *boot_sector, char *label) {
    // Read the root directory entries to find the disk label
    int num_bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int num_reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
    int num_sectors_per_fat = boot_sector[22] + (boot_sector[23] << 8);
    int num_fats = boot_sector[16];
    int root_dir_entries = boot_sector[17] + (boot_sector[18] << 8);

    // Root directory starts after reserved sectors and FATs
    int root_dir_offset = (num_reserved_sectors + (num_fats * num_sectors_per_fat)) * num_bytes_per_sector;

    // Raise error if the root directory cannot be found
    if (fseek(fp, root_dir_offset, SEEK_SET) != 0) {
        printf("Error seeking to root directory\n");
        return;
    }

    // Create an array for directory entry's 32 bytes
    unsigned char entry[32];

    for (int i = 0; i < root_dir_entries; i++) {
        if (fread(entry, 1, 32, fp) != 32) break;
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        // Read the attribute byte to determine the type of entry
        unsigned char attr = entry[11];

        // Skip long filename entries, which have attribute 0x0F and are not valid disk labels
        if (attr == 0x0F) continue;

        // If we find the disk name, then copy it into the label char array in print_disk_info and return
        if (attr & 0x08) {
            memcpy(label, entry, 11);
            label[11] = '\0';

            // Remove trailing spaces
            for (int j = 10; j >= 0 && label[j] == ' '; j--) {
                label[j] = '\0';   
            }

            return;
        }
    }

    // Return "No label" if no disk label is found
    strcpy(label, "No label");
}

// Calculate the total size of the disk
int get_total_size(unsigned char *boot_sector) {
    // Calculate total disk size by multiplying bytes per sector and total number of sectors
    int bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int num_sectors = boot_sector[19] + (boot_sector[20] << 8);

    // return total disk size in bytes to be printed
    return num_sectors * bytes_per_sector;
}

// Calculate the free size of the disk 
int get_free_size(FILE *fp, unsigned char *boot_sector) {
    // Read the FAT to calculate the number of free clusters
    int num_bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int num_reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
    int num_sectors_per_fat = boot_sector[22] + (boot_sector[23] << 8);
    int num_sectors_per_cluster = boot_sector[13];

    // Calculate the offset and size of the FAT
    int fat_offset = num_reserved_sectors * num_bytes_per_sector;
    int fat_size = num_sectors_per_fat * num_bytes_per_sector;

    // Initialize the number of free clusters to 0
    int num_free_clusters = 0;

    // Calculate the number of total clusters and the number of free clusters
    int total_sectors = boot_sector[19] + (boot_sector[20] << 8);
    int num_fats = boot_sector[16];
    int root_dir_entries = boot_sector[17] + (boot_sector[18] << 8);
    int root_dir_sectors = (root_dir_entries * 32) / num_bytes_per_sector;
    int data_sectors = total_sectors - num_reserved_sectors - (num_fats * num_sectors_per_fat) - root_dir_sectors;
    int total_clusters = data_sectors / num_sectors_per_cluster;

    // Load FAT into memory
    unsigned char *fat = NULL;
    if (load_fat12_metadata(fp, boot_sector, &fat) != 0) {
        return 0;
    }

    // Calculate the number of free clusters
    for (int cluster = 2; cluster < total_clusters + 2; cluster++) {
        int value = get_fat_entry(fat, cluster);

        // If the value of the FAT entry is 0x000, then it is free
        if (value == 0x000) {
            num_free_clusters++;
        }
    }

    // Release allocated memory
    free(fat);

    // Return the free size of the disk by multiplying the number of free clusters, sectors per cluster, and bytes per sector
    return num_free_clusters * num_sectors_per_cluster * num_bytes_per_sector;
}


// Find the FAT entry for a given cluster
int get_fat_entry(unsigned char *fat, int cluster) {
    // Calculate the offset of the cluster's FAT entry.
    // FAT12 entries are 12 bits each, so FAT packing must be used.
    // To find the byte offset: multiply cluster by 3/2 (1.5 bytes per entry).
    // Even clusters use the low 12 bits of the 2 bytes at offset and odd clusters use the high 12 bits.
    int offset = (cluster * 3) / 2;

    if (cluster % 2 == 0) {
        return fat[offset] | ((fat[offset + 1] & 0x0F) << 8);
    } else {
        return ((fat[offset] >> 4) & 0x0F) | (fat[offset + 1] << 4);
    }
}

// Calculate the number of FAT copies from the boot sector
int get_num_fat_copies(unsigned char *boot_sector) {
    // Stored in the boot sector at offset 16
    return boot_sector[16];
}

// Calculate the number of sectors per FAT from the boot sector
int get_sectors_per_fat(unsigned char *boot_sector) {
    // Stored in the boot sector at offset 22
    // Combine the two bytes with bit shifting (little endian) to get the number of sectors per FAT
    return boot_sector[22] + (boot_sector[23] << 8);
}

// Recursively count files in a subdirectory by following its cluster chain
int count_files_in_dir(FILE *fp, unsigned char *fat, unsigned char *boot_sector, int cluster) {
    // Read the boot sector for disk information
    int num_bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int num_reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
    int num_sectors_per_fat = boot_sector[22] + (boot_sector[23] << 8);
    int num_fats = boot_sector[16];
    int num_sectors_per_cluster = boot_sector[13];
    int root_dir_entries = boot_sector[17] + (boot_sector[18] << 8);
    int root_dir_sectors = (root_dir_entries * 32) / num_bytes_per_sector;

    // Calculate where the data area starts on disk
    int data_start = (num_reserved_sectors + (num_fats * num_sectors_per_fat) + root_dir_sectors) * num_bytes_per_sector;

    // Initialize the number of files to 0
    int num_files = 0;
    unsigned char entry[32];

    // Use cluster chain until the end-of-file marker
    while (cluster >= 0x002 && cluster <= 0xFEF) {
        // Find the current cluster in the data area
        int sector_offset = data_start + (cluster - 2) * num_sectors_per_cluster * num_bytes_per_sector;
        if (fseek(fp, sector_offset, SEEK_SET) != 0) break;

        // Read all entries in this cluster
        int entries_per_cluster = (num_sectors_per_cluster * num_bytes_per_sector) / 32;
        for (int i = 0; i < entries_per_cluster; i++) {
            if (fread(entry, 1, 32, fp) != 32) break;

            // 0x00 indicates no more entries
            if (entry[0] == 0x00) return num_files;
            // 0xE5 indicates a deleted file
            if (entry[0] == 0xE5) continue;

            // Read the attribute byte to determine the type of entry
            unsigned char attr = entry[11];

            // Skip long filename entries
            if (attr == 0x0F) continue;
            // Skip volume labels
            if (attr & 0x08) continue;

            if (attr & 0x10) {
                // Skip "." and ".." entries
                if (entry[0] == '.') continue;

                // Subdirectory found — recurse into it to count its files
                int subdir_cluster = entry[26] + (entry[27] << 8);
                if (subdir_cluster > 1) {
                    long pos = ftell(fp);
                    num_files += count_files_in_dir(fp, fat, boot_sector, subdir_cluster);
                    fseek(fp, pos, SEEK_SET);
                }
                continue;
            }

            // If a valid file is found then increment file count
            num_files++;
        }

        // Follow to the next cluster in the chain
        cluster = get_fat_entry(fat, cluster);
    }

    // Return the number of files found in this directory and its subdirectories
    return num_files;
}

// Calculate the number of files in the root directory and subdirectories
int get_num_files(FILE *fp, unsigned char *boot_sector) {
     // Read the boot sector for disk geometry information
    int num_bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int num_reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
    int num_sectors_per_fat = boot_sector[22] + (boot_sector[23] << 8);
    int num_fats = boot_sector[16];
    int root_dir_entries = boot_sector[17] + (boot_sector[18] << 8);

    

    // Root directory starts after reserved sectors and FATs
    int root_dir_offset = (num_reserved_sectors + (num_fats * num_sectors_per_fat)) * num_bytes_per_sector;

    // Load FAT into memory to follow cluster chains for subdirectories
    unsigned char *fat = NULL;
    if (load_fat12_metadata(fp, boot_sector, &fat) != 0) {
        return 0;
    }

    // Find to the root directory
    if (fseek(fp, root_dir_offset, SEEK_SET) != 0) {
        printf("Error seeking to root directory\n");
        //free(fat);
        return 0;
    }

    // Initialize the number of files to 0
    int num_files = 0;
    unsigned char entry[32];

    // Read through root directory entries to count regular files
    for (int i = 0; i < root_dir_entries; i++) {
        if (fread(entry, 1, 32, fp) != 32) break;

        // 0x00 indicates no more entries
        if (entry[0] == 0x00) break;
        // 0xE5 indicates a deleted file
        if (entry[0] == 0xE5) continue;

        // Read the attribute byte to determine the type of entry
        unsigned char attr = entry[11];

        // Skip long filename entries
        if (attr == 0x0F) continue;
        // Skip volume labels
        if (attr & 0x08) continue;

        if (attr & 0x10) {
            // Subdirectory found — recurse into it to count its files
            int subdir_cluster = entry[26] + (entry[27] << 8);
            if (subdir_cluster > 1) {
                long pos = ftell(fp);
                num_files += count_files_in_dir(fp, fat, boot_sector, subdir_cluster);
                fseek(fp, pos, SEEK_SET);
            }
            continue;
        }

        // If a valid file is found in the root directory then increment file count
        num_files++;
    }

    // Release allocated memory
    free(fat);

    // Return the total number of files including those in subdirectories
    return num_files;
}

// Helper to format a FAT12 8.3 filename for printing
void get_formatted_filename(unsigned char *entry, char *filename) {
    char name[9];
    char ext[4];

    memcpy(name, entry, 8);
    name[8] = '\0';

    memcpy(ext, entry + 8, 3);
    ext[3] = '\0';

    // Remove trailing spaces from name
    for (int i = 7; i >= 0; i--) {
        if (name[i] == ' ') {
            name[i] = '\0';
        } else {
            break;
        }
    }

    // Remove trailing spaces from extension
    for (int i = 2; i >= 0; i--) {
        if (ext[i] == ' ') {
            ext[i] = '\0';
        } else {
            break;
        }
    }

    // Format as NAME.EXT or NAME
    if (ext[0] != '\0') {
        sprintf(filename, "%s.%s", name, ext);
    } else {
        sprintf(filename, "%s", name);
    }
}