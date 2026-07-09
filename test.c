#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/dirent.h>

#include "fs.h"
#include "tar.h"
#include "dir.h"
#include "fnv1a.h"
#include "inode.h"
#if 1
int main(int argc, char **argv) {

    const char *filename    = argc > 1 ? argv[1] : "tarfs.tar";
    const char *rebase_link = "/\?\?/D:/Arduino/dev";
#if 0
    void *os_handle         = NULL;
    size_t size             = 0;
    unsigned char *buf;
#endif

    


    int err = tarfs_mount(filename, "/jopa", rebase_link);

    printf("tarfs: mounting resource '%s', err = %d\r\n", filename, err);



    DIR *dir = tard_opendir((0), "/Гарвульзепа/Зыка/");


    log("dir=%p\r\n", dir);

    if (dir != NULL)
      tard_closedir((0), dir);

    tarfs_unmount("/jopa");
    
    
    
#if 0
    printf("tarfs: mounting resource '%s':\n", filename);
    buf = tarfs_os_map_tarfile(filename, &os_handle, &size);

    if (buf == NULL) {
      printf("tarfs: failed to mmap() '%s', err=%d\n", filename, errno);
      return -1;
    }

    printf("tarfs: resource '%s' is mapped (%ld bytes), VADDR=%p\n", filename, size, buf);


    struct tarfs_fs fs = {0};

    int err = inode_mount(&fs,buf,size,rebase_link);
    printf("inode_mount() : err = %d\r\n",err);

    inode_unmount(&fs, buf, size);

    printf("tarfs: unmap the filesystem blob\r\n");
    tarfs_os_unmap_tarfile(os_handle, buf, size);
#endif
    return 0;
}
     #endif