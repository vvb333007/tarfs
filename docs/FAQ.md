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
tarfs_mount();
```

After that, your files are available through TARFS.

---

## How do I create a TARFS filesystem?

TARFS uses regular TAR archives.

For example:

```bash
mkdir data
echo "Hello" > data/test.txt

tar cf filesystem.tar data/
```

The resulting `filesystem.tar` file can be used as a TARFS filesystem image.

To add CRC64 protection:

```bash
tarsum filesystem.tar output.tar
```

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

---

[1]: https://documentation.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html "Espressif Documentation"
