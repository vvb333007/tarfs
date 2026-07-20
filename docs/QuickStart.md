# Creating a TARFS Filesystem Image, Setp by step guide.

TARFS stores the filesystem as a standard POSIX TAR archive.

No special image creation tools are required. A regular GNU `tar` utility is sufficient to create, modify, and extract TARFS images.

## Quick Start

### 1. Create the filesystem root directory

Create a directory that will become the filesystem root. Its name will also become the mount point.

For example: `mkdir tarfs`

```text
tarfs/
```

### 2. Populate the filesystem

Copy the required files, subdirectories, symbolic links, hard links, or (on Windows) directory junctions into this directory.

For maximum performance (both execution speed and memory usage), relatively short file and directory names are recommended (preferably no longer than 100 bytes).

There is no limit on filename length or character encoding. However, short ASCII names produce the most compact and efficient filesystem.

> **Note**
>
> Be careful when creating symbolic links with long UTF-8 names. Depending on the system locale, the `tar` utility may replace non-ASCII characters with `???` inside the archive. This is a limitation of the archiver itself, not of TARFS.

Example filesystem layout:

```text
tarfs/
├── www/
│   └── index.html
└── ftp/
    └── pub/
        ├── drivers.tgz
        ├── docs.tgz
        └── runme!.exe
```

### 3. Create the TAR archive

Run:

```sh
tar -cf tarfile.tar tarfs
```

This creates the archive `tarfile.tar` from the `tarfs` directory.

### 4. Write `tarfile.tar` to the appropriate ESP Flash partition using `esptool`.

On ESP32, the TARFS filesystem is stored in a dedicated Flash memory partition.

To use it, add a corresponding partition to your `partitions.csv` file, then write the TAR archive to that partition using `esptool.py`.

If you are using the Arduino IDE, place the `partitions.csv` file in your project directory, alongside your source code:

![Main window](Sketch_Folder.jpg)

In the Arduino IDE settings, select the **`custom`** partition scheme:

![Arduino IDE Settings](Arduino_IDE_Settings.jpg)


### 4.1 Create a TARFS partition

Add a partition of type `data` to `partitions.csv`.

Example for a 16 MiB Flash device, with approximately 13 MiB allocated to TARFS:

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x300000,
tarfs,    data, 0xF0,    0x310000,0xCE0000,
```

The fields have the following meaning:

| Field       | Description                                                           |
| ----------- | --------------------------------------------------------------------- |
| **Name**    | Partition name. Used when mounting the filesystem.                    |
| **Type**    | Must be `data`.                                                       |
| **SubType** | Any subtype may be used for `data` partitions. `0xF0` is recommended. |
| **Offset**  | Flash address of the partition.                                       |
| **Size**    | Maximum TAR archive size.                                             |

The partition size must be at least as large as the TAR archive.

Put your `partitions.csv` to your source code directory

### 4.2 Write the image to Flash

Write the archive into the TARFS partition:

```sh
esptool.py --chip esp32 --port COM5 --baud 921600 write_flash 0x310000 tarfile.tar
```

on Windows,

or

```sh
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash 0x310000 tarfile.tar
```

on Linux.

Replace:

* `COM5` or `/dev/ttyUSB0` with the serial port name;
* `0x310000` with the partition offset specified in `partitions.csv`.

---

### 5. What's Next?

Now it's time to write your own sketch.

Don't forget to add:

```cpp
#include "tarfs.h"
```

to your `.ino` file, and call `tarfs_init()` and `tarfs_mount()` from your `setup()` function, just as shown in the example sketch `examples/tarfs/tarfs.ino`.

If you have made it this far, your chances of success are pretty good.


# Additional features:

## Selecting the Mount Point

The mount point is normally determined automatically from the archive contents, but it may also be specified explicitly.

**IMPORTANT:** if the filesystem is corrupted, automatic mount point detection may fail. Therefore, production systems should always specify the mount point explicitly.

For reliable automatic detection, always create a root directory as described in Step 1 and place all filesystem contents inside it. The name of this directory becomes the mount point.

If this behavior is not desired, the mount point can be overridden when calling `tarfs_mount()`.

---

## Filesystem Integrity Checking

TARFS supports optional filesystem integrity verification while remaining fully compatible with standard TAR archives.

By default, only TAR headers (inode metadata) are protected. Each TAR header already contains the standard TAR checksum, allowing TARFS to detect corrupted metadata during mounting without introducing any format extensions.

For stronger protection, the `tarsum` utility may be used:

```sh
./tarsum input.tar [output.tar]
```

Instructions for building the `tarsum` utility can be found in [Compiling the Tarsum Utility](../tarsum/Compiling_Tarsum_Utility.md).

It stores an additional 8-byte hash in the unused padding area of every TAR header.

The hash is based on CRC64/ECMA-182 and remains completely transparent to standard TAR utilities, since these bytes are ignored by the TAR format.

When mounting an archive processed by `tarsum`, TARFS automatically detects the embedded hashes and verifies filesystem integrity.

Integrity verification can be disabled if minimizing mount time is more important. A typical workflow is to run tarsum on each newly created TAR archive immediately before flashing it to the device with `esptool.py`.

**IMPORTANT!** After flashing the CRC64-enabled TAR archive to flash, make sure to uncomment
`#define CONFIG_TARFS_INTEGRITY 1` in `src/config.h`.

If this option is left disabled, no integrity verification will be performed.

**IMPORTANT!** When `CONFIG_TARFS_INTEGRITY` is enabled, TARFS expects the filesystem image to
contain embedded CRC64 checksums. Images without checksums will be treated as corrupted.


Integrity verification can also be started manually using the `tarfs_fsck()` API:

```c
tarfs_fsck(const char *partition_label);
```


---

## Path and Link Rebasing

Depending on the platform and the version of `tar`, the archive may contain absolute paths or absolute symbolic link targets.

For example, instead of

```text
tarfs/file.txt
```

the archive may contain

```text
/home/user/work/project/tarfs/file.txt
```

In this case, TARFS would treat `/home` as the filesystem root instead of `tarfs`.

A similar situation may occur on Windows (especially when using Cygwin), where symbolic links may look like

```text
/??/C:/Users/John/tarfs/link_name
```

instead of

```text
tarfs/link_name
```

To handle this, TARFS provides an optional mount parameter:

* `link_rebase`

It specifies a path prefix that will be removed while mounting the filesystem.

For example,

```text
link_rebase = "/??/C:/Users/John"
```

converts

```text
/??/C:/Users/John/tarfs/link
```

into

```text
/tarfs/link
```


---

# Common Mistakes & Security Notes

Do not make the partition significantly larger than the actual TAR archive. TARFS attempts to mount everything within the specified flash range. This behavior is intentional: it improves reliability when the filesystem is damaged (for example, due to a bad flash sector), but it also causes TARFS to spend additional time scanning unrelated data.

If you overwrite a smaller TAR archive over a previously flashed larger one, make sure the partition size matches the new archive exactly. Otherwise, TARFS may interpret leftover data from the previous archive as valid TAR headers and mount files that should no longer exist.

Always check function return values. If an operation fails, inspect `errno` to determine the cause.

If TARFS does not work correctly or behaves unexpectedly, consider enabling verbose logging by setting `CONFIG_TARFS_LOG` in `config.h`. This produces extensive debug output, including API calls, return values, and detailed information about the mounting process.

For example, verbose logs may contain messages about failed symbolic link normalization, such as a *"floating link"* warning with a path like:

```
/??/D:/Users/John/tarfs/file.txt
```

This usually indicates that the `link_rebase` parameter passed to `tarfs_mount()` needs to be adjusted. For example, setting it to:

```
/??/D:/Users/John/
```

removes the absolute path prefix, leaving only valid relative paths inside the archive.
