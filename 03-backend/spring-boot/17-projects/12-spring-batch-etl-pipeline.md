# Project 12: High-Throughput Spring Batch ETL Data Pipeline

| Metadata Field | Value |
|---|---|
| **Spring Boot Version** | 3.4.2 |
| **Spring Framework** | 6.2.2 |
| **Java Version** | 21 (LTS) |
| **Build System** | Maven 3.9+ |
| **Key Dependencies** | Spring Boot Starter Batch, Spring Boot Starter Data JPA, PostgreSQL Driver, Testcontainers |
| **Architecture Style** | Chunk-Oriented Batch ETL Processing with Fault Tolerance, Skip/Retry Policies & Execution Metadata Tracking |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture

This project implements an enterprise batch ETL pipeline designed to ingest millions of external financial transaction records from flat CSV files, sanitize and transform the records via domain validators and currency converters, and write them in atomic chunks (chunk size: 1000) into PostgreSQL. It leverages Spring Batch 5+ metadata tables (`BATCH_JOB_INSTANCE`, `BATCH_JOB_EXECUTION`, `BATCH_STEP_EXECUTION`), tolerates up to 10 bad rows via `SkipPolicy`, and exposes a REST trigger endpoint.

```
       [ Client / Cron Trigger / REST ]
                       │
             (POST /api/v1/jobs/etl)
                       ▼
   ┌─────────────────────────────────────────────────────────┐
   │            Spring Batch 5 Job Launcher                  │
   │  Job: "financialTransactionEtlJob"                      │
   └───────────────────────────┬─────────────────────────────┘
                               │
                               ▼
   ┌─────────────────────────────────────────────────────────┐
   │                     Step: "etlStep"                     │
   │                                                         │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │ FlatFileItemReader<TransactionCsvRow>             │  │
   │  │ Reads line-by-line from source CSV file           │  │
   │  └─────────────────────────┬─────────────────────────┘  │
   │                            │ (Chunk: 1,000 items)       │
   │                            ▼                            │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │ ItemProcessor<TransactionCsvRow, CleanTransaction>│  │
   │  │ - Validates IBAN & currency                       │  │
   │  │ - Skips corrupt rows (FaultTolerant / SkipPolicy) │  │
   │  └─────────────────────────┬─────────────────────────┘  │
   │                            │                            │
   │                            ▼                            │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │ RepositoryItemWriter<CleanTransaction>            │  │
   │  │ Writes chunk into PostgreSQL via JDBC Batch       │  │
   │  └───────────────────────────────────────────────────┘  │
   └───────────────────────────┬─────────────────────────────┘
                               │
                               ▼
   ┌─────────────────────────────────────────────────────────┐
   │                 PostgreSQL Persistence                  │
   │  ┌─────────────────────────┐ ┌────────────────────────┐ │
   │  │ Spring Batch Metadata   │ │ Domain Data            │ │
   │  │ - BATCH_JOB_INSTANCE    │ │ - financial_tx_clean   │ │
   │  │ - BATCH_JOB_EXECUTION   │ │ - skipped_bad_records  │ │
   │  │ - BATCH_STEP_EXECUTION  │ │                        │ │
   │  └─────────────────────────┘ └────────────────────────┘ │
   └─────────────────────────────────────────────────────────┘
```

### 1.2 Directory Layout

```
12-spring-batch-etl-pipeline/
├── pom.xml
├── src/
│   ├── main/
│   │   ├── java/com/example/batch/
│   │   │   ├── BatchEtlApplication.java
│   │   │   ├── config/
│   │   │   │   └── FinancialEtlBatchConfig.java
│   │   │   ├── domain/
│   │   │   │   ├── TransactionCsvRow.java
│   │   │   │   ├── CleanTransaction.java
│   │   │   │   └── SkippedRecord.java
│   │   │   ├── listener/
│   │   │   │   └── EtlJobExecutionListener.java
│   │   │   ├── repository/
│   │   │   │   ├── CleanTransactionRepository.java
│   │   │   │   └── SkippedRecordRepository.java
│   │   │   └── web/
│   │   │       ├── BatchJobController.java
│   │   │       └── dto/JobStatusResponse.java
│   │   └── resources/
│   │       ├── application.yaml
│   │       ├── data/transactions_input.csv
│   │       └── db/migration/V1__init_batch_schema.sql
│   └── test/
│       └── java/com/example/batch/
│           └── BatchPipelineIntegrationTest.java
├── Dockerfile
└── compose.yaml
```

---

## 2. Complete Production Implementation

### 2.1 `pom.xml`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         https://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>
    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.4.2</version>
        <relativePath/>
    </parent>
    <groupId>com.example</groupId>
    <artifactId>batch-etl-pipeline</artifactId>
    <version>1.0.0</version>
    <name>batch-etl-pipeline</name>
    <description>Enterprise Chunk-Oriented Spring Batch 5 ETL Pipeline</description>

    <properties>
        <java.version>21</java.version>
        <testcontainers.version>1.20.4</testcontainers.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-batch</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-data-jpa</artifactId>
        </dependency>
        <dependency>
            <groupId>org.postgresql</groupId>
            <artifactId>postgresql</artifactId>
            <scope>runtime</scope>
        </dependency>

        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-test</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.springframework.batch</groupId>
            <artifactId>spring-batch-test</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-testcontainers</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.testcontainers</groupId>
            <artifactId>postgresql</artifactId>
            <scope>test</scope>
        </dependency>
    </dependencies>

    <build>
        <plugins>
            <plugin>
                <groupId>org.springframework.boot</groupId>
                <artifactId>spring-boot-maven-plugin</artifactId>
            </plugin>
        </plugins>
    </build>
</project>
```

### 2.2 `application.yaml`

```yaml
server:
  port: 8080

spring:
  application:
    name: batch-etl-pipeline
  datasource:
    url: jdbc:postgresql://${DB_HOST:localhost}:${DB_PORT:5432}/${DB_NAME:batch_db}
    username: ${DB_USER:postgres}
    password: ${DB_PASS:postgres}
  jpa:
    hibernate:
      ddl-auto: update
    open-in-view: false
    show-sql: false
  batch:
    jdbc:
      initialize-schema: always
    job:
      enabled: false # Do not auto-run on startup; triggered via REST

batch:
  etl:
    chunk-size: 500
    skip-limit: 20
```

### 2.3 Domain Entities & DTOs

```java
package com.example.batch.domain;

public record TransactionCsvRow(
        String rawTransactionId,
        String rawIban,
        String rawAmount,
        String rawCurrency,
        String rawTimestamp
) {}
```

```java
package com.example.batch.domain;

import jakarta.persistence.*;
import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "clean_transactions")
public class CleanTransaction {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(nullable = false, unique = true, length = 64)
    private String transactionId;

    @Column(nullable = false, length = 34)
    private String iban;

    @Column(nullable = false, precision = 12, scale = 2)
    private BigDecimal amount;

    @Column(nullable = false, length = 3)
    private String currency;

    @Column(nullable = false)
    private Instant processedAt = Instant.now();

    public CleanTransaction() {}

    public CleanTransaction(String transactionId, String iban, BigDecimal amount, String currency) {
        this.transactionId = transactionId;
        this.iban = iban;
        this.amount = amount;
        this.currency = currency;
    }

    public UUID getId() { return id; }
    public String getTransactionId() { return transactionId; }
    public String getIban() { return iban; }
    public BigDecimal getAmount() { return amount; }
    public String getCurrency() { return currency; }
}
```

```java
package com.example.batch.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "skipped_records")
public class SkippedRecord {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(nullable = false)
    private String rawData;

    @Column(nullable = false)
    private String reason;

    @Column(nullable = false)
    private Instant recordedAt = Instant.now();

    public SkippedRecord() {}

    public SkippedRecord(String rawData, String reason) {
        this.rawData = rawData;
        this.reason = reason;
    }

    public UUID getId() { return id; }
    public String getRawData() { return rawData; }
    public String getReason() { return reason; }
}
```

```java
package com.example.batch.repository;

import com.example.batch.domain.CleanTransaction;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.UUID;

public interface CleanTransactionRepository extends JpaRepository<CleanTransaction, UUID> {}
```

```java
package com.example.batch.repository;

import com.example.batch.domain.SkippedRecord;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.UUID;

public interface SkippedRecordRepository extends JpaRepository<SkippedRecord, UUID> {}
```

### 2.4 Spring Batch Configuration

```java
package com.example.batch.config;

import com.example.batch.domain.CleanTransaction;
import com.example.batch.domain.SkippedRecord;
import com.example.batch.domain.TransactionCsvRow;
import com.example.batch.repository.CleanTransactionRepository;
import com.example.batch.repository.SkippedRecordRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.batch.core.Job;
import org.springframework.batch.core.Step;
import org.springframework.batch.core.job.builder.JobBuilder;
import org.springframework.batch.core.launch.support.RunIdIncrementer;
import org.springframework.batch.core.repository.JobRepository;
import org.springframework.batch.core.step.builder.StepBuilder;
import org.springframework.batch.item.ItemProcessor;
import org.springframework.batch.item.data.RepositoryItemWriter;
import org.springframework.batch.item.file.FlatFileItemReader;
import org.springframework.batch.item.file.builder.FlatFileItemReaderBuilder;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.io.ClassPathResource;
import org.springframework.transaction.PlatformTransactionManager;

import java.math.BigDecimal;

@Configuration
public class FinancialEtlBatchConfig {

    private static final Logger log = LoggerFactory.getLogger(FinancialEtlBatchConfig.class);

    @Bean
    public FlatFileItemReader<TransactionCsvRow> transactionItemReader() {
        return new FlatFileItemReaderBuilder<TransactionCsvRow>()
                .name("transactionCsvReader")
                .resource(new ClassPathResource("data/transactions_input.csv"))
                .linesToSkip(1)
                .delimited()
                .names("rawTransactionId", "rawIban", "rawAmount", "rawCurrency", "rawTimestamp")
                .targetType(TransactionCsvRow.class)
                .build();
    }

    @Bean
    public ItemProcessor<TransactionCsvRow, CleanTransaction> transactionItemProcessor(SkippedRecordRepository skippedRepo) {
        return item -> {
            try {
                // Validation logic
                if (item.rawIban() == null || item.rawIban().length() < 10) {
                    throw new IllegalArgumentException("Invalid IBAN length: " + item.rawIban());
                }
                BigDecimal amount = new BigDecimal(item.rawAmount());
                if (amount.compareTo(BigDecimal.ZERO) <= 0) {
                    throw new IllegalArgumentException("Transaction amount must be positive: " + item.rawAmount());
                }

                return new CleanTransaction(
                        item.rawTransactionId(),
                        item.rawIban().toUpperCase(),
                        amount,
                        item.rawCurrency().toUpperCase()
                );
            } catch (Exception ex) {
                log.warn("Skipping corrupt transaction row: {} due to: {}", item.rawTransactionId(), ex.getMessage());
                skippedRepo.save(new SkippedRecord(item.toString(), ex.getMessage()));
                return null; // Return null to filter out corrupt record
            }
        };
    }

    @Bean
    public RepositoryItemWriter<CleanTransaction> transactionItemWriter(CleanTransactionRepository repository) {
        RepositoryItemWriter<CleanTransaction> writer = new RepositoryItemWriter<>();
        writer.setRepository(repository);
        writer.setMethodName("save");
        return writer;
    }

    @Bean
    public Step etlStep(JobRepository jobRepository,
                        PlatformTransactionManager transactionManager,
                        FlatFileItemReader<TransactionCsvRow> reader,
                        ItemProcessor<TransactionCsvRow, CleanTransaction> processor,
                        RepositoryItemWriter<CleanTransaction> writer) {
        return new StepBuilder("etlStep", jobRepository)
                .<TransactionCsvRow, CleanTransaction>chunk(500, transactionManager)
                .reader(reader)
                .processor(processor)
                .writer(writer)
                .faultTolerant()
                .skip(IllegalArgumentException.class)
                .skipLimit(20)
                .build();
    }

    @Bean
    public Job financialTransactionEtlJob(JobRepository jobRepository, Step etlStep) {
        return new JobBuilder("financialTransactionEtlJob", jobRepository)
                .incrementer(new RunIdIncrementer())
                .start(etlStep)
                .build();
    }
}
```

### 2.5 REST Trigger Controller

```java
package com.example.batch.web.dto;

public record JobStatusResponse(
        Long executionId,
        String jobName,
        String status,
        String exitCode
) {}
```

```java
package com.example.batch.web;

import com.example.batch.web.dto.JobStatusResponse;
import org.springframework.batch.core.Job;
import org.springframework.batch.core.JobExecution;
import org.springframework.batch.core.JobParameters;
import org.springframework.batch.core.JobParametersBuilder;
import org.springframework.batch.core.launch.JobLauncher;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/v1/jobs")
public class BatchJobController {

    private final JobLauncher jobLauncher;
    private final Job financialTransactionEtlJob;

    public BatchJobController(JobLauncher jobLauncher, Job financialTransactionEtlJob) {
        this.jobLauncher = jobLauncher;
        this.financialTransactionEtlJob = financialTransactionEtlJob;
    }

    @PostMapping("/etl")
    public ResponseEntity<JobStatusResponse> triggerEtlJob() throws Exception {
        JobParameters params = new JobParametersBuilder()
                .addLong("timestamp", System.currentTimeMillis())
                .toJobParameters();

        JobExecution execution = jobLauncher.run(financialTransactionEtlJob, params);

        return ResponseEntity.ok(new JobStatusResponse(
                execution.getId(),
                execution.getJobInstance().getJobName(),
                execution.getStatus().name(),
                execution.getExitStatus().getExitCode()
        ));
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.batch;

import com.example.batch.repository.CleanTransactionRepository;
import org.junit.jupiter.api.Test;
import org.springframework.batch.core.ExitStatus;
import org.springframework.batch.core.Job;
import org.springframework.batch.core.JobExecution;
import org.springframework.batch.test.JobLauncherTestUtils;
import org.springframework.batch.test.context.SpringBatchTest;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@SpringBatchTest
@Testcontainers
public class BatchPipelineIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private JobLauncherTestUtils jobLauncherTestUtils;

    @Autowired
    private Job financialTransactionEtlJob;

    @Autowired
    private CleanTransactionRepository transactionRepository;

    @Test
    void shouldExecuteBatchEtlJobSuccessfully() throws Exception {
        jobLauncherTestUtils.setJob(financialTransactionEtlJob);
        JobExecution jobExecution = jobLauncherTestUtils.launchJob();

        assertThat(jobExecution.getExitStatus().getExitCode()).isEqualTo(ExitStatus.COMPLETED.getExitCode());
    }
}
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
FROM maven:3.9.6-eclipse-temurin-21-alpine AS builder
WORKDIR /app
COPY pom.xml .
RUN mvn dependency:go-offline -B
COPY src ./src
RUN mvn clean package -DskipTests

FROM eclipse-temurin:21-jre-alpine
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
WORKDIR /app
COPY --from=builder /app/target/*.jar app.jar
USER appuser
EXPOSE 8080
ENTRYPOINT ["java", "-XX:+UseZGC", "-XX:+ZGenerational", "-jar", "app.jar"]
```

### 4.2 `compose.yaml`

```yaml
services:
  batch-db:
    image: postgres:16-alpine
    container_name: batch-db
    environment:
      POSTGRES_DB: batch_db
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: batchpassword
    ports:
      - "5432:5432"
    volumes:
      - batch_postgres_data:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5

  batch-app:
    build: .
    container_name: batch-app
    ports:
      - "8080:8080"
    environment:
      DB_HOST: batch-db
      DB_PORT: 5432
      DB_NAME: batch_db
      DB_USER: postgres
      DB_PASS: batchpassword
    depends_on:
      batch-db:
        condition: service_healthy

volumes:
  batch_postgres_data:
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Start application stack
docker compose up -d

# 2. Check health
curl -i http://localhost:8080/actuator/health

# 3. Trigger ETL batch job run
curl -i -X POST http://localhost:8080/api/v1/jobs/etl

# Response:
# HTTP/1.1 200 OK
# {
#   "executionId": 1,
#   "jobName": "financialTransactionEtlJob",
#   "status": "COMPLETED",
#   "exitCode": "COMPLETED"
# }
```
