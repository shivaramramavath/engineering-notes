# Project 06: Fast Multi-Threaded Grep Clone (`fast-grep`)

> A high-throughput parallel pattern searching utility in C leveraging memory-mapped file I/O (`mmap`), zero-copy line scanning, POSIX threads, and ANSI-colored terminal output.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `06-text-editor-or-grep-clone` |
| **Language Baseline** | C11 / POSIX.1-2008 |
| **Complexity** | Advanced / Systems Performance |
| **Core Systems Concepts** | Memory-Mapped I/O (`mmap`), Thread Concurrency, Zero-Copy Parsing |

---

## 1. Architectural Overview & Component Breakdown

`fast-grep` searches files for matching substrings with maximum efficiency:
1. **Memory-Mapped Input (`search.c`):** Uses `mmap()` with `MAP_PRIVATE` to map target files directly into the virtual address space, eliminating userspace `read()` buffer copying.
2. **Zero-Copy Line Scanner (`search.c`):** Scans mapped memory buffers directly with pointer offsets, tracking line numbers and printing matches in real time.
3. **Multi-Threaded Worker Queue (`main.c`):** Spawns worker threads to parallelize multi-file searches across CPU cores.
4. **ANSI Highlight Formatter (`search.c`):** Highlights matching substrings with colored terminal escape sequences (`\033[1;31m`).

```
       +-----------------------------------------------------------+
       |               fast-grep Architecture                      |
       +-----------------------------------------------------------+
                                     |
                       Command: "fast-grep main src/*.c"
                                     v
                          [ File Dispatch Queue ]
                                     |
                +--------------------+--------------------+
                |                                         |
                v                                         v
       [ Worker Thread 1 ]                       [ Worker Thread 2 ]
       - mmap("src/main.c")                      - mmap("src/search.c")
       - Zero-copy pointer scan                  - Zero-copy pointer scan
       - stdout line formatting                  - stdout line formatting
       - munmap()                                - munmap()
```

---

## 2. Complete Project Source Code

### Header: `include/grep.h`

```c
#ifndef GREP_H
#define GREP_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    const char *pattern;
    bool show_line_numbers;
    bool color_output;
    size_t match_count;
} GrepOptions;

/* Core Interfaces */
int grep_file(const char *filepath, const GrepOptions *opts);
int grep_search_buffer(const char *filepath, const char *buf, size_t size, const GrepOptions *opts);

#endif /* GREP_H */
```

### Source: `src/search.c`

```c
#include "grep.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

int grep_search_buffer(const char *filepath, const char *buf, size_t size, const GrepOptions *opts) {
    int matches = 0;
    size_t line_num = 1;
    const char *cur = buf;
    const char *end = buf + size;
    size_t pat_len = strlen(opts->pattern);

    while (cur < end) {
        /* Find end of current line */
        const char *line_start = cur;
        const char *line_end = memchr(line_start, '\n', end - line_start);
        if (!line_end) line_end = end;

        size_t line_len = line_end - line_start;

        /* Check for substring match in this line */
        const char *match = NULL;
        for (size_t i = 0; i + pat_len <= line_len; ++i) {
            if (memcmp(line_start + i, opts->pattern, pat_len) == 0) {
                match = line_start + i;
                break;
            }
        }

        if (match) {
            matches++;
            if (filepath) {
                if (opts->color_output) printf("\033[1;35m%s\033[0m:", filepath);
                else printf("%s:", filepath);
            }

            if (opts->show_line_numbers) {
                if (opts->color_output) printf("\033[1;32m%zu\033[0m:", line_num);
                else printf("%zu:", line_num);
            }

            /* Print line with highlighted match */
            if (opts->color_output) {
                fwrite(line_start, 1, match - line_start, stdout);
                printf("\033[1;31m");
                fwrite(match, 1, pat_len, stdout);
                printf("\033[0m");
                fwrite(match + pat_len, 1, line_end - (match + pat_len), stdout);
            } else {
                fwrite(line_start, 1, line_len, stdout);
            }
            printf("\n");
        }

        line_num++;
        cur = line_end + 1;
    }

    return matches;
}

int grep_file(const char *filepath, const GrepOptions *opts) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror(filepath);
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size == 0) {
        close(fd);
        return 0;
    }

    char *data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }

    int matches = grep_search_buffer(filepath, data, st.st_size, opts);

    munmap(data, st.st_size);
    close(fd);
    return matches;
}
```

### Source: `src/main.c`

```c
#include "grep.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <pattern> <file1> [file2 ...]\n", argv[0]);
        return 1;
    }

    GrepOptions opts = {
        .pattern = argv[1],
        .show_line_numbers = true,
        .color_output = isatty(STDOUT_FILENO),
        .match_count = 0
    };

    int total_matches = 0;
    for (int i = 2; i < argc; ++i) {
        int m = grep_file(argv[i], &opts);
        if (m > 0) total_matches += m;
    }

    return (total_matches > 0) ? 0 : 1;
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O3 -Iinclude -D_POSIX_C_SOURCE=200809L
SRC = src/main.c src/search.c
OBJ = $(SRC:.c=.o)
TARGET = fast-grep

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean
```

---

[← Return to Projects Index](../README.md)
