/*
 * TARFS Example Sketch
 *
 * This sketch demonstrates basic usage of the TARFS filesystem library
 * on an Arduino-compatible platform.
 *
 * The example initializes TARFS, mounts a TAR filesystem stored in the
 * "tarfs" flash partition at the "/My_FS" mount point, and then reads
 * a file from the mounted filesystem using the standard POSIX file API.
 *
 * The TARFS image must be stored in a flash partition named "tarfs".
 * The partition table should therefore contain a partition with this name
 * and a size large enough to hold the TAR archive.
 *
 * The example demonstrates the following basic operations:
 *
 *   - Initialize the TARFS library with tarfs_init().
 *   - Mount a TAR filesystem with tarfs_mount().
 *   - Open a file using open().
 *   - Read file contents using read().
 *   - Close the file using close().
 *
 * TARFS provides a read-only, POSIX-like filesystem interface for files
 * stored in a TAR archive. Once mounted, files can be accessed using
 * standard file-descriptor-based APIs.
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
  // Read a file using the POSIX API.
  //
  puts("\n--- TEST1: Reading file ---");

  int fd = open("/My_FS/untar/src1/tarfs.h", O_RDONLY);

  if (fd < 0) {
    puts("open() failed");
    return;
  }

  char buf[128];
  ssize_t n;

  while ((n = read(fd, buf, sizeof(buf))) > 0)
    fwrite(buf, 1, n, stdout);

  close(fd);

}
