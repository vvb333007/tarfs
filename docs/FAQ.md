#### [ этот же самый текст, но на русском языке находится здесь](FAQ_RU.md).

# TARFS FAQ

## What is TARFS?

**TARFS** is a read-only filesystem for ESP32 based on the TAR archive format.

TARFS allows you to use a regular `.tar` file as a filesystem image without extracting it. The `.tar` file is written directly to a Flash partition, just like a FAT/SPIFFS/LittleFS image created by the corresponding tools.

After mounting, files inside the filesystem are available through POSIX-like functions:

```c
open()/fopen()
read()/fread()
mmap()
opendir()
```

and others.

---

## Why TAR?

TAR is a sequential archive format with a simple on-disk layout and no central allocation tables or 
metadata structures. Each file header stores the complete pathname, making every archive entry 
self-contained and independently discoverable. 

As a result, corruption of one directory entry or metadata record does not affect access to 
other files, since directory hierarchy is reconstructed from pathnames rather than explicit 
parent-child links. 

TAR archives can also be created and manipulated using standard tools, eliminating the need 
for custom image builders or conversion utilities

All TAR headers are aligned to 512-byte boundaries, making damaged archives straightforward to 
analyze and allowing file headers to be located by scanning the image for valid TAR records..


---
## How fast it is comparing to other filesystems on ESP32?

| File system     | read()¹       |  open()    | opendir() | readdir() | mount time  |
|-----------------|---------------|------------|-----------|-----------|-------------|
| **TarFS**       | **26 MiB/s**¹ | 21..92 µs² | 27..102µs²|   49 µs³  |       66 ms |
| **LittleFS**    |   ~2.24 MiB/s |   5740 µs  | 3182 µs   | 1688 µs   |     1600 ms |

¹ Tested by read()ing from a 4MB file to 1KB buffer in a tight loop

² Smaller numbers were obtained when using DRAM for file index. Larger is for PSRAM. 
  Second call to open() while using PSRAM is as fast as no-PSRAM code.

³ Does not depend on RAM type (DRAM or PSRAM) : the opendir() causes CPU to cache our 
  region of intereset, so subsequent readdir() hits the cache

### TarFS Test Configuration

* **Hardware:** ESP32-S3
* **CPU:** 240 MHz
* **Flash:** 32 MiB, Quad SPI
* **PSRAM:** 16 MiB OPI
* **Filesystem:** 12 MiB TAR archive
* **Entries:** 640 total

  * ~20 directories
  * ~620 HTML files

¹ TarFS sequential read using **1 KiB chunks**.

² TarFS `open()` with the filesystem index stored in **IRAM/cache**.

³ TarFS `open()` with the filesystem index stored in **PSRAM**. Approximately **16 KiB of PSRAM** is used by the index.

---

## How do I start using TARFS?

1. Create a TAR archive with your files (in this example, files are stored in the `www` directory):

```bash
tar cf www.tar www/
```

2. Optionally add CRC64 integrity checking:

```bash
tarsum www.tar
```

3. Flash the TARFS partition into ESP32 Flash.

4. In your sketch/application call:

```c
tarfs_init();
tarfs_mount(Flash_Partition_Name, Mount_Point, NULL, NULL);
```

After that, your files are available through TARFS.

---

## How do I add CRC64 protection to my `.tar` file?

Run the `tarsum` utility:

```bash
tarsum filesystem.tar output.tar
```

Then enable integrity checking by uncommenting the following line in `src/config.h`:

```c
#define CONFIG_TARFS_INTEGRITY 1
```

If `CONFIG_TARFS_INTEGRITY` is disabled, embedded CRC64 checksums will be ignored during mount.

If `CONFIG_TARFS_INTEGRITY` is enabled, TARFS expects CRC64 to present and treats files with no CRC64 as damaged.

The `tarsum` utility can be built on Linux and Windows (Cygwin) by running `make` in the `tarsum` directory.

---

## I ran `tarsum` twice on the same file. Will it still mount?

Yes.

`tarsum` detects existing CRC64 records and updates them instead of creating duplicates.

---

## Is there a simple way to tell whether a TAR archive contains CRC64 checksums?

Yes.

Open the archive in a hex or text editor and search for the ASCII string `"C64"`. A TAR archive processed by `tarsum` will contain multiple `C64` signatures.

> **Note:** The `C64` signature is intended for diagnostic purposes only. TARFS ignores it during mounting and always reads the embedded CRC64 field when `CONFIG_TARFS_INTEGRITY` is enabled.


---

## How do I flash a TARFS image into ESP32?

### Windows:

```sh
esptool.py --chip esp32 --port COM5 --baud 921600 write_flash 0x310000 tarfile.tar
```

### Linux:

```sh
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash 0x310000 tarfile.tar
```

Replace:

* `COM5` or `/dev/ttyUSB0` with your actual ESP32 port.
* `0x310000` with the actual TARFS partition offset from your `partitions.csv` file.

---

## What is partitions.csv?

`partitions.csv` describes the Flash partition layout of your ESP32.

It defines:

* where the application is located;
* where NVS is stored;
* where OTA partitions are located;
* where your TARFS image is stored;
* the size of each partition.

ESP-IDF uses this file to generate the binary partition table.

An example file is included in:

```
examples/tarfs/partitions.csv
```

See the [Espressif Documentation][1] for more information.

---

## Where should partitions.csv be located?

### Arduino IDE

Put `partitions.csv` next to your `.ino` file:

```
MySketch/
│
├── MySketch.ino
│
└── partitions.csv
```

The Arduino ESP32 framework will automatically use this file during compilation.

(Custom CSV partition tables are based on the ESP-IDF partition table mechanism.)

See the [Espressif Documentation][1].

---

### ESP-IDF

Usually, the file is placed in the project root directory:

```
my_project/
│
├── main/
│   └── main.c
│
├── partitions.csv
│
└── CMakeLists.txt
```

In `menuconfig`, select:

```
Partition Table
    Custom partition table CSV
```

and specify the filename.

See the [Espressif Documentation][1].

---

## Do I need to extract the TAR archive before flashing?

No.

TARFS works directly with the TAR archive.

```
Flash
 |
 +-- TARFS partition
       |
       +-- filesystem.tar
              |
              +-- file1.txt
              +-- image.png
              +-- index.html
```

---

## Can I use normal TAR tools?

Yes.

Create an archive:

```bash
tar cf fs.tar directory/
```

List archive contents:

```bash
tar tf fs.tar
```

Extract files:

```bash
tar xf fs.tar
```

---

## Can I write to TARFS?

No.

TARFS is a read-only filesystem.

For writable storage use other filesystems such as NVS, LittleFS, FATFS, or an SD card.

---
## Can I use compressed tar archives?

No. Compression is not supported.

## I followed the documentation, but TARFS does not work. What should I do?

Enable debug logging.

Open:

```
src/config.h
```

and uncomment:

```c
#define CONFIG_TARFS_LOG 1
```

Then rebuild your project.

Additional debug messages will appear in the terminal and may help identify the problem.

---

## How much RAM does TARFS use?

TARFS uses a small in-memory file index (24 byte per filesystem object).

The amount of RAM depends on the number of files stored in the archive.

Typical example: 1000 files — around 24Kbytes;

## Can I use TARFS to mount filesystem from .rodata section of the firmware?

Yes, you can. On ESP32 you can use EMBED_FILES to embed a tar archive which can then be mounter
using tarfs_mount_from_memory() API


## Is TARFS multithread-safe?

Yes,  it is. 


## Is TARFS portable?

Yes. TARFS is written in standard C11, does not use any GCC-specific language extensions, and compiles without modifications in a Cygwin environment.

To port TARFS to another architecture, you need to implement the `os_<arch>.c` file. Examples for ESP32 and STM32 are provided as `os_esp32.c` and `os_stm32.c`. The STM32 implementation is provided primarily to illustrate the porting concept.

The main function that must be implemented is `tarfs_os_map_tarfile()`. This function must return the address where the TAR archive resides. Additionally, it may perform flash-to-RAM address space mapping, as implemented in `os_esp32.c`.

This abstraction allows TARFS to access TAR archives regardless of where the underlying data is physically stored.

---

[1]: https://documentation.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html "Espressif Documentation"
