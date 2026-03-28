#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fat12.h"

// #include <stdint.h>


#include <stdio.h>
#include <string.h>

int main() {
    FILE *fp = fopen("disk.IMA", "rb");

    if (fp == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    unsigned char buffer[512];
    fread(buffer, 1, 512, fp);

    char os[9];
    memcpy(os, buffer + 3, 8);
    os[8] = '\0';

    printf("OS Name: %s\n", os);

    fclose(fp);
    return 0;
}