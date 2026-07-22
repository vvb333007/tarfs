On ESP32S3 and P4 these optimized versions of tarfs_os_memcpy() can be used: 

  1. copy one .S file to the library source directory (../../src)
  2. uncomment CONFIG_TARFS_HAVE_OPTIMIZED_MEMCPY macro in src/config.h