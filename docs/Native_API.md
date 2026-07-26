## Native API

On systems that do not provide a Virtual File System (VFS), the standard file I/O functions such as `open()` and `read()` may be unavailable or tightly coupled to the underlying storage system.

To make TARFS portable to such environments, TARFS provides a **Native API** that mirrors the familiar POSIX-like file and directory operations:

```text
open()     → tarf_open()
close()    → tarf_close()
opendir()  → tard_opendir()
readdir()  → tard_readdir()
mmap()     → tarf_mmap()
ioctl()    → tarf_ioctl()
```

and so on.

The main difference in the function signatures is that every Native API function takes one additional parameter: the **file system identifier** as its first argument.

This identifier is the value returned by `tarfs_mount()`.

For example:

```c
int fd = tarf_open(fs_id, "/index.html", O_RDONLY);
```

If only a single TARFS instance is mounted, `0` (zero) can be used instead of the file system identifier:

```c
int fd = tarf_open(0, "/index.html", O_RDONLY);
```

This allows TARFS to be used directly on bare-metal systems or on embedded platforms without requiring a Virtual File System layer.
