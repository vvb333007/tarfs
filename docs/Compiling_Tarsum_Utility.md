### Linux and Windows

Copy `tarsum.c` into the `src/` directory. Also copy `docs/Makefile` and `docs/os_cygwin.c` into the same directory.
Check that CONFIG_TARFS_HAVE_FDOPENDIR is commented out, otherwise you can get compilation errors

Run:

```sh
make tarsum
```

If you are building on Windows, you will need to have Cygwin installed.
