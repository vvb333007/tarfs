# TARFS FAQ

## Что такое TARFS?

**TARFS** - это read-only файловая система для ESP32, основанная на формате TAR-архивов.
TARFS позволяет использовать обычный `.tar` файл как файловую систему без распаковки - `.tar` файл 
записывается в раздел flash также, как туда записывается файловая система FAT/SPIFFS/LittleFS , созданная 
при помощи соответсвубщих утилит. 

Файлы внутри файловой системы, после монтирования будут доступны через POSIX функции 
(`open()/fopen()`, `read()/fread()`, `mmap()`, `opendir()` и т.д.).

---

## Как начать пользоваться TARFS?

1. Создайте файловую систему - TAR-архив с вашими файлами (в примере файлы расположены в каталоге www):

```bash
tar cf www.tar www/
```

2. При необходимости добавьте проверку целоснтости CRC64:

```bash
tarsum www.tar
```

3. Прошейте TARFS-раздел во Flash ESP32.

4. В скетче вызовите:

```c
tarfs_init();
tarfs_mount();
```

После этого все ваши файлы будут доступны через TARFS.

---

## Как создать TARFS-файловую систему?

TARFS использует обычные TAR-архивы. Можно использовать утилиту `tar`, можно - файловые менеджеры типа `Far` или `Midnight Commander`

Например:

```bash
mkdir data
echo "Hello" > data/test.txt

tar cf filesystem.tar data/
```

Полученный `filesystem.tar` можно использовать как TARFS image.

Для добавления CRC64 защиты:

```bash
tarsum filesystem.tar output.tar
```

---

## Как прошить TARFS image в ESP32?

На Windows:

```sh
esptool.py --chip esp32 --port COM5 --baud 921600 write_flash 0x310000 tarfile.tar
```

На Linux

```sh
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash 0x310000 tarfile.tar
```
Замените:

* `COM5` или `/dev/ttyUSB0` на реальный порт
* `0x310000` на реальное смещение из файла `partitions.csv`.


## Что такое partitions.csv?

`partitions.csv` - это описание расположения разделов Flash ESP32.

В нем указано:

* где находится приложение;
* где NVS;
* где OTA;
* где находится ваш TARFS image;
* размер каждого раздела.

ESP-IDF использует этот файл для создания бинарной таблицы разделов. ([Espressif Documentation][1])
Пример файла находится в examples/tarfs/partitions.csv

---

## Где должен находиться partitions.csv?

### Arduino IDE

Положите `partitions.csv` рядом с вашим `.ino` файлом:

```
MySketch/
│
├── MySketch.ino
│
└── partitions.csv
```

Arduino ESP32 framework автоматически использует этот файл при сборке. (Поддержка пользовательских CSV-разделов основана на механизме таблиц разделов ESP-IDF.) ([Espressif Documentation][1])


### ESP-IDF

Обычно файл находится в корне проекта:

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

В `menuconfig` нужно выбрать:

```
Partition Table
    Custom partition table CSV
```

и указать имя файла. ([Espressif Documentation][1])

---

## Нужно ли распаковывать TAR перед прошивкой?

Нет.

TARFS работает непосредственно с TAR-архивом.

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

## Можно ли использовать обычные TAR-инструменты?

Да.

Создание архива:

```bash
tar cf fs.tar directory/
```

Просмотр содержимого:

```bash
tar tf fs.tar
```

Извлечение:

```bash
tar xf fs.tar
```

---

## Можно ли писать в TARFS?

Нет.

---

## Я все сделал, как написано в документации, но ничего не работает

Отредактируйте файл src/config.h библиотеки tarsf, расскоментируйте строку `#define CONFIG_TARFS_LOG 1`,
пересоберите свой проект. Теперь в терминал будут попадать отладочные сообщения, по которым можно попытаться определить причину неудач


## Сколько RAM занимает TARFS?

TARFS использует небольшой индекс файлов.

Размер зависит от количества файлов.

Например:

* несколько сотен файлов - обычно десятки килобайт RAM;
* тысячи файлов - порядка десятков килобайт.

---




[1]: https://documentation.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html?utm_source=chatgpt.com "Espressif Centralized Documentation Platform (CDP) | Espressif Documentation"
