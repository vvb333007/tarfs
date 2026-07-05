/*
 * TARFS - Immutable (read-only) filesystem for embedded systems.
 *
 * Copyright (c) 2024-2026 Viacheslav Logunov
 * SPDX-License-Identifier: MIT
 *
 * Author:
 *   Viacheslav Logunov <vvb333007@gmail.com>
 *
 * Project:
 *   https://github.com/vvb333007/tarfs
 */

#pragma once 


/* Big fat note on error handling in TARFS
 * All API functions below, whose return type is esp_err_t can only return 3 (three) error codes
 *
 * These are:
 *          - ESP_OK                  Success: No errors!
 *          - ESP_ERR_INVALID_ARG     Failure: Passed parameters are bad (wrong format, length, value so on)
 *          - ESP_FAIL                Failure: See errno for the details
 *
 * To quick check it is enough to check abovementioned codes. To narrow down the reason for the error
 * please check the errno value
 *
 *              ENOMEM  - Memory allocation problems
 *              EIO     - MMU problems (mmap failed, munmap failed)
 *              EBUSY   - Too many active filesystems.
 *              ENOENT  - Partition does not exist
 *              EINVAL  - Partition exists but data is not in TARv0 format
 *              EBADF   - File descriptor is invalid
 *              ENFILE  - Too many opened files, can not open one more
 *              EROFS   - Any modification operation (e.g. write(), mkdir() etc) will return ReadOnlyFileSystem error
 */


/**
 * @brief Configuration structure for esp_vfs_tarfs_register
 */
typedef struct {

  const char* base_path;          /*!< File path prefix associated with the filesystem. */
  const char* softlink_rebase;    /*!< Softlinks MAY have absolute paths (e.g. instead of "TAR/file.link", tar on Windows10 creates "/??/D:/Arduino/TAR/file.link").
                                       set softlink_rebase to "/??/D:/Arduino/" to cut "/??/D:/Arduino/" from such TARFS entries. */
  const char* partition_label;    /*!< Optional, label of TARFS partition to use. If set to NULL, first partition with subtype=spiffs will be used. */

} esp_vfs_tarfs_conf_t;


/**
 * @brief Default initializer for TARFS filesystem configuration structure
 */
#define ESP_TARFS_DEFAULT_CONFIG( Max_Files_ ) \
  { \
    .base_path = MULL, \
    .softlink_rebase = NULL, \
    .partition_label = TARFS_DEFAULT_PARTITION_NAME, \
  }



/**
 * Register and mount TARFS to VFS with given path prefix.
 *
 * @param   conf                      Pointer to esp_vfs_tarfs_conf_t configuration structure
 *
 * @return
 *          - ESP_OK                  if success
 *          - ESP_ERR_INVALID_ARG     if configuration parameters are NULL: (i.e. conf==NULL or conf->base_path is NULL or empty and so on)
 *          - ESP_FAIL                other errors: see errno for the details
 *
 *              ENOMEM  - memory allocation problems
 *              EIO     - MMU problems (mmap failed)
 *              EBUSY   - too many active filesystems
 *              ENOENT  - partition does not exist
 *              EINVAL  - partition exists but data is not in TARv0 format
 *
 * @note  
 *        This is required for POSIX mmap() implementation: mmap() is not the part of vfs_ops, so 
 *        has no access to local fd numbers
 */
esp_err_t esp_tarfs_register(const esp_vfs_tarfs_conf_t * conf);

/**
 * Unregister and unmount TARFS from VFS
 *
 * @param partition_label  Same label as passed to esp_vfs_tarfs_register.
 *
 * @return
 *          - ESP_OK                  if success
 *          - ESP_ERR_INVALID_ARG     if base_path is NULL, empty string or string which is too long
 *          - ESP_FAIL                other errors: see errno for the details
 *
 *              EBUSY   - unregistering failed (there are opened files)
 *              ENOENT  - base path does not represent a valid mounted TARFS partition
 */
esp_err_t esp_tarfs_unregister(const char* base_path);


/**
 * Check TARFS filesystem integrity. TAR header entries are checked, file content is NOT checked
 * as there no checksums stored for files
 *
 * @param base_path Base path (i.e. a mountpoint), cant be NULL
 *
 * @param out Output stream if required. Can be `stdout`, `stderr` or 
 *            NULL (means no report will be printed during fs check)
 *        
 *
 * @return
 *          - <0    FS looks completely unusable (wrong format or trahsed first TAR header)
 *          -  0    Some of FS entries are corrupted
 *          - >0    Healthy FS
 */
int esp_tarfs_fsck(const char* base_path, FILE *out);


/**
 * Get information for TARFS
 *
 * @param partition_label           Same label as passed to esp_vfs_tarfs_register
 * @param[out] total_bytes          Size of the file system
 * @param[out] used_bytes           Size of the filesystem minus the size of all TAR headers and paddings.
 *
 * @return
 *          - ESP_OK              if success
 *          - ESP_ERR_NOT_FOUND   if not mounted
 */
esp_err_t esp_tarfs_info(const char* mountpoint, size_t *raw_size, size_t *data_size);

/**
 * Check if a filesystem at mountpoint is a mounted TARFS
 *
 * @param Base path (i.e. a mountpoint)
 *        
 *
 * @return
 *          - true    if mounted
 *          - false   if not mounted
 */
static inline bool esp_tarfs_is_registered(const char *mountpoint) {
  return ESP_OK == esp_tarfs_info(mountpoint, NULL, NULL);
}


/**
 * Dump runtime stats and open file descriptors. 
 *
 * @param vty           Opaque pointer, required by the vtyout
 * @param vtyout        Pointer to the fprintf-like function, which is called as vtyout(vty, format, ...)
 *
 * Usage: esp_tarfs_dump(stdout, fprintf)
 */
void esp_tarfs_dump(void *vty, int (*vtyout)(void *, const char *, ...));
