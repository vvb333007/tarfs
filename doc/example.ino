#include <Arduino.h>
#include "tarfs.h"

void setup() {

  Serial.begin(115200);
  delay(500);

  
 
  const char *filename = "ffat";  // Flash partition name. My TAR file was stored in a partition
                                  // that previously contained a FAT filesystem.

  // Sometimes the tar utility puts absolute paths into the archive.
  // If this happens, you can specify a prefix to remove.
  // For example, sometimes my archives contain paths like:
  // "/??/D:/Arduino/dev/..."
  const char *rebase_link = "/\?\?/D:/Arduino/dev";  

  // Initialize TARFS (call once)
  tarfs_init();

  // Multiple filesystems can be mounted at the same time.
  // The mount point can be omitted; in this case it will be automatically
  // calculated from the TAR archive contents.
  int err = tarfs_mount(filename, "/My_FS", rebase_link, NULL);

  printf("tarfs: mounting resource '%s', err = %d\r\n", filename, err);

    
  // int fd = open("/My_FS/list", O_RDONLY|O_DIRECTORY);
  // DIR *dir = fdopendir(fd);
  // DIR *dir2 = opendir("/My_FS/list");

  // Open a file and play with it
  int fd = open("/My_FS/list/example.c", O_RDONLY);

  // Open the same file again using fopen()
  FILE *fp = fopen("/My_FS/list/example.c", "rb");

  char ch;
  puts("1, SEEK_END");

  /* Move position to EOF */
  lseek(fd, 1, SEEK_END);

  while(read(fd, &ch, 1) == 1)
    putchar(ch);

  /* Move position before the beginning of the file */
  puts("-1, SEEK_SET");
  lseek(fd, -1, SEEK_SET);

  while(read(fd, &ch, 1) == 1)
    putchar(ch);

  /* Move position far before the beginning of the file */
  puts("-4000, SEEK_CUR");
  lseek(fd, -4000, SEEK_CUR);

  while(read(fd, &ch, 1) == 1)
    putchar(ch);

  close(fd);

  /* We forgot to close fp, so unmount will be deferred */
  tarfs_unmount("/My_FS"); 

  /* The actual unmount happens here */
  fclose(fp); 
}


void loop() {

  delay(1000);
}