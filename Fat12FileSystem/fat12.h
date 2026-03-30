#ifndef FAT12_H
#define FAT12_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Function prototypes from fat12.c
int load_fat12_metadata(FILE *fp, unsigned char *boot_sector, unsigned char **fat);
void print_disk_info(FILE *fp);
void get_disk_label(FILE *fp, unsigned char *boot_sector, char *label);
int get_total_size(unsigned char *boot_sector);
int get_free_size(FILE *fp, unsigned char *boot_sector);
int get_fat_entry(unsigned char *fat, int cluster);
int get_num_fat_copies(unsigned char *boot_sector);
int get_sectors_per_fat(unsigned char *boot_sector);
int get_num_files(FILE *fp, unsigned char *boot_sector);
void get_formatted_filename(unsigned char *entry, char *filename);

// Function prototypes from disklist.c
void print_all_directory_info(FILE *fp);
void list_root_directory(FILE *fp, unsigned char *fat, unsigned char *boot_sector);
void list_subdirectory(FILE *fp, unsigned char *fat, unsigned char *boot_sector, int cluster, char *dir_name);
void print_directory_entry(unsigned char *entry);

void get_formatted_datetime(unsigned char *entry, char *datetime);

// Function prototypes from diskget.c
//void get_formatted_filename(unsigned char *entry, char *filename);


// Function prototypes from diskput.c

// Function prototypes from diskget.c




#endif

