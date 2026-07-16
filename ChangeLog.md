## 0.1.2

- ChangeLog.md added
- tarsum utility was moved to a separate folder along with the Makefile.
- Couple of wrong errno return values were fixed
- Some dead code was removed

## 0.1.1

- Fixed CRC64 validation.
- Corrupted file contents now correctly return EIO.
- Entries with corrupted TAR headers are ignored during indexing and are reported as ENOENT.


## 0.1.0

- Initial release