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

void tar_addsum(uint8_t *tar_start, size_t tar_length) {

    uint32_t size;
    size_t off = 0, hdr_no = 0;
    int bad = 0, bad_total = 0;
    uintptr_t tar_end = (uintptr_t )(tar_start + tar_length);

    while (off + sizeof(tarhdr_t) <= tar_length) {

        tarhdr_t *hdr = (tarhdr_t *)(tar_start + off);

        if (tar_badhdr(hdr)) {

          if (!bad) {
bad_header:
            bad++;
            
          }

          off += sizeof(tarhdr_t);
          continue;
        }

        if (bad) {
          bad_total += bad;
          bad = 0;
        }

        size = tar_octal(hdr->size, sizeof(hdr->size));

        /* Check if size is sane: current pointer + 512 bytes + size must be < tar_end */
        if (((uintptr_t)(hdr + 1)) + size >= tar_end) {
          goto bad_header;
        }

        /* For entries having data (including PAX headers) we calculate CRC64 and inject it
         * into header->padding[] field.
         *
         */
        if (size > 0) {

          void const *data = (void *)(hdr + 1);

          uint64_t icv = hash64(0, data, size);

          for (int i = 0; i < 8; i++) {

            uint8_t octet;
            octet = icv & 0xff;
            icv >>= 8;
            hdr->digest[i] = octet;
          }

          hdr->md[0] = 'C';
          hdr->md[1] = '6';
          hdr->md[2] = '4';

          hdr->zero = 0;

          uint32_t new_sum = tar_hdrsum(hdr);
          char tmp[9] = { ' ',' ',' ',' ',' ',' ',' ',' ',' '};
          snprintf(tmp, sizeof(tmp), "%-8o", new_sum);
          memcpy(hdr->checksum, tmp, 8);
        }
  
        off += sizeof(tarhdr_t);


        /* Real size is 512 bytes aligned */
        off += ((size_t)size + 511) & ~511u;

        hdr_no++;
    }
}


int main(int argc, char **argv) {

    void *os_handle;
    size_t size;

    const char *filename    = argc > 1 ? argv[1] : "tarfs.tar";
    const char *filename2    = "out.tar";

    unsigned char *buf = (unsigned char *)tarfs_os_map_tarfile(filename, &os_handle, &size);

    if (buf == NULL) {
      printf("tarfs: failed to mmap() '%s', err=%d\n", filename, errno);
      return -1;
    }
    printf("tarfs: resource '%s' is mapped (%ld bytes), VADDR=%p\n", filename, size, buf);

    tar_addsum(buf, size);

    FILE *f = fopen(filename2, "wb");

    if (f != NULL) {

      fwrite(buf, size, 1, f);
      fclose(f);
      printf("file '%s' has been created\r\n", filename2);      

    }


    printf("tarfs: unmap the filesystem blob\r\n");
    tarfs_os_unmap_tarfile(os_handle, buf, size);

    return 0;
}
