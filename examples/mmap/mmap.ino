/*
 * TARFS mmap() Example
 *
 * This sketch demonstrates how to access a file stored in a TARFS
 * filesystem using the standard POSIX mmap() API.
 *
 * The example initializes TARFS, mounts a TAR filesystem stored in the
 * "tarfs" flash partition at the "/My_FS" mount point, opens a file,
 * obtains its size using fstat(), and maps the file contents directly
 * into the process address space using mmap().
 *
 * After a successful mmap(), the file descriptor can be closed while
 * the mapped memory remains accessible. The file contents are then
 * accessed directly through the memory mapping and the mapping is
 * released with munmap() when it is no longer needed.
 *
 * This demonstrates TARFS's mmap-oriented design, which allows file
 * contents to be accessed directly from the underlying TAR image without
 * copying the file data into a separate buffer.
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
  // Access a file using mmap().
  //
  puts("\n--- TEST1: mmap() example ---");

  int fd = open("/My_FS/untar/src/tarfs.h", O_RDONLY);

  if (fd >= 0) {

    struct stat st;

    if (fstat(fd, &st) == 0) {

      char *ptr = (char *)mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

      // Can be closed
      close(fd);

      if (ptr != MAP_FAILED) {

        Serial.printf("mmap() succeeded!\r\n");

        for (int i=0; i < st.st_size; i++)
          Serial.printf("%c", ptr[i] );

        munmap(ptr, st.st_size);

        return ;
      }
    }
    Serial.printf("fstat() failed\r\n");
    close(fd);
  }
  Serial.printf("Test failed\r\n");
}

