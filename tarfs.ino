
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

    // Откроем файлик и будем с ним играться
    int fd = open("/My_FS/list/example.c", O_RDONLY);
    FILE *fp = fopen("/My_FS/list/example.c", "rb");

    char ch;
    puts("1, SEEK_END");
    lseek(fd, 1, SEEK_END);

    while(read(fd, &ch, 1) == 1)
      putchar(ch);

    puts("-1, SEEK_SET");
    lseek(fd, -1, SEEK_SET);

    while(read(fd, &ch, 1) == 1)
      putchar(ch);

    puts("-4000, SEEK_CUR");
    lseek(fd, -4000, SEEK_CUR);

    while(read(fd, &ch, 1) == 1)
      putchar(ch);

    close(fd);

    tarfs_unmount("/My_FS"); // мы забыли закрыть fp, unmount будет отложен
    fclose(fp); // а вот тут произойдет unmount
}


void loop() {



  delay(1000);
}
