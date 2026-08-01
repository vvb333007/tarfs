#include "tarfs.h"



int main(int argc, char **argv) {

    const char *filename    = argc > 1 ? argv[1] : "filesystem.tar";
    const char *rebase_link = "/\?\?/D:/Arduino/tarfs/untar";


    tarfs_init();

    tarfs_integrity(0);
    tarfs_logging(1);

    
    int err = tarfs_mount(filename, NULL, NULL, NULL);

    printf("tarfs: mounting resource '%s', err = %d\r\n", filename, err);

    tarfs_dump(err);

//    tarfs_integrity_on_open(err, 1);


    DIR *dir = tard_opendir((0), "/untar/src1/");
    struct dirent *ent;
    while((ent = tard_readdir(0, dir)) != NULL) {
      printf("READDIR: type=%d, '%s'\r\n", ent->d_type, ent->d_name);
    }

    if (dir != NULL)
      tard_closedir((0), dir);



    int fd = tarf_open(0, "/untar/src/tarfs.h", O_RDONLY, 0);
    char ch;
    while(tarf_read(0, fd, &ch, 1) == 1)
      putchar(ch);

    tarf_close(0, fd);

    tarfs_unmount("/tarfs");
    return 0;
}
