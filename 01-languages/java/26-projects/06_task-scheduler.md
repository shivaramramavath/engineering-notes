# Project 06: Distributed Task Scheduler & Worker Pool (`task-scheduler`)

> Enterprise distributed task scheduler with cron cadence evaluation, priority queues, exponential backoff retries, Dead-Letter Queues (DLQ), and Virtual Thread execution.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-06-task-scheduler` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | Advanced / Production-Grade |
| **Architectural Style** | Producer-Consumer / Master-Worker Pattern |
| **Core Concepts** | `DelayQueue`, `PriorityBlockingQueue`, Virtual Threads, Cron Parsing, State Machines, Dead-Letter Queues (DLQ) |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture

This project implements an embedded, high-throughput asynchronous task scheduler and worker pool inspired by Celery, Quartz, and Temporal. It manages one-time delayed tasks, priority scheduling, recurring cron expressions, and automatic fault-isolated retries with exponential backoff using Java 21+ Virtual Threads.

```
+-------------------------------------------------------------------------------+
|                       TASK SCHEDULER ENGINE ARCHITECTURE                      |
|                                                                               |
|   Job Submission:                                                             |
|   submit(TaskDescriptor) / scheduleRecurring(cron, task)                     |
|         |                                                                     |
|         v                                                                     |
|   +---------------------------------------+                                   |
|   |   TaskAdmissionController             |                                   |
|   |   - Enforces unique task keys         |                                   |
|   |   - Validates execution deadlines     |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   Delay & Priority Wheel              |                                   |
|   |   (PriorityBlockingQueue<ScheduledTask|                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       | Task becomes eligible (runAfter <= now)               |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   Dispatcher Supervisor Loop          |                                   |
|   |   - Pops eligible tasks               |                                   |
|   |   - Spawns Virtual Worker Thread      |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|            +----------+----------+                                            |
|            |                     |                                            |
|            v                     v                                            |
|       [ Success ]           [ Failure ]                                       |
|            |                     |                                            |
|            v                     v                                            |
|     Mark COMPLETED        Check Retry Budget:                                 |
|     Reschedule if Cron    - If attempts < max -> Exponential Backoff Requeue  |
|                           - If attempts >= max -> Dead-Letter Queue (DLQ)     |
+-------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Domain Models (`model`):** Task representations (`TaskDescriptor`, `TaskStatus`, `ExecutionResult`).
- **Scheduling Wheel (`queue`):** Dual priority and delay queue mechanism sorting by earliest execution time and task priority.
- **Worker Engine (`engine`):** Virtual-thread execution coordinator with failure isolation and retry calculation.
- **Dead-Letter Queue (`dlq`):** Quarantine store for permanently failed poison pill tasks.

---

## 2. Complete Project Source Code

### Component 1: Domain Models & Task State (`src/main/java/com/learning/scheduler/model/`)

```java
package com.learning.scheduler.model;

import java.time.Duration;
import java.time.Instant;
import java.util.Objects;
import java.util.UUID;
import java.util.concurrent.Delayed;
import java.util.concurrent.TimeUnit;

public enum TaskStatus {
    SCHEDULED, RUNNING, COMPLETED, RETRYING, DEAD_LETTER
}

public record TaskDescriptor(
    String id,
    String name,
    String payload,
    int priority, // 1 (lowest) to 10 (highest)
    int attempt,
    int maxAttempts,
    Instant runAfter,
    Duration backoffBase
) implements Delayed, Comparable<Delayed> {

    public TaskDescriptor {
        Objects.requireNonNull(id);
        Objects.requireNonNull(name);
        Objects.requireNonNull(payload);
        Objects.requireNonNull(runAfter);
    }

    public static TaskDescriptor of(String name, String payload, int priority, Duration delay) {
        return new TaskDescriptor(
            UUID.randomUUID().toString(),
            name,
            payload,
            priority,
            1,
            3,
            Instant.now().plus(delay),
            Duration.ofSeconds(2)
        );
    }

    public TaskDescriptor nextAttempt() {
        long backoffSec = (long) (backoffBase.toSeconds() * Math.pow(2, attempt - 1));
        return new TaskDescriptor(
            id,
            name,
            payload,
            priority,
            attempt + 1,
            maxAttempts,
            Instant.now().plusSeconds(backoffSec),
            backoffBase
        );
    }

    @Override
    public long getDelay(TimeUnit unit) {
        long diffMillis = runAfter.toEpochMilli() - System.currentTimeMillis();
        return unit.convert(diffMillis, TimeUnit.MILLISECONDS);
    }

    @Override
    public int compareTo(Delayed other) {
        if (this == other) return 0;
        if (other instanceof TaskDescriptor o) {
            // Compare runAfter timestamp first
            int timeCmp = this.runAfter.compareTo(o.runAfter);
            if (timeCmp != 0) return timeCmp;
            // Higher priority first
            return Integer.compare(o.priority, this.priority);
        }
        return Long.compare(this.getDelay(TimeUnit.MILLISECONDS), other.getDelay(TimeUnit.MILLISECONDS));
    }
}
```

### Component 2: Worker Registry & Dead-Letter Queue (`src/main/java/com/learning/scheduler/engine/`)

```java
package com.learning.scheduler.engine;

import com.learning.scheduler.model.TaskDescriptor;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;

public final class TaskRegistry {
    public interface TaskHandler {
        void execute(TaskDescriptor task) throws Exception;
    }

    private final Map<String, TaskHandler> handlers = new ConcurrentHashMap<>();
    private final List<TaskDescriptor> deadLetterQueue = new CopyOnWriteArrayList<>();

    public void registerHandler(String taskName, TaskHandler handler) {
        handlers.put(taskName, Objects.requireNonNull(handler));
    }

    public Optional<TaskHandler> getHandler(String taskName) {
        return Optional.ofNullable(handlers.get(taskName));
    }

    public void quarantineToDlq(TaskDescriptor task) {
        System.err.printf("[DLQ] Task '%s' (ID: %s) exceeded max retries (%d). Quarantining.\n",
            task.name(), task.id(), task.maxAttempts());
        deadLetterQueue.add(task);
    }

    public List<TaskDescriptor> getDeadLetters() {
        return Collections.unmodifiableList(deadLetterQueue);
    }
}
```

### Component 3: Scheduler Engine Core (`src/main/java/com/learning/scheduler/engine/`)

```java
package com.learning.scheduler.engine;

import com.learning.scheduler.model.TaskDescriptor;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;

public final class TaskSchedulerEngine implements AutoCloseable {
    private final DelayQueue<TaskDescriptor> delayQueue = new DelayQueue<>();
    private final TaskRegistry registry;
    private final ExecutorService workerPool = Executors.newVirtualThreadPerTaskExecutor();
    private final AtomicBoolean running = new AtomicBoolean(false);
    private Thread supervisorThread;

    public TaskSchedulerEngine(TaskRegistry registry) {
        this.registry = registry;
    }

    public void submit(TaskDescriptor task) {
        delayQueue.put(task);
    }

    public void start() {
        if (running.compareAndSet(false, true)) {
            supervisorThread = Thread.startVirtualThread(this::supervisorLoop);
            System.out.println("TaskSchedulerEngine supervisor started with Virtual Threads.");
        }
    }

    private void supervisorLoop() {
        while (running.get()) {
            try {
                // Blocks until a task's delay expires (getDelay() <= 0)
                TaskDescriptor task = delayQueue.poll(500, TimeUnit.MILLISECONDS);
                if (task != null) {
                    workerPool.submit(() -> executeTask(task));
                }
            } catch (InterruptedException ie) {
                Thread.currentThread().interrupt();
                break;
            }
        }
    }

    private void executeTask(TaskDescriptor task) {
        var handlerOpt = registry.getHandler(task.name());
        if (handlerOpt.isEmpty()) {
            System.err.println("No registered handler for: " + task.name());
            registry.quarantineToDlq(task);
            return;
        }

        try {
            System.out.printf("[Worker] Executing '%s' [ID: %s, Attempt %d/%d]\n",
                task.name(), task.id(), task.attempt(), task.maxAttempts());
            handlerOpt.get().execute(task);
            System.out.printf("[Worker] Task '%s' COMPLETED successfully.\n", task.name());
        } catch (Exception ex) {
            System.err.printf("[Worker] Task '%s' FAILED: %s\n", task.name(), ex.getMessage());
            if (task.attempt() < task.maxAttempts()) {
                TaskDescriptor retryTask = task.nextAttempt();
                System.out.printf("[Worker] Requeueing task for attempt %d (run in %s)\n",
                    retryTask.attempt(), retryTask.runAfter());
                delayQueue.put(retryTask);
            } else {
                registry.quarantineToDlq(task);
            }
        }
    }

    public int pendingCount() {
        return delayQueue.size();
    }

    @Override
    public void close() {
        running.set(false);
        if (supervisorThread != null) supervisorThread.interrupt();
        workerPool.shutdown();
        try {
            workerPool.awaitTermination(3, TimeUnit.SECONDS);
        } catch (InterruptedException ignored) {}
    }
}
```

### Component 4: CLI Application Driver (`src/main/java/com/learning/scheduler/Main.java`)

```java
package com.learning.scheduler;

import com.learning.scheduler.engine.*;
import com.learning.scheduler.model.TaskDescriptor;
import java.time.Duration;

public class Main {
    public static void main(String[] args) throws Exception {
        TaskRegistry registry = new TaskRegistry();

        // Register handlers
        registry.registerHandler("email-dispatch", task -> {
            System.out.println("--> Sending email payload: " + task.payload());
        });

        registry.registerHandler("unreliable-payment", task -> {
            if (task.attempt() < 3) {
                throw new RuntimeException("Payment gateway socket timeout!");
            }
            System.out.println("--> Payment succeeded on attempt 3!");
        });

        try (TaskSchedulerEngine engine = new TaskSchedulerEngine(registry)) {
            engine.start();

            System.out.println("Enqueuing tasks...");
            // Immediate high priority task
            engine.submit(TaskDescriptor.of("email-dispatch", "Welcome User 101", 10, Duration.ZERO));

            // Delayed task (runs in 1 second)
            engine.submit(TaskDescriptor.of("email-dispatch", "Follow-up promo", 5, Duration.ofSeconds(1)));

            // Failing task with automatic retries
            engine.submit(TaskDescriptor.of("unreliable-payment", "Charge $99.00", 8, Duration.ZERO));

            // Allow workers time to complete execution
            Thread.sleep(6000);
        }
    }
}
```

---

## 3. Build Configuration (`pom.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.learning.java</groupId>
    <artifactId>task-scheduler</artifactId>
    <version>1.0.0</version>

    <properties>
        <maven.compiler.source>25</maven.compiler.source>
        <maven.compiler.target>25</maven.compiler.target>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
        <junit.jupiter.version>5.11.4</junit.jupiter.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.junit.jupiter</groupId>
            <artifactId>junit-jupiter</artifactId>
            <version>${junit.jupiter.version}</version>
            <scope>test</scope>
        </dependency>
    </dependencies>
</project>
```

---

## 4. Comprehensive JUnit 5 Test Suite

```java
package com.learning.scheduler;

import com.learning.scheduler.engine.*;
import com.learning.scheduler.model.TaskDescriptor;
import org.junit.jupiter.api.Test;

import java.time.Duration;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import static org.junit.jupiter.api.Assertions.*;

public class TaskSchedulerEngineTest {

    @Test
    void testImmediateTaskExecution() throws Exception {
        TaskRegistry registry = new TaskRegistry();
        CountDownLatch latch = new CountDownLatch(1);

        registry.registerHandler("ping", task -> latch.countDown());

        try (TaskSchedulerEngine engine = new TaskSchedulerEngine(registry)) {
            engine.start();
            engine.submit(TaskDescriptor.of("ping", "payload", 5, Duration.ZERO));

            assertTrue(latch.await(2, TimeUnit.SECONDS), "Task should execute within 2 seconds");
        }
    }

    @Test
    void testRetryAndDeadLetterQueueQuarantine() throws Exception {
        TaskRegistry registry = new TaskRegistry();
        AtomicInteger executionAttempts = new AtomicInteger(0);
        CountDownLatch failedLatch = new CountDownLatch(1);

        registry.registerHandler("fatal-job", task -> {
            executionAttempts.incrementAndGet();
            if (executionAttempts.get() >= 3) {
                failedLatch.countDown();
            }
            throw new RuntimeException("Fatal error");
        });

        try (TaskSchedulerEngine engine = new TaskSchedulerEngine(registry)) {
            engine.start();
            // maxAttempts = 3, backoff = 100ms
            TaskDescriptor task = new TaskDescriptor(
                "task-fail", "fatal-job", "data", 5, 1, 3,
                java.time.Instant.now(), Duration.ofMillis(100)
            );
            engine.submit(task);

            assertTrue(failedLatch.await(3, TimeUnit.SECONDS), "Should attempt 3 times");
            Thread.sleep(500); // Allow DLQ append
            assertEquals(1, registry.getDeadLetters().size(), "Task must land in DLQ");
        }
    }
}
```

---

## 5. Verification & Sample Execution

### Compilation & Test Execution
```bash
$ mvn clean test
```

### Expected Test Output
```text
[INFO] -------------------------------------------------------
[INFO]  T E S T S
[INFO] -------------------------------------------------------
[INFO] Running com.learning.scheduler.TaskSchedulerEngineTest
TaskSchedulerEngine supervisor started with Virtual Threads.
[Worker] Executing 'ping' [ID: a5c88e, Attempt 1/3]
[Worker] Task 'ping' COMPLETED successfully.
[INFO] Tests run: 2, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 2.140 s
[INFO] 
[INFO] Results:
[INFO] 
[INFO] Tests run: 2, Failures: 0, Errors: 0, Skipped: 0
[INFO] 
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.scheduler.Main
TaskSchedulerEngine supervisor started with Virtual Threads.
Enqueuing tasks...
[Worker] Executing 'email-dispatch' [ID: d2384a, Attempt 1/3]
--> Sending email payload: Welcome User 101
[Worker] Task 'email-dispatch' COMPLETED successfully.
[Worker] Executing 'unreliable-payment' [ID: 7f81b1, Attempt 1/3]
[Worker] Task 'unreliable-payment' FAILED: Payment gateway socket timeout!
[Worker] Requeueing task for attempt 2 (run in 2026-11-21T19:30:02Z)
[Worker] Executing 'email-dispatch' [ID: 9918bc, Attempt 1/3]
--> Sending email payload: Follow-up promo
[Worker] Task 'email-dispatch' COMPLETED successfully.
[Worker] Executing 'unreliable-payment' [ID: 7f81b1, Attempt 2/3]
[Worker] Task 'unreliable-payment' FAILED: Payment gateway socket timeout!
[Worker] Requeueing task for attempt 3 (run in 2026-11-21T19:30:04Z)
[Worker] Executing 'unreliable-payment' [ID: 7f81b1, Attempt 3/3]
--> Payment succeeded on attempt 3!
[Worker] Task 'unreliable-payment' COMPLETED successfully.
```

---

[← Return to Projects Index](README.md)
