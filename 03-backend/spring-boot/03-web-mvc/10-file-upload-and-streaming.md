# File Upload & Streaming in Spring MVC

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Servlet 6.0 Multipart, Java I/O Streams, Spring MVC Controllers |
| **Estimated Reading Time** | 24 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Tomcat 10.1, OpenJDK 21.0.2 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Configuration](#4-syntax--configuration)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison: Multipart vs Direct Stream vs Presigned URLs](#9-comparison-multipart-vs-direct-stream-vs-presigned-urls)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

File ingestion and retrieval are ubiquitous in modern applications: document uploads, CSV data processing, media streaming, and PDF generation.

### The Problem
Improper file handling causes severe production incidents:
1. Buffering entire multi-gigabyte files into JVM heap memory triggers catastrophic `OutOfMemoryError` (OOM).
2. Malicious clients upload executable files (`.sh`, `.exe`, `.jsp`) into public web directories, enabling Remote Code Execution (RCE).
3. Concurrent downloads exhaust Tomcat worker threads if transfers are handled synchronously without streaming.

### The Solution: Spring MVC Multipart & Streaming Abstractions
Spring Boot leverages Servlet 6.0 multipart handling alongside non-blocking streaming constructs (`StreamingResponseBody`, `ResponseBodyEmitter`, and direct chunked I/O). Files are spooled safely to temporary disk locations or streamed directly across network boundaries, keeping JVM heap consumption constant regardless of file size.

---

## 2. Core Concepts

### Mental Model & Upload/Download Lifecycle
```
                 Incoming Multipart HTTP POST (e.g. 50MB file)
                                      │
                                      ▼
                        StandardServletMultipartResolver
                                      │
           ┌──────────────────────────┴──────────────────────────┐
           │ File size <= file-size-threshold                    │ File size > file-size-threshold
           ▼                                                     ▼
     Keep in Heap Memory                                Spool to Temporary Disk Directory
   (Fast small file handling)                         (Zero memory explosion risk)
           │                                                     │
           └──────────────────────────┬──────────────────────────┘
                                      ▼
                      Injects MultipartFile into Controller
                                      │
                                      ▼
         ═══════════════════════════════════════════════════════════
                 Outgoing Large File Download (e.g. 1GB video)
                                      │
                                      ▼
                      Controller returns StreamingResponseBody
                                      │
                                      ▼
               Tomcat releases worker thread back to pool
               Background task streams 8KB chunks from disk to client
                                      │
                                      ▼
                         Chunked HTTP Output Stream
```

### Invariants
1. **Never Call `.getBytes()` on Untrusted Files**: Calling `file.getBytes()` buffers the entire payload into the JVM heap. Always use `file.getInputStream()` and stream bytes in chunks.
2. **Threshold Disk Spooling**: Files smaller than `file-size-threshold` remain in memory for speed; larger files are spooled to temporary disk directories and deleted automatically when the request completes.
3. **Async Streaming Boundary**: Returning `StreamingResponseBody` delegates writing to Spring's `TaskExecutor`, freeing the Servlet container thread.

---

## 3. How It Works Internally

### Multipart Resolution Mechanics
1. When a request with `Content-Type: multipart/form-data` arrives, `DispatcherServlet.checkMultipart()` invokes `StandardServletMultipartResolver`.
2. It delegates to the embedded container (Tomcat's `ApplicationPart` / Servlet 6.0 `Part` API).
3. Tomcat inspects configured constraints (`max-file-size`, `max-request-size`, `file-size-threshold`). If sizes exceed limits, it throws `MaxUploadSizeExceededException`.
4. Spring wraps each part into a `StandardMultipartFile` providing access to `.getInputStream()`, `.getOriginalFilename()`, and `.getContentType()`.

### `StreamingResponseBody` Internals
When a controller returns a `StreamingResponseBody`:
1. `DispatcherServlet` initiates asynchronous request processing via Servlet 3.0+ `request.startAsync()`.
2. The controller method returns immediately, and the container worker thread is released back to the servlet pool.
3. Spring submits the `StreamingResponseBody.writeTo(OutputStream)` lambda to the configured `mvc.async` `TaskExecutor`.
4. The worker writes chunks directly to the response output stream and completes the async context when finished.

---

## 4. Syntax & Configuration

### Standard `application.yaml` Multipart Configuration
```yaml
spring:
  servlet:
    multipart:
      enabled: true
      max-file-size: 50MB # Max size for a single file part
      max-request-size: 100MB # Max total size for entire multipart request
      file-size-threshold: 2MB # Threshold after which files are spooled to disk
      location: /tmp/spring-uploads # Directory for temporary disk spooling
  mvc:
    async:
      request-timeout: 60000 # 60 seconds async streaming timeout
```

---

## 5. Practical Examples

### Example 1: Secure Chunked File Upload with Path Traversal Protection

```java
package com.example.web.controller;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.util.StringUtils;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;

import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.UUID;

@RestController
@RequestMapping("/api/v1/documents")
public class SecureUploadController {

    private static final Logger log = LoggerFactory.getLogger(SecureUploadController.class);
    private static final Path UPLOAD_STORAGE_DIR = Path.of("/var/app/storage/uploads");

    @PostMapping("/upload")
    public ResponseEntity<String> uploadDocument(@RequestParam("file") MultipartFile file) throws Exception {
        if (file.isEmpty()) {
            return ResponseEntity.badRequest().body("Uploaded file is empty.");
        }

        // Clean filename to prevent directory traversal attacks (e.g., "../../etc/passwd")
        String originalFilename = StringUtils.cleanPath(file.getOriginalFilename() != null ? file.getOriginalFilename() : "file");
        if (originalFilename.contains("..")) {
            return ResponseEntity.badRequest().body("Filename contains invalid path sequence.");
        }

        // Generate collision-resistant unique storage filename
        String safeStorageName = UUID.randomUUID() + "_" + originalFilename;
        Path targetDestination = UPLOAD_STORAGE_DIR.resolve(safeStorageName);

        // Stream file directly to target disk path without reading into JVM heap
        try (InputStream in = file.getInputStream()) {
            Files.copy(in, targetDestination, StandardCopyOption.REPLACE_EXISTING);
        }

        log.info("Successfully stored document: {} (Size: {} bytes)", safeStorageName, file.getSize());
        return ResponseEntity.status(HttpStatus.CREATED).body("File uploaded successfully as: " + safeStorageName);
    }
}
```

### Example 2: Enterprise Streaming Download with `StreamingResponseBody`

```java
package com.example.web.controller;

import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.servlet.mvc.method.annotation.StreamingResponseBody;

import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;

@RestController
@RequestMapping("/api/v1/downloads")
public class StreamingDownloadController {

    private static final Path MEDIA_DIR = Path.of("/var/app/storage/media");

    @GetMapping("/{fileName}")
    public ResponseEntity<StreamingResponseBody> streamLargeMediaFile(@PathVariable String fileName) {
        Path filePath = MEDIA_DIR.resolve(fileName).normalize();

        if (!Files.exists(filePath) || !filePath.startsWith(MEDIA_DIR)) {
            return ResponseEntity.notFound().build();
        }

        StreamingResponseBody responseBody = outputStream -> {
            byte[] buffer = new byte[8192]; // 8KB stream buffer
            try (InputStream inputStream = Files.newInputStream(filePath)) {
                int bytesRead;
                while ((bytesRead = inputStream.read(buffer)) != -1) {
                    outputStream.write(buffer, 0, bytesRead);
                    outputStream.flush();
                }
            }
        };

        return ResponseEntity.ok()
                .header(HttpHeaders.CONTENT_DISPOSITION, "attachment; filename=\"" + fileName + "\"")
                .contentType(MediaType.APPLICATION_OCTET_STREAM)
                .body(responseBody);
    }
}
```

### Example 3: Edge Case — Handling `MaxUploadSizeExceededException` Cleanly

When an upload exceeds configured limits, Spring throws `MaxUploadSizeExceededException` before reaching the controller.

```java
package com.example.web.error;

import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.http.ProblemDetail;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;
import org.springframework.web.multipart.MaxUploadSizeExceededException;

import java.net.URI;
import java.time.Instant;

@RestControllerAdvice
public class FileUploadExceptionHandler {

    @ExceptionHandler(MaxUploadSizeExceededException.class)
    public ResponseEntity<ProblemDetail> handleMaxSizeExceeded(MaxUploadSizeExceededException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
                HttpStatus.PAYLOAD_TOO_LARGE,
                "The uploaded file exceeds the configured maximum allowable limit (" + ex.getMaxUploadSize() + " bytes)."
        );
        problem.setTitle("Payload Too Large");
        problem.setType(URI.create("https://api.example.com/errors/payload-too-large"));
        problem.setProperty("timestamp", Instant.now());
        problem.setProperty("maxAllowedBytes", ex.getMaxUploadSize());

        return ResponseEntity.status(HttpStatus.PAYLOAD_TOO_LARGE)
                .contentType(MediaType.APPLICATION_PROBLEM_JSON)
                .body(problem);
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Reading Entire File into Byte Array in Heap
```java
// ANTI-PATTERN: Buffering entire upload into JVM memory
@PostMapping("/bad-upload")
public String handleUpload(@RequestParam("file") MultipartFile file) throws IOException {
    // Calling getBytes() creates a byte[] of the ENTIRE file in heap memory!
    // Multiple concurrent 100MB uploads will crash the JVM with OutOfMemoryError!
    byte[] content = file.getBytes(); 
    s3Service.upload(content);
    return "ok";
}
```

#### Production Fix: Stream Directly via InputStream
```java
// PRODUCTION FIX: Stream directly from MultipartFile.getInputStream()
@PostMapping("/good-upload")
public String handleUpload(@RequestParam("file") MultipartFile file) throws IOException {
    try (InputStream in = file.getInputStream()) {
        s3Client.putObject(bucketName, key, in, file.getSize());
    }
    return "ok";
}
```

### Example 5: High-Performance CSV Streaming Exporter

```java
package com.example.web.controller;

import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.servlet.mvc.method.annotation.StreamingResponseBody;

import java.io.BufferedWriter;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;

@RestController
@RequestMapping("/api/v1/export")
public class CsvExportStreamingController {

    @GetMapping(value = "/transactions.csv", produces = "text/csv")
    public ResponseEntity<StreamingResponseBody> streamCsvExport() {
        StreamingResponseBody stream = outputStream -> {
            try (BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(outputStream, StandardCharsets.UTF_8))) {
                writer.write("id,transaction_date,amount,status\n");
                // Stream rows continuously without buffering millions of objects into memory
                for (int i = 1; i <= 500_000; i++) {
                    writer.write(String.format("%d,2026-11-24,%.2f,COMPLETED\n", i, i * 1.5));
                    if (i % 1000 == 0) {
                        writer.flush(); // Periodic flush
                    }
                }
            }
        };

        return ResponseEntity.ok()
                .header(HttpHeaders.CONTENT_DISPOSITION, "attachment; filename=\"transactions.csv\"")
                .contentType(MediaType.parseMediaType("text/csv"))
                .body(stream);
    }
}
```

---

## 6. Best Practices

1. **Never Call `file.getBytes()`**: Always stream uploaded files using `file.getInputStream()`.
2. **Sanitize Original Filenames**: Always use `StringUtils.cleanPath()` and check for `..` sequences to prevent Directory Traversal vulnerabilities.
3. **Validate MIME Types via Magic Numbers**: Do not trust `file.getContentType()` alone (it is supplied by the client and easily forged). Verify file magic bytes using libraries like Apache Tika.
4. **Use `StreamingResponseBody` for File Downloads**: Avoid loading complete files into memory before sending; use asynchronous chunked streaming.
5. **Set Explicit Multipart Limits**: Configure `max-file-size` and `max-request-size` in `application.yaml` to prevent denial-of-service via huge payload uploads.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Path Traversal via `getOriginalFilename()`** | File named `../../../../etc/shadow` overwrites arbitrary server files when resolved against base paths. | Sanitize using `StringUtils.cleanPath()` and verify `destination.startsWith(storageDir)`. |
| **Trusting Client Content-Type** | An attacker uploads a `.jsp` or `.exe` file disguised as `image/jpeg`. | Inspect binary magic numbers or store files outside the public execution path. |
| **Heap Exhaustion with `.getBytes()`** | Buffering large files directly into byte arrays triggers JVM `OutOfMemoryError`. | Stream directly using `InputStream` and `Files.copy()`. |
| **Missing Async TaskExecutor Configuration** | `StreamingResponseBody` defaults to a `SimpleAsyncTaskExecutor` which spawns unpooled platform threads. | Configure a dedicated `ThreadPoolTaskExecutor` for `mvc.async`. |
| **Missing Temporary Directory Permissions** | Tomcat cannot write spooled files if `spring.servlet.multipart.location` points to a non-writable directory. | Ensure the target temporary directory exists and has proper OS file permissions. |
| **Unbounded File Size Limits** | Leaving multipart sizes unset allows attackers to upload 100GB files, filling server storage. | Configure strict `max-file-size` and `max-request-size` limits. |

---

## 8. Performance & Security Considerations

### Direct S3/Blob Storage Presigned URLs
For ultra-high-throughput architectures (e.g. video or multi-gigabyte asset uploads), proxying files through Spring Boot application servers wastes network bandwidth and thread capacity. Instead, have Spring Boot generate a pre-signed S3/GCS upload URL; clients upload directly to object storage, notifying Spring via a lightweight webhook upon completion.

### Zip Bomb / Decompression Attacks
If uploaded files are compressed archives (`.zip`, `.tar.gz`), extracting them naively can trigger a Zip Bomb attack (a 42KB zip decompressing into 4.5 petabytes). When extracting, track total uncompressed byte size and abort if threshold limits are exceeded.

---

## 9. Comparison: Multipart vs Direct Stream vs Presigned URLs

| Mechanism | Multipart Upload (`MultipartFile`) | Direct Streaming (`InputStream`) | Presigned Cloud URL (S3/GCS) |
| :--- | :--- | :--- | :--- |
| **Complexity** | Very Low (Standard Spring MVC) | Moderate (Custom stream reader) | Moderate (Cloud SDK setup) |
| **Memory Footprint** | Low (spools to disk over threshold) | Extremely Low (Zero disk spooling) | Zero (Bypasses app server) |
| **Server Bandwidth** | Consumed by App Server | Consumed by App Server | Zero (Direct to Cloud Provider) |
| **Best Used For** | Standard enterprise uploads (<50MB) | High-volume single file pipes | Massive files, video, media assets |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: How does Spring Boot ensure that large file uploads do not consume all JVM heap memory?</strong></summary>

**Answer:**
Spring Boot configures Servlet 6.0 multipart handling with a `file-size-threshold` (default 2MB). Files below this threshold are held in memory for low latency. Files exceeding this threshold are automatically spooled to a temporary directory on the host filesystem (configured via `spring.servlet.multipart.location`). When the request completes, the temporary disk files are automatically deleted by the container.
</details>

<details>
<summary><strong>Q2: What is the architectural benefit of returning `StreamingResponseBody` instead of `byte[]`?</strong></summary>

**Answer:**
Returning `byte[]` requires allocating a contiguous array in the JVM heap holding the entire file contents before writing begins. This limits concurrency and risks `OutOfMemoryError`. `StreamingResponseBody` leverages asynchronous servlet processing: Tomcat releases the servlet worker thread back to its pool, and a background thread streams data in small chunks (e.g. 8KB) directly to the client socket, maintaining constant memory overhead regardless of whether the file is 1MB or 10GB.
</details>

<details>
<summary><strong>Q3: What exception is thrown when an upload exceeds configured size limits, and how should it be handled?</strong></summary>

**Answer:**
Spring throws `MaxUploadSizeExceededException`. It should be handled inside a `@RestControllerAdvice` method returning an RFC 9457 `ProblemDetail` response with HTTP 413 (Payload Too Large).
</details>

<details>
<summary><strong>Q4: Why should you avoid trusting `MultipartFile.getContentType()` for security verification?</strong></summary>

**Answer:**
The `Content-Type` header in a multipart request is provided entirely by the client. An attacker can upload a malicious script (`exploit.sh`) while explicitly sending `Content-Type: image/png`. Secure applications verify the file's binary magic bytes using an inspection library like Apache Tika.
</details>

<details>
<summary><strong>Q5: What is Directory Traversal in file uploads and how do you prevent it?</strong></summary>

**Answer:**
Directory Traversal occurs when an attacker includes `../` sequences in the uploaded file's original name (e.g. `../../../../etc/cron.d/malicious`). If the application naively concatenates this name to a storage path, it writes outside the intended upload directory. Prevention requires calling `StringUtils.cleanPath()`, rejecting names containing `..`, and generating server-controlled UUIDs for file storage.
</details>

<details>
<summary><strong>Q6: How does Spring's `ResponseBodyEmitter` differ from `StreamingResponseBody`?</strong></summary>

**Answer:**
- `StreamingResponseBody`: Designed for raw binary or character stream output (writing directly to `OutputStream`).
- `ResponseBodyEmitter`: Designed for sending multiple distinct typed objects over an open HTTP connection over time, passing each object through Spring's `HttpMessageConverter` pipeline (used for Server-Sent Events).
</details>

<details>
<summary><strong>Q7: Can a controller method accept multiple files and form metadata in a single request?</strong></summary>

**Answer:**
Yes. You can declare `@RequestParam("files") List<MultipartFile> files` alongside standard DTO parameters (e.g. `@RequestPart("metadata") DocumentMetadataDto metadata`).
</details>

<details>
<summary><strong>Q8: What happens to spooled temporary files if the server crashes mid-upload?</strong></summary>

**Answer:**
The operating system's standard temporary file cleanup mechanism (or container termination) will eventually purge uncollected files in `/tmp`. In production, a scheduled cron task can periodically clear orphaned temporary files older than a specified duration.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Configure Multipart Size Limits in YAML</strong></summary>

**Task**: Configure `application.yaml` to set maximum file size to 20MB and max request size to 40MB.

```yaml
# Solution:
spring:
  servlet:
    multipart:
      max-file-size: 20MB
      max-request-size: 40MB
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Multiple File Upload Controller</strong></summary>

**Task**: Write an endpoint `/api/uploads/batch` accepting `List<MultipartFile> files` and returning the total count of files received.

```java
// Solution:
@RestController
@RequestMapping("/api/uploads")
public class BatchUploadController {
    @PostMapping("/batch")
    public String uploadBatch(@RequestParam("files") List<MultipartFile> files) {
        return "Received " + files.size() + " files.";
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Streaming Text Log Viewer</strong></summary>

**Task**: Build an endpoint `/api/logs/{logName}` that streams a log file line by line to the client using `StreamingResponseBody`.

```java
// Solution:
@GetMapping("/api/logs/{logName}")
public ResponseEntity<StreamingResponseBody> streamLog(@PathVariable String logName) {
    Path logPath = Path.of("/var/log/app").resolve(logName).normalize();
    StreamingResponseBody stream = out -> {
        try (BufferedReader reader = Files.newBufferedReader(logPath)) {
            String line;
            while ((line = reader.readLine()) != null) {
                out.write((line + "\n").getBytes(StandardCharsets.UTF_8));
                out.flush();
            }
        }
    };
    return ResponseEntity.ok().contentType(MediaType.TEXT_PLAIN).body(stream);
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): File Type Validation with Magic Bytes</strong></summary>

**Task**: Implement a validator method verifying that an uploaded `MultipartFile` starts with PNG magic bytes (`0x89, 0x50, 0x4E, 0x47`).

```java
// Solution:
public boolean isPng(MultipartFile file) throws IOException {
    try (InputStream in = file.getInputStream()) {
        byte[] magic = in.readNBytes(4);
        return magic.length == 4 &&
               magic[0] == (byte) 0x89 &&
               magic[1] == 0x50 &&
               magic[2] == 0x4E &&
               magic[3] == 0x47;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Asynchronous Multipart Zip Processor</strong></summary>

**Task**: Build an endpoint accepting an uploaded ZIP archive, extracting entries asynchronously on a background thread pool, and emitting extraction progress updates via `ResponseBodyEmitter`.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Direct S3 Presigned URL Upload Gateway</strong></summary>

**Task**: Implement an architectural endpoint that generates an AWS S3 presigned PUT URL with strict content-length and content-type policies, returning the pre-signed URL to the client for direct-to-cloud upload.
</details>

---

## 12. Summary & Quick Reference

- **Never Use `.getBytes()`**: Always read via `file.getInputStream()`.
- **Threshold Disk Spooling**: Configured via `spring.servlet.multipart.file-size-threshold`.
- **Non-blocking Downloads**: Return `StreamingResponseBody` to stream chunked bytes without holding servlet worker threads.
- **Security Invariant**: Always sanitize filenames with `StringUtils.cleanPath()` and check magic numbers.

---

## 13. Further Reading

- [Spring Framework Reference: Multipart Resolver](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-servlet/multipart.html)
- [Spring MVC Asynchronous Request Processing](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-ann-async.html)
- [OWASP File Upload Security Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/File_Upload_Cheat_Sheet.html)
- [Jakarta Servlet Specification: Section on Multipart](https://jakarta.ee/specifications/servlet/6.0/)
