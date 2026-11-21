# Project 07: Work-Stealing / FIFO Thread Pool (`thread-pool`)

> A production-grade concurrent worker thread pool in C utilizing POSIX threads (`pthreads`), condition variables, mutex synchronization, task queue scheduling, and synchronization barriers.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `07-thread-pool` |
| **Language Baseline** | C11 / POSIX.1-2008 |
| **Complexity** | Advanced / Concurrency |
| **Core Systems Concepts** | POSIX Threads, Mutexes, Condition Variables, Task Queues, Barriers |

---

## 1. Architectural Overview & Component Breakdown

The Thread Pool manages a fixed pool of persistent worker threads processing asynchronous tasks:
1. **Thread Worker Pool (`thread_pool.c`):** Spawns $N$ background threads that sleep efficiently on a condition variable when idle, consuming zero CPU cycles.
2. **Synchronized Task Queue (`thread_pool.c`):** Thread-safe circular FIFO queue protected by a mutex lock, queuing task function pointers and argument payloads (`void (*func)(void *arg)`).
3. **Task Completion Barrier (`thread_pool_wait`):** Blocks the calling thread until all submitted queue tasks have completed.
4. **Graceful Shutdown (`thread_pool_destroy`):** Signals all threads to terminate, flushes remaining tasks, and joins thread handles cleanly.

```
       +-----------------------------------------------------------+
       |               FIFO Thread Pool Architecture               |
       +-----------------------------------------------------------+
                                     |
               Client: thread_pool_submit(pool, task, arg)
                                     v
                        [ Mutex-Protected Task Queue ]
                                     |
                       pthread_cond_signal(&has_work)
                                     |
         +---------------------------+---------------------------+
         |                                                       |
         v                                                       v
[ Worker Thread 1 ]                                     [ Worker Thread 2 ]
- pthread_cond_wait(&has_work)                          - pthread_cond_wait(&has_work)
- Pop task from queue                                   - Pop task from queue
- Execute: task->func(task->arg)                        - Execute: task->func(task->arg)
- Decrement active tasks count                          - Decrement active tasks count
```

---

## 2. Complete Project Source Code

### Header: `include/thread_pool.h`

```c
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stddef.h>
#include <stdbool.h>

typedef struct ThreadPool ThreadPool;
typedef void (*TaskRoutine)(void *arg);

/* Thread Pool Lifecycle */
ThreadPool* thread_pool_create(size_t num_threads, size_t queue_capacity);
bool thread_pool_submit(ThreadPool *pool, TaskRoutine routine, void *arg);
void thread_pool_wait(ThreadPool *pool);
void thread_pool_destroy(ThreadPool **pool_ptr);

#endif /* THREAD_POOL_H */
```

### Source: `src/thread_pool.c`

```c
#include "thread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct Task {
    TaskRoutine routine;
    void *arg;
} Task;

struct ThreadPool {
    pthread_t *threads;
    size_t num_threads;

    Task *queue;
    size_t queue_cap;
    size_t queue_head;
    size_t queue_tail;
    size_t queue_size;

    size_t active_tasks;
    bool shutdown;

    pthread_mutex_t lock;
    pthread_cond_t notify_work;
    pthread_cond_t notify_empty;
    pthread_cond_t notify_all_idle;
};

static void* worker_loop(void *arg) {
    ThreadPool *pool = (ThreadPool*)arg;

    while (1) {
        pthread_mutex_lock(&pool->lock);

        while (pool->queue_size == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->notify_work, &pool->lock);
        }

        if (pool->shutdown && pool->queue_size == 0) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        /* Dequeue task */
        Task task = pool->queue[pool->queue_head];
        pool->queue_head = (pool->queue_head + 1) % pool->queue_cap;
        pool->queue_size--;
        pool->active_tasks++;

        pthread_cond_signal(&pool->notify_empty);
        pthread_mutex_unlock(&pool->lock);

        /* Execute task outside mutex lock */
        task.routine(task.arg);

        pthread_mutex_lock(&pool->lock);
        pool->active_tasks--;
        if (pool->queue_size == 0 && pool->active_tasks == 0) {
            pthread_cond_broadcast(&pool->notify_all_idle);
        }
        pthread_mutex_unlock(&pool->lock);
    }

    return NULL;
}

ThreadPool* thread_pool_create(size_t num_threads, size_t queue_capacity) {
    if (num_threads == 0 || queue_capacity == 0) return NULL;

    ThreadPool *pool = calloc(1, sizeof(ThreadPool));
    if (!pool) return NULL;

    pool->num_threads = num_threads;
    pool->queue_cap = queue_capacity;
    pool->queue = malloc(sizeof(Task) * queue_capacity);
    pool->threads = malloc(sizeof(pthread_t) * num_threads);

    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->notify_work, NULL);
    pthread_cond_init(&pool->notify_empty, NULL);
    pthread_cond_init(&pool->notify_all_idle, NULL);

    for (size_t i = 0; i < num_threads; ++i) {
        pthread_create(&pool->threads[i], NULL, worker_loop, pool);
    }

    return pool;
}

bool thread_pool_submit(ThreadPool *pool, TaskRoutine routine, void *arg) {
    if (!pool || !routine) return false;

    pthread_mutex_lock(&pool->lock);

    while (pool->queue_size == pool->queue_cap && !pool->shutdown) {
        pthread_cond_wait(&pool->notify_empty, &pool->lock);
    }

    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        return false;
    }

    pool->queue[pool->queue_tail] = (Task){ .routine = routine, .arg = arg };
    pool->queue_tail = (pool->queue_tail + 1) % pool->queue_cap;
    pool->queue_size++;

    pthread_cond_signal(&pool->notify_work);
    pthread_mutex_unlock(&pool->lock);
    return true;
}

void thread_pool_wait(ThreadPool *pool) {
    if (!pool) return;
    pthread_mutex_lock(&pool->lock);
    while (pool->queue_size > 0 || pool->active_tasks > 0) {
        pthread_cond_wait(&pool->notify_all_idle, &pool->lock);
    }
    pthread_mutex_unlock(&pool->lock);
}

void thread_pool_destroy(ThreadPool **pool_ptr) {
    if (!pool_ptr || !*pool_ptr) return;
    ThreadPool *pool = *pool_ptr;

    pthread_mutex_lock(&pool->lock);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->notify_work);
    pthread_mutex_unlock(&pool->lock);

    for (size_t i = 0; i < pool->num_threads; ++i) {
        pthread_join(pool->threads[i], NULL);
    }

    free(pool->threads);
    free(pool->queue);
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->notify_work);
    pthread_cond_destroy(&pool->notify_empty);
    pthread_cond_destroy(&pool->notify_all_idle);

    free(pool);
    *pool_ptr = NULL;
}
```

### Source: `src/main.c`

```c
#include "thread_pool.h"
#include <stdio.h>
#include <unistd.h>

static void sample_task(void *arg) {
    int id = *(int*)arg;
    printf("[Worker Thread] Processing task ID: %d\n", id);
    usleep(10000); /* 10ms simulated computation */
}

int main(void) {
    printf("===========================================\n");
    printf("   POSIX Concurrent Worker Thread Pool     \n");
    printf("===========================================\n");

    ThreadPool *pool = thread_pool_create(4, 16);
    int task_ids[8];

    printf("Submitting 8 tasks to 4 worker threads...\n");
    for (int i = 0; i < 8; ++i) {
        task_ids[i] = i + 1;
        thread_pool_submit(pool, sample_task, &task_ids[i]);
    }

    printf("Waiting for all tasks to complete...\n");
    thread_pool_wait(pool);
    printf("All 8 tasks completed successfully!\n");

    thread_pool_destroy(&pool);
    printf("Thread pool cleanly terminated.\n");
    return 0;
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude -pthread -D_POSIX_C_SOURCE=200809L
SRC = src/main.c src/thread_pool.c
OBJ = $(SRC:.c=.o)
TARGET = thread-pool

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -pthread -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean
```

---

[← Return to Projects Index](../README.md)
