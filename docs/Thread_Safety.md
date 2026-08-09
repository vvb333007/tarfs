AI-assisted translation (RU-ENG)

## TARFS - Multithreading, Thread Safety and Locking

TARFS is designed to be safe for concurrent use from multiple threads while being lockless in its hotpath.

The thread-safety model is intentionally lightweight: TARFS protects its own global filesystem table and 
internal object lifetime where necessary. It does not attempt to provide application-level synch for an 
arbitrary sequence of API calls.

In particluar, TARFS does not serialize normal file I/O with a global mutex.

The only global mutex used by TARFS protects the table of mounted filesystems. It is used only for operations that modify or inspect this table and is held for a very short time.

---

## What TARFS guarantees

TARFS guarantees that concurrent operations will not corrupt the global mounted-filesystem table or 
cause TARFS itself to access freed filesystem descriptors as a result of a TARFS-internal race.

---

## `read()` and `pread()`

### `pread()`

`pread()` is naturally independent of the file current position.

A `pread()` operation uses the explicitely supplied file offset and thus does not depend on, or modify, the current file position.

Concurrent `pread()` operations on the same file descriptor do not interfere with each other offsets.

### `read()`

TARFS `read()` has an important property concerning concurrent `seek()` or `read()` operations.

When `read()` starts, it obtains the current file position and performs the actual filesystem 
access using that position. The actual data access is performed independently of subsequent 
changes to the file position.

Therefore once  `read()` has started, another thread calling `seek()` or `read()` can not redirect that 
already-running `read()` to another location.

For example:

```text
Initial file position = 1000

Thread A: read(100) <-- starts at offset 1000
Thread B: seek(5000) <-- changes the file position
Thread A: continues reading from offset 1000
```

The `read()` already in progress continues to operate on the offset it captured when it started.

Read operations are atomic: they can not be interrupted.


### Important:

This does **not** mean that arbitrary sequences of `read()` and `seek()` performed by diffrent application threads are logically serialized.

For example the application that wants to execute a sequence:

```text
seek()
read()
seek()
read()
```

as one indivisible operation must provide its own synchronization.

**TARFS protects its internal state; it does not protect application-level protocols!**

---
## `lseek()` and `seekdir()`

Both functions can be called concurently


## `readdir()` and `closedir()`

`readdir()` and `closedir()` must not be called concurrently on the same `DIR *` object.

The `DIR` object is allocated by `opendir()` and released by `closedir()`. It also contains the current directory iteration state.

Thus the application must ensure that the lifetime of the `DIR` object and access to it are properly synchronized.

In particular, this is invalid:

```text
Thread A: readdir(dir)
Thread B: closedir(dir)
Thread A: readdir(dir)    <-- undefined behavior
```

Once `closedir()` has released the `DIR` object, any subsequent use of that pointer is undefined behavior.

Likewise, concurrent calls to `readdir()` using the same `DIR *` must not be assumed to provide a coherent 
directory traversal: some entries may be skipped. If multiple threads need to iterate over the same 
`DIR *` then user application must provide its own synchronization.

---

## `close()`

`close()` may be called concurrently with other TARFS operations. `close()` WILL NOT interrupt 
already-started read() operation - so `close()` coupled with `tarfs_unmount()` and a 
`read()` **at the same time is an UB**.

`close()` and `tarfs_unmount()` will start unmounting process, while `read()` operation may be still
active. This will result in MMU unmapping memory being used --> MMU error.

Just `close()`+`read()` or `tarfs_unmount()`+`read()` is safe.

TARFS uses reference counting for file system objects: this allows any object that is logically unmounted or closed to remain alive while active users still 
hold references to it. close()+tarfs_unmount() will drop reference counter to zero.


---

## `tarfs_unmount()` and `tarfs_unmount_fs()`

The `tarfs_unmount()` operation does not blindly free a filesystem descriptor while another TARFS 
operation is using it: the filesystem enters a delayed unmount state until its active references are 
released.

`tarfs_unmount()` can be safely called from multiple threads.

Operations like:

```c
tarfs_unmount("/www");
tarfs_unmount_fs(0);
```

performs the lookup of the mountpoint and the corresponding unmount operation while holding the TARFS filesystem-table mutex.

Thus, the lookup and removal cannot be separated by another TARFS mount/unmount operation.



---

## `tarfs_mount()` atomicity

Mount operations are protected by the same filesystem-table mutex.

The critical section covers the selection and occupation of a filesystem-table slot and the check for an already-used mountpoint.

Therefore, two concurrent mount operations cannot both observe the same free slot or both successfully claim the same mountpoint.

For example:

```text
Thread A: mount("/foo")
Thread B: mount("/foo")
```

cannot result in two filesystems being registered for `/foo`.

One operation obtains the table lock first, reserves the slot, and releases the lock. The other operation subsequently observes the updated filesystem table and fails with the appropriate error (`EBUSY` for an already occupied mountpoint or when no filesystem slots remain).

### TARFS's global mutex (yes, we have one)

The single global mutex protects the TARFS mounted-filesystem table, including:

* the filesystem slot array;
* filesystem slot allocation;
* filesystem slot release;
* the mounted-filesystem count;
* mountpoint lookup while performing operations that must be atomic with the lookup;
* filesystem descriptor lifetime

Normal file reads/lookups do not require acquiring this global mutex.

The filesystem-table mutex is not held while the TAR archive is being mounted.

The critical section are limited to operations on the global filesystem table.

TARFS follows a simple rule: *TARFS protects TARFS. The application protects its own logic.*
