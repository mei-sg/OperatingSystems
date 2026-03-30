#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include "fat12.h"

// Helper to compare two names without case sensitivity
int names_match_ignore_case(const char *a, const char *b) {
    // Compare both strings one character at a time
    while (*a != '\0' && *b != '\0') {
        // Convert both characters to uppercase before comparing
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return 0;
        }

        // Move to the next character in both strings
        a++;
        b++;
    }

    // Only return true if both strings ended at the same time
    return (*a == '\0' && *b == '\0');
}

// Helper to calculate common FAT12 values from the boot sector
void get_fat12_offsets(unsigned char *boot_sector, int *bytes_per_sector, int *reserved_sectors,
                       int *sectors_per_fat, int *num_fats, int *root_dir_entries,
                       int *sectors_per_cluster, int *root_dir_offset,
                       int *root_dir_sectors, int *first_data_sector) {
    // Read bytes per sector from boot sector
    *bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);

    // Read number of reserved sectors
    *reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);

    // Read sectors per FAT
    *sectors_per_fat = boot_sector[22] + (boot_sector[23] << 8);

    // Read number of FAT copies
    *num_fats = boot_sector[16];

    // Read number of root directory entries
    *root_dir_entries = boot_sector[17] + (boot_sector[18] << 8);

    // Read sectors per cluster
    *sectors_per_cluster = boot_sector[13];

    // Calculate the byte offset of the root directory
    *root_dir_offset = (*reserved_sectors + (*num_fats * *sectors_per_fat)) * *bytes_per_sector;

    // Calculate how many sectors the root directory occupies
    *root_dir_sectors = ((*root_dir_entries * 32) + *bytes_per_sector - 1) / *bytes_per_sector;

    // Calculate the first data sector
    *first_data_sector = *reserved_sectors + (*num_fats * *sectors_per_fat) + *root_dir_sectors;
}

// Helper to calculate the byte offset of a cluster in the data area
int get_cluster_offset(unsigned char *boot_sector, int cluster) {
    // Create local variables to hold FAT12 metadata
    int bytes_per_sector;
    int reserved_sectors;
    int sectors_per_fat;
    int num_fats;
    int root_dir_entries;
    int sectors_per_cluster;
    int root_dir_offset;
    int root_dir_sectors;
    int first_data_sector;

    // Fill the values from the boot sector
    get_fat12_offsets(boot_sector, &bytes_per_sector, &reserved_sectors, &sectors_per_fat,
            &num_fats, &root_dir_entries, &sectors_per_cluster,
            &root_dir_offset, &root_dir_sectors, &first_data_sector);

    // Convert the cluster number into a byte offset
    return (first_data_sector + (cluster - 2) * sectors_per_cluster) * bytes_per_sector;
}

// Helper to set a FAT12 entry in the FAT array in memory
void set_fat_entry(unsigned char *fat, int cluster, int value) {
    // FAT12 stores entries in 12 bits, so offsets are a little tricky
    int offset = (cluster * 3) / 2;

    // Even cluster numbers use one layout
    if (cluster % 2 == 0) {
        fat[offset] = value & 0xFF;
        fat[offset + 1] = (fat[offset + 1] & 0xF0) | ((value >> 8) & 0x0F);
    }
    // Odd cluster numbers use a different layout
    else {
        fat[offset] = (fat[offset] & 0x0F) | ((value << 4) & 0xF0);
        fat[offset + 1] = (value >> 4) & 0xFF;
    }
}

// Helper to write the FAT array back to every FAT copy in the image
int write_fat_copies(FILE *fp, unsigned char *boot_sector, unsigned char *fat) {
    // Read the FAT-related metadata from the boot sector
    int bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
    int sectors_per_fat = boot_sector[22] + (boot_sector[23] << 8);
    int num_fats = boot_sector[16];

    // Calculate the FAT size in bytes
    int fat_size = bytes_per_sector * sectors_per_fat;

    // Write the FAT back to each FAT copy
    for (int i = 0; i < num_fats; i++) {
        // Calculate the start of this FAT copy
        int fat_offset = (reserved_sectors + i * sectors_per_fat) * bytes_per_sector;

        // Seek to the FAT location
        if (fseek(fp, fat_offset, SEEK_SET) != 0) {
            printf("Error seeking to FAT\n");
            return -1;
        }

        // Write the FAT contents to disk
        if (fwrite(fat, 1, fat_size, fp) != (size_t)fat_size) {
            printf("Error writing FAT\n");
            return -1;
        }
    }

    return 0;
}

// Helper to find an unused cluster in the FAT
int find_free_cluster(unsigned char *fat, unsigned char *boot_sector) {
    // Read important values from the boot sector
    int bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
    int sectors_per_fat = boot_sector[22] + (boot_sector[23] << 8);
    int num_fats = boot_sector[16];
    int root_dir_entries = boot_sector[17] + (boot_sector[18] << 8);
    int sectors_per_cluster = boot_sector[13];
    int total_sectors = boot_sector[19] + (boot_sector[20] << 8);

    // Calculate how many sectors the root directory uses
    int root_dir_sectors = (root_dir_entries * 32 + bytes_per_sector - 1) / bytes_per_sector;

    // Calculate how many sectors remain for the data area
    int data_sectors = total_sectors - reserved_sectors - (num_fats * sectors_per_fat) - root_dir_sectors;

    // Calculate how many clusters exist in the data area
    int total_clusters = data_sectors / sectors_per_cluster;

    // Search from cluster 2 onward because 0 and 1 are reserved
    for (int cluster = 2; cluster < total_clusters + 2; cluster++) {
        // A FAT entry of 0 means the cluster is free
        if (get_fat_entry(fat, cluster) == 0x000) {
            return cluster;
        }
    }

    // Return -1 if no free cluster was found
    return -1;
}

// Helper to split a path like /subdir1/subdir2/foo.txt
// into directory path "/subdir1/subdir2" and filename "foo.txt"
void split_path_and_filename(const char *full_path, char *dir_path, char *filename) {
    // Find the last slash in the path
    const char *last_slash = strrchr(full_path, '/');

    // If there is no slash, the file goes to the root directory
    if (last_slash == NULL) {
        strcpy(dir_path, "");
        strcpy(filename, full_path);
        return;
    }

    // Copy the file name after the last slash
    strcpy(filename, last_slash + 1);

    // Special case: path like "/foo.txt" means root directory
    if (last_slash == full_path) {
        strcpy(dir_path, "/");
        return;
    }

    // Copy everything before the last slash as the directory path
    strncpy(dir_path, full_path, last_slash - full_path);
    dir_path[last_slash - full_path] = '\0';
}

// Helper to convert a Linux file name into FAT12 8.3 format
void make_fat_filename(const char *input, unsigned char *fat_name) {
    // Fill all 11 bytes with spaces first
    for (int i = 0; i < 11; i++) {
        fat_name[i] = ' ';
    }

    // Find the final dot separating name and extension
    const char *dot = strrchr(input, '.');

    // Variables to hold the base name and extension lengths
    int name_length;
    int ext_length = 0;

    // If a normal extension exists, split at the dot
    if (dot != NULL && dot != input) {
        name_length = dot - input;
        ext_length = strlen(dot + 1);
    }
    // Otherwise treat the whole string as the file name
    else {
        name_length = strlen(input);
    }

    // FAT12 supports only 8 characters for the name
    if (name_length > 8) {
        name_length = 8;
    }

    // FAT12 supports only 3 characters for the extension
    if (ext_length > 3) {
        ext_length = 3;
    }

    // Copy the base file name into the first 8 bytes in uppercase
    for (int i = 0; i < name_length; i++) {
        fat_name[i] = toupper((unsigned char)input[i]);
    }

    // Copy the extension into bytes 8 to 10 in uppercase
    for (int i = 0; i < ext_length; i++) {
        fat_name[8 + i] = toupper((unsigned char)dot[1 + i]);
    }
}

// Helper to search the root directory for a named subdirectory
int find_subdirectory_in_root(FILE *fp, unsigned char *boot_sector, const char *target_name) {
    // Read root directory layout information from the boot sector
    int bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
    int sectors_per_fat = boot_sector[22] + (boot_sector[23] << 8);
    int num_fats = boot_sector[16];
    int root_dir_entries = boot_sector[17] + (boot_sector[18] << 8);

    // Calculate where the root directory starts
    int root_dir_offset = (reserved_sectors + (num_fats * sectors_per_fat)) * bytes_per_sector;

    // Move to the start of the root directory
    if (fseek(fp, root_dir_offset, SEEK_SET) != 0) {
        return -1;
    }

    unsigned char entry[32];

    // Check each root directory entry
    for (int i = 0; i < root_dir_entries; i++) {
        if (fread(entry, 1, 32, fp) != 32) {
            break;
        }

        // 0x00 means there are no more entries
        if (entry[0] == 0x00) {
            break;
        }

        // 0xE5 means deleted entry
        if (entry[0] == 0xE5) {
            continue;
        }

        unsigned char attr = entry[11];

        // Skip long file name entries
        if (attr == 0x0F) {
            continue;
        }

        // Skip volume labels
        if (attr & 0x08) {
            continue;
        }

        // Only accept directories
        if (!(attr & 0x10)) {
            continue;
        }

        // Skip "." and ".."
        if (entry[0] == '.') {
            continue;
        }

        // Convert entry name to a readable string
        char dir_name[20];
        get_formatted_filename(entry, dir_name);

        // Compare to the target name
        if (names_match_ignore_case(dir_name, target_name)) {
            return entry[26] + (entry[27] << 8);
        }
    }

    return -1;
}

// Helper to search a subdirectory cluster chain for a named subdirectory
int find_subdirectory_in_cluster(FILE *fp, unsigned char *fat, unsigned char *boot_sector,
                                 int start_cluster, const char *target_name) {
    // Read cluster sizing information
    int bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int sectors_per_cluster = boot_sector[13];

    // Start at the first cluster of the directory
    int cluster = start_cluster;

    // Walk the whole cluster chain for this directory
    while (cluster < 0xFF8) {
        // Convert the cluster to a byte offset
        int offset = get_cluster_offset(boot_sector, cluster);

        // Calculate how many directory entries fit in one cluster
        int entries_per_cluster = (sectors_per_cluster * bytes_per_sector) / 32;

        // Seek to the start of this cluster
        if (fseek(fp, offset, SEEK_SET) != 0) {
            return -1;
        }

        // Read each 32-byte directory entry
        for (int i = 0; i < entries_per_cluster; i++) {
            unsigned char entry[32];

            if (fread(entry, 1, 32, fp) != 32) {
                return -1;
            }

            // 0x00 means no more active entries in this directory area
            if (entry[0] == 0x00) {
                break;
            }

            // Skip deleted entries
            if (entry[0] == 0xE5) {
                continue;
            }

            unsigned char attr = entry[11];

            // Skip long file name entries
            if (attr == 0x0F) {
                continue;
            }

            // Skip volume labels
            if (attr & 0x08) {
                continue;
            }

            // Only accept directories
            if (!(attr & 0x10)) {
                continue;
            }

            // Skip "." and ".."
            if (entry[0] == '.') {
                continue;
            }

            // Convert the FAT name into normal format
            char dir_name[20];
            get_formatted_filename(entry, dir_name);

            // Return the starting cluster if the name matches
            if (names_match_ignore_case(dir_name, target_name)) {
                return entry[26] + (entry[27] << 8);
            }
        }

        // Move to the next cluster in the directory chain
        cluster = get_fat_entry(fat, cluster);
    }

    return -1;
}

// Helper to resolve a destination directory path inside the FAT12 image
int resolve_destination_directory(FILE *fp, unsigned char *fat, unsigned char *boot_sector,
                                  const char *dir_path, int *is_root, int *dir_cluster) {
    // Empty string or "/" means root directory
    if (dir_path[0] == '\0' || strcmp(dir_path, "/") == 0) {
        *is_root = 1;
        *dir_cluster = 0;
        return 0;
    }

    // Make a modifiable copy because strtok changes the string
    char path_copy[256];
    strncpy(path_copy, dir_path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    // Start at the root
    int current_cluster = 0;
    int current_is_root = 1;

    // Split path into tokens like "subdir1", "subdir2", etc.
    char *token = strtok(path_copy, "/");

    while (token != NULL) {
        int next_cluster;

        // Search in root if currently at root
        if (current_is_root) {
            next_cluster = find_subdirectory_in_root(fp, boot_sector, token);
        }
        // Otherwise search inside the current directory cluster chain
        else {
            next_cluster = find_subdirectory_in_cluster(fp, fat, boot_sector, current_cluster, token);
        }

        // If a path component was not found, the directory does not exist
        if (next_cluster < 2) {
            return -1;
        }

        // Move into the next subdirectory
        current_cluster = next_cluster;
        current_is_root = 0;

        // Get the next directory component
        token = strtok(NULL, "/");
    }

    // Return the final directory location
    *is_root = current_is_root;
    *dir_cluster = current_cluster;
    return 0;
}

// Helper to find a free entry in the root directory
int find_free_root_entry(FILE *fp, unsigned char *boot_sector) {
    // Read root directory information
    int bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int reserved_sectors = boot_sector[14] + (boot_sector[15] << 8);
    int sectors_per_fat = boot_sector[22] + (boot_sector[23] << 8);
    int num_fats = boot_sector[16];
    int root_dir_entries = boot_sector[17] + (boot_sector[18] << 8);

    // Calculate where the root directory begins
    int root_dir_offset = (reserved_sectors + (num_fats * sectors_per_fat)) * bytes_per_sector;

    // Check each entry for a free spot
    for (int i = 0; i < root_dir_entries; i++) {
        int entry_offset = root_dir_offset + i * 32;
        unsigned char entry[32];

        // Seek to the entry location
        if (fseek(fp, entry_offset, SEEK_SET) != 0) {
            return -1;
        }

        // Read the entry
        if (fread(entry, 1, 32, fp) != 32) {
            return -1;
        }

        // 0x00 means never used, 0xE5 means deleted
        if (entry[0] == 0x00 || entry[0] == 0xE5) {
            return entry_offset;
        }
    }

    return -1;
}

// Helper to find the last cluster in a cluster chain
int find_last_cluster_in_chain(unsigned char *fat, int start_cluster) {
    // Start at the given cluster
    int cluster = start_cluster;

    // Keep following the chain until reaching end-of-chain
    while (get_fat_entry(fat, cluster) < 0xFF8) {
        cluster = get_fat_entry(fat, cluster);
    }

    return cluster;
}

// Helper to find a free entry in a subdirectory
// If the directory is full, expand it by adding a new cluster
int find_free_subdirectory_entry(FILE *fp, unsigned char *fat, unsigned char *boot_sector, int dir_cluster) {
    // Read cluster size information
    int bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int sectors_per_cluster = boot_sector[13];
    int cluster_size = bytes_per_sector * sectors_per_cluster;

    // Start at the first cluster of the directory
    int cluster = dir_cluster;

    // Walk through all clusters belonging to this directory
    while (cluster < 0xFF8) {
        int offset = get_cluster_offset(boot_sector, cluster);
        int entries_per_cluster = cluster_size / 32;

        // Seek to the start of this cluster
        if (fseek(fp, offset, SEEK_SET) != 0) {
            return -1;
        }

        // Check each directory entry in this cluster
        for (int i = 0; i < entries_per_cluster; i++) {
            unsigned char entry[32];

            if (fread(entry, 1, 32, fp) != 32) {
                return -1;
            }

            // Unused or deleted entry means free space for a new file
            if (entry[0] == 0x00 || entry[0] == 0xE5) {
                return offset + i * 32;
            }
        }

        // Stop if this is the last cluster in the directory chain
        if (get_fat_entry(fat, cluster) >= 0xFF8) {
            break;
        }

        // Otherwise move to the next cluster
        cluster = get_fat_entry(fat, cluster);
    }

    // No free entry found, so try to expand the directory by one cluster
    int new_cluster = find_free_cluster(fat, boot_sector);
    if (new_cluster < 2) {
        return -1;
    }

    // Find the current last cluster of the directory
    int last_cluster = find_last_cluster_in_chain(fat, dir_cluster);

    // Link the old last cluster to the new cluster
    set_fat_entry(fat, last_cluster, new_cluster);

    // Mark the new cluster as end-of-chain
    set_fat_entry(fat, new_cluster, 0xFFF);

    // Write the updated FAT back to the disk image
    if (write_fat_copies(fp, boot_sector, fat) != 0) {
        return -1;
    }

    // Allocate a zero-filled buffer for the new cluster
    unsigned char *empty_cluster = calloc(1, cluster_size);
    if (empty_cluster == NULL) {
        return -1;
    }

    // Write zeros into the new cluster so it starts empty
    int new_offset = get_cluster_offset(boot_sector, new_cluster);

    if (fseek(fp, new_offset, SEEK_SET) != 0) {
        free(empty_cluster);
        return -1;
    }

    if (fwrite(empty_cluster, 1, cluster_size, fp) != (size_t)cluster_size) {
        free(empty_cluster);
        return -1;
    }

    // Free the zero-filled buffer
    free(empty_cluster);

    // The first entry in the new cluster is free
    return new_offset;
}

// Helper to convert Linux last write time to FAT12 date and time format
void get_fat_datetime(time_t linux_time, unsigned short *fat_date, unsigned short *fat_time) {
    // Convert time_t to a broken-down local time structure
    struct tm *time_info = localtime(&linux_time);

    // FAT dates start at year 1980
    int year = time_info->tm_year + 1900;
    if (year < 1980) {
        year = 1980;
    }

    // FAT date layout:
    // bits 15-9 = year since 1980
    // bits 8-5  = month
    // bits 4-0  = day
    *fat_date = ((year - 1980) << 9) | ((time_info->tm_mon + 1) << 5) | time_info->tm_mday;

    // FAT time layout:
    // bits 15-11 = hour
    // bits 10-5  = minute
    // bits 4-0   = seconds / 2
    *fat_time = (time_info->tm_hour << 11) | (time_info->tm_min << 5) | (time_info->tm_sec / 2);
}

// Helper to write a directory entry for the copied file
int write_directory_entry(FILE *fp, int entry_offset, const char *filename,
                          int first_cluster, long file_size, time_t mod_time) {
    // FAT directory entries are exactly 32 bytes
    unsigned char entry[32];
    memset(entry, 0, 32);

    // Fill in the 8.3 file name
    make_fat_filename(filename, entry);

    // Mark it as a normal file (archive attribute)
    entry[11] = 0x20;

    // Convert the Linux file modification time to FAT format
    unsigned short fat_date;
    unsigned short fat_time;
    get_fat_datetime(mod_time, &fat_date, &fat_time);

    // Write creation time
    entry[14] = fat_time & 0xFF;
    entry[15] = (fat_time >> 8) & 0xFF;

    // Write creation date
    entry[16] = fat_date & 0xFF;
    entry[17] = (fat_date >> 8) & 0xFF;

    // Write last access date
    entry[18] = fat_date & 0xFF;
    entry[19] = (fat_date >> 8) & 0xFF;

    // Write last write time
    entry[22] = fat_time & 0xFF;
    entry[23] = (fat_time >> 8) & 0xFF;

    // Write last write date
    entry[24] = fat_date & 0xFF;
    entry[25] = (fat_date >> 8) & 0xFF;

    // Write the first logical cluster
    entry[26] = first_cluster & 0xFF;
    entry[27] = (first_cluster >> 8) & 0xFF;

    // Write the file size in bytes
    entry[28] = file_size & 0xFF;
    entry[29] = (file_size >> 8) & 0xFF;
    entry[30] = (file_size >> 16) & 0xFF;
    entry[31] = (file_size >> 24) & 0xFF;

    // Seek to the target directory entry position
    if (fseek(fp, entry_offset, SEEK_SET) != 0) {
        return -1;
    }

    // Write the 32-byte directory entry
    if (fwrite(entry, 1, 32, fp) != 32) {
        return -1;
    }

    return 0;
}

// Helper to allocate enough free clusters for a file
int allocate_file_clusters(unsigned char *fat, unsigned char *boot_sector, int needed_clusters, int *clusters) {
    // Find each needed free cluster one by one
    for (int i = 0; i < needed_clusters; i++) {
        clusters[i] = find_free_cluster(fat, boot_sector);

        // Stop if the disk ran out of free clusters
        if (clusters[i] < 2) {
            return -1;
        }

        // Temporarily mark this cluster as end-of-chain
        set_fat_entry(fat, clusters[i], 0xFFF);
    }

    // Link each cluster to the next cluster in the chain
    for (int i = 0; i < needed_clusters - 1; i++) {
        set_fat_entry(fat, clusters[i], clusters[i + 1]);
    }

    return 0;
}

// Helper to write the Linux file data into the allocated FAT12 clusters
int write_file_clusters(FILE *fp, unsigned char *boot_sector, FILE *source_file,
                        int *clusters, int needed_clusters, long file_size) {
    // Read cluster size information from the boot sector
    int bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int sectors_per_cluster = boot_sector[13];
    int cluster_size = bytes_per_sector * sectors_per_cluster;

    // Allocate a buffer large enough for one whole cluster
    unsigned char *buffer = calloc(1, cluster_size);
    if (buffer == NULL) {
        return -1;
    }

    // Keep track of how many bytes remain to be copied
    long remaining = file_size;

    // Write each allocated cluster one by one
    for (int i = 0; i < needed_clusters; i++) {
        // Only read as many bytes as remain for the final cluster
        int bytes_to_read = (remaining < cluster_size) ? remaining : cluster_size;

        // Clear the whole buffer so any unused bytes stay zero
        memset(buffer, 0, cluster_size);

        // Read file data from the Linux file into the buffer
        if (bytes_to_read > 0) {
            if (fread(buffer, 1, bytes_to_read, source_file) != (size_t)bytes_to_read) {
                free(buffer);
                return -1;
            }
        }

        // Calculate where this cluster lives in the disk image
        int offset = get_cluster_offset(boot_sector, clusters[i]);

        // Seek to the cluster location
        if (fseek(fp, offset, SEEK_SET) != 0) {
            free(buffer);
            return -1;
        }

        // Write the full cluster to the image
        if (fwrite(buffer, 1, cluster_size, fp) != (size_t)cluster_size) {
            free(buffer);
            return -1;
        }

        // Decrease the number of bytes left to copy
        remaining -= bytes_to_read;
    }

    // Free the cluster buffer
    free(buffer);
    return 0;
}

// Main to copy a Linux file into the FAT12 image
int main(int argc, char *argv[]) {
    FILE *fp;

    // Check for the correct number of arguments
    if (argc != 3) {
        printf("Usage: %s disk.IMA /subdir1/subdir2/foo.txt\n", argv[0]);
        return 1;
    }

    // Split the user path into destination directory and file name
    char dest_dir[256];
    char linux_filename[256];
    split_path_and_filename(argv[2], dest_dir, linux_filename);

    // Open the Linux source file that will be copied into the image
    FILE *source_file = fopen(linux_filename, "rb");
    if (source_file == NULL) {
        printf("File not found.\n");
        return 1;
    }

    // Read Linux file metadata such as size and modification time
    struct stat source_stat;
    if (stat(linux_filename, &source_stat) != 0) {
        fclose(source_file);
        printf("File not found.\n");
        return 1;
    }

    // Open the FAT12 disk image in read/write binary mode
    fp = fopen(argv[1], "rb+");
    if (fp == NULL) {
        fclose(source_file);
        printf("Error occurred while opening file\n");
        return 1;
    }

    // Store the boot sector and FAT data in memory
    unsigned char boot_sector[512];
    unsigned char *fat = NULL;

    // Load FAT12 metadata and FAT into memory
    if (load_fat12_metadata(fp, boot_sector, &fat) != 0) {
        fclose(source_file);
        fclose(fp);
        return 1;
    }

    // Make sure the disk image has enough free space for the file data
    if (get_free_size(fp, boot_sector) < source_stat.st_size) {
        printf("No enough free space in the disk image.\n");
        free(fat);
        fclose(source_file);
        fclose(fp);
        return 1;
    }

    // Resolve the destination directory path inside the FAT12 image
    int is_root;
    int dir_cluster;
    if (resolve_destination_directory(fp, fat, boot_sector, dest_dir, &is_root, &dir_cluster) != 0) {
        printf("The directory not found.\n");
        free(fat);
        fclose(source_file);
        fclose(fp);
        return 1;
    }

    // Find a free directory entry in the destination directory
    int entry_offset;
    if (is_root) {
        entry_offset = find_free_root_entry(fp, boot_sector);
    } else {
        entry_offset = find_free_subdirectory_entry(fp, fat, boot_sector, dir_cluster);
    }

    // If no directory entry can be found or created, fail
    if (entry_offset < 0) {
        printf("No enough free space in the disk image.\n");
        free(fat);
        fclose(source_file);
        fclose(fp);
        return 1;
    }

    // Calculate the cluster size in bytes
    int bytes_per_sector = boot_sector[11] + (boot_sector[12] << 8);
    int sectors_per_cluster = boot_sector[13];
    int cluster_size = bytes_per_sector * sectors_per_cluster;

    // Calculate how many clusters are needed for this file
    int needed_clusters = (source_stat.st_size + cluster_size - 1) / cluster_size;

    // Default first cluster is 0 for an empty file
    int first_cluster = 0;

    // This array will store the cluster numbers allocated to the file
    int *clusters = NULL;

    // Only allocate clusters if the file is not empty
    if (needed_clusters > 0) {
        clusters = malloc(sizeof(int) * needed_clusters);
        if (clusters == NULL) {
            printf("Memory allocation failed\n");
            free(fat);
            fclose(source_file);
            fclose(fp);
            return 1;
        }

        // Allocate free clusters in the FAT
        if (allocate_file_clusters(fat, boot_sector, needed_clusters, clusters) != 0) {
            printf("No enough free space in the disk image.\n");
            free(clusters);
            free(fat);
            fclose(source_file);
            fclose(fp);
            return 1;
        }

        // The first allocated cluster is the file's starting cluster
        first_cluster = clusters[0];

        // Write the file data into the allocated clusters
        if (write_file_clusters(fp, boot_sector, source_file, clusters, needed_clusters, source_stat.st_size) != 0) {
            printf("Error writing file data\n");
            free(clusters);
            free(fat);
            fclose(source_file);
            fclose(fp);
            return 1;
        }
    }

    // Write the updated FAT back to the image
    if (write_fat_copies(fp, boot_sector, fat) != 0) {
        free(clusters);
        free(fat);
        fclose(source_file);
        fclose(fp);
        return 1;
    }

    // Write the file's new directory entry into the destination directory
    if (write_directory_entry(fp, entry_offset, linux_filename, first_cluster,
                              source_stat.st_size, source_stat.st_mtime) != 0) {
        printf("Error writing directory entry\n");
        free(clusters);
        free(fat);
        fclose(source_file);
        fclose(fp);
        return 1;
    }

    // Free allocated memory and close files before exiting
    free(clusters);
    free(fat);
    fclose(source_file);
    fclose(fp);

    return 0;
}