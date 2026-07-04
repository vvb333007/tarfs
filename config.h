#pragma once

#define CONFIG_TARFS_BIG_ENDIAN 0 

#define CONFIG_TARFS_INTEGRITY 1 /*!< Support for file data integrity records */
#define CONFIG_TARFS_PAX       1 /*!< Process PAX headers; Required for looooong filenames, especially UTF8-encoded */
#define CONFIG_TARFS_LINKS     1 /*!< Support symlinks and hardlinks */
#define CONFIG_TARFS_DIROPS    1 /*!< Support opendir()/closedir()/readdir()/closedir()/telldir() and so on 
                                      Disabling it will not disable directory support in TARFS */

#define CONFIG_TARFS_MAX_FS  4     /*!< Max number of active TARFS filesystems */
#define CONFIG_TARFS_MAX_FDS 5     /*!< Max number of active opened files (per filesystem) */
