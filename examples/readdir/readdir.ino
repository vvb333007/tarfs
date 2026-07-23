/*
 * TARFS Directory Listing Example
 *
 * This sketch demonstrates how to list the contents of directories
 * stored in a TARFS filesystem using the standard POSIX directory API.
 *
 * The example initializes TARFS, mounts a TAR filesystem stored in the
 * "tarfs" flash partition at the "/My_FS" mount point, and periodically
 * lists the contents of two directories using opendir(), readdir(),
 * and closedir().
 *
 * The second directory is accessed through a hard link, demonstrating
 * that TARFS correctly resolves hard-linked directories and allows
 * their contents to be enumerated through the standard directory API.
 *
 * The example filesystem (filesystem.tar) must be flashed to the "tarfs"
 * partition before running this sketch.
 *
 * See docs/QuickStart.md for instructions.
 */

#include <Arduino.h>
#include "tarfs.h"

const char *partition_name = "tarfs";
const char *link_rebase = NULL;

void setup() {

  Serial.begin(115200);

  // Initialize TARFS library
  tarfs_init();

  // Mount the filesystem. Partition name="tarfs". Mountpoint is "/My_FS"
  int fs_idx = tarfs_mount(partition_name, "/My_FS", link_rebase, NULL);

  if (fs_idx < 0)
    Serial.printf("TARFS: partition '%s' NOT mounted\r\n", partition_name);
  else
    Serial.printf("TARFS: partition '%s' mounted, FS index is %d\r\n", partition_name, fs_idx);
}

void loop() {

  delay(1000);

  //
  // List directory contents.
  //
  puts("\n\n--- TEST2: Directory listing ---");

  DIR *dir = opendir("/My_FS/untar/src1");
  if (dir) {

    struct dirent *de;

    while ((de = readdir(dir)) != NULL)
      printf("%s\n", de->d_name);

    closedir(dir);
  }
  //
  // List directory contents referenced by a hardlink
  //
  puts("\n\n--- TEST2: Hardlink Directory listing ---");

  dir = opendir("/My_FS/untar/src");
  if (dir) {

    struct dirent *de;

    while ((de = readdir(dir)) != NULL)
      printf("%s\n", de->d_name);

    closedir(dir);
  }

}
