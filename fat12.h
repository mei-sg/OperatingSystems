#ifndef FAT12_H
#define FAT12_H

#include <stdint.h>

void readBootSector(FILE *fp);
int getNextCluster(int cluster, FILE *fp);
void readRootDirectory(FILE *fp);

#endif