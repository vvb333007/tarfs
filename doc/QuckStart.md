# Filesystem Image Format

TARFS stores its filesystem as a standard POSIX TAR archive.

No special image creation tools are required—the regular GNU `tar` utility is sufficient for creating, modifying, and extracting TARFS images.

# Creating a TARFS Filesystem Image

TARFS uses the standard GNU `tar` utility to create and modify filesystem images.

## Quick Start

### 1. Create the filesystem directory

Create a directory that will become the filesystem root. Its name will also become the filesystem mount point.

For example:

```text
tarfs/
```

### 2. Populate the filesystem

Copy your files, directories, symbolic links, hard links, or (on Windows) directory junctions into this directory.

For the best performance (both speed and memory usage), use relatively short file and directory names (preferably under 100 bytes).

There are no restrictions on filename length or character encoding, but short ASCII names produce the most compact and efficient filesystem.

> **Note**
>
> Be careful when creating long UTF-8 symbolic link names. Depending on the system locale, the `tar` utility may replace non-ASCII characters with `???` inside the archive. This is a limitation of the archiver, not of TARFS.

Example filesystem:

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

This creates `tarfile.tar` from the `tarfs` directory.

### 4. Flash the archive

Write `tarfile.tar` to the appropriate ESP partition using `esptool`.

---

# Link and Path Name Rewriting

Depending on the platform and the version of `tar`, the archive may contain absolute path names or absolute link targets.

For example, instead of storing:

```text
tarfs/file.txt
```

the archive may contain:

```text
/home/user/work/project/tarfs/file.txt
```

This would cause TARFS to treat `/home` as the filesystem root instead of `tarfs`.

A similar problem may occur under Windows (especially in Cygwin environments), where symbolic links may look like:

```text
/??/C:/Users/John/tarfs/link_name
```

instead of:

```text
tarfs/link_name
```

To solve this problem, TARFS provides two optional mount parameters:

* `path_rebase`
* `link_rebase`

These parameters specify path prefixes that should be removed while mounting the filesystem.

For example:

```text
link_rebase = "/??/C:/Users/John/"
```

would rewrite

```text
/??/C:/Users/John/tarfs/link
```

into

```text
tarfs/link
```

`path_rebase` works in exactly the same way but affects regular file and directory paths rather than link targets.

---

# Choosing the Mount Point

The mount point is determined automatically from the archive contents.

For this reason, always create the top-level directory first (as described in Step 1), then place all filesystem contents inside it.

The name of this directory becomes the filesystem mount point.

---

# Data Integrity

TARFS supports optional filesystem integrity verification while remaining fully compatible with standard TAR archives.

By default, TARFS protects only TAR headers (inode metadata). Each header contains its standard TAR checksum, allowing TARFS to detect damaged metadata during mounting without introducing any TARFS-specific extensions.

For stronger protection, the `gentarsum` utility can be used:

```sh
./gentarsum tarfs.tar
```

The utility injects an additional 8-byte hash into the unused padding area of each TAR header. The hash is derived from an MD5 digest and remains completely invisible to standard TAR utilities, since those padding bytes are ignored by the TAR format.

The choice of an MD5-based hash is intentional. TARFS is primarily designed for ESP32-family microcontrollers, all of which provide an optimized MD5 implementation in on-chip ROM. This eliminates the need to link an external hashing library while keeping checksum generation and verification fast and lightweight.

When mounting an archive processed by `gentarsum`, TARFS automatically detects the embedded hashes and verifies filesystem integrity. This verification is optional and may be disabled if faster mount times are preferred.

A complete integrity check can also be performed at any time by calling:

```c
tarfs_fsck(const char *partition_label);
```

Unlike the mount-time verification, `tarfs_fsck()` validates both the filesystem metadata and the contents of every file.

---

# Recovery from Corruption

Unlike traditional filesystems that depend on a central superblock, TARFS treats the archive as a sequential tape.
 rather than a monolithic filesystem image.

If a damaged TAR header is encountered, mounting does not immediately fail. Instead, TARFS scans forward until it finds the next valid TAR header and continues processing the remaining archive.

As a result, even severely damaged filesystem images often remain partially usable: corrupted files become inaccessible, while all intact files located after the damaged region can still be accessed.
