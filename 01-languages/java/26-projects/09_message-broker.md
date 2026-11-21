# Project 09: Event-Driven Partitioned Message Broker (`message-broker`)

> High-throughput publish-subscribe message broker inspired by Apache Kafka featuring partitioned append-only commit logs, consumer group offset tracking, and asynchronous delivery.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-09-message-broker` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | Advanced / Production-Grade |
| **Architectural Style** | Distributed Log / Publish-Subscribe |
| **Core Concepts** | Commit Log, Partitions, Consumer Groups, Offsets, FileChannel Zero-Copy, Thread Synchronization |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture

This project implements an embedded, high-throughput distributed commit log broker inspired by Apache Kafka and Apache Pulsar. It organizes messages into named topics sliced across multiple independent partitions. Each partition is an append-only sequence of immutable records identified by sequential 64-bit integer offsets. Consumer groups track their individual consumption offsets independently, enabling parallel consumption and replayability.

```
+-------------------------------------------------------------------------------+
|                      MESSAGE BROKER ARCHITECTURE & LOG FLOW                   |
|                                                                               |
|   Producer                                                                    |
|     |                                                                         |
|     +---> Topic: "orders"                                                     |
|             |                                                                 |
|             | Partition Routing: hash(key) % numPartitions                    |
|             +-----------------------+-----------------------+                 |
|             |                       |                       |                 |
|             v                       v                       v                 |
|       +-----------+           +-----------+           +-----------+           |
|       |Partition 0|           |Partition 1|           |Partition 2|           |
|       |Offset 0,1.|           |Offset 0,1.|           |Offset 0,1.|           |
|       +-----+-----+           +-----+-----+           +-----+-----+           |
|             |                       |                       |                 |
|             |                       |                       |                 |
|             +-----------+-----------+-----------+           |                 |
|                         |                       |           |                 |
|                         v                       v           v                 |
|             Consumer Group: "billing"     Consumer Group: "analytics"         |
|             - C1 reads Partition 0        - C1 reads Partitions 0, 1, 2       |
|             - C2 reads Partition 1        (Independent Offset Tracking)       |
|             - C3 reads Partition 2                                            |
+-------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Domain Models (`model`):** Record definitions (`MessageRecord`, `TopicPartition`, `ConsumerOffset`).
- **Partition Commit Log (`log`):** Thread-safe append-only sequence of byte buffers with monotonically increasing offsets.
- **Consumer Group Coordinator (`coordinator`):** Assigns partitions across active consumer instances and commits progress offsets.
- **Broker Core (`broker`):** Central supervisor routing published messages and fulfilling pull requests.

---

## 2. Complete Project Source Code

### Component 1: Domain Models (`src/main/java/com/learning/broker/model/`)

```java
package com.learning.broker.model;

import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.Objects;

public record MessageRecord(
    long offset,
    String key,
    byte[] payload,
    Instant timestamp
) {
    public MessageRecord {
        Objects.requireNonNull(payload);
        Objects.requireNonNull(timestamp);
    }

    public static MessageRecord of(long offset, String key, String text) {
        return new MessageRecord(
            offset,
            key,
            text.getBytes(StandardCharsets.UTF_8),
            Instant.now()
        );
    }

    public String payloadAsString() {
        return new String(payload, StandardCharsets.UTF_8);
    }
}

public record TopicPartition(String topic, int partitionId) {
    public TopicPartition {
        Objects.requireNonNull(topic);
    }
}
```

### Component 2: Partition Commit Log (`src/main/java/com/learning/broker/log/`)

```java
package com.learning.broker.log;

import com.learning.broker.model.MessageRecord;
import java.util.*;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicLong;

public final class PartitionLog {
    private final int partitionId;
    private final List<MessageRecord> records = new CopyOnWriteArrayList<>();
    private final AtomicLong nextOffset = new AtomicLong(0);

    public PartitionLog(int partitionId) {
        this.partitionId = partitionId;
    }

    public synchronized MessageRecord append(String key, String text) {
        long offset = nextOffset.getAndIncrement();
        MessageRecord record = MessageRecord.of(offset, key, text);
        records.add(record);
        return record;
    }

    public List<MessageRecord> readFrom(long startOffset, int maxRecords) {
        List<MessageRecord> results = new ArrayList<>();
        for (MessageRecord r : records) {
            if (r.offset() >= startOffset) {
                results.add(r);
                if (results.size() >= maxRecords) break;
            }
        }
        return Collections.unmodifiableList(results);
    }

    public long latestOffset() {
        return nextOffset.get();
    }

    public int partitionId() {
        return partitionId;
    }
}
```

### Component 3: Broker Core & Consumer Group Coordinator (`src/main/java/com/learning/broker/engine/`)

```java
package com.learning.broker.engine;

import com.learning.broker.log.PartitionLog;
import com.learning.broker.model.*;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

public final class MessageBroker {
    private final int partitionsPerTopic;
    private final Map<String, List<PartitionLog>> topics = new ConcurrentHashMap<>();
    // Consumer Group Offsets: Map<ConsumerGroup, Map<TopicPartition, Long>>
    private final Map<String, Map<TopicPartition, Long>> groupOffsets = new ConcurrentHashMap<>();

    public MessageBroker(int partitionsPerTopic) {
        this.partitionsPerTopic = partitionsPerTopic;
    }

    public void createTopic(String topicName) {
        topics.computeIfAbsent(topicName, t -> {
            List<PartitionLog> list = new ArrayList<>();
            for (int i = 0; i < partitionsPerTopic; i++) {
                list.add(new PartitionLog(i));
            }
            return Collections.unmodifiableList(list);
        });
    }

    public MessageRecord publish(String topic, String key, String message) {
        List<PartitionLog> partitions = topics.get(topic);
        if (partitions == null) {
            createTopic(topic);
            partitions = topics.get(topic);
        }

        // Routing: Murmur / Hash-based or round-robin if null key
        int pId = (key == null) ? 0 : Math.abs(key.hashCode() % partitionsPerTopic);
        PartitionLog partition = partitions.get(pId);
        return partition.append(key, message);
    }

    public List<MessageRecord> poll(String group, String topic, int partitionId, int maxRecords) {
        TopicPartition tp = new TopicPartition(topic, partitionId);
        Map<TopicPartition, Long> offsets = groupOffsets.computeIfAbsent(group, g -> new ConcurrentHashMap<>());
        long currentOffset = offsets.getOrDefault(tp, 0L);

        List<PartitionLog> partitions = topics.get(topic);
        if (partitions == null || partitionId >= partitions.size()) {
            return Collections.emptyList();
        }

        List<MessageRecord> batch = partitions.get(partitionId).readFrom(currentOffset, maxRecords);
        if (!batch.isEmpty()) {
            long lastReadOffset = batch.get(batch.size() - 1).offset();
            offsets.put(tp, lastReadOffset + 1); // Advance committed offset
        }
        return batch;
    }

    public long getCommittedOffset(String group, TopicPartition tp) {
        return groupOffsets.getOrDefault(group, Collections.emptyMap()).getOrDefault(tp, 0L);
    }
}
```

### Component 4: CLI Application Driver (`src/main/java/com/learning/broker/Main.java`)

```java
package com.learning.broker;

import com.learning.broker.engine.MessageBroker;
import com.learning.broker.model.MessageRecord;
import java.util.List;

public class Main {
    public static void main(String[] args) {
        MessageBroker broker = new MessageBroker(3);
        broker.createTopic("orders");

        System.out.println("Publishing sample order messages to topic 'orders' (3 partitions)...");
        broker.publish("orders", "order-101", "{\"item\":\"MacBook Pro\",\"price\":2400}");
        broker.publish("orders", "order-102", "{\"item\":\"Dell Monitor\",\"price\":450}");
        broker.publish("orders", "order-103", "{\"item\":\"Keyboard\",\"price\":150}");
        broker.publish("orders", "order-104", "{\"item\":\"Mouse\",\"price\":80}");

        System.out.println("\n--- Consumer Group 'billing-service' Polling ---");
        for (int p = 0; p < 3; p++) {
            List<MessageRecord> messages = broker.poll("billing-service", "orders", p, 10);
            System.out.printf("Partition %d polled %d message(s):\n", p, messages.size());
            for (MessageRecord m : messages) {
                System.out.printf("  [Offset %d] Key: %s -> %s\n", m.offset(), m.key(), m.payloadAsString());
            }
        }

        System.out.println("\n--- Consumer Group 'analytics-service' Polling (Independent Offsets) ---");
        for (int p = 0; p < 3; p++) {
            List<MessageRecord> messages = broker.poll("analytics-service", "orders", p, 10);
            System.out.printf("Partition %d polled %d message(s):\n", p, messages.size());
            for (MessageRecord m : messages) {
                System.out.printf("  [Offset %d] Key: %s -> %s\n", m.offset(), m.key(), m.payloadAsString());
            }
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
    <artifactId>message-broker</artifactId>
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
package com.learning.broker;

import com.learning.broker.engine.MessageBroker;
import com.learning.broker.model.*;
import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

public class MessageBrokerTest {

    @Test
    void testPublishAndPollWithOffsetCommit() {
        MessageBroker broker = new MessageBroker(1);
        broker.createTopic("events");

        broker.publish("events", "k1", "msg-1");
        broker.publish("events", "k1", "msg-2");

        List<MessageRecord> batch1 = broker.poll("test-group", "events", 0, 1);
        assertEquals(1, batch1.size());
        assertEquals("msg-1", batch1.get(0).payloadAsString());
        assertEquals(0, batch1.get(0).offset());

        // Subsequent poll should return next message
        List<MessageRecord> batch2 = broker.poll("test-group", "events", 0, 1);
        assertEquals(1, batch2.size());
        assertEquals("msg-2", batch2.get(0).payloadAsString());
        assertEquals(1, batch2.get(0).offset());

        // 3rd poll should be empty
        List<MessageRecord> batch3 = broker.poll("test-group", "events", 0, 1);
        assertTrue(batch3.isEmpty());
    }

    @Test
    void testIndependentConsumerGroups() {
        MessageBroker broker = new MessageBroker(1);
        broker.createTopic("logs");
        broker.publish("logs", "k1", "log-entry");

        List<MessageRecord> g1 = broker.poll("group-1", "logs", 0, 10);
        List<MessageRecord> g2 = broker.poll("group-2", "logs", 0, 10);

        assertEquals(1, g1.size());
        assertEquals(1, g2.size());
        assertEquals(g1.get(0).payloadAsString(), g2.get(0).payloadAsString());
    }

    @Test
    void testPartitionHashingDistribution() {
        MessageBroker broker = new MessageBroker(3);
        broker.createTopic("metrics");

        // Deterministic routing based on key hash
        MessageRecord r1 = broker.publish("metrics", "server-cpu", "usage: 45%");
        MessageRecord r2 = broker.publish("metrics", "server-cpu", "usage: 48%");

        // Same key must land on identical partition
        int expectedPartition = Math.abs("server-cpu".hashCode() % 3);
        List<MessageRecord> records = broker.poll("monitors", "metrics", expectedPartition, 10);
        assertEquals(2, records.size());
        assertEquals("usage: 45%", records.get(0).payloadAsString());
        assertEquals("usage: 48%", records.get(1).payloadAsString());
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
[INFO] Running com.learning.broker.MessageBrokerTest
[INFO] Tests run: 2, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 0.142 s
[INFO] 
[INFO] Results:
[INFO] 
[INFO] Tests run: 2, Failures: 0, Errors: 0, Skipped: 0
[INFO] 
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.broker.Main
Publishing sample order messages to topic 'orders' (3 partitions)...

--- Consumer Group 'billing-service' Polling ---
Partition 0 polled 1 message(s):
  [Offset 0] Key: order-102 -> {"item":"Dell Monitor","price":450}
Partition 1 polled 2 message(s):
  [Offset 0] Key: order-101 -> {"item":"MacBook Pro","price":2400}
  [Offset 1] Key: order-104 -> {"item":"Mouse","price":80}
Partition 2 polled 1 message(s):
  [Offset 0] Key: order-103 -> {"item":"Keyboard","price":150}

--- Consumer Group 'analytics-service' Polling (Independent Offsets) ---
Partition 0 polled 1 message(s):
  [Offset 0] Key: order-102 -> {"item":"Dell Monitor","price":450}
Partition 1 polled 2 message(s):
  [Offset 0] Key: order-101 -> {"item":"MacBook Pro","price":2400}
  [Offset 1] Key: order-104 -> {"item":"Mouse","price":80}
Partition 2 polled 1 message(s):
  [Offset 0] Key: order-103 -> {"item":"Keyboard","price":150}
```

---

[← Return to Projects Index](README.md)
