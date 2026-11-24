# Spring Batch in Production: Fault Tolerance, Skips, Retries, and Scaling

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `spring-batch-core:5.2.x`, `SkipPolicy`, `RetryPolicy`, Virtual Threads |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
Processing millions of records in production environments is fraught with operational anomalies: dirty data rows (corrupt dates, missing required fields, unparseable numbers), transient network timeouts when invoking downstream payment or credit gateways, and strict SLA deadlines requiring batch jobs to complete within tight overnight maintenance windows. In naive batch jobs, encountering a single corrupt record among 5 million rows aborts the entire job, rolling back hours of processing and demanding urgent 3:00 AM on-call developer triage.

### The Evolution
Spring Batch provides production-grade fault-tolerance mechanisms out of the box: **Skip Policies** (safely bypassing bad records while logging them for review), **Retry Policies** (automatically retrying transient network or database lock exceptions with backoff), **Listeners** (monitoring execution milestones and capturing metrics), and **Scaling Architectures** (multi-threaded steps, parallel steps, remote chunking, and partitioners).

### Relevance in Modern Engineering
In enterprise data processing, 100% clean data does not exist. Production batch architectures must tolerate noisy real-world data gracefully, isolate corrupt records into audit dead-letter tables, scale horizontally across CPU cores or Kubernetes worker pods, and meet strict operational SLAs.

---

## 2. Core Concepts

- **Fault Tolerance**: Configuring a Step with `.faultTolerant()` to enable skip and retry capabilities.
- **Skip Policy (`.skip()`, `.skipLimit()`)**: Allows a step to bypass a specified maximum threshold of fatal exceptions (e.g., `FlatFileParseException`, `IllegalArgumentException`) without aborting the job.
- **SkipListener**: Callback interface (`onSkipInRead`, `onSkipInProcess`, `onSkipInWrite`) intercepting skipped items to log errors or persist them to a corrupt data audit table.
- **Retry Policy (`.retry()`, `.retryLimit()`)**: Retries transient exceptions (e.g., `DeadlockLoserDataAccessException`, `ConnectException`) up to $N$ times per item before failing or skipping.
- **Multi-Threaded Step**: Distributing chunk processing across a thread pool within a single JVM node via a `TaskExecutor`.
- **Partitioning**: Dividing a large dataset into discrete, independent subsets (partitions) processed concurrently by master/worker step configurations across multiple nodes or threads.
- **JobExplorer & JobOperator**: Operational APIs used by admin dashboards to inspect metadata, stop running executions, and trigger programmatic restarts.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Fault-Tolerant Step Execution Flow                         |
|                                                                                   |
| 1. Item Reading / Processing in Chunk Loop                                        |
|    ItemReader.read() ---> Returns Item                                            |
|                   |                                                               |
|                   v                                                               |
| 2. ItemProcessor Throws Exception (e.g. ValidationException)                      |
|    +---------------------------------------------------------------------+        |
|    | Fault-Tolerant Step Intercepts Exception                            |        |
|    +---------------------------------------------------------------------+        |
|           |                                                      |                |
|           | Exception is configured in .skip(...)                | Unhandled      |
|           v                                                      v                |
|    3. Evaluate Skip Limit                                      Abort Job          |
|       Current Skips < skipLimit(10)                             (Status = FAILED) |
|           |                                                                       |
|           v (Increment Skip Count in BATCH_STEP_EXECUTION)                        |
|    4. Invoke SkipListener.onSkipInProcess(item, ex)                               |
|       Log bad record to database/file -> Discard item from current chunk          |
|           |                                                                       |
|           v                                                                       |
|    5. Resume Reading Next Item (Step CONTINUES without rolling back!)             |
|                                                                                   |
| 6. ItemWriter Retry Logic (Transient Network Error)                               |
|    ItemWriter throws ConnectException                                             |
|    Step rolls back chunk, decreases chunk to size 1, retries item-by-item         |
|    Succeeds on retry -> Re-establishes normal chunk size                          |
+-----------------------------------------------------------------------------------+
```

When a step is marked `.faultTolerant()`, Spring Batch wraps chunk execution in fault-tolerant interceptors. If an exception occurs during reading, processing, or writing, the engine checks its classification. If classified as skippable and the current skip count is below `skipLimit`, the exception is swallowed, the item is dropped, and `SkipListener` is invoked. If an exception in the writer is retryable, Spring Batch rolls back the chunk transaction, scans the items one-by-one to identify which specific record caused the failure, retries the safe records, and commits.

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-batch</artifactId>
</dependency>
```

### Fault-Tolerant Step Configuration Syntax
```java
return new StepBuilder("productionStep", jobRepository)
        .<InputData, OutputData>chunk(100, transactionManager)
        .reader(reader)
        .processor(processor)
        .writer(writer)
        .faultTolerant()
        .skip(DataValidationException.class)
        .skipLimit(50)
        .retry(TransientDataAccessException.class)
        .retryLimit(3)
        .listener(new CustomSkipListener())
        .build();
```

---

## 5. Practical Examples

### Example 1: Enterprise Step with Skip, Retry, and Custom SkipListener
```java
package com.example.platform.batch.config;

import com.example.platform.batch.listener.AuditSkipListener;
import com.example.platform.batch.model.BillingRecord;
import com.example.platform.batch.model.Invoice;
import org.springframework.batch.core.Step;
import org.springframework.batch.core.repository.JobRepository;
import org.springframework.batch.core.step.builder.StepBuilder;
import org.springframework.batch.item.ItemProcessor;
import org.springframework.batch.item.ItemReader;
import org.springframework.batch.item.ItemWriter;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.dao.DeadlockLoserDataAccessException;
import org.springframework.transaction.PlatformTransactionManager;

import java.net.SocketTimeoutException;

public class InvalidBillingDataException extends RuntimeException {
    public InvalidBillingDataException(String msg) { super(msg); }
}

@Configuration
public class ProductionStepConfig {

    @Bean
    public Step robustBillingStep(
            JobRepository jobRepository,
            PlatformTransactionManager transactionManager,
            ItemReader<BillingRecord> reader,
            ItemProcessor<BillingRecord, Invoice> processor,
            ItemWriter<Invoice> writer,
            AuditSkipListener skipListener) {

        return new StepBuilder("robustBillingStep", jobRepository)
                .<BillingRecord, Invoice>chunk(200, transactionManager)
                .reader(reader)
                .processor(processor)
                .writer(writer)
                .faultTolerant()
                // Skip invalid data rows up to 100 times before failing
                .skip(InvalidBillingDataException.class)
                .skipLimit(100)
                // Retry transient database deadlocks up to 3 times per item
                .retry(DeadlockLoserDataAccessException.class)
                .retry(SocketTimeoutException.class)
                .retryLimit(3)
                // Audit skipped records for operational inspection
                .listener(skipListener)
                .build();
    }
}
```

### Example 2: Dedicated Audit SkipListener Persisting Corrupt Records
```java
package com.example.platform.batch.listener;

import com.example.platform.batch.model.BillingRecord;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.batch.core.SkipListener;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Component;

import java.sql.Timestamp;
import java.time.Instant;

@Component
public class AuditSkipListener implements SkipListener<BillingRecord, Object> {

    private static final Logger log = LoggerFactory.getLogger(AuditSkipListener.class);
    private final JdbcTemplate jdbcTemplate;

    public AuditSkipListener(JdbcTemplate jdbcTemplate) {
        this.jdbcTemplate = jdbcTemplate;
    }

    @Override
    public void onSkipInRead(Throwable t) {
        log.error("Corrupted record encountered during READ phase: {}", t.getMessage());
    }

    @Override
    public void onSkipInProcess(BillingRecord item, Throwable t) {
        log.warn("Skipping corrupt billing item [{}] during PROCESS phase: {}", item.recordId(), t.getMessage());
        
        // Persist to corrupt_records audit table for compliance and offline correction
        jdbcTemplate.update(
                "INSERT INTO corrupt_records (record_id, payload, error_message, skipped_at) VALUES (?, ?, ?, ?)",
                item.recordId(), item.toString(), t.getMessage(), Timestamp.from(Instant.now())
        );
    }

    @Override
    public void onSkipInWrite(Object item, Throwable t) {
        log.error("Fatal exception during WRITE phase for item [{}]: {}", item, t.getMessage());
    }
}
```

### Example 3: Multi-Threaded Step Scaling with Virtual Threads
```java
package com.example.platform.batch.config;

import com.example.platform.batch.model.BillingRecord;
import com.example.platform.batch.model.Invoice;
import org.springframework.batch.core.Step;
import org.springframework.batch.core.repository.JobRepository;
import org.springframework.batch.core.step.builder.StepBuilder;
import org.springframework.batch.item.ItemProcessor;
import org.springframework.batch.item.ItemReader;
import org.springframework.batch.item.ItemWriter;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.task.support.TaskExecutorAdapter;
import org.springframework.transaction.PlatformTransactionManager;

import java.util.concurrent.Executors;

@Configuration
public class ParallelStepConfig {

    @Bean
    public Step parallelBillingStep(
            JobRepository jobRepository,
            PlatformTransactionManager transactionManager,
            ItemReader<BillingRecord> synchronizedReader,
            ItemProcessor<BillingRecord, Invoice> processor,
            ItemWriter<Invoice> writer) {

        // Use Java 21 Virtual Threads for non-blocking concurrent chunk processing
        TaskExecutorAdapter virtualThreadExecutor =
                new TaskExecutorAdapter(Executors.newVirtualThreadPerTaskExecutor());

        return new StepBuilder("parallelBillingStep", jobRepository)
                .<BillingRecord, Invoice>chunk(100, transactionManager)
                .reader(synchronizedReader) // MUST BE THREAD-SAFE (e.g. SynchronizedItemStreamReader)
                .processor(processor)
                .writer(writer)
                .taskExecutor(virtualThreadExecutor) // Enables multi-threaded concurrent chunk processing
                .throttleLimit(16) // Max 16 concurrent chunks
                .build();
    }
}
```

### Example 4: Non-Thread-Safe ItemReader in Multi-Threaded Step (Anti-Pattern Fix)
```java
// ANTI-PATTERN: Using a standard non-thread-safe FlatFileItemReader or JdbcCursorItemReader
// inside a multi-threaded step (.taskExecutor()).
// Multiple worker threads call read() simultaneously, reading duplicate rows or corrupting cursor state!

// FIX: Wrap the reader with SynchronizedItemStreamReader or use a Paging reader
package com.example.platform.batch.util;

import com.example.platform.batch.model.BillingRecord;
import org.springframework.batch.item.ItemReader;
import org.springframework.batch.item.file.FlatFileItemReader;
import org.springframework.batch.item.support.SynchronizedItemStreamReader;
import org.springframework.batch.item.support.builder.SynchronizedItemStreamReaderBuilder;

public class ThreadSafeReaderFactory {
    public static SynchronizedItemStreamReader<BillingRecord> makeThreadSafe(FlatFileItemReader<BillingRecord> rawReader) {
        return new SynchronizedItemStreamReaderBuilder<BillingRecord>()
                .delegate(rawReader)
                .build();
    }
}
```

### Example 5: Operational Job Restarter via `JobOperator`
```java
package com.example.platform.batch.operator;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.batch.core.explore.JobExplorer;
import org.springframework.batch.core.launch.JobOperator;
import org.springframework.stereotype.Service;

@Service
public class BatchOperationsService {

    private static final Logger log = LoggerFactory.getLogger(BatchOperationsService.class);
    private final JobOperator jobOperator;
    private final JobExplorer jobExplorer;

    public BatchOperationsService(JobOperator jobOperator, JobExplorer jobExplorer) {
        this.jobOperator = jobOperator;
        this.jobExplorer = jobExplorer;
    }

    public long restartFailedJob(long failedExecutionId) {
        log.info("Attempting to restart failed JobExecution ID: {}", failedExecutionId);
        try {
            // Automatically recovers the execution context and resumes from the failed step/chunk
            long newExecutionId = jobOperator.restart(failedExecutionId);
            log.info("Job successfully resumed under new JobExecution ID: {}", newExecutionId);
            return newExecutionId;
        } catch (Exception e) {
            log.error("Failed to restart job execution: {}", failedExecutionId, e);
            throw new RuntimeException("Job restart failure", e);
        }
    }
}
```

---

## 6. Best Practices

1. **Always Implement a Dedicated `SkipListener`**: Never configure `.skip()` without an audit listener; skipping records silently creates data discrepancies between source files and destination databases.
2. **Ensure ItemReaders are Thread-Safe in Multi-Threaded Steps**: Wrap stateful readers with `SynchronizedItemStreamReader` or use `JdbcPagingItemReader` when attaching a `taskExecutor`.
3. **Keep `ItemProcessor` Stateless**: Multi-threaded and partitioned steps execute across concurrent threads; stateful processors cause race conditions and data corruption.
4. **Use Idempotent `ItemWriter` Implementations**: When retries occur, Spring Batch rolls back and retries chunk items individually; writers must safely support UPSERT operations.
5. **Monitor Metadata Tables**: Establish alerts on `BATCH_JOB_EXECUTION` for jobs in `FAILED` or `STOPPED` status.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Using non-thread-safe `ItemReader` with `taskExecutor` | Concurrent threads call `read()` simultaneously, resulting in missed or duplicate records | Wrap reader with `SynchronizedItemStreamReader`. |
| Setting `skipLimit` too high (e.g. 100,000) | A bug in the processor skips 90% of the entire file without failing the job | Set conservative skip limits (e.g., 50-100) to fail on systemic bugs. |
| Retrying non-transient exceptions (e.g., `NullPointerException`) | Wastes CPU retrying code bugs that can never succeed | Limit `.retry()` to transient classes (`SocketTimeoutException`, `TransientDataAccessException`). |
| Forgetting to make ItemWriter idempotent during retries | Individual item retries can duplicate records if writer does not support UPSERT | Use `INSERT ... ON CONFLICT DO UPDATE` or merge operations. |
| Losing restartability in multi-threaded steps | Multi-threaded steps do not maintain precise item restart offsets in metadata | If exact restartability is required, use Partitioning rather than multi-threaded steps. |
| Incomplete transactions during crash leaving orphaned data | Non-transactional resources outside the `PlatformTransactionManager` are not rolled back | Ensure all side-effect resources participate in the step's transaction manager. |

---

## 8. Performance & Security Notes

- **Multi-Threaded Steps vs Partitioning**: Multi-threaded steps share the same reader and writer within one JVM; Partitioning splits datasets into independent master/worker slices that can be distributed across multiple physical nodes.
- **Database Connection Pool Sizing**: When setting `throttleLimit(16)` on multi-threaded steps, ensure the `HikariCP` connection pool maximum size is at least $16 + 2$ to prevent connection acquisition deadlocks.
- **Audit Table Pruning**: Implement a scheduled maintenance routine to archive and delete rows in `BATCH_*` metadata tables older than 90 days.

---

## 9. Comparison / When to Use What

| Scaling Strategy | Multi-Threaded Step | Step Partitioning | Remote Chunking |
|---|---|---|---|
| **Architecture** | Single JVM thread pool | Master/Worker (Single or Multi-Node) | Master reader + Remote message workers |
| **Restartability** | Non-restartable from exact chunk | 100% restartable per partition | Restartable |
| **Implementation Complexity** | Minimal (`.taskExecutor()`) | Moderate (`Partitioner`) | High (Requires message broker / Spring Integration) |
| **Bottleneck** | CPU & Reader synchronization | Shard boundaries | Network latency transferring chunks |
| **Best For** | Fast processing on single multi-core server | Massive datasets partitioned by ID/Date | Heavy CPU-bound processing per item |

---

## 10. Interview Questions

<details>
<summary>1. How does Spring Batch handle exceptions when fault tolerance is enabled?</summary>
When a step is configured with `.faultTolerant()`, Spring Batch evaluates thrown exceptions against its skip and retry rules. If retryable, it retries the operation up to `retryLimit`. If skippable and `skipLimit` is not exceeded, the exception is swallowed, the item is recorded via `SkipListener`, and processing continues to the next item without failing the job.
</details>

<details>
<summary>2. Why does a multi-threaded step lose exact restartability?</summary>
In a multi-threaded step, multiple chunks are processed concurrently across different worker threads. Chunks do not commit in strict sequential order. If a crash occurs, the metadata cannot record a single continuous offset representing all processed records, meaning the restarted job must either re-process from the beginning or risk skipping items.
</details>

<details>
<summary>3. What is the difference between Multi-Threaded Steps and Partitioning in Spring Batch?</summary>
A Multi-Threaded Step uses a shared `ItemReader` and dispatches chunk processing to a thread pool within a single JVM. In Partitioning, a `Partitioner` divides the dataset into independent partitions (e.g., Partition 1: IDs 1-1000, Partition 2: IDs 1001-2000). Each partition runs as an independent Step with its own dedicated reader, processor, writer, and execution context, supporting full restartability and distributed multi-node execution.
</details>

<details>
<summary>4. What happens when an ItemWriter throws an exception in a chunk-oriented step?</summary>
By default, the entire chunk transaction rolls back. If fault tolerance and retry/skip are enabled, Spring Batch enters a "scan" mode: it decreases the chunk size to 1, processing and writing each item individually to identify which specific record caused the failure, skipping the bad record and committing the valid ones.
</details>

<details>
<summary>5. What is the role of the SkipListener interface?</summary>
`SkipListener` provides lifecycle callback methods (`onSkipInRead`, `onSkipInProcess`, `onSkipInWrite`) that are invoked whenever a record is skipped due to an exception. It is primarily used to log corrupt records, emit metrics, and write bad payloads to an error audit table.
</details>

<details>
<summary>6. How can an administrator stop a running Spring Batch job cleanly?</summary>
By calling `JobOperator.stop(executionId)`. Spring Batch sets the status in `BATCH_JOB_EXECUTION` to `STOPPING`. The running step detects the stop flag at the end of its current chunk, safely commits the current transaction, and terminates cleanly with status `STOPPED`, ready to be restarted later.
</details>

<details>
<summary>7. Why should you wrap a FlatFileItemReader in SynchronizedItemStreamReader when using multi-threading?</summary>
`FlatFileItemReader` is stateful (it tracks current line numbers and file pointers). If multiple threads call `read()` concurrently, race conditions occur, causing threads to read the same line multiple times or skip lines entirely. `SynchronizedItemStreamReader` synchronizes calls to `read()`, guaranteeing thread safety.
</details>

<details>
<summary>8. How does Spring Batch manage its metadata tables across database restarts?</summary>
Spring Batch maintains six core tables (`BATCH_JOB_INSTANCE`, `BATCH_JOB_EXECUTION`, `BATCH_JOB_EXECUTION_PARAMS`, `BATCH_STEP_EXECUTION`, `BATCH_STEP_EXECUTION_CONTEXT`, `BATCH_JOB_EXECUTION_CONTEXT`). These tables store execution statuses, parameters, and serialized execution contexts, ensuring full durability across application restarts.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure a fault-tolerant step with skip limit</summary>
Write a Step configuration with chunk size 50 that skips `NumberFormatException` up to 10 times.
</details>

<details>
<summary>Exercise 2 (Easy): Implement an onSkipInProcess listener</summary>
Create a class implementing `SkipListener` that logs the skipped item and exception message.
</details>

<details>
<summary>Exercise 3 (Medium): Configure retry with exponential backoff on ItemWriter</summary>
Configure a step that retries `DeadlockLoserDataAccessException` up to 3 times with exponential backoff before failing.
</details>

<details>
<summary>Exercise 4 (Medium): Build a multi-threaded step with Virtual Threads</summary>
Configure a Step that uses a `TaskExecutorAdapter` backed by Java 21 Virtual Threads and a thread-safe reader.
</details>

<details>
<summary>Exercise 5 (Hard): Implement a ColumnRangePartitioner for database sharding</summary>
Write a custom `Partitioner` that queries the minimum and maximum ID from a database table and divides the dataset into 4 distinct `ExecutionContext` partitions.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test testing skip recovery</summary>
Write a `@SpringBatchTest` with a dataset containing 2 bad records and verify that the job completes with `COMPLETED` status and exactly 2 skipped items.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                      Spring Batch Production Summary                            |
+---------------------------------------------------------------------------------+
| Fault Tolerance:    .faultTolerant().skip(Ex.class).skipLimit(50)               |
| Retries:            .retry(TransientEx.class).retryLimit(3)                     |
| Audit Listener:     .listener(new CustomSkipListener())                         |
| Multi-Threading:    .taskExecutor(executor).throttleLimit(16)                   |
| Reader Thread Safety: Wrap with SynchronizedItemStreamReader                    |
| Partitioning:       stepBuilder.partitioner("slaveStep", partitioner)           |
| Operator Control:   jobOperator.restart(executionId) / jobOperator.stop(id)     |
| Scaling Rule:       Multi-threading for single JVM; Partitioning for multi-node |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring Batch Scaling and Parallel Processing](https://docs.spring.io/spring-batch/reference/scalability.html)
- [Spring Batch Fault Tolerance and Skip Policies](https://docs.spring.io/spring-batch/reference/step/chunk-oriented-processing.html#fault-tolerance)
- [Database Partitioning with Spring Batch](https://spring.io/guides/gs/batch-processing/)
