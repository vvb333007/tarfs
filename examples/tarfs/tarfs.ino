#include <Arduino.h>
#include "tarfs.h"



void setup() {

  Serial.begin(115200);


  // Initialize TARFS library
  tarfs_init();

  // Mount the filesystem. Partition name="tarfs". Mountpoint is "/My_FS"
  //
  // The mount point may be NULL, in which case TARFS will determine
  // it automatically from the archive contents.
  int fs_idx = tarfs_mount(partition_name, "/My_FS", NULL, NULL);

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

  int fd = open("/My_FS/list/example.c", O_RDONLY);
  if (fd < 0) {
    puts("open() failed");
    return;
  }

  char buf[128];
  ssize_t n;

  while ((n = read(fd, buf, sizeof(buf))) > 0)
    fwrite(buf, 1, n, stdout);

  close(fd);

  //
  // List directory contents.
  //
  puts("\n\n--- TEST2: Directory listing ---");

  DIR *dir = opendir("/My_FS/list");
  if (dir) {

    struct dirent *de;

    while ((de = readdir(dir)) != NULL)
      printf("%s\n", de->d_name);

    closedir(dir);
  }

  //
  // Access a file using mmap().
  //
  puts("\n--- TEST3: mmap() example ---");

  fd = open("/My_FS/list/example.c", O_RDONLY);

  if (fd >= 0) {

    struct stat st;

    if (fstat(fd, &st) == 0) {

      void *ptr = mmap(NULL,
                       st.st_size,
                       PROT_READ,
                       MAP_SHARED,
                       fd,
                       0);

      if (ptr != MAP_FAILED) {

        printf("mmap() succeeded\r\n");

        munmap(ptr, st.st_size);
      }
    }

    close(fd);
  }

}

