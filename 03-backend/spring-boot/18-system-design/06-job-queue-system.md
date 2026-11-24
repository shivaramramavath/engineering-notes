# 06: Designing a Distributed Asynchronous Job Queue System

| Metadata Field | Value |
|---|---|
| **Topic** | Distributed Task Queues, Retry Mechanisms & Worker Pool Scaling |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Key Middleware** | RabbitMQ (DLX, Priority & Delayed Exchanges), Redis (Heartbeats), PostgreSQL |
| **Key Scale** | 50,000 Tasks / Minute, Exactly-Once Processing Semantics |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Practical Usage](#4-syntax--practical-usage)
5. [5 Practical Examples](#5-5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison / Decision Matrix](#9-comparison--decision-matrix)
10. [Real-World Interview Q&A](#10-real-world-interview-qa)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference Cheatsheet](#12-summary--quick-reference-cheatsheet)
13. [Further Reading & Official Resources](#13-further-reading--official-resources)

---

## 1. What & Why

In modern backend architectures, long-running, resource-intensive, or I/O-heavy operations (e.g., generating monthly PDF statements, computing AI embeddings, syncing warehouse inventory, firing bulk webhooks) cannot run synchronously within an HTTP request-response cycle. Doing so exhausts client timeouts and starves Web server threads.

A **Distributed Asynchronous Job Queue** decouples task submission from task execution. Producers enqueue tasks in milliseconds, while dedicated, independently scalable worker pools process jobs in the background with automatic retries, exponential backoffs, dead-letter routing, and fine-grained concurrency control.

---

## 2. Core Concepts

### 2.1 Delayed Message Exchanges & Dead Letter Exchanges (DLX)
- **Dead Letter Exchange (DLX)**: If a worker fails to process a message after $N$ attempts (or the message is rejected with `basic.nack(requeue = false)`), RabbitMQ automatically routes the poisoned message to a Dead Letter Exchange for manual inspection or dead-letter storage.
- **Exponential Backoff via TTL & DLX**: Tasks that fail transiently (e.g., network glitch) are republished to a retry queue with a Per-Message Time-To-Live (TTL). When the TTL expires without consumers, the message dead-letters back to the primary work queue, achieving automatic delayed retries without sleeping threads.

### 2.2 Worker Heartbeats & Task Leases
To prevent tasks from being abandoned if a worker pod crashes mid-execution:
- Workers operate under manual acknowledgements (`AcknowledgeMode.MANUAL`).
- If a consumer TCP connection drops before `basic.ack`, RabbitMQ immediately requeues the message for another worker.

---

## 3. Internals & Architecture

```
       [ Spring Boot REST API / Producers ]
                        │
             (Enqueue Task / HTTP 202)
                        ▼
   ┌────────────────────────────────────────────────────────┐
   │             RabbitMQ: 'tasks.direct' Exchange          │
   └────────────┬─────────────────────────────┬─────────────┘
                │                             │ (Routing Key: 'high')
                │ (Routing Key: 'default')    ▼
                │             ┌─────────────────────────────┐
                │             │ Queue: 'tasks.high-priority'│
                ▼             └──────────────┬──────────────┘
 ┌────────────────────────────┐              │
 │ Queue: 'tasks.default'     │              │
 └──────────────┬─────────────┘              │
                │                            │
                └──────────────┬─────────────┘
                               ▼
 ┌──────────────────────────────────────────────────────────┐
 │               Spring Boot Worker Pool                    │
 │  1. Consume Task (Virtual Threads)                       │
 │  2. Acquire Redis Idempotency Lock: "job:exec:{jobId}"   │
 │  3. Execute Heavy Processing (e.g. PDF Report)           │
 └──────────────────────┬───────────────────────────────────┘
                        │
         ┌──────────────┴──────────────┐
         ▼ (Success)                   ▼ (Failure after 3 retries)
   [ basic.ack ]               [ basic.nack(requeue = false) ]
                                       │
                                       ▼
                       ┌────────────────────────────────────┐
                       │ RabbitMQ Dead Letter Exchange (DLX)│
                       │ Exchange: 'tasks.dlx'              │
                       │ Queue: 'tasks.poison-pill'         │
                       └────────────────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 RabbitMQ Queue Configuration with DLX
```java
@Bean
public Queue defaultTasksQueue() {
    return QueueBuilder.durable("tasks.default")
            .withArgument("x-dead-letter-exchange", "tasks.dlx")
            .withArgument("x-dead-letter-routing-key", "tasks.poison")
            .build();
}
```

---

## 5. 5 Practical Examples

### Example 1: Robust RabbitMQ Infrastructure Configuration

```java
package com.example.queue.config;

import org.springframework.amqp.core.*;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class JobQueueRabbitConfig {

    public static final String TASK_EXCHANGE = "tasks.exchange";
    public static final String TASK_QUEUE = "tasks.default";
    public static final String DLX_EXCHANGE = "tasks.dlx";
    public static final String DLQ_QUEUE = "tasks.dead-letter";

    @Bean
    public DirectExchange taskExchange() {
        return new DirectExchange(TASK_EXCHANGE, true, false);
    }

    @Bean
    public DirectExchange deadLetterExchange() {
        return new DirectExchange(DLX_EXCHANGE, true, false);
    }

    @Bean
    public Queue deadLetterQueue() {
        return QueueBuilder.durable(DLQ_QUEUE).build();
    }

    @Bean
    public Binding deadLetterBinding() {
        return BindingBuilder.bind(deadLetterQueue()).to(deadLetterExchange()).with("dead-letter");
    }

    @Bean
    public Queue taskQueue() {
        return QueueBuilder.durable(TASK_QUEUE)
                .withArgument("x-dead-letter-exchange", DLX_EXCHANGE)
                .withArgument("x-dead-letter-routing-key", "dead-letter")
                .build();
    }

    @Bean
    public Binding taskBinding() {
        return BindingBuilder.bind(taskQueue()).to(taskExchange()).with("task.process");
    }
}
```

### Example 2: Job Dispatcher Service

```java
package com.example.queue.service;

import com.example.queue.config.JobQueueRabbitConfig;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.amqp.rabbit.core.RabbitTemplate;
import org.springframework.stereotype.Service;

import java.util.Map;
import java.util.UUID;

@Service
public class JobDispatcherService {

    private static final Logger log = LoggerFactory.getLogger(JobDispatcherService.class);
    private final RabbitTemplate rabbitTemplate;

    public JobDispatcherService(RabbitTemplate rabbitTemplate) {
        this.rabbitTemplate = rabbitTemplate;
    }

    public record JobSubmissionResponse(String jobId, String status) {}

    public JobSubmissionResponse submitJob(String taskType, Map<String, Object> payload) {
        String jobId = UUID.randomUUID().toString();
        log.info("Submitting asynchronous job {} of type: {}", jobId, taskType);

        Map<String, Object> message = Map.of(
                "jobId", jobId,
                "taskType", taskType,
                "payload", payload,
                "attempt", 1
        );

        rabbitTemplate.convertAndSend(
                JobQueueRabbitConfig.TASK_EXCHANGE,
                "task.process",
                message
        );

        return new JobSubmissionResponse(jobId, "QUEUED");
    }
}
```

### Example 3: Manual-Ack Worker Listener with Idempotency

```java
package com.example.queue.worker;

import com.rabbitmq.client.Channel;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.amqp.rabbit.annotation.RabbitListener;
import org.springframework.amqp.support.AmqpHeaders;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.messaging.handler.annotation.Header;
import org.springframework.messaging.handler.annotation.Payload;
import org.springframework.stereotype.Component;

import java.io.IOException;
import java.time.Duration;
import java.util.Map;

@Component
public class AsyncJobWorker {

    private static final Logger log = LoggerFactory.getLogger(AsyncJobWorker.class);
    private final StringRedisTemplate redisTemplate;

    public AsyncJobWorker(StringRedisTemplate redisTemplate) {
        this.redisTemplate = redisTemplate;
    }

    @RabbitListener(queues = "tasks.default", ackMode = "MANUAL")
    public void processTask(@Payload Map<String, Object> task,
                            Channel channel,
                            @Header(AmqpHeaders.DELIVERY_TAG) long deliveryTag) throws IOException {

        String jobId = (String) task.get("jobId");
        log.info("Worker received task: {}", jobId);

        // Deduplication check via Redis
        Boolean isFirst = redisTemplate.opsForValue().setIfAbsent("job:exec:" + jobId, "PROCESSING", Duration.ofHours(2));
        if (Boolean.FALSE.equals(isFirst)) {
            log.warn("Job {} has already been processed or is currently running. Dropping duplicate.", jobId);
            channel.basicAck(deliveryTag, false);
            return;
        }

        try {
            // Execute business logic (e.g., report generation)
            executeBusinessLogic(task);

            // Acknowledge successful processing
            channel.basicAck(deliveryTag, false);
            log.info("Task {} successfully completed and acknowledged.", jobId);

        } catch (Exception ex) {
            log.error("Task {} failed with error: {}. Rejecting to DLX...", jobId, ex.getMessage());
            // nack with requeue=false routes message to Dead Letter Exchange
            channel.basicNack(deliveryTag, false, false);
        }
    }

    private void executeBusinessLogic(Map<String, Object> task) throws InterruptedException {
        // Simulating heavy CPU or I/O work
        Thread.sleep(200);
    }
}
```

### Example 4: Delayed Queue Republisher for Exponential Backoff

```java
package com.example.queue.retry;

import org.springframework.amqp.core.AmqpTemplate;
import org.springframework.stereotype.Component;

import java.util.Map;

@Component
public class DelayedRetryPublisher {

    private final AmqpTemplate amqpTemplate;

    public DelayedRetryPublisher(AmqpTemplate amqpTemplate) {
        this.amqpTemplate = amqpTemplate;
    }

    public void republishWithBackoff(String exchange, String routingKey, Map<String, Object> task, int attempt) {
        int delayMs = (int) (Math.pow(2, attempt) * 1000); // 2s, 4s, 8s...
        task.put("attempt", attempt + 1);

        amqpTemplate.convertAndSend(exchange, routingKey, task, message -> {
            message.getMessageProperties().setDelayLong((long) delayMs);
            return message;
        });
    }
}
```

### Example 5: Job Status Tracking Entity

```java
package com.example.queue.domain;

import jakarta.persistence.*;
import java.time.Instant;

@Entity
@Table(name = "job_executions")
public class JobExecutionRecord {

    @Id
    @Column(name = "job_id", length = 64)
    private String jobId;

    @Column(name = "task_type", nullable = false)
    private String taskType;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    private JobStatus status;

    @Column(name = "error_details", columnDefinition = "TEXT")
    private String errorDetails;

    @Column(name = "updated_at", nullable = false)
    private Instant updatedAt = Instant.now();

    public enum JobStatus { QUEUED, PROCESSING, COMPLETED, FAILED }

    public JobExecutionRecord() {}

    public JobExecutionRecord(String jobId, String taskType, JobStatus status) {
        this.jobId = jobId;
        this.taskType = taskType;
        this.status = status;
    }

    // Getters and Setters
    public String getJobId() { return jobId; }
    public JobStatus getStatus() { return status; }
    public void setStatus(JobStatus status) { 
        this.status = status; 
        this.updatedAt = Instant.now();
    }
}
```

---

## 6. Best Practices

- **Always Use Manual Acknowledgements**: Never configure auto-ack (`ackMode = NONE`); if a worker pod terminates unexpectedly, in-flight messages will be lost permanently.
- **Enforce Worker Concurrency Boundaries**: Set `prefetchCount` (e.g. 10–20) on RabbitMQ listeners so a single worker does not greedily consume all messages while others sit idle.
- **Implement Distributed Idempotency**: Due to network failures during `basic.ack`, messages can be redelivered. Guard worker logic using atomic Redis locks or DB unique keys.
- **Never Block Indefinitely**: Set reasonable timeouts on all database and external HTTP operations within the worker.
- **Monitor Queue Depth**: Configure alerts on `messages_ready` in RabbitMQ; if ready counts spike, trigger Kubernetes HPA to scale up worker pods.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Auto-Acking on Receipt** | If the worker pod crashes 2 seconds into processing, the message is lost forever. | Use `ackMode = MANUAL` and call `basic.ack()` only after the transaction commits. |
| **Infinite Requeue on Failure** | Poison pill messages with formatting bugs trigger endless retry loops, pegging CPU at 100%. | Set `basic.nack(requeue = false)` and route to Dead Letter Exchange after $N$ attempts. |
| **Prefetch Unset (Default: Unlimited)** | One worker downloads all 10,000 tasks into memory; other idle workers receive nothing. | Set `spring.rabbitmq.listener.simple.prefetch: 10`. |
| **Large Message Payloads in Queue** | Sending 50 MB PDFs inside AMQP message payloads degrades RabbitMQ RAM and clustering performance. | Send file S3 keys in messages; download files on demand in the worker. |
| **Synchronous Thread Sleep for Retries** | Calling `Thread.sleep(5000)` inside the worker ties up the consumer thread unnecessarily. | Use RabbitMQ Delayed Exchange or TTL Dead-Lettering for non-blocking delays. |
| **Unmonitored Dead Letter Queues** | Poison messages accumulate silently until disk space fills up. | Hook DLQs into Slack/PagerDuty alerts for engineering visibility. |

---

## 8. Performance & Security Considerations

### 8.1 Prefetch Count Tuning
Tuning prefetch prevents starvation and maximizes throughput:
$$\text{Optimal Prefetch} = \frac{\text{Average Task Processing Time}}{\text{Network Roundtrip Time (RTT)}}$$
For short tasks (10ms): Prefetch = 50. For long tasks (5s): Prefetch = 1.

### 8.2 RabbitMQ Clustering & High Availability (Quorum Queues)
Use RabbitMQ **Quorum Queues** (Raft consensus) rather than classic mirrored queues for critical enterprise tasks to prevent split-brain data loss during network partitions.

---

## 9. Comparison / Decision Matrix

| Dimension | RabbitMQ | Apache Kafka | AWS SQS |
|---|---|---|---|
| **Delivery Model** | Push (Broker pushes to workers) | Pull (Consumer polls log offsets) | Pull (Polling) |
| **Message Routing** | Advanced (Direct, Topic, Fanout, Headers) | Simple (Partition Key) | Basic |
| **Per-Message Ack** | Yes (`basic.ack` / `basic.nack`) | No (Partition offset commit) | Yes (`DeleteMessage`) |
| **Replayability** | No (Messages deleted after ack) | Yes (Retained by time/size) | No |
| **Ideal Use Case** | Complex job queues, task workers | Event streaming, log analytics | Serverless AWS native apps |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. Why is RabbitMQ preferred over Kafka for asynchronous task queues?</strong></summary>

RabbitMQ supports granular per-message acknowledgements, dead letter routing, priority queues, and complex exchange routing. Kafka processes partitions in order, meaning a single failing task can block the entire partition.
</details>

<details>
<summary><strong>2. What does `prefetchCount` do in RabbitMQ?</strong></summary>

It specifies how many unacknowledged messages the broker will deliver to a worker at once. A low prefetch (e.g., 1–5) ensures fair work distribution across workers for long-running jobs.
</details>

<details>
<summary><strong>3. How do you implement non-blocking exponential backoff in RabbitMQ?</strong></summary>

Publish the failed message to a retry queue configured with a Per-Message TTL and DLX pointing back to the main queue. When the TTL expires, the broker automatically transfers the message back to the primary queue.
</details>

<details>
<summary><strong>4. How do you prevent duplicate job execution if a worker crashes before sending an ack?</strong></summary>

The next worker will receive the redelivered message (with `redelivered = true`). The worker checks an atomic state store (Redis or database) to verify if the task has already completed. If yes, it immediately acknowledges and discards the message.
</details>

<details>
<summary><strong>5. What is the difference between Quorum Queues and Classic Mirrored Queues in RabbitMQ?</strong></summary>

Quorum queues use the Raft consensus algorithm to provide strong consistency and data safety during network partitions, whereas classic mirrored queues are prone to synchronization blockages and data loss.
</details>

<details>
<summary><strong>6. How do you autoscale worker pods based on queue backlog?</strong></summary>

Deploy KEDA (Kubernetes Event-driven Autoscaling) configured with a RabbitMQ scaler that monitors queue depth (`messages_ready`). When depth exceeds a threshold (e.g., 100 tasks per pod), KEDA provisions more pods.
</details>

<details>
<summary><strong>7. How do you handle poison pill messages?</strong></summary>

Track attempt counts in the message headers. When `attempt > maxRetries`, reject the message without requeueing (`basic.nack(false, false)`), which triggers Dead Letter Exchange (DLX) forwarding.
</details>

<details>
<summary><strong>8. Why should large binary files never be passed directly inside queue messages?</strong></summary>

Passing large binaries (e.g. 20 MB files) exhausts broker RAM, slows down serialization, and impairs queue replication. Pass an S3 object key instead, and let the worker fetch the file directly.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure RabbitMQ Quorum Queue in Spring Boot</strong></summary>

**Prompt**: Declare a durable Quorum Queue using Spring AMQP `QueueBuilder`.

**Solution**:
```java
@Bean
public Queue quorumJobQueue() {
    return QueueBuilder.durable("tasks.quorum")
            .withArgument("x-queue-type", "quorum")
            .build();
}
```
</details>

<details>
<summary><strong>Exercise 2: Implement Manual Ack Consumer</strong></summary>

**Prompt**: Write a `@RabbitListener` method that explicitly acknowledges successful processing and nacks on error.

**Solution**:
Inspect Example 3: inject `Channel` and `@Header(AmqpHeaders.DELIVERY_TAG)`, call `channel.basicAck()` in try block and `channel.basicNack()` in catch block.
</details>

<details>
<summary><strong>Exercise 3: Configure Listener Container Prefetch</strong></summary>

**Prompt**: Set listener prefetch to 5 in `application.yaml`.

**Solution**:
```yaml
spring:
  rabbitmq:
    listener:
      simple:
        prefetch: 5
        acknowledge-mode: manual
```
</details>

<details>
<summary><strong>Exercise 4: Build a Job Status API Endpoint</strong></summary>

**Prompt**: Implement a REST controller endpoint returning the execution status of a background job by ID.

**Solution**:
```java
@GetMapping("/api/v1/jobs/{jobId}")
public ResponseEntity<JobStatusResponse> getJobStatus(@PathVariable String jobId) {
    return jobRepository.findById(jobId)
            .map(r -> ResponseEntity.ok(new JobStatusResponse(r.getJobId(), r.getStatus().name())))
            .orElse(ResponseEntity.notFound().build());
}
```
</details>

<details>
<summary><strong>Exercise 5: Implement Redis Job Lock</strong></summary>

**Prompt**: Write a method that acquires an exclusive 10-minute lock for a given job execution ID.

**Solution**:
```java
public boolean acquireJobLock(String jobId) {
    return Boolean.TRUE.equals(
        redis.opsForValue().setIfAbsent("lock:job:" + jobId, "RUNNING", Duration.ofMinutes(10))
    );
}
```
</details>

<details>
<summary><strong>Exercise 6: Test Dead Letter Routing in Testcontainers</strong></summary>

**Prompt**: Write an integration test using Testcontainers RabbitMQ throwing an exception in a listener and asserting that the message arrives in the dead-letter queue.

**Solution**:
Publish a bad message -> wait for consumer to nack -> read from `tasks.dead-letter` queue using `rabbitTemplate.receiveAndConvert()` -> assert message is present.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   DISTRIBUTED JOB QUEUE CHEATSHEET                     │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Broker Choice: RabbitMQ Quorum Queues for per-message acks & DLX    │
│ 2. Acknowledge Mode: Always MANUAL; ack after DB transaction commits   │
│ 3. Prefetch: 1-10 for long tasks; prevents worker starving/hoarding    │
│ 4. Error Handling: DLX for poison pills; exponential backoff via TTL   │
│ 5. Autoscaling: Scale worker pods using KEDA based on ready queue depth│
│ 6. Payload Rule: Pass metadata & S3 keys; never pass large binaries    │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring AMQP / RabbitMQ Reference Documentation](https://docs.spring.io/spring-amqp/reference/)
- [RabbitMQ Quorum Queues Official Guide](https://www.rabbitmq.com/docs/quorum-queues)
- [KEDA (Kubernetes Event-driven Autoscaling)](https://keda.sh/)
- [Enterprise Integration Patterns (Gregor Hohpe)](https://www.enterpriseintegrationpatterns.com/)
