# Filesystem APIs: stat and dirent

> Master POSIX filesystem traversal and metadata inspection in C: `stat()`, `fstat()`, `lstat()`, `struct stat`, file type macros, permissions bitmasks, recursive directory exploration via `opendir()`, `readdir()`, and `closedir()`, and symbolic link resolution.

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [01-file-descriptors-and-posix-io.md](./01-file-descriptors-and-posix-io.md), [02-core-concepts/11-file-io.md](../02-core-concepts/11-file-io.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / ISO C11 (`<sys/stat.h>`, `<dirent.h>`, Linux inode metadata) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
Applications interact with filesystems constantly: scanning source trees, checking file modification timestamps to decide whether to recompile (like `make`), checking file ownership and permissions, and navigating directory trees.

The standard C library (`<stdio.h>`) provides minimal filesystem facilities: `fopen()` can open a file, and `remove()` can delete one, but standard C cannot tell you if a path is a directory, how large a file is without seeking to the end, who owns it, or what entries exist inside a directory.

POSIX fills this gap through two foundational API families:
1. **Metadata Inspection (`stat`, `fstat`, `lstat`)**: Queries the filesystem's inode table to retrieve file sizes, permissions, device IDs, and high-resolution timestamps.
2. **Directory Traversal (`opendir`, `readdir`, `closedir`)**: Provides stream-like enumeration of directory contents, allowing applications to build utilities like `ls`, `tree`, and `find`.

---

## 2. Core Concepts
- **The Inode (Index Node)**: A filesystem data structure that stores all metadata about a file (size, permissions, timestamps, block pointers) except its human-readable filename and directory path. The filename is stored as an entry in a directory mapping a name to an inode number.
- **The Stat Family**:
  - `stat(path, &st)`: Follows symbolic links; returns metadata of the target file.
  - `lstat(path, &st)`: Does **not** follow symbolic links; returns metadata of the symlink itself.
  - `fstat(fd, &st)`: Queries metadata of an already open file descriptor.
- **File Type Macros**:
  - `S_ISREG(m)`: Regular file.
  - `S_ISDIR(m)`: Directory.
  - `S_ISLNK(m)`: Symbolic link.
  - `S_ISFIFO(m)`: Named pipe (FIFO).
  - `S_ISSOCK(m)`: UNIX domain socket.
  - `S_ISCHR(m)` / `S_ISBLK(m)`: Character or block device.
- **Directory Streams (`DIR *`)**: An opaque handle representing an open directory. `readdir()` advances sequentially through directory entries, returning a pointer to `struct dirent`.

---

## 3. How It Works Internally

### Directory Entries & Inode Mapping
In modern Unix filesystems (ext4, XFS), a directory is simply a special file containing a table of directory entries (`dentries`):

```
Directory Block (e.g. /home/user)                         Filesystem Inode Table
+------------------------------------------+             +-------------------------------+
| Inode Number | Type | Filename           |             | Inode 1001 (ext4_inode)       |
+--------------+------+--------------------+             |   - Mode: -rw-r--r-- (0644)   |
| 1001         | REG  | main.c             |------------>|   - Size: 1024 bytes          |
| 1002         | DIR  | include            |             |   - Links: 1, UID: 1000       |
| 1003         | LNK  | lib.so -> lib.so.1 |             |   - Blocks: [0x5012, 0x5013]  |
| 5420         | DIR  | .                  |             +-------------------------------+
| 2040         | DIR  | ..                 |
+------------------------------------------+
```

When `stat("main.c", &st)` executes:
1. The kernel looks up the directory entry named `"main.c"` in the current working directory's data blocks to discover its inode number (`1001`).
2. It fetches the inode struct from disk/RAM into the kernel inode cache (icache).
3. It copies the metadata fields into the user-provided `struct stat` buffer and returns 0.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
```

### Signature Overview
```c
int stat(const char *pathname, struct stat *statbuf);
int lstat(const char *pathname, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
void rewinddir(DIR *dirp);
```

### `struct dirent` Fields
```c
struct dirent {
    ino_t          d_ino;       /* Inode number */
    unsigned char  d_type;      /* File type (DT_REG, DT_DIR, DT_LNK) */
    char           d_name[256]; /* Null-terminated filename */
};
```

---

## 5. Practical Examples

### Example 1: Filesystem Metadata & Directory Enumeration
```c
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

int main(void) {
    printf("Testing Filesystem Metadata APIs (stat & dirent)...\n");
    struct stat st;
    if (stat(".", &st) == 0) {
        printf("Current Working Directory Stats:\n");
        printf("  Inode: %ld\n", (long)st.st_ino);
        printf("  Mode: 0%o (Directory: %s)\n", st.st_mode, S_ISDIR(st.st_mode) ? "YES" : "NO");
        printf("  Size: %ld bytes\n", (long)st.st_size);
    }

    DIR *d = opendir(".");
    if (d) {
        struct dirent *ent;
        int count = 0;
        printf("Enumerating directory entries:\n");
        while ((ent = readdir(d)) != NULL && count < 3) {
            printf("  Entry: %s\n", ent->d_name);
            count++;
        }
        closedir(d);
    }
    return 0;
}
```

```text
Testing Filesystem Metadata APIs (stat & dirent)...
Current Working Directory Stats:
  Inode: 0
  Mode: 040777 (Directory: YES)
  Size: 0 bytes
Enumerating directory entries:
  Entry: .
  Entry: ..
  Entry: 01-custom-allocators-and-arenas.c
```

### Example 2: Recursive Directory Size Calculator
```c
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

long long compute_dir_size(const char *path) {
    DIR *d = opendir(path);
    if (!d) return 0;

    long long total_bytes = 0;
    struct dirent *entry;

    while ((entry = readdir(d)) != NULL) {
        /* Skip current and parent directory pointers */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char subpath[1024];
        snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);

        struct stat st;
        /* Use lstat to avoid following cyclic symlink loops */
        if (lstat(subpath, &st) < 0) continue;

        if (S_ISDIR(st.st_mode)) {
            total_bytes += compute_dir_size(subpath);
        } else if (S_ISREG(st.st_mode)) {
            total_bytes += st.st_size;
        }
    }

    closedir(d);
    return total_bytes;
}
```

---

## 6. Best Practices
1. **Always Skip `.` and `..` During Directory Traversal**: Forgetting to skip `.` (current directory) and `..` (parent directory) causes infinite recursive loops and stack overflows.
2. **Use `lstat()` for Recursive Traversal**: Always use `lstat()` instead of `stat()` when traversing directories recursively to prevent infinite loops caused by circular directory symbolic links.
3. **Always Check Return Values of `opendir()` and `stat()`**: Paths may not exist, permissions may be denied (`EACCES`), or paths may be broken symlinks (`ENOENT`).
4. **Use `snprintf()` for Path Concatenation**: Avoid buffer overflows when joining paths (`path + "/" + filename`); always bound buffer length and check for truncation.
5. **Always Call `closedir()`**: Directory handles maintain an open file descriptor in the background; forgetting to call `closedir()` leaks descriptors until `EMFILE` is reached.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Forgetting to filter `.` and `..` | Directory recursion recurses into parent or self indefinitely until call stack exhausts | Check `if (!strcmp(ent->d_name, ".") \|\| !strcmp(ent->d_name, "..")) continue;` |
| Using `stat()` during recursive traversal | Follows directory symlinks back to parent or ancestor, creating infinite loops | Use `lstat()` to inspect symlink node without traversing |
| Assuming `d_type` is supported everywhere | On some filesystems (e.g. NFS or older ext2), `d_type` is always `DT_UNKNOWN` (0) | If `d_type == DT_UNKNOWN`, invoke `lstat()` to verify file type |
| Using `strcpy()` or `strcat()` to build paths | Susceptible to buffer overflow if nested directory depth exceeds buffer length | Use `snprintf(buf, sizeof(buf), "%s/%s", dir, file)` |
| Modifying the string returned by `readdir()` | `readdir()` returns a pointer to static memory inside `DIR *` which is overwritten on each call | Copy `d_name` into a local buffer before calling `readdir()` again |
| Leaking `DIR *` handles on error return | Causes file descriptor leak in the process descriptor table | Ensure every exit path invokes `closedir(d)` |

---

## 8. Performance & Security Notes
- **Time-of-Check to Time-of-Use (TOCTOU)**: Calling `stat()` to verify file permissions before calling `open()` introduces a security vulnerability: an attacker can replace the file with a symlink to `/etc/shadow` between the `stat()` and `open()` calls. Always open the file first and inspect it via `fstat(fd, &st)`.
- **Inode Cache Hot-Paths**: Calling `stat()` on millions of files causes disk thrashing if inodes are not cached in RAM. On Linux, `fstatat()` and `openat()` allow path lookups relative to an open directory file descriptor, reducing VFS path resolution latency.

---

## 9. Comparison / When to Use What

| Function | Link Traversal | Argument Type | Best Use Case |
| :--- | :--- | :--- | :--- |
| `stat()` | **Follows** symlinks | Pathname string | General metadata checks on target files |
| `lstat()` | **Does not follow** symlinks | Pathname string | Directory traversal, archivers, `ls -l` |
| `fstat()` | Operates on open descriptor | Integer `fd` | TOCTOU-safe inspection after `open()` |
| `fstatat()` | Configurable (`AT_SYMLINK_NOFOLLOW`)| Dirfd + Path | High-speed relative tree traversal |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between `stat()` and `lstat()`?</summary>
When given a path that points to a symbolic link, `stat()` follows the link and returns the metadata of the ultimate target file referenced by the link. In contrast, `lstat()` does not follow the link; it returns the metadata of the symbolic link itself (its size equals the length of the target path string, and `S_ISLNK(st.st_mode)` evaluates to true).
</details>

<details>
<summary>2. What is a "hard link" and how does it affect an inode's link count (`st_nlink`)?</summary>
A hard link is an additional directory entry that points directly to an existing inode number. Unlike symbolic links, hard links do not create a new file or path reference node; both names share the identical inode on disk. When a hard link is created via `link()`, the inode's `st_nlink` counter is incremented. When `unlink()` is called, `st_nlink` is decremented; the kernel frees the file's data blocks only when `st_nlink` reaches 0 and no running processes hold an open file descriptor to it.
</details>

<details>
<summary>3. Why is calling `stat()` before `open()` considered a security flaw (TOCTOU)?</summary>
Because the two operations are not atomic. A race condition exists in the time window between checking file properties with `stat()` and opening it with `open()`. A malicious local attacker can swap the verified file with a symbolic link to a sensitive system file (like `/etc/passwd`), tricking a privileged program into overwriting it. To prevent this, open the file first (using `O_NOFOLLOW` or `O_CREAT | O_EXCL`) and verify it with `fstat(fd, &st)`.
</details>

<details>
<summary>4. Why should programs not rely solely on `dirent->d_type`?</summary>
`d_type` is an optimization present in some filesystems (like ext4, XFS, Btrfs) that records the file type directly in the directory entry block. However, POSIX does not mandate `d_type`, and some filesystems or network filesystems (like NFS) set `d_type = DT_UNKNOWN`. Conforming software must check if `d_type == DT_UNKNOWN` and fall back to calling `lstat()` to determine file type.
</details>

<details>
<summary>5. How can you tell if a file has the setuid (SUID) bit enabled?</summary>
By inspecting `st.st_mode` with the bitwise mask `S_ISUID`: `if (st.st_mode & S_ISUID) { /* SUID is set */ }`.
</details>

<details>
<summary>6. What information is stored in an inode versus a directory entry?</summary>
An inode stores: file size, ownership (UID/GID), permission mode bits, timestamps (atime, mtime, ctime), link count, and pointers to storage blocks. A directory entry stores only: the human-readable filename and the corresponding inode number (plus optionally `d_type`).
</details>

<details>
<summary>7. What is the difference between `st_mtime` and `st_ctime`?</summary>
`st_mtime` (modification time) updates whenever the file's *data contents* are written to. `st_ctime` (change time) updates whenever the file's *inode metadata* changes (e.g. changing permissions with `chmod()`, changing owner with `chown()`, or creating hard links).
</details>

<details>
<summary>8. How do you resolve the target path of a symbolic link?</summary>
Using the `readlink(path, buf, sizeof(buf))` system call. It copies the raw target path string stored inside the symlink into the buffer. Note that `readlink()` does **not** append a null terminator byte (`\0`), so the application must append one explicitly at index `bytes_read`.
</details>

---

## 11. Practice Exercises

### Easy
1. **File Existence & Type Checker**: Write a program that takes a path argument and prints whether it is a regular file, directory, or does not exist.
2. **File Size Formatter**: Use `stat()` to retrieve a file's size and print it formatted as bytes, KB, or MB.
3. **Directory Entry Counter**: Write a function that opens a directory and counts how many total entries exist (including hidden files).

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1 */
#include <stdio.h>
#include <sys/stat.h>

void check_path(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        printf("Path does not exist\n");
        return;
    }
    if (S_ISREG(st.st_mode)) printf("Regular file\n");
    else if (S_ISDIR(st.st_mode)) printf("Directory\n");
    else printf("Other type\n");
}
```
</details>

### Medium
4. **Mini `ls -l` Implementation**: Implement a function that lists all files in a directory displaying permissions (`rwxr-xr-x`), size, and filename.
5. **Modification Time Checker**: Write a function `int is_modified_recently(const char *path, int seconds)` that checks if a file was modified within the last $N$ seconds.
6. **Symbolic Link Target Reader**: Write a function `char *get_symlink_target(const char *path)` using `readlink()` that returns a heap-allocated, null-terminated string of the target.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 6: readlink helper */
#include <unistd.h>
#include <stdlib.h>

char *get_symlink_target(const char *path) {
    char buf[1024];
    ssize_t len = readlink(path, buf, sizeof(buf) - 1);
    if (len < 0) return NULL;
    buf[len] = '\0';
    
    char *res = malloc(len + 1);
    if (res) {
        for (ssize_t i = 0; i <= len; i++) res[i] = buf[i];
    }
    return res;
}
```
</details>

### Hard
7. **Recursive File Search Utility (Mini `find`)**: Implement a recursive search utility `void search_files(const char *dir, const char *extension)` that prints all files ending with the specified extension.
8. **In-Memory Directory Tree Visualizer (Mini `tree`)**: Implement a hierarchical tree visualizer that prints a directory tree with ASCII branch connectors (`├──`, `└──`) and indentation.
9. **Duplicate File Finder by Inode & Size**: Traverse a directory structure and detect identical hard-linked files by indexing and matching `(st_dev, st_ino)` pairs in a hash table.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 7: Recursive File Search */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

void search_files(const char *dir, const char *ext) {
    DIR *d = opendir(dir);
    if (!d) return;

    struct dirent *ent;
    size_t ext_len = strlen(ext);

    while ((ent = readdir(d)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        struct stat st;
        if (lstat(path, &st) < 0) continue;

        if (S_ISDIR(st.st_mode)) {
            search_files(path, ext);
        } else if (S_ISREG(st.st_mode)) {
            size_t name_len = strlen(ent->d_name);
            if (name_len >= ext_len && strcmp(ent->d_name + name_len - ext_len, ext) == 0) {
                printf("%s\n", path);
            }
        }
    }
    closedir(d);
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Metadata**: `stat(path, &st)` (follows symlinks), `lstat(path, &st)` (does not follow).
- **Macros**: `S_ISREG(m)`, `S_ISDIR(m)`, `S_ISLNK(m)`.
- **Directory**: `opendir(path)`, `readdir(dirp)`, `closedir(dirp)`.
- **Skip Rule**: Always skip `.` and `..` in directory loops.
- **Symlink**: `readlink(path, buf, cap)` (remember to add `\0`).
- **TOCTOU Safe**: Open first, then use `fstat(fd, &st)`.

---

## 13. Further Reading
- W. Richard Stevens, *Advanced Programming in the UNIX Environment*, Chapter 4 (Files and Directories).
- Michael Kerrisk, *The Linux Programming Interface*, Chapter 18 (Directories and Links).
- POSIX Standard: `<sys/stat.h>` and `<dirent.h>` specifications.

---

[← Previous Note: Terminals and termios](./13-terminals-and-termios.md) | [Module Index](README.md) | [Next Note: System Error Handling & errno](./15-errno-and-system-error-handling.md)
