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
    [ "Creating a TARFS Filesystem Image, Setp by step guide.", "md__quick_start.html", [
      [ "Quick Start", "md__quick_start.html#autotoc_md21", [
        [ "1. Create the filesystem root directory", "md__quick_start.html#autotoc_md22", null ],
        [ "2. Populate the filesystem", "md__quick_start.html#autotoc_md23", null ],
        [ "3. Create the TAR archive", "md__quick_start.html#autotoc_md24", null ],
        [ "4. Write <span class=\"tt\">tarfile.tar</span> to the appropriate ESP Flash partition using <span class=\"tt\">esptool</span>.", "md__quick_start.html#autotoc_md25", null ],
        [ "4.1 Create a TARFS partition", "md__quick_start.html#autotoc_md26", null ],
        [ "4.2 Write the image to Flash", "md__quick_start.html#autotoc_md27", null ],
        [ "5. What's Next?", "md__quick_start.html#autotoc_md29", null ]
      ] ],
      [ "Additional features:", "md__quick_start.html#autotoc_md30", [
        [ "Selecting the Mount Point", "md__quick_start.html#autotoc_md31", null ],
        [ "Filesystem Integrity Checking", "md__quick_start.html#autotoc_md33", null ],
        [ "Path and Link Rebasing", "md__quick_start.html#autotoc_md35", null ]
      ] ],
      [ "Common Mistakes &amp; Security Notes", "md__quick_start.html#autotoc_md37", null ]
    ] ],
    [ "Native API", "md__native___a_p_i.html", null ],
    [ "TARFS FAQ", "md__f_a_q.html", [
      [ "What is TARFS?", "md__f_a_q.html#autotoc_md40", null ],
      [ "Why TAR?", "md__f_a_q.html#autotoc_md42", null ],
      [ "How fast it is comparing to other filesystems on ESP32?", "md__f_a_q.html#autotoc_md44", [
        [ "TarFS Test Configuration", "md__f_a_q.html#autotoc_md45", null ]
      ] ],
      [ "How do I start using TARFS?", "md__f_a_q.html#autotoc_md47", null ],
      [ "How do I add CRC64 protection to my <span class=\"tt\">.tar</span> file?", "md__f_a_q.html#autotoc_md49", null ],
      [ "I ran <span class=\"tt\">tarsum</span> twice on the same file. Will it still mount?", "md__f_a_q.html#autotoc_md51", null ],
      [ "Is there a simple way to tell whether a TAR archive contains CRC64 checksums?", "md__f_a_q.html#autotoc_md53", null ],
      [ "How do I flash a TARFS image into ESP32?", "md__f_a_q.html#autotoc_md55", [
        [ "Windows:", "md__f_a_q.html#autotoc_md56", null ],
        [ "Linux:", "md__f_a_q.html#autotoc_md57", null ]
      ] ],
      [ "What is partitions.csv?", "md__f_a_q.html#autotoc_md59", null ],
      [ "Where should partitions.csv be located?", "md__f_a_q.html#autotoc_md61", [
        [ "Arduino IDE", "md__f_a_q.html#autotoc_md62", null ],
        [ "ESP-IDF", "md__f_a_q.html#autotoc_md64", null ]
      ] ],
      [ "Do I need to extract the TAR archive before flashing?", "md__f_a_q.html#autotoc_md66", null ],
      [ "Can I use normal TAR tools?", "md__f_a_q.html#autotoc_md68", null ],
      [ "Can I write to TARFS?", "md__f_a_q.html#autotoc_md70", null ],
      [ "Can I use compressed tar archives?", "md__f_a_q.html#autotoc_md72", null ],
      [ "I followed the documentation, but TARFS does not work. What should I do?", "md__f_a_q.html#autotoc_md73", null ],
      [ "How much RAM does TARFS use?", "md__f_a_q.html#autotoc_md75", null ],
      [ "Can I use TARFS to mount filesystem from .rodata section of the firmware?", "md__f_a_q.html#autotoc_md76", null ],
      [ "Is TARFS multithread-safe?", "md__f_a_q.html#autotoc_md77", null ],
      [ "Is TARFS portable?", "md__f_a_q.html#autotoc_md78", null ]
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
"md__quick_start.html#autotoc_md33"
];

const SYNCONMSG = 'click to disable panel synchronization';
const SYNCOFFMSG = 'click to enable panel synchronization';
const LISTOFALLMEMBERS = 'List of all members';