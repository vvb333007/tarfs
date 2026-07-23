/*
 * TARFS Filesystem Statistics Example
 *
 * This sketch demonstrates how to retrieve filesystem statistics from
 * a mounted TARFS filesystem using the POSIX statvfs() API.
 *
 * The example initializes TARFS, mounts a TAR filesystem stored in the
 * "tarfs" flash partition at the "/My_FS" mount point, and periodically
 * prints filesystem statistics to the serial console.
 *
 * In addition to standard statvfs() fields, TARFS provides filesystem-
 * specific statistics such as the number of files, links and directories,
 * the amount of data read through read()/pread(), the amount of data
 * accessed through mmap(), the number of filesystem failures, and the
 * amount of RAM used by the filesystem.
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

  struct statvfs st;

  if (statvfs("/My_FS", &st) == 0) {

    Serial.printf(" Filesystem ID: %u\r\n",st.f_fsid); 
    Serial.printf("----------------\r\n"); 
    Serial.printf(" Filesystem block size: %u\r\n",st.f_bsize);
    Serial.printf(" Fragment size: %u\r\n",st.f_frsize);   
    Serial.printf(" Number of blocks: %u\r\n",st.f_blocks);   
    Serial.printf(" Maximum filename length: %u\r\n",st.f_namemax);  

    Serial.printf(" Bad/unrecognized blocks: %u\r\n",st.f_badblocks); 

    Serial.printf(" Number of files: %u\r\n",st.f_files);
    Serial.printf(" Number of links: %u\r\n",st.f_links); 
    Serial.printf(" Number of directories: %u\r\n",st.f_dirs);  

    Serial.printf(" Total bytes read()+pread(): %llu\r\n",st.f_bread);
    Serial.printf(" Total bytes mmap(): %llu\r\n",st.f_bmmap);
    Serial.printf(" Total number of failures: %lu\r\n",st.f_nfail);

    Serial.printf("   Total RAM used by the FS: %u \r\n",st.f_ram);   
  } else
    Serial.printf("Failed to statvfs()\r\n");

  return -1;
}

