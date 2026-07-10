#include "tarfs.h"


#if 1
int main(int argc, char **argv) {

    const char *filename    = argc > 1 ? argv[1] : "tarfs.tar";
    const char *rebase_link = "/\?\?/D:/Arduino/dev";


    tarfs_init();

#if 0
    void *os_handle         = NULL;
    size_t size             = 0;
    unsigned char *buf;
#endif

    


    int err = tarfs_mount(filename, "/jopa", rebase_link);

    printf("tarfs: mounting resource '%s', err = %d\r\n", filename, err);


#if 0
    DIR *dir = tard_opendir((0), "/Гарвульзепа/Чучундровна/");


    log("dir=%p\r\n", dir);

    struct dirent *ent;
    while((ent = tard_readdir(0, dir)) != NULL) {
      printf("READDIR: type=%d, '%s'\r\n", ent->d_type, ent->d_name);
    }

    if (dir != NULL)
      tard_closedir((0), dir);
#endif
    int fd = tarf_open(0, "/list/example.c", O_RDONLY, 0);
    char ch;
    puts("1, SEEK_END");
    tarf_lseek(0, fd, 1, SEEK_END);
    while(tarf_read(0, fd, &ch, 1) == 1)
      putchar(ch);

    puts("-1, SEEK_SET");
    tarf_lseek(0, fd, -1, SEEK_SET);
    while(tarf_read(0, fd, &ch, 1) == 1)
      putchar(ch);

    puts("-4000, SEEK_CUR");
    tarf_lseek(0, fd, -4000, SEEK_CUR);
    while(tarf_read(0, fd, &ch, 1) == 1)
      putchar(ch);



    tarf_close(0, fd);
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