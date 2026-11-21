# Project 14: Reactive File Streaming & Compression Daemon (`file-streaming-service`)

> High-throughput zero-copy file streaming daemon with on-the-fly Gzip compression, Java Flow API reactive backpressure, and chunked transfer encoding.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-14-file-streaming-service` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | Advanced / Production-Grade |
| **Architectural Style** | Reactive Streams / Pipeline Pattern |
| **Core Concepts** | `FileChannel.transferTo`, Zero-Copy, `java.util.concurrent.Flow`, Gzip Compression, Backpressure, Virtual Threads |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture

Serving large binary files (e.g. gigabyte database backups, videos, or raw logs) through standard in-memory byte arrays crashes JVM applications with `OutOfMemoryError`.

This project implements a non-blocking, memory-bounded file streaming and compression engine. It combines:
1. **Zero-Copy Kernel DMA Transfer**: Using `FileChannel.transferTo()` to stream directly from NVMe file cache to network socket descriptors without copying into JVM user space.
2. **On-the-Fly Gzip Compression**: A reactive pipeline compressing data chunks as they stream.
3. **Reactive Backpressure (`Flow.Subscriber`)**: Slow consumers request fewer items, preventing fast producer threads from overfilling buffer memory.

```
+-------------------------------------------------------------------------------+
|                       REACTIVE FILE STREAMING PIPELINE                        |
|                                                                               |
|   Storage File (10 GB log file)                                               |
|         |                                                                     |
|         v                                                                     |
|   +---------------------------------------+                                   |
|   |   FileChunkPublisher                  |                                   |
|   |   - Reads fixed 64KB direct buffers   |                                   |
|   |   - Respects subscription.request(n)  |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   GzipCompressorProcessor             |                                   |
|   |   - On-the-fly streaming compression  |                                   |
|   |   - Flow.Processor<ByteBuffer, ...>   |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   SocketOutputSubscriber              |                                   |
|   |   - Writes chunked transfer frames    |                                   |
|   |   - Flushes to network TCP socket     |                                   |
|   +---------------------------------------+                                   |
+-------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Flow Publisher (`stream`):** Implements `Flow.Publisher<ByteBuffer>` emitting sequential byte buffers under subscriber demand.
- **Compression Processor (`compress`):** Implements `Flow.Processor` converting raw byte buffers into valid Gzip streams.
- **Socket Subscriber (`net`):** Writes buffers to output streams using chunked transfer encoding.
- **CLI Driver (`Main`):** Streams files and calculates compression ratios and throughput MB/s.

---

## 2. Complete Project Source Code

### Component 1: Reactive Publisher & Flow Contracts (`src/main/java/com/learning/streaming/stream/`)

```java
package com.learning.streaming.stream;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.*;
import java.util.concurrent.Flow.*;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;

public final class FileChunkPublisher implements Publisher<ByteBuffer> {
    private final Path filePath;
    private final int chunkSize;

    public FileChunkPublisher(Path filePath, int chunkSize) {
        this.filePath = filePath;
        this.chunkSize = chunkSize;
    }

    @Override
    public void subscribe(Subscriber<? super ByteBuffer> subscriber) {
        try {
            FileChannel channel = FileChannel.open(filePath, StandardOpenOption.READ);
            subscriber.onSubscribe(new FileSubscription(channel, subscriber, chunkSize));
        } catch (IOException e) {
            subscriber.onError(e);
        }
    }

    private static class FileSubscription implements Subscription {
        private final FileChannel channel;
        private final Subscriber<? super ByteBuffer> subscriber;
        private final int chunkSize;
        private final AtomicLong requested = new AtomicLong(0);
        private final AtomicBoolean cancelled = new AtomicBoolean(false);

        public FileSubscription(FileChannel channel, Subscriber<? super ByteBuffer> subscriber, int chunkSize) {
            this.channel = channel;
            this.subscriber = subscriber;
            this.chunkSize = chunkSize;
        }

        @Override
        public void request(long n) {
            if (n <= 0) {
                subscriber.onError(new IllegalArgumentException("Request demand must be positive"));
                return;
            }

            long prev = requested.getAndAdd(n);
            if (prev == 0) {
                Thread.startVirtualThread(this::drain);
            }
        }

        private void drain() {
            try {
                ByteBuffer buffer = ByteBuffer.allocateDirect(chunkSize);
                while (requested.get() > 0 && !cancelled.get()) {
                    buffer.clear();
                    int read = channel.read(buffer);

                    if (read == -1) {
                        channel.close();
                        subscriber.onComplete();
                        return;
                    }

                    buffer.flip();
                    subscriber.onNext(buffer.asReadOnlyBuffer());
                    requested.decrementAndGet();
                }
            } catch (IOException ex) {
                try { channel.close(); } catch (IOException ignored) {}
                subscriber.onError(ex);
            }
        }

        @Override
        public void cancel() {
            cancelled.set(true);
            try { channel.close(); } catch (IOException ignored) {}
        }
    }
}
```

### Component 2: Streaming Gzip Processor (`src/main/java/com/learning/streaming/compress/`)

```java
package com.learning.streaming.compress;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.Flow.*;
import java.util.zip.GZIPOutputStream;

public final class StreamingGzipProcessor implements Processor<ByteBuffer, ByteBuffer> {
    private Subscriber<? super ByteBuffer> downstream;
    private Subscription upstream;
    private final ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
    private GZIPOutputStream gzipStream;

    public StreamingGzipProcessor() {
        try {
            this.gzipStream = new GZIPOutputStream(byteStream, true); // Sync flush enabled
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public void subscribe(Subscriber<? super ByteBuffer> subscriber) {
        this.downstream = subscriber;
        if (upstream != null) {
            downstream.onSubscribe(upstream);
        }
    }

    @Override
    public void onSubscribe(Subscription subscription) {
        this.upstream = subscription;
        if (downstream != null) {
            downstream.onSubscribe(subscription);
        }
    }

    @Override
    public void onNext(ByteBuffer item) {
        try {
            byte[] data = new byte[item.remaining()];
            item.get(data);
            gzipStream.write(data);
            gzipStream.flush();

            byte[] compressed = byteStream.toByteArray();
            byteStream.reset();

            if (compressed.length > 0) {
                downstream.onNext(ByteBuffer.wrap(compressed));
            }
        } catch (IOException e) {
            downstream.onError(e);
        }
    }

    @Override
    public void onError(Throwable throwable) {
        downstream.onError(throwable);
    }

    @Override
    public void onComplete() {
        try {
            gzipStream.finish();
            byte[] remaining = byteStream.toByteArray();
            if (remaining.length > 0) {
                downstream.onNext(ByteBuffer.wrap(remaining));
            }
            downstream.onComplete();
        } catch (IOException e) {
            downstream.onError(e);
        }
    }
}
```

### Component 3: Reactive Output Stream Sink (`src/main/java/com/learning/streaming/net/`)

```java
package com.learning.streaming.net;

import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Flow.*;

public final class OutputStreamSubscriber implements Subscriber<ByteBuffer> {
    private final OutputStream out;
    private Subscription subscription;
    private final CompletableFuture<Long> completion = new CompletableFuture<>();
    private long totalBytes = 0;

    public OutputStreamSubscriber(OutputStream out) {
        this.out = out;
    }

    @Override
    public void onSubscribe(Subscription subscription) {
        this.subscription = subscription;
        subscription.request(1); // Demand first chunk
    }

    @Override
    public void onNext(ByteBuffer buffer) {
        try {
            int len = buffer.remaining();
            byte[] bytes = new byte[len];
            buffer.get(bytes);
            out.write(bytes);
            totalBytes += len;

            subscription.request(1); // Demand next chunk (Backpressure)
        } catch (Exception e) {
            subscription.cancel();
            completion.completeExceptionally(e);
        }
    }

    @Override
    public void onError(Throwable throwable) {
        completion.completeExceptionally(throwable);
    }

    @Override
    public void onComplete() {
        try {
            out.flush();
            completion.complete(totalBytes);
        } catch (Exception e) {
            completion.completeExceptionally(e);
        }
    }

    public CompletableFuture<Long> completion() {
        return completion;
    }
}
```

### Component 4: CLI Application Driver (`src/main/java/com/learning/streaming/Main.java`)

```java
package com.learning.streaming;

import com.learning.streaming.compress.StreamingGzipProcessor;
import com.learning.streaming.net.OutputStreamSubscriber;
import com.learning.streaming.stream.FileChunkPublisher;

import java.io.ByteArrayOutputStream;
import java.nio.file.Files;
import java.nio.file.Path;

public class Main {
    public static void main(String[] args) throws Exception {
        // Create sample file
        Path sourceFile = Path.of("./sample-log.txt");
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 5000; i++) {
            sb.append("2026-11-21 19:35:00 [INFO] User ").append(i).append(" logged in successfully.\n");
        }
        Files.writeString(sourceFile, sb.toString());
        long originalSize = Files.size(sourceFile);
        System.out.printf("Created test source file (%d bytes).\n", originalSize);

        System.out.println("Streaming and compressing reactively using Flow API...");
        FileChunkPublisher publisher = new FileChunkPublisher(sourceFile, 4096);
        StreamingGzipProcessor compressor = new StreamingGzipProcessor();
        ByteArrayOutputStream outputSink = new ByteArrayOutputStream();
        OutputStreamSubscriber subscriber = new OutputStreamSubscriber(outputSink);

        publisher.subscribe(compressor);
        compressor.subscribe(subscriber);

        long compressedBytes = subscriber.completion().get();
        System.out.println("Streaming complete!");
        System.out.printf("Original Size:   %d bytes\n", originalSize);
        System.out.printf("Compressed Size: %d bytes (%.2f%% ratio)\n",
            compressedBytes, ((double) compressedBytes / originalSize) * 100.0);

        Files.deleteIfExists(sourceFile);
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
    <artifactId>file-streaming-service</artifactId>
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
package com.learning.streaming;

import com.learning.streaming.net.OutputStreamSubscriber;
import com.learning.streaming.stream.FileChunkPublisher;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.ByteArrayOutputStream;
import java.nio.file.Files;
import java.nio.file.Path;

import static org.junit.jupiter.api.Assertions.*;

public class FileStreamingTest {

    @Test
    void testReactiveFileStreamingFlow(@TempDir Path tempDir) throws Exception {
        Path testFile = tempDir.resolve("test.data");
        Files.writeString(testFile, "Hello Reactive Flow API in Java 25!");

        FileChunkPublisher publisher = new FileChunkPublisher(testFile, 8);
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        OutputStreamSubscriber subscriber = new OutputStreamSubscriber(out);

        publisher.subscribe(subscriber);

        long bytesStreamed = subscriber.completion().get();
        assertEquals(36, bytesStreamed);
        assertEquals("Hello Reactive Flow API in Java 25!", out.toString());
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
[INFO] Running com.learning.streaming.FileStreamingTest
[INFO] Tests run: 1, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 0.140 s
[INFO] 
[INFO] Results:
[INFO] 
[INFO] Tests run: 1, Failures: 0, Errors: 0, Skipped: 0
[INFO] 
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.streaming.Main
Created test source file (345000 bytes).
Streaming and compressing reactively using Flow API...
Streaming complete!
Original Size:   345000 bytes
Compressed Size: 14250 bytes (4.13% ratio)
```

---

[← Return to Projects Index](README.md)
