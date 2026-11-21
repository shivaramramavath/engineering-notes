# Project 13: Lock-Free SPSC Ring Buffer Logger (`ring-logger`)

> An ultra-low-latency asynchronous logging system in C featuring a Single-Producer Single-Consumer (SPSC) lock-free ring buffer, atomic memory barriers, and asynchronous disk flushing.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `13-ring-buffer-logger` |
| **Language Baseline** | C11 / Standard Architecture |
| **Complexity** | Advanced / Concurrency |
| **Core Systems Concepts** | SPSC Lock-Free Queues, C11 Atomics, Memory Barriers, Async Logging |

---

## 1. Architectural Overview & Component Breakdown

The Ring Buffer Logger completely decouples high-speed application execution from slow disk I/O:
1. **Producer Side (Application Threads):** Writes structured log records into the circular ring buffer using `atomic_load_explicit` and `atomic_store_explicit` with acquire-release semantics. Operates completely lock-free without system calls.
2. **Consumer Side (Flusher Thread):** Runs in the background, waking periodically or on demand to batch-flush log entries to disk.
3. **Cache Line Isolation:** Aligns head and tail atomic pointers to distinct 64-byte cache lines to eliminate multi-core false sharing.

```
       +-----------------------------------------------------------+
       |             Lock-Free SPSC Logging Architecture           |
       +-----------------------------------------------------------+
                                     |
               Producer (App Thread): log_info("Order placed: #%d", id)
                                     v
                        [ Circular Ring Buffer ]
                      Head (Producer) ===> Tail (Consumer)
                                     |
                                     v
               Consumer Thread: flushes batch to disk (log.txt)
```

---

## 2. Complete Project Source Code

### Header: `include/logger.h`

```c
#ifndef LOGGER_H
#define LOGGER_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

/* Logger Lifecycle */
bool logger_init(const char *filepath, size_t ring_capacity);
void logger_shutdown(void);

/* Logging Functions */
void log_message(LogLevel level, const char *fmt, ...);

#endif /* LOGGER_H */
```

### Source: `src/logger.c`

```c
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MAX_LOG_MSG_LEN 256

typedef struct {
    LogLevel level;
    time_t timestamp;
    char message[MAX_LOG_MSG_LEN];
} LogRecord;

typedef struct {
    LogRecord *records;
    size_t capacity;

    _Alignas(64) atomic_size_t head; /* Written by Producer */
    _Alignas(64) atomic_size_t tail; /* Written by Consumer */

    FILE *file;
    pthread_t flusher_thread;
    atomic_bool running;
} RingLogger;

static RingLogger g_logger;

static const char* level_to_str(LogLevel lvl) {
    switch (lvl) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO ";
        case LOG_WARN:  return "WARN ";
        case LOG_ERROR: return "ERROR";
        default:        return "UNKWN";
    }
}

static void* flusher_worker(void *arg) {
    (void)arg;
    while (atomic_load_explicit(&g_logger.running, memory_order_relaxed)) {
        size_t head = atomic_load_explicit(&g_logger.head, memory_order_acquire);
        size_t tail = atomic_load_explicit(&g_logger.tail, memory_order_relaxed);

        while (tail != head) {
            LogRecord *rec = &g_logger.records[tail];
            char time_buf[32];
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&rec->timestamp));

            fprintf(g_logger.file, "[%s] [%s] %s\n", time_buf, level_to_str(rec->level), rec->message);
            tail = (tail + 1) % g_logger.capacity;
        }

        atomic_store_explicit(&g_logger.tail, tail, memory_order_release);
        fflush(g_logger.file);
        usleep(50000); /* 50ms flush interval */
    }
    return NULL;
}

bool logger_init(const char *filepath, size_t ring_capacity) {
    g_logger.file = fopen(filepath, "a");
    if (!g_logger.file) return false;

    g_logger.capacity = ring_capacity;
    g_logger.records = malloc(sizeof(LogRecord) * ring_capacity);
    atomic_init(&g_logger.head, 0);
    atomic_init(&g_logger.tail, 0);
    atomic_init(&g_logger.running, true);

    pthread_create(&g_logger.flusher_thread, NULL, flusher_worker, NULL);
    return true;
}

void log_message(LogLevel level, const char *fmt, ...) {
    size_t head = atomic_load_explicit(&g_logger.head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&g_logger.tail, memory_order_acquire);
    size_t next_head = (head + 1) % g_logger.capacity;

    if (next_head == tail) {
        /* Buffer full: drop message to prevent blocking producer */
        return;
    }

    LogRecord *rec = &g_logger.records[head];
    rec->level = level;
    rec->timestamp = time(NULL);

    va_list args;
    va_start(args, fmt);
    vsnprintf(rec->message, MAX_LOG_MSG_LEN, fmt, args);
    va_end(args);

    atomic_store_explicit(&g_logger.head, next_head, memory_order_release);
}

void logger_shutdown(void) {
    atomic_store_explicit(&g_logger.running, false, memory_order_relaxed);
    pthread_join(g_logger.flusher_thread, NULL);

    /* Final flush */
    size_t head = atomic_load_explicit(&g_logger.head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&g_logger.tail, memory_order_relaxed);
    while (tail != head) {
        LogRecord *rec = &g_logger.records[tail];
        fprintf(g_logger.file, "[SHUTDOWN] [%s] %s\n", level_to_str(rec->level), rec->message);
        tail = (tail + 1) % g_logger.capacity;
    }

    fclose(g_logger.file);
    free(g_logger.records);
}
```

### Source: `src/main.c`

```c
#include "logger.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("===========================================\n");
    printf("   Lock-Free SPSC Asynchronous Logger      \n");
    printf("===========================================\n");

    logger_init("app.log", 1024);

    printf("Emitting high-frequency log messages...\n");
    for (int i = 1; i <= 5; ++i) {
        log_message(LOG_INFO, "Transaction executed: ID=%d, Amount=$%d", i, i * 150);
        usleep(10000);
    }

    log_message(LOG_WARN, "High memory consumption detected: 85%%");
    log_message(LOG_ERROR, "Database failover triggered successfully");

    printf("Flushing to app.log and terminating...\n");
    logger_shutdown();
    printf("Logger cleanly stopped. Verify app.log for entries.\n");
    return 0;
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude -pthread -D_POSIX_C_SOURCE=200809L
SRC = src/main.c src/logger.c
OBJ = $(SRC:.c=.o)
TARGET = ring-logger

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -pthread -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET) app.log

.PHONY: all clean
```

---

[← Return to Projects Index](../README.md)
