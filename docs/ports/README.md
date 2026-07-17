## STM32 Support

Dear STM32 developer,

Use `os_stm32.c` instead of `os_esp32.c`.

Adjust the `TARFS_ADDRESS` and `TARFS_SIZE` macros to match your target device (see the comments in `os_stm32.c`).

Use `os_stm32.c` as a starting point for implementing the platform layer for your STM32-based project.
