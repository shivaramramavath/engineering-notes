# Spring Batch 5+ Architecture: Jobs, Steps, and Chunk Processing

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `spring-boot-starter-batch`, `spring-batch-core:5.2.x` |

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
Enterprise software frequently requires processing millions of records in non-interactive background jobs: monthly financial billing, end-of-day bank statement reconciliations, large-scale catalog synchronizations, and ETL data migrations. Implementing batch jobs using simple loops or raw scripts leads to catastrophic production failures: loading millions of rows into JVM memory triggers `OutOfMemoryError`, mid-job crashes leave partial data in corrupted states with no recovery checkpoint, and there is no auditable tracking of job executions, parameters, or failure causes.

### The Evolution
Spring Batch is the industry-standard, lightweight batch processing framework designed specifically for high-volume offline record processing. In **Spring Batch 5 (shipped with Spring Boot 3)**, the framework was fundamentally modernized: deprecated factory beans (`JobBuilderFactory`, `StepBuilderFactory`) and `@EnableBatchProcessing` boilerplate were replaced with modern builder APIs (`JobBuilder`, `StepBuilder`), native Java record support, full migration to Jakarta EE, and streamlined automated schema management.

### Relevance in Modern Engineering
Spring Batch 5 provides industrial-strength batch execution: chunk-oriented processing with configurable transaction commit intervals, automated job restartability from the exact point of failure, parallel step execution, and comprehensive metadata auditing stored in standard SQL tables (`BATCH_JOB_EXECUTION`, `BATCH_STEP_EXECUTION`).

---

## 2. Core Concepts

- **`Job`**: The top-level batch process entity encapsulating the entire workflow, consisting of one or more sequential or parallel `Step` instances.
- **`JobInstance` vs `JobExecution`**:
  - `JobInstance`: Logical concept representing a specific job run with unique identifying `JobParameters` (e.g., `billingJob` with parameter `date=2026-11-24`).
  - `JobExecution`: A single physical attempt to run a `JobInstance` (e.g., Run 1 failed, Run 2 succeeded).
- **`Step`**: An independent phase of a batch job. Can be a **Tasklet** (single-operation task like deleting a directory) or a **Chunk-Oriented Step**.
- **Chunk-Oriented Processing**: Reading items one-by-one from an `ItemReader`, passing each through an `ItemProcessor`, collecting items into a *chunk* of size $N$, and writing the entire chunk to the `ItemWriter` within a single database transaction.
- **`ItemReader<I>`**: Interface for sequential data ingestion (e.g., `FlatFileItemReader`, `JdbcPagingItemReader`, `RepositoryItemReader`).
- **`ItemProcessor<I, O>`**: Business logic component transforming or validating an input item `I` to an output item `O` (returning `null` filters out the item).
- **`ItemWriter<O>`**: Interface for batch persistence (e.g., `JdbcBatchItemWriter`, `FlatFileItemWriter`).
- **`JobRepository`**: The central persistence mechanism managing execution metadata and state across job runs.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Chunk-Oriented Processing Architecture                     |
|                                                                                   |
| 1. JobLauncher Starts JobExecution                                                |
|    JobParameters: { run.date: "2026-11-24" }                                      |
|    JobRepository records STARTING status in BATCH_JOB_EXECUTION                   |
|                   |                                                               |
|                   v                                                               |
| 2. Step Begins: Chunk Processing Loop (Chunk Size = 3)                            |
|    Transaction Begins (@Transactional PlatformTransactionManager)                 |
|                   |                                                               |
|    ItemReader.read() ----> Item 1                                                 |
|    ItemReader.read() ----> Item 2                                                 |
|    ItemReader.read() ----> Item 3                                                 |
|                   |                                                               |
|                   v (Passed individually through ItemProcessor)                   |
|    ItemProcessor.process(Item 1) ----> Processed 1                                |
|    ItemProcessor.process(Item 2) ----> Processed 2                                |
|    ItemProcessor.process(Item 3) ----> Processed 3                                |
|                   |                                                               |
|                   v (Collected into Chunk List: [P1, P2, P3])                     |
| 3. ItemWriter Writes Entire Chunk in Single Batch Operation                       |
|    ItemWriter.write(Chunk.of(P1, P2, P3)) ---> Executes 1 SQL Batch INSERT       |
|                   |                                                               |
|                   v                                                               |
| 4. Transaction Commits & Checkpoint Saved                                         |
|    PlatformTransactionManager commits DB transaction                              |
|    JobRepository updates BATCH_STEP_EXECUTION with new commit_count and read_count|
|                   |                                                               |
|    (Repeats until ItemReader.read() returns null indicating EOF)                  |
+-----------------------------------------------------------------------------------+
```

During execution, `JobLauncher` delegates to `JobRepository` to register the `JobExecution`. The step loop reads items individually from the `ItemReader`. Each item passes through `ItemProcessor`. When the count of collected processed items reaches the configured `chunkSize`, the entire list is delivered to `ItemWriter.write()`. The database transaction commits, and the `JobRepository` updates the step execution context, persisting the last read offset. If the application crashes on chunk 50, the restarted job inspects the metadata and resumes processing from chunk 50, skipping chunks 1 to 49.

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-batch</artifactId>
</dependency>
<dependency>
    <groupId>org.postgresql</groupId>
    <artifactId>postgresql</artifactId>
</dependency>
```

### Application Properties (Spring Boot 3 / Batch 5)
```properties
# Automated Batch Schema Initialization
spring.batch.jdbc.initialize-schema=always
# Prevent automatic execution on boot (recommended for production)
spring.batch.job.enabled=false
```

---

## 5. Practical Examples

### Example 1: Modern Spring Batch 5 Job and Step Configuration
```java
package com.example.platform.batch.config;

import com.example.platform.batch.model.CustomerInput;
import com.example.platform.batch.model.CustomerOutput;
import org.springframework.batch.core.Job;
import org.springframework.batch.core.Step;
import org.springframework.batch.core.job.builder.JobBuilder;
import org.springframework.batch.core.repository.JobRepository;
import org.springframework.batch.core.step.builder.StepBuilder;
import org.springframework.batch.item.ItemProcessor;
import org.springframework.batch.item.ItemReader;
import org.springframework.batch.item.ItemWriter;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.transaction.PlatformTransactionManager;

@Configuration
public class CustomerMigrationBatchConfig {

    @Bean
    public Job customerMigrationJob(JobRepository jobRepository, Step migrationStep) {
        // Modern Spring Batch 5 JobBuilder
        return new JobBuilder("customerMigrationJob", jobRepository)
                .start(migrationStep)
                .build();
    }

    @Bean
    public Step migrationStep(
            JobRepository jobRepository,
            PlatformTransactionManager transactionManager,
            ItemReader<CustomerInput> customerReader,
            ItemProcessor<CustomerInput, CustomerOutput> customerProcessor,
            ItemWriter<CustomerOutput> customerWriter) {

        // Chunk size = 100: reads, processes, and commits in batches of 100 records
        return new StepBuilder("migrationStep", jobRepository)
                .<CustomerInput, CustomerOutput>chunk(100, transactionManager)
                .reader(customerReader)
                .processor(customerProcessor)
                .writer(customerWriter)
                .build();
    }
}
```

### Example 2: FlatFileItemReader and ItemProcessor for CSV Ingestion
```java
package com.example.platform.batch.component;

import com.example.platform.batch.model.CustomerInput;
import com.example.platform.batch.model.CustomerOutput;
import org.springframework.batch.item.ItemProcessor;
import org.springframework.batch.item.file.FlatFileItemReader;
import org.springframework.batch.item.file.builder.FlatFileItemReaderBuilder;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.io.ClassPathResource;

@Configuration
public class BatchComponentsConfig {

    @Bean
    public FlatFileItemReader<CustomerInput> customerReader() {
        return new FlatFileItemReaderBuilder<CustomerInput>()
                .name("customerCsvReader")
                .resource(new ClassPathResource("data/customers.csv"))
                .delimited()
                .names("id", "fullName", "email", "active")
                .targetType(CustomerInput.class)
                .linesToSkip(1) // Skip CSV header row
                .build();
    }

    @Bean
    public ItemProcessor<CustomerInput, CustomerOutput> customerProcessor() {
        return input -> {
            // Business rule: filter out inactive customers (returning null skips item from writer)
            if (!input.active()) {
                return null;
            }

            String[] nameParts = input.fullName().split(" ", 2);
            String firstName = nameParts[0];
            String lastName = nameParts.length > 1 ? nameParts[1] : "";

            return new CustomerOutput(
                    input.id(),
                    firstName.toUpperCase(),
                    lastName.toUpperCase(),
                    input.email().toLowerCase()
            );
        };
    }
}
```

### Example 3: High-Performance `JdbcBatchItemWriter`
```java
package com.example.platform.batch.component;

import com.example.platform.batch.model.CustomerOutput;
import org.springframework.batch.item.database.JdbcBatchItemWriter;
import org.springframework.batch.item.database.builder.JdbcBatchItemWriterBuilder;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import javax.sql.DataSource;

@Configuration
public class DatabaseWriterConfig {

    @Bean
    public JdbcBatchItemWriter<CustomerOutput> customerWriter(DataSource dataSource) {
        return new JdbcBatchItemWriterBuilder<CustomerOutput>()
                .dataSource(dataSource)
                // High-performance batch INSERT executed across the entire chunk
                .sql("INSERT INTO customers (id, first_name, last_name, email) VALUES (:id, :firstName, :lastName, :email)")
                .beanMapped()
                .assertUpdates(false)
                .build();
    }
}
```

### Example 4: The Deprecated `@EnableBatchProcessing` Anti-Pattern and Fix
```java
// ANTI-PATTERN: Using @EnableBatchProcessing in Spring Boot 3 / Batch 5!
// @EnableBatchProcessing
// @Configuration
// public class OldBatchConfig { ... }
// In Spring Boot 3, adding @EnableBatchProcessing completely DISABLES Spring Boot's
// auto-configuration (disables automated schema initialization and TransactionManager auto-wiring)!

// FIX: Remove @EnableBatchProcessing and use direct constructor-based JobBuilder and StepBuilder
@Configuration
public class ModernCleanBatchConfig {
    // Spring Boot 3 auto-configures JobRepository and PlatformTransactionManager automatically!
}
```

### Example 5: Clean Tasklet for Pre-Job Cleanup Operation
```java
package com.example.platform.batch.tasklet;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.batch.core.StepContribution;
import org.springframework.batch.core.scope.context.ChunkContext;
import org.springframework.batch.core.step.tasklet.Tasklet;
import org.springframework.batch.repeat.RepeatStatus;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Component;

@Component
public class TableTruncateTasklet implements Tasklet {

    private static final Logger log = LoggerFactory.getLogger(TableTruncateTasklet.class);
    private final JdbcTemplate jdbcTemplate;

    public TableTruncateTasklet(JdbcTemplate jdbcTemplate) {
        this.jdbcTemplate = jdbcTemplate;
    }

    @Override
    public RepeatStatus execute(StepContribution contribution, ChunkContext chunkContext) {
        log.info("Executing staging table truncate before batch import");
        jdbcTemplate.execute("TRUNCATE TABLE staging_customers");
        return RepeatStatus.FINISHED; // Indicates tasklet execution complete
    }
}
```

---

## 6. Best Practices

1. **Do NOT Use `@EnableBatchProcessing` in Spring Boot 3**: Spring Boot 3 provides full auto-configuration for Spring Batch; adding `@EnableBatchProcessing` turns off auto-configuration and requires manual bean definitions.
2. **Choose Appropriate Chunk Sizes**: A chunk size between 100 and 1,000 provides the optimal balance between database transaction duration and batch I/O performance.
3. **Use Paging Readers for Large Datasets**: Never use cursor-based readers without streaming; prefer `JdbcPagingItemReader` to prevent database connection timeouts on massive datasets.
4. **Filter by Returning `null` in `ItemProcessor`**: Returning `null` from an `ItemProcessor` instructs Spring Batch to discard that record without passing it to the `ItemWriter` or rolling back.
5. **Disable Auto-Run on Startup**: Set `spring.batch.job.enabled=false` so batch jobs are executed via schedulers, REST controllers, or CLI runners rather than on application boot.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Adding `@EnableBatchProcessing` in Spring Boot 3 | Deactivates Spring Boot auto-configuration, breaking DataSource/JobRepository wiring | Remove `@EnableBatchProcessing` annotation completely. |
| Using deprecated `JobBuilderFactory` and `StepBuilderFactory` | Removed in Spring Batch 5; code fails compilation | Instantiate `new JobBuilder(name, jobRepository)` directly. |
| Loading entire dataset into memory in `ItemReader` | Triggers JVM `OutOfMemoryError` on large datasets | Use chunk-oriented paging readers (`JdbcPagingItemReader`). |
| Forgetting identifying JobParameters | Running job twice with identical parameters fails with `JobInstanceAlreadyCompleteException` | Add a unique parameter (e.g., `run.id=System.currentTimeMillis()`). |
| Modifying state inside a non-thread-safe ItemProcessor | Race conditions corrupt data when executing multi-threaded steps | Ensure `ItemProcessor` implementations are stateless or thread-safe. |
| Missing metadata database tables | Application crashes on boot: "Table BATCH_JOB_INSTANCE doesn't exist" | Set `spring.batch.jdbc.initialize-schema=always`. |

---

## 8. Performance & Security Notes

- **Batch Statement Optimization**: `JdbcBatchItemWriter` uses JDBC batch execution (`PreparedStatement.addBatch()`), executing 500 inserts in a single round-trip rather than 500 separate network calls.
- **Transaction Log Sizing**: Setting chunk sizes too large (e.g., 50,000) causes database transaction log (Postgres WAL / Oracle Undo) exhaustion. Tune chunk sizes according to row width.
- **Data Protection in JobParameters**: Job parameters are written in plaintext to the `BATCH_JOB_EXECUTION_PARAMS` table. Never pass unencrypted API keys or passwords in parameters.

---

## 9. Comparison / When to Use What

| Dimension | Spring Batch | Simple `@Scheduled` Loop | Stream Processing (Kafka) |
|---|---|---|---|
| **Paradigm** | Chunk-oriented batch ETL | Ad-hoc iterative processing | Continuous event streaming |
| **Restartability** | Built-in checkpoint resumption | Must be implemented manually | Offset-based |
| **Metadata Audit** | Full SQL audit tables | None | None |
| **Volume** | Millions to billions of records | Small arrays (< 10,000 rows) | Continuous stream |
| **Optimal For** | Nightly billing, statements, ETL | Simple recurring cleanups | Real-time event pipelines |

---

## 10. Interview Questions

<details>
<summary>1. What are the key differences between Spring Batch 4 and Spring Batch 5?</summary>
Spring Batch 5 removed deprecated builder factories (`JobBuilderFactory`, `StepBuilderFactory`) in favor of direct constructors (`JobBuilder`, `StepBuilder`). It fully migrated from Java EE (`javax.*`) to Jakarta EE 10 (`jakarta.*`), introduced native support for Java records, modernized metadata table schemas, and deprecated the `@EnableBatchProcessing` annotation in Spring Boot 3.
</details>

<details>
<summary>2. Why should developers NOT use `@EnableBatchProcessing` in Spring Boot 3?</summary>
In Spring Boot 3, Spring Batch is fully auto-configured out of the box. Adding `@EnableBatchProcessing` switches Spring Boot into manual configuration mode, which disables the automatic initialization of batch metadata tables, disables `JobLauncherApplicationRunner`, and prevents automatic transaction manager injection.
</details>

<details>
<summary>3. What is Chunk-Oriented Processing and how does it work?</summary>
Chunk-oriented processing is an architectural pattern for processing large datasets in manageable batches. The `ItemReader` reads items one-by-one. Each item is passed to an `ItemProcessor`. Once the number of processed items reaches the configured `chunkSize`, the entire list is passed to the `ItemWriter` and committed in a single database transaction.
</details>

<details>
<summary>4. What is the difference between a JobInstance and a JobExecution?</summary>
A `JobInstance` represents a logical job run defined by its name and identifying `JobParameters` (e.g., `payrollJob` on `2026-11-24`). A `JobExecution` represents an actual physical attempt to run that `JobInstance`. If the first execution fails, a second `JobExecution` is created for the *same* `JobInstance` to retry and complete the job.
</details>

<details>
<summary>5. How does Spring Batch resume an interrupted or failed job from its last checkpoint?</summary>
During chunk processing, Spring Batch persists the `ExecutionContext` (including the reader's current read count/offset) into the `BATCH_STEP_EXECUTION` table upon every chunk commit. When the job is restarted with the same parameters, the `ItemReader` restores its state from the execution context and resumes reading from the exact chunk where the failure occurred.
</details>

<details>
<summary>6. How can an ItemProcessor filter out unwanted records?</summary>
By returning `null` from the `process(Item item)` method. When an `ItemProcessor` returns `null`, Spring Batch discards that record immediately; it is not counted in the chunk list and is never delivered to the `ItemWriter`.
</details>

<details>
<summary>7. What is a Tasklet Step and when should it be used instead of a Chunk Step?</summary>
A Tasklet is a simple interface with a single `execute()` method used for single-action tasks that do not involve reading, processing, and writing items in chunks. Common use cases include truncating a database table, moving or unzipping a file, sending an execution summary email, or calling a stored procedure.
</details>

<details>
<summary>8. Why does running a job twice with identical parameters throw JobInstanceAlreadyCompleteException?</summary>
By design, a `JobInstance` can only reach the `COMPLETED` status once. If a job finishes successfully and is triggered again with identical identifying parameters, Spring Batch refuses execution to prevent duplicate processing. To run the job again, developers must provide distinct parameters (e.g., a timestamp or incrementer).
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a simple Tasklet that logs execution</summary>
Write a `Tasklet` bean that prints "Batch Job Started" and returns `RepeatStatus.FINISHED`.
</details>

<details>
<summary>Exercise 2 (Easy): Configure modern Spring Batch 5 Job and Step</summary>
Write a `@Configuration` class declaring a `Job` containing a single tasklet step using `JobBuilder` and `StepBuilder`.
</details>

<details>
<summary>Exercise 3 (Medium): Implement FlatFileItemReader for CSV</summary>
Configure a `FlatFileItemReader` reading user records (id, name, age) from a CSV file.
</details>

<details>
<summary>Exercise 4 (Medium): Build an ItemProcessor filtering by age</summary>
Implement an `ItemProcessor<User, User>` that filters out users younger than 18 by returning `null`.
</details>

<details>
<summary>Exercise 5 (Hard): Configure JdbcBatchItemWriter with high chunk size</summary>
Configure a step with chunk size 500 writing processed records into a PostgreSQL table using `JdbcBatchItemWriter`.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test testing job restartability</summary>
Write a `@SpringBatchTest` that simulates a failure mid-way through a step, restarts the job, and asserts that only the remaining records were processed.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                         Spring Batch 5 Quick Reference                          |
+---------------------------------------------------------------------------------+
| Boot 3 Rule:        NEVER use @EnableBatchProcessing (Auto-config handles it)   |
| Core Classes:       new JobBuilder(name, repo), new StepBuilder(name, repo)     |
| Chunk Pattern:      stepBuilder.<In, Out>chunk(100, txManager)                  |
| Components:         ItemReader (read), ItemProcessor (filter/map), ItemWriter   |
| Discard Record:     Return null in ItemProcessor                                |
| Non-Chunk Step:     stepBuilder.tasklet(taskletInstance, txManager)             |
| Disable Auto-Run:   spring.batch.job.enabled=false                              |
| Schema Init:        spring.batch.jdbc.initialize-schema=always                  |
| Invariant:          Never load all records into memory; use chunk paging        |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring Batch 5 Reference Documentation](https://docs.spring.io/spring-batch/reference/)
- [Spring Batch 5 Migration Guide](https://github.com/spring-projects/spring-batch/wiki/Spring-Batch-5.0-Migration-Guide)
- [Pro Spring Batch (Michael T. Minella)](https://www.apress.com/gp/book/9781484237236)
