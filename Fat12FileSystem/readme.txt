These programs are used for interacting with a FAT12 file system:
- diskinfo: Displays file system information
- disklist: Lists directory contents recursively
- diskget: Copies a file from the disk image to Linux
- diskput: Copies a file from Linux into the disk image

To compile the programs, run:
$ make

This will generate the following executables:
diskinfo, disklist, diskget, diskput

To clean:
$ make clean

Usage:
./diskinfo disk.IMA
./disklist disk.IMA
./diskget disk.IMA FILENAME
./diskput disk.IMA /path/to/file

Files:
diskinfo.c
disklist.c
diskget.c
diskput.c
fat12.c
fat12.h
Makefile
readme.txt

Error Handling:
- Check for file open failures
- Validate inputs
- Handles file-not-found and directory-not-found cases
- Check there is enough space on the disk before writing