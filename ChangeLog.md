# Changelog

## 0.1.3

- Initial STM32 port.
- Added `tarfs_fsck()`.
- Converted source code comments to Doxygen style and added a Doxygen configuration to `docs/`.
- Added runtime statistics and the corresponding API (total bytes read, total bytes memory-mapped, total errors).
- Added an option to verify CRC64 on every `open()`, intended for long-running systems where checking the filesystem only at mount time is insufficient.

## 0.1.2

- Added `ChangeLog.md`.
- Moved the `tarsum` utility and its `Makefile` to a separate directory.
- Fixed several incorrect `errno` return values.
- Removed unused code.

## 0.1.1

- Fixed CRC64 validation.
- Reading corrupted file contents now correctly returns `EIO`.
- Entries with corrupted TAR headers are now skipped during indexing and reported as `ENOENT`.

## 0.1.0

- Initial release.