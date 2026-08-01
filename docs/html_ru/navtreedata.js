/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "TARFS", "index.html", [
    [ "TARFS — файловая система только для чтения поверх TAR-архивов с поддержкой <span class=\"tt\">mmap()</span>", "md__r_e_a_d_m_e___r_u.html", [
      [ "1. Введение", "md__r_e_a_d_m_e___r_u.html#autotoc_md4", null ],
      [ "2. Зачем ещё одна файловая система?", "md__r_e_a_d_m_e___r_u.html#autotoc_md6", [
        [ "<a class=\"el\" href=\"md__quick_start___r_u.html\" title=\"Создание образа файловой системы TARFS и загрузка ее во flash\">Как создать и прошить свою файловую систему, инструкция</a>.", "md__r_e_a_d_m_e___r_u.html#autotoc_md1", null ],
        [ "<a class=\"el\" href=\"md__f_a_q___r_u.html\" title=\"TARFS FAQ\">Часто Задаваемые Вопросы (FAQ)</a>.", "md__r_e_a_d_m_e___r_u.html#autotoc_md2", null ],
        [ "<a class=\"el\" href=\"md__native___a_p_i___r_u.html\" title=\"Нативный API\">Нативный API</a>.", "md__r_e_a_d_m_e___r_u.html#autotoc_md3", null ],
        [ "1. Эффективное хранение неизменяемых данных на устройствах с небольшим объёмом памяти", "md__r_e_a_d_m_e___r_u.html#autotoc_md7", null ],
        [ "2. Упрощение портирования программ, написанных под Linux", "md__r_e_a_d_m_e___r_u.html#autotoc_md8", null ],
        [ "3. Более высокая устойчивость к повреждениям", "md__r_e_a_d_m_e___r_u.html#autotoc_md9", null ]
      ] ],
      [ "3. Возможности", "md__r_e_a_d_m_e___r_u.html#autotoc_md11", [
        [ "3.1 Ничего революционного", "md__r_e_a_d_m_e___r_u.html#autotoc_md12", null ],
        [ "3.2 Использование памяти", "md__r_e_a_d_m_e___r_u.html#autotoc_md14", null ],
        [ "3.3 POSIX-совместимость лучше, чем у существующих файловых систем ESP-IDF", "md__r_e_a_d_m_e___r_u.html#autotoc_md16", null ],
        [ "3.4 Проверка целостности файлов (CRC64/ECMA-182)", "md__r_e_a_d_m_e___r_u.html#autotoc_md18", null ],
        [ "3.5 Устойчивость к повреждениям", "md__r_e_a_d_m_e___r_u.html#autotoc_md20", null ],
        [ "3.6 Производительность и предсказуемость", "md__r_e_a_d_m_e___r_u.html#autotoc_md22", null ]
      ] ]
    ] ],
    [ "Создание образа файловой системы TARFS и загрузка ее во flash", "md__quick_start___r_u.html", [
      [ "Быстрый старт", "md__quick_start___r_u.html#autotoc_md24", [
        [ "1. Создайте каталог файловой системы", "md__quick_start___r_u.html#autotoc_md25", null ],
        [ "2. Заполните файловую систему", "md__quick_start___r_u.html#autotoc_md26", null ],
        [ "3. Создайте TAR-архив", "md__quick_start___r_u.html#autotoc_md27", null ],
        [ "4. Запишите <span class=\"tt\">tarfile.tar</span> в соответствующий раздел ESP с помощью <span class=\"tt\">esptool</span>:", "md__quick_start___r_u.html#autotoc_md28", null ],
        [ "5. Что дальше?", "md__quick_start___r_u.html#autotoc_md29", null ]
      ] ],
      [ "Выбор точки монтирования", "md__quick_start___r_u.html#autotoc_md30", null ],
      [ "Контроль целостности данных", "md__quick_start___r_u.html#autotoc_md32", null ],
      [ "Переписывание путей и ссылок", "md__quick_start___r_u.html#autotoc_md34", null ],
      [ "Типичные ошибки и замечания по безопасности", "md__quick_start___r_u.html#autotoc_md35", null ]
    ] ],
    [ "Нативный API", "md__native___a_p_i___r_u.html", null ],
    [ "TARFS FAQ", "md__f_a_q___r_u.html", [
      [ "Что такое TARFS?", "md__f_a_q___r_u.html#autotoc_md38", null ],
      [ "Почему был выбран формат TAR?", "md__f_a_q___r_u.html#autotoc_md40", null ],
      [ "Насколько быстро работает TARFS?", "md__f_a_q___r_u.html#autotoc_md41", [
        [ "Тестовая конфигурация", "md__f_a_q___r_u.html#autotoc_md42", null ]
      ] ],
      [ "Как начать пользоваться TARFS?", "md__f_a_q___r_u.html#autotoc_md43", null ],
      [ "Как добавить защиту CRC64 в <span class=\"tt\">.tar</span> файл?", "md__f_a_q___r_u.html#autotoc_md45", null ],
      [ "Я запустил <span class=\"tt\">tarsum</span> два раза для одного и того же файла. Будет ли он монтироваться?", "md__f_a_q___r_u.html#autotoc_md47", null ],
      [ "Есть ли простой способ определить, содержит ли TAR-архив контрольные суммы CRC64?", "md__f_a_q___r_u.html#autotoc_md49", null ],
      [ "Как прошить TARFS image в ESP32?", "md__f_a_q___r_u.html#autotoc_md51", null ],
      [ "Что такое partitions.csv?", "md__f_a_q___r_u.html#autotoc_md52", null ],
      [ "Где должен находиться partitions.csv?", "md__f_a_q___r_u.html#autotoc_md54", [
        [ "Arduino IDE", "md__f_a_q___r_u.html#autotoc_md55", null ],
        [ "ESP-IDF", "md__f_a_q___r_u.html#autotoc_md56", null ]
      ] ],
      [ "Нужно ли распаковывать TAR перед прошивкой?", "md__f_a_q___r_u.html#autotoc_md58", null ],
      [ "Можно ли использовать обычные TAR-инструменты?", "md__f_a_q___r_u.html#autotoc_md60", null ],
      [ "Можно ли писать в TARFS?", "md__f_a_q___r_u.html#autotoc_md62", null ],
      [ "Я все сделал, как написано в документации, но ничего не работает", "md__f_a_q___r_u.html#autotoc_md64", null ],
      [ "Сколько RAM занимает TARFS?", "md__f_a_q___r_u.html#autotoc_md65", null ],
      [ "А что с многопоточностью?", "md__f_a_q___r_u.html#autotoc_md67", null ],
      [ "Портируем ли TARFS на другие платформы?", "md__f_a_q___r_u.html#autotoc_md69", null ]
    ] ],
    [ "Data Structures", "annotated.html", [
      [ "Data Structures", "annotated.html", "annotated_dup" ],
      [ "Data Structure Index", "classes.html", null ],
      [ "Data Fields", "functions.html", [
        [ "All", "functions.html", null ],
        [ "Functions", "functions_func.html", null ],
        [ "Variables", "functions_vars.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "Globals", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"os__esp32_8c.html"
];

const SYNCONMSG = 'click to disable panel synchronization';
const SYNCOFFMSG = 'click to enable panel synchronization';
const LISTOFALLMEMBERS = 'List of all members';