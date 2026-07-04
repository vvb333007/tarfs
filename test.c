#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "fs.h"
#include "tar.h"
#include "fnv1a.h"
#include "inode.h"

int main(int argc, char **argv) {

    const char *filename    = argc > 1 ? argv[1] : "tarfs.tar";
    void *os_handle         = NULL;
    size_t size             = 0;
    const char *rebase_link = "/\?\?/D:/Arduino/dev";
    char base_dir[100];
    int nino;

    unsigned char *buf;


    printf("tarfs: mounting resource '%s':\n", filename, size);
    buf = tarfs_os_map_tarfile(filename, &os_handle, &size);

    if (buf == NULL) {
      printf("tarfs: failed to mmap() '%s', err=%d\n", filename, errno);
      return -1;
    }

    printf("tarfs: resource '%s' is mapped (%ld bytes), VADDR=%p\n", filename, size, buf);

    // PASS1: count inodes, count all required memory
    printf("tarfs: PASS1, analyzing..\n");
    nino = tar_getnino(buf, size);

    printf("tarfs: %u inodes, expected RAM usage: %u bytes of RAM\n",nino, sizeof(struct tarfs_fs) + nino * (sizeof(struct tarfs_inode) + sizeof(struct tarfs_inode *)));

    // PASS2: guess TARFS root (will be the mountpoint)
    printf("tarfs: PASS2, analyzing..\n");
    if (false == tar_rootdir(buf, size, base_dir, sizeof(base_dir)))
      base_dir[0] = '\0';

    printf("tarfs: found root prefix '%s' \n", base_dir);

    struct tarfs_inode **index = inode_alloc( nino );
    struct tarfs_inode *inodes = (struct tarfs_inode *)(index + nino);

    // PASS3: populate inodes
    printf("tarfs: PASS3, populating inodes..\n");
    inode_populate(inodes, nino, buf, size, rebase_link, base_dir);


    /* Sort inode index table (pointers to inodes are sorted by inode's hash value)
     * so inode_lookup() can be used
     */
    printf("tarfs: building binary search index..\n");
    inode_sort(index, nino);

    /*Resolve symlinks and hardlinks; For inodes which can not be resolved to a valid type5 or type0
    * tar entries, the corresponding ->in_dvaddr is set to NULL, indicating that this inode has no valid data.
    * Resolve other dpendencies; Free tmp memory used by linkpath strings. Link path pointer is placed to the in_next
    * by the code above and is freed by the inode_resolve().
    * 
    */
    printf("tarfs: symlinks and hardlinks resolution..\n");
    inode_resolve(index, nino);

    /* 
     * Perform alphasorting by the in_path; index is not changed, only ->in_next is manipulated
     * to build an alphasorted list of entries.
     * root
     */
    printf("tarfs: lexigraphical sorting..\n");
    struct tarfs_inode *root = inode_alphasort(inodes, nino);

    if (root != NULL) {
      
      printf("tarfs: root inode is exp=<2a0c975e>, real=<%08x> \"%s\"\r\n",root->in_hash, (const char *)root->in_path);
      inode_dumppath_sorted(root);

    } else {
      printf("tarfs: WARNING: no root inode after alphasort, opendir() is disabled\r\n");
      inode_dumphash_sorted((struct tarfs_inode const * const * )index, nino);
    }

    /* PUBLISH */

    // fs->fs_ino = index;
    // fs->fs_nino = nino;
    // fs->fs_root = root;
    //

    printf("Free inodes\r\n");
    inode_free(index, nino, (uintptr_t )buf, size);

    tarfs_os_unmap_tarfile(os_handle, buf, size);
    return 0;
}
