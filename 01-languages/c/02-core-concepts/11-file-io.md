# File I/O

The standard library reads and writes files through `FILE *` streams, using the same buffered functions you already know from `printf` and `scanf`. The recurring themes are: check every call, understand buffering, and distinguish text from binary.

**Prerequisites:** [Strings](./02-strings.md), [pointers](./03-pointers.md), [input/output](../01-fundamentals/09-input-output.md).

## Opening and closing

```c
FILE *f = fopen("data.txt", "r");
if (f == NULL) {
    perror("data.txt");          /* prints: data.txt: No such file or directory */
    return 1;
}
/* ... use f ... */
fclose(f);
```

| Mode | Meaning | If file exists | If missing |
|---|---|---|---|
| `"r"` | read | opens | fails |
| `"w"` | write | **truncates to empty** | creates |
| `"a"` | append (writes go to the end) | keeps | creates |
| `"r+"` | read and write | opens | fails |
| `"w+"` | read and write | truncates | creates |
| `"wx"` (C11) | write, exclusive | fails | creates |

Add `b` for binary mode (`"rb"`, `"wb"`). On POSIX systems it makes no difference, but on Windows text mode translates `\r\n`.

Every successful `fopen` needs a matching `fclose`. The number of open files per process is limited, and `fclose` flushes buffered data.

### Error reporting

When a library call fails, it sets `errno` (from `<errno.h>`). Convert it to text with `strerror(errno)` or print it with `perror`:

```c
fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
```

Read `errno` immediately; later calls can overwrite it.

## Reading text

```c
char line[256];
while (fgets(line, sizeof line, f) != NULL) {
    line[strcspn(line, "\n")] = '\0';
    printf("[%s]\n", line);
}
if (ferror(f)) { perror("read error"); }
```

- `fgets` reads one line (up to `size - 1` characters). Lines longer than the buffer arrive in pieces.
- For arbitrary-length lines on POSIX, use `getline(&buf, &cap, f)`, which allocates for you.
- Character by character: `int c; while ((c = fgetc(f)) != EOF) ...` (use `int`, not `char`).
- Formatted: `fscanf(f, "%d", &x)`, with the same cautions as `scanf`.

### The `feof` trap

```c
while (!feof(f)) {              /* WRONG */
    fgets(line, sizeof line, f);
    process(line);              /* runs once more with stale data */
}
```

`feof` becomes true only **after** a read attempt hits the end. Test the read's return value instead (as above). After a loop ends, `feof(f)` vs `ferror(f)` tells you why.

## Writing text

```c
fprintf(f, "%s,%d\n", name, score);
fputs("done\n", f);
fputc('x', f);
```

Check the result for errors on important output. Buffered writes may only fail at `fclose`/`fflush`, so check those too:

```c
if (fclose(f) != 0) { perror("close"); /* data may be lost */ }
```

## Binary I/O

```c
int values[100];

FILE *f = fopen("out.bin", "wb");
size_t written = fwrite(values, sizeof values[0], 100, f);   /* elements, not bytes */
if (written != 100) { /* short write: error */ }
fclose(f);

f = fopen("out.bin", "rb");
size_t got = fread(values, sizeof values[0], 100, f);
```

`fread`/`fwrite` return the number of **items** transferred; a smaller number means end-of-file or an error.

**Don't dump structs and expect portability:** padding, byte order, and type sizes differ between compilers and machines. For files shared across systems, write each field explicitly with a defined size and endianness. See [integer representation](../01-fundamentals/03-integer-representation.md).

## Seeking

```c
fseek(f, 0, SEEK_END);       /* go to end */
long size = ftell(f);        /* current position = file size (for regular binary files) */
rewind(f);                   /* back to the start */

fseek(f, 128, SEEK_SET);     /* absolute offset; also SEEK_CUR, SEEK_END */
```

`ftell` returns `long`, which may be 32 bits and limit you to 2 GB. POSIX provides `fseeko`/`ftello` with `off_t` for large files. Seeking on terminals and pipes fails.

### Reading a whole file

```c
char *read_file(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) goto fail;
    long size = ftell(f);
    if (size < 0) goto fail;
    rewind(f);

    char *buf = malloc((size_t)size + 1);
    if (buf == NULL) goto fail;

    size_t n = fread(buf, 1, (size_t)size, f);
    if (n != (size_t)size) { free(buf); goto fail; }
    buf[n] = '\0';

    fclose(f);
    if (len_out) *len_out = n;
    return buf;                      /* caller frees */

fail:
    fclose(f);
    return NULL;
}
```

## Buffering

Streams are buffered: data sits in memory until the buffer fills, `fflush` is called, or the file is closed.

```c
fflush(f);                           /* push buffered output to the OS now */
setvbuf(f, NULL, _IOFBF, 1 << 16);   /* choose buffering mode and size, before any I/O */
```

`fflush` hands data to the OS, not necessarily to the disk. For durability on POSIX, follow with `fsync(fileno(f))`. If a program crashes, unflushed buffer contents are lost.

## Safe updates

Overwriting a file with `"w"` truncates it first; a crash mid-write leaves a corrupt file. A safer pattern: write to a temporary file in the same directory, flush and close it, then `rename()` it over the original (atomic on POSIX).

## Beyond `FILE *`

POSIX offers file descriptors (`open`, `read`, `write`, `close`) for unbuffered access, directories, permissions, and pipes. See [POSIX files and directories](../04-advanced/09-posix/01-files-and-directories.md).

## Common mistakes

| Mistake | Effect |
|---|---|
| Not checking `fopen` for `NULL` | Crash on first use |
| `while (!feof(f))` | Processes the last item twice |
| Forgetting `fclose` | Leaks handles, loses buffered data |
| Opening with `"w"` by accident | Destroys the existing file |
| `fread`/`fwrite` with swapped size/count arguments | Wrong byte counts |
| Writing structs to portable files | Padding and endianness break compatibility |
| Reading and writing on `"r+"` without `fseek`/`fflush` between | Undefined behavior; switch direction with a seek or flush |
| Ignoring `fclose` errors on written files | Silent data loss |
| `char c = fgetc(f)` | EOF confusion |

## Debugging

- `strace -e trace=file,read,write ./prog` shows the actual system calls ([strace and ltrace](../06-debugging-testing/05-strace-ltrace.md)).
- Missing output after a crash usually means unflushed buffers.
- Verify with `hexdump -C file` or `xxd file` when debugging binary files.

## Summary

- `fopen` → check for `NULL` → read/write → `fclose`, checking results.
- Use `fgets` for lines, `fread`/`fwrite` for binary; test return values, never `feof` alone.
- Streams are buffered; flush or close to push data out.
- Report failures with `perror`/`strerror(errno)`.

**Next:** [Intermediate: function pointers](../03-intermediate/01-function-pointers.md)
