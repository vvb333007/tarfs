# tarsum

`tarsum` is a small command-line utility for adding **CRC64/ECMA-182 checksums** in TAR 
archives.

It is designed to work with **TarFS** and stores the checksum directly in the unused padding area of the 
TAR header, without changing the standard TAR file structure.


# Compiling on Linux and Windows

Run:

```sh
make tarsum
```

**NOTE:** If you are building on Windows, you will need to have Cygwin installed.

## Usage

Calculate and embed checksums into a TAR archive:

```text
tarsum input.tar   --> creates "output.tar"
```

or

```text
tarsum input.tar my_fs.tar    -->creates my_fs.tar
```


The resulting TAR archive contains CRC64 checksums that can later be verified by TarFS. Source TAR archive is not modified, a new file is created.

## How It Works

A TAR header is 512 bytes long and contains unused padding space in its last 12 bytes.

`tarsum` uses the first 12 bytes of this space to store an 8-byte CRC64/ECMA-182 checksum along with a small marker:

```text
[ 0x00 ]['C']['6']['4'][ 8-byte CRC64/ECMA-182 checksum ]
```

The `C64` ASCII signature is used for visual inspection only and is ignored by TarFS.

Conceptually:

```text
┌───────────────────────────────┐
│          TAR Header           │
│                               │
│  File metadata                │
│  File name                    │
│  File size                    │
│  ...                          │
│                               │
│  [ CRC64 checksum ]           │
└───────────────────────────────┘
              │
              ▼
          File data
```

TarFS can detect the embedded checksum and verify the integrity of the corresponding data (see src/config.h , CONFIG_TARFS_INTEGRITY option).

If the data has been corrupted, the integrity check fails and TarFS reports an I/O error (specifically `errno` is set to `EIO`).
If filesystem metadata is corrupted (e.g. file name is corrupted) then TarFS report `ENOENT` standart code instead of `EIO`

## Why CRC64?

CRC64 provides a compact and fast integrity check suitable for embedded systems.
