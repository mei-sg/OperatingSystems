#include "fat12.h"

/* 🔧 Examples of helpers (VERY IMPORTANT)

From your assignment + FAT search steps:

File system reading
readBootSector()
getOSName()
getDiskLabel()
FAT table logic
getNextCluster(int cluster)
isFreeCluster()
Directory parsing
readDirectory(int cluster)
parseDirectoryEntry()
File reading (like your steps doc)
follow cluster chain (Step 6–7 logic)
🧱 What goes in each file
fat12.c (shared logic)
All helpers
NO main()
diskinfo.c
Calls helpers like:
readBootSector()
countFreeSectors()
disklist.c
Calls:
readDirectory()
recursion for subdirectories
diskget.c
Calls:
search file in root
follow cluster chain`
copy data
diskput.c
Calls:
find free clusters
update FAT
write directory entry
🚨 How to decide: helper vs main logic

Ask yourself:

“Will I need this function in more than one program?”

YES → goes in fat12.c
NO → stays in that program’s .c
🎯 Simple rule (this will save you)
Type of code	Where it goes
FAT parsing	fat12.c
Directory reading	fat12.c
Cluster traversal	fat12.c
Printing output format	diskinfo.c etc
CLI argument handling	each program*/