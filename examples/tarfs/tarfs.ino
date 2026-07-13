
#include <Arduino.h>
#include "tarfs.h"

void setup() {

  Serial.begin(115200);
  delay(500);

  
 
  const char *filename    = "ffat";  // название раздела на флешке. у меня тарфайл был зашит в раздел, в котором раньше была FAT

  // Если тар утилита напихала (такое бывает) абсолютных путей в архив, то можно 
  // задать эту переменную, чтобы обрезать лишнее. У меня, например, иногда приклеивается "/??/D:/Arduino/dev"
  const char *rebase_link = "/\?\?/D:/Arduino/dev";  

  // Вызываем однажды
  tarfs_init();

  // Можно монтировать несколько файловых систем, но мы смонтируем одну
  // точку монтирования можно не указывать, в таком случае точка монтирования будет вычислена из содержимого тар-архива
  int err = tarfs_mount(filename, "/My_FS", rebase_link, NULL);

  printf("tarfs: mounting resource '%s', err = %d\r\n", filename, err);

    
    // int fd = open("/My_FS/list", O_RDONLY|O_DIRECTORY);
    // DIR *dir = fdopendir(fd);
    // DIR *dir2 = opendir("/My_FS/list");
    // void *t = mmap(NULL, 2000, PROT_READ, MAP_SHARED, fd, 0);

    // Откроем файлик и будем с ним играться
    int fd = open("/My_FS/list/example.c", O_RDONLY);

    // И еще разок, но через fopen()
    FILE *fp = fopen("/My_FS/list/example.c", "rb");

    char ch;
    puts("1, SEEK_END");

    /* Позиционируемся на EOF*/
    lseek(fd, 1, SEEK_END);

    while(read(fd, &ch, 1) == 1)
      putchar(ch);

    /* Позиционируемся за пределы файла*/
    puts("-1, SEEK_SET");
    lseek(fd, -1, SEEK_SET);

    while(read(fd, &ch, 1) == 1)
      putchar(ch);

    /* Позиционируемся за пределы файла*/
    puts("-4000, SEEK_CUR");
    lseek(fd, -4000, SEEK_CUR);

    while(read(fd, &ch, 1) == 1)
      putchar(ch);

    close(fd);

    /* мы забыли закрыть fp, unmount будет отложен */
    tarfs_unmount("/My_FS"); 

    /* а вот тут произойдет unmount */
    fclose(fp); 
}


void loop() {



  delay(1000);
}

