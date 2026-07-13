#include <Arduino.h>
#include "espshell.h"
#include "tarfs.h"

void setup() {

  Serial.begin(115200);
  delay(500);

  // Name of the flash partition containing the TAR archive.
  // In this example, the archive was written into a partition
  // previously used as a FAT filesystem.
  const char *partition_name = "ffat";

  // Some versions of tar may store absolute paths in the archive.
  // If this happens, specify the path prefix to strip during mount.
  //
  // Example:
  // const char *rebase_link = "/\\?\\?/D:/Arduino/dev";
  const char *rebase_link = NULL;

  // Initialize TARFS (call once).
  tarfs_init();

  // Mount the filesystem.
  //
  // The mount point may be NULL, in which case TARFS will determine
  // it automatically from the archive contents.
  int err = tarfs_mount(partition_name, "/My_FS", rebase_link, NULL);

  printf("tarfs: mounting resource '%s', err = %d\r\n", partition_name, err);

  //
  // Read a file using the POSIX API.
  //
  puts("\n--- Reading file ---");

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
  puts("\n\n--- Directory listing ---");

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
  puts("\n--- mmap() example ---");

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

void loop() {
  delay(1000);
}
