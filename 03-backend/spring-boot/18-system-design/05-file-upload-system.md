# 05: Designing a Distributed File Upload & Media Processing System

| Metadata Field | Value |
|---|---|
| **Topic** | Scalable File Ingestion, S3 Presigned URLs & Async Media Pipelines |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Key Middleware** | AWS S3 / MinIO (Object Storage), CloudFront (CDN), Kafka / SQS, PostgreSQL |
| **Key Scale** | Multi-Gigabyte Uploads, Zero Backend Memory Buffering |

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

Uploading large files (e.g., 500 MB video clips, high-resolution medical imagery, raw PDF documents) directly through a Spring Boot application server is a major anti-pattern. Streaming hundreds of megabytes through JVM memory and HTTP request threads exhausts heap space, starves connection pools, and exposes the cluster to Denial of Service (DoS) attacks.

A modern, production-grade file ingestion architecture completely decouples **metadata authorization** from **binary transport**. Clients obtain a cryptographically signed **Presigned URL** or initiate an **S3 Multipart Upload** directly against cloud object storage (Amazon S3, Google Cloud Storage, or MinIO). The backend server only processes lightweight JSON metadata, while asynchronous event-driven workers handle post-upload virus scanning, thumbnail generation, and CDN invalidation.

---

## 2. Core Concepts

### 2.1 Presigned S3 URLs
An S3 Presigned URL allows an unauthorized client (e.g., Web browser or Mobile app) to execute an HTTP `PUT` request directly to an S3 bucket key with temporary permissions (e.g., 15 minutes TTL) cryptographically signed by the backend's IAM credentials.

### 2.2 S3 Multipart Upload Flow
For files larger than 100 MB:
1. Client calls Backend: `POST /api/v1/files/multipart/initiate` -> returns `UploadId`.
2. Client requests Presigned URLs for each 10 MB chunk: `partNumber=1, 2, 3...`
3. Client uploads chunks concurrently and directly to S3.
4. Client calls Backend: `POST /api/v1/files/multipart/complete` with part ETags -> S3 stitches the parts together atomically.

---

## 3. Internals & Architecture

```
        [ Client (Browser / Mobile) ]
             │                     │
   1. Request Presigned URL        │ 3. Upload Binary Directly
      (Metadata only)              │    (HTTP PUT / Multipart)
             ▼                     │
 ┌───────────────────────┐         │
 │ Spring Boot Backend   │         │
 │ - Generates S3 Token  │         │
 │ - Stores metadata:    │         │
 │   status: 'UPLOADING' │         │
 └───────────────────────┘         │
                                   ▼
                       ┌────────────────────────┐
                       │ AWS S3 / MinIO Storage │
                       │ Bucket: "media-raw"    │
                       └───────────┬────────────┘
                                   │ 4. S3 ObjectCreated Event
                                   ▼
                       ┌────────────────────────┐
                       │ Apache Kafka / AWS SQS │
                       │ Topic: 's3-file-events'│
                       └───────────┬────────────┘
                                   │
                                   ▼
 ┌──────────────────────────────────────────────────────────────┐
 │                Media Worker Processing Cluster               │
 │  1. Antivirus Scan (ClamAV)                                  │
 │  2. Image Resizing / Video Transcoding (FFmpeg)              │
 │  3. Move clean asset to "media-public" bucket                │
 │  4. Update PostgreSQL: status = 'READY'                      │
 └─────────────────────────────┬────────────────────────────────┘
                               │
                               ▼
                       ┌────────────────────────┐
                       │ CloudFront Edge CDN    │
                       │ - Global low-latency   │
                       └────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 S3 Presigner SDK v2 Syntax
```java
PutObjectPresignRequest presignRequest = PutObjectPresignRequest.builder()
        .signatureDuration(Duration.ofMinutes(15))
        .putObjectRequest(b -> b.bucket("my-bucket").key("uploads/file.mp4").contentType("video/mp4"))
        .build();
PresignedPutObjectRequest presigned = s3Presigner.presignPutObject(presignRequest);
String uploadUrl = presigned.url().toString();
```

---

## 5. 5 Practical Examples

### Example 1: S3 Presigned URL Generation Service

```java
package com.example.files.service;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import software.amazon.awssdk.services.s3.model.PutObjectRequest;
import software.amazon.awssdk.services.s3.presigner.S3Presigner;
import software.amazon.awssdk.services.s3.presigner.model.PresignedPutObjectRequest;
import software.amazon.awssdk.services.s3.presigner.model.PutObjectPresignRequest;

import java.time.Duration;
import java.util.UUID;

@Service
public class S3PresignedUploadService {

    private final S3Presigner s3Presigner;

    @Value("${aws.s3.bucket-name:app-media-uploads}")
    private String bucketName;

    public S3PresignedUploadService(S3Presigner s3Presigner) {
        this.s3Presigner = s3Presigner;
    }

    public record PresignedUploadResponse(String fileKey, String uploadUrl, long expiresInSeconds) {}

    public PresignedUploadResponse createPresignedUploadUrl(String originalFileName, String contentType) {
        String fileKey = "raw/" + UUID.randomUUID() + "-" + originalFileName;

        PutObjectRequest objectRequest = PutObjectRequest.builder()
                .bucket(bucketName)
                .key(fileKey)
                .contentType(contentType)
                .build();

        PutObjectPresignRequest presignRequest = PutObjectPresignRequest.builder()
                .signatureDuration(Duration.ofMinutes(15))
                .putObjectRequest(objectRequest)
                .build();

        PresignedPutObjectRequest presigned = s3Presigner.presignPutObject(presignRequest);
        return new PresignedUploadResponse(fileKey, presigned.url().toString(), 900);
    }
}
```

### Example 2: Multipart Upload Coordination Controller

```java
package com.example.files.web;

import com.example.files.service.S3PresignedUploadService;
import org.springframework.web.bind.annotation.*;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.s3.model.*;

import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/api/v1/files/multipart")
public class MultipartUploadController {

    private final S3Client s3Client;
    private final String bucket = "app-media-uploads";

    public MultipartUploadController(S3Client s3Client) {
        this.s3Client = s3Client;
    }

    @PostMapping("/initiate")
    public Map<String, String> initiate(@RequestParam String fileName, @RequestParam String contentType) {
        String key = "multipart/" + fileName;
        CreateMultipartUploadResponse resp = s3Client.createMultipartUpload(b -> b.bucket(bucket).key(key).contentType(contentType));
        return Map.of("uploadId", resp.uploadId(), "fileKey", key);
    }

    public record PartETagDto(int partNumber, String etag) {}
    public record CompleteUploadRequest(String fileKey, String uploadId, List<PartETagDto> parts) {}

    @PostMapping("/complete")
    public Map<String, String> complete(@RequestBody CompleteUploadRequest req) {
        List<CompletedPart> completedParts = req.parts().stream()
                .map(p -> CompletedPart.builder().partNumber(p.partNumber()).eTag(p.etag()).build())
                .toList();

        CompletedMultipartUpload completedUpload = CompletedMultipartUpload.builder().parts(completedParts).build();
        s3Client.completeMultipartUpload(b -> b.bucket(bucket).key(req.fileKey()).uploadId(req.uploadId()).multipartUpload(completedUpload));
        return Map.of("status", "SUCCESS", "fileKey", req.fileKey());
    }
}
```

### Example 3: File Metadata Entity & Repository

```java
package com.example.files.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "file_records")
public class FileRecord {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(name = "file_key", nullable = false, unique = true)
    private String fileKey;

    @Column(nullable = false)
    private String fileName;

    @Column(nullable = false)
    private String contentType;

    @Column(nullable = false)
    private long sizeBytes;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    private FileStatus status = FileStatus.PENDING_UPLOAD;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    public enum FileStatus { PENDING_UPLOAD, PROCESSING, READY, REJECTED_VIRUS }

    public FileRecord() {}

    public FileRecord(String fileKey, String fileName, String contentType, long sizeBytes) {
        this.fileKey = fileKey;
        this.fileName = fileName;
        this.contentType = contentType;
        this.sizeBytes = sizeBytes;
    }

    // Getters and Setters
    public UUID getId() { return id; }
    public String getFileKey() { return fileKey; }
    public FileStatus getStatus() { return status; }
    public void setStatus(FileStatus status) { this.status = status; }
}
```

### Example 4: Asynchronous S3 Event Worker with Thumbnail Generation

```java
package com.example.files.worker;

import com.example.files.domain.FileRecord;
import com.example.files.repository.FileRecordRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.stereotype.Component;
import software.amazon.awssdk.services.s3.S3Client;

@Component
public class MediaProcessingWorker {

    private static final Logger log = LoggerFactory.getLogger(MediaProcessingWorker.class);
    private final FileRecordRepository repository;
    private final S3Client s3Client;

    public MediaProcessingWorker(FileRecordRepository repository, S3Client s3Client) {
        this.repository = repository;
        this.s3Client = s3Client;
    }

    @KafkaListener(topics = "s3-file-uploaded-events", groupId = "media-processing-group")
    public void onFileUploaded(String fileKey) {
        log.info("Processing uploaded media asset: {}", fileKey);
        FileRecord record = repository.findByFileKey(fileKey).orElse(null);
        if (record == null) return;

        record.setStatus(FileRecord.FileStatus.PROCESSING);
        repository.save(record);

        try {
            // 1. Download file stream
            // 2. Perform ClamAV virus check
            // 3. Generate 300x300 thumbnail image
            // 4. Upload thumbnail back to S3 'thumbnails/' prefix
            record.setStatus(FileRecord.FileStatus.READY);
            repository.save(record);
            log.info("Asset {} successfully processed and marked READY.", fileKey);
        } catch (Exception ex) {
            log.error("Failed to process file asset {}: {}", fileKey, ex.getMessage());
            record.setStatus(FileRecord.FileStatus.REJECTED_VIRUS);
            repository.save(record);
        }
    }
}
```

### Example 5: CloudFront Presigned Cookie Generator for Private Content

```java
package com.example.files.security;

import org.springframework.stereotype.Component;

import java.time.Instant;
import java.time.temporal.ChronoUnit;

@Component
public class CloudFrontUrlSignerService {

    public String generateCdnDownloadUrl(String distributionDomain, String s3ObjectKey) {
        // Generates edge CDN URL with expires parameter
        long expiresAt = Instant.now().plus(1, ChronoUnit.HOURS).getEpochSecond();
        return "https://" + distributionDomain + "/" + s3ObjectKey + "?exp=" + expiresAt;
    }
}
```

---

## 6. Best Practices

- **Never Proxy Binary Payloads through Tomcat**: Always generate presigned S3 URLs so browser clients upload directly to object storage.
- **Implement Antivirus Scanning**: Scan files asynchronously using ClamAV containers before making them publicly accessible via CDN.
- **Validate File Magic Bytes**: Never trust the HTTP `Content-Type` header or file extension. Verify file headers (magic bytes) inside the worker before processing.
- **Set Up S3 Lifecycle Rules**: Automatically abort incomplete multipart uploads after 7 days (`AbortIncompleteMultipartUpload`) to avoid orphaned storage costs.
- **Serve via CloudFront CDN**: Distribute public images and video assets via CloudFront or Fastly with aggressive cache headers (`Cache-Control: public, max-age=31536000`).

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Uploading Files to Local Disk (`/tmp`)** | Cloud containers are ephemeral. Scaling out or restarting pods deletes uploaded files. | Store directly in S3 or compatible distributed object storage. |
| **Proxying Uploads through Spring Controller** | Uploading 1 GB file ties up a Tomcat thread and buffers memory, leading to `OutOfMemoryError`. | Issue S3 Presigned URLs for direct client-to-S3 uploads. |
| **Publicly Writable S3 Buckets** | Attackers can overwrite objects or run up multi-thousand dollar bandwidth bills. | Keep buckets 100% private. Restrict access to backend-generated presigned URLs. |
| **Trusting Client `Content-Type`** | An attacker uploads a `.jsp` or `.exe` disguised as `image/png`. | Check binary magic numbers (e.g. `89 50 4E 47` for PNG) and sanitize filenames. |
| **Serving Large Files from Origin S3** | S3 egress charges are significantly higher than CDN edge distribution costs. | Front S3 with CloudFront CDN for caching and origin shielding. |
| **No Incomplete Multipart Cleanup** | Failed multi-gigabyte uploads accumulate orphaned chunks on S3, costing thousands monthly. | Configure S3 bucket lifecycle rule to automatically abort after 7 days. |

---

## 8. Performance & Security Considerations

### 8.1 S3 Bucket Encryption & Access Controls
Enable Server-Side Encryption (SSE-S3 or SSE-KMS) by default on the bucket. Use AWS IAM Roles (OIDC in Kubernetes) instead of static hardcoded AWS credentials in `application.yaml`.

### 8.2 Client Direct Upload Rate Limiting
Rate limit the `/api/v1/files/presigned-url` endpoint to prevent abusive clients from generating millions of presigned upload tokens.

---

## 9. Comparison / Decision Matrix

| Strategy | Max File Size | Memory Overhead | Network Bandwidth on Backend |
|---|---|---|---|
| **Spring MultipartFile (`StandardServletMultipartResolver`)** | $\sim 50\text{ MB}$ | High (Buffers in JVM heap/disk) | $100\%$ traverses Spring Boot server |
| **S3 Presigned PUT URL** | $5\text{ GB}$ | Zero (Only JSON metadata) | $0\%$ traverses Spring Boot server |
| **S3 Multipart Upload** | $5\text{ TB}$ | Zero (Chunks go direct to S3) | $0\%$ traverses Spring Boot server |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. Why shouldn't large files be uploaded directly through a Spring Boot controller?</strong></summary>

Uploading large files directly through Spring Boot consumes JVM heap memory, ties up worker threads for prolonged durations, increases connection pool starvation risk, and saturates backend network interfaces.
</details>

<details>
<summary><strong>2. How does the S3 Presigned URL workflow work?</strong></summary>

The client requests a presigned URL with file metadata (size, content-type). The backend validates permissions, generates a cryptographically signed URL with a 15-minute expiration using the AWS SDK, and returns it. The client issues an HTTP PUT request with the raw binary directly to S3.
</details>

<details>
<summary><strong>3. How does S3 Multipart Upload handle 50 GB video uploads?</strong></summary>

The client initiates the upload, splits the file into chunks (e.g., 10 MB each), and uploads parts concurrently to S3 using presigned URLs. Once all parts finish, the client calls a complete endpoint, and S3 stitches the parts together into a single object.
</details>

<details>
<summary><strong>4. How do you trigger thumbnail generation after an upload completes?</strong></summary>

Configure S3 Event Notifications to publish an `ObjectCreated:Put` event to AWS SQS or Apache Kafka. A pool of Spring Boot media workers consumes the event, downloads the file, resizes it with ImageIO/FFmpeg, uploads thumbnails to S3, and updates the database.
</details>

<details>
<summary><strong>5. How do you protect private uploaded documents (e.g., medical records)?</strong></summary>

Keep the S3 bucket private. Serve private downloads using CloudFront Presigned URLs or signed cookies tied to the authenticated user's session.
</details>

<details>
<summary><strong>6. What is the purpose of S3 Lifecycle Rules for multipart uploads?</strong></summary>

If a client disconnects halfway through a 10 GB upload, the uploaded chunks remain in S3 indefinitely, accruing storage fees. S3 lifecycle rules automatically purge uncompleted multipart parts after 7 days.
</details>

<details>
<summary><strong>7. How do you prevent malicious executable uploads disguised as images?</strong></summary>

Never trust file extensions. Inspect the first few bytes (magic numbers) of the file using Apache Tika or file header validators, and run automated antivirus scans (e.g. ClamAV) in the background pipeline.
</details>

<details>
<summary><strong>8. How do you optimize global download speeds for media assets?</strong></summary>

Route all asset downloads through a global CDN (Amazon CloudFront or Cloudflare) with edge caching and HTTP/3 support.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create an S3 Presigned URL Endpoint</strong></summary>

**Prompt**: Implement a Spring `@RestController` endpoint returning a presigned URL valid for 10 minutes.

**Solution**:
Inspect Example 1: inject `S3Presigner`, build `PutObjectPresignRequest` with 10-minute signature duration, and return the generated URL.
</details>

<details>
<summary><strong>Exercise 2: Validate File Content-Type Whitelist</strong></summary>

**Prompt**: Write a validator rejecting any upload request that is not an image (`image/jpeg`, `image/png`) or PDF (`application/pdf`).

**Solution**:
```java
public void validateContentType(String contentType) {
    Set<String> allowed = Set.of("image/jpeg", "image/png", "application/pdf");
    if (!allowed.contains(contentType)) {
        throw new IllegalArgumentException("Unsupported media type: " + contentType);
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Configure AWS SDK v2 S3 Client Bean</strong></summary>

**Prompt**: Configure an `S3Client` bean in Spring Boot using default credentials provider.

**Solution**:
```java
@Bean
public S3Client s3Client() {
    return S3Client.builder()
            .region(Region.US_EAST_1)
            .credentialsProvider(DefaultCredentialsProvider.create())
            .build();
}
```
</details>

<details>
<summary><strong>Exercise 4: Implement Magic Number Image Checker</strong></summary>

**Prompt**: Write a Java utility method checking if the first 4 bytes of an `InputStream` match PNG or JPEG magic bytes.

**Solution**:
```java
public boolean isImage(byte[] header) {
    // PNG: 89 50 4E 47
    boolean isPng = header[0] == (byte)0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47;
    // JPEG: FF D8 FF
    boolean isJpeg = header[0] == (byte)0xFF && header[1] == (byte)0xD8 && header[2] == (byte)0xFF;
    return isPng || isJpeg;
}
```
</details>

<details>
<summary><strong>Exercise 5: Simulate S3 Event Processing in JUnit</strong></summary>

**Prompt**: Write a test verifying that `MediaProcessingWorker` updates the file record status from `PENDING_UPLOAD` to `READY`.

**Solution**:
Mock the `S3Client`, save a `FileRecord` in an in-memory repository, call `onFileUploaded(key)`, and assert that `record.getStatus() == FileStatus.READY`.
</details>

<details>
<summary><strong>Exercise 6: Generate CloudFront Signed URL</strong></summary>

**Prompt**: Write a method generating a time-limited signed URL using CloudFront URL signer.

**Solution**:
Use `CloudFrontUtilities.getInstance().getSignedUrlWithCannedPolicy(...)` passing the resource URL, key pair ID, private key path, and expiration instant.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   FILE UPLOAD ARCHITECTURE CHEATSHEET                  │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Zero JVM Buffering: Client uploads directly to S3 via Presigned URL │
│ 2. Large Files (>100MB): S3 Multipart Upload (concurrent chunks)       │
│ 3. Post-Processing: S3 Event -> Kafka/SQS -> Media Workers (ClamAV)    │
│ 4. Asset Delivery: Front S3 with CloudFront CDN edge caching           │
│ 5. Private Files: Protect downloads via CloudFront Presigned URLs      │
│ 6. Cost Control: S3 Lifecycle rule to abort uncompleted uploads in 7d  │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [AWS SDK for Java 2.x S3 Presigner Documentation](https://docs.aws.amazon.com/sdk-for-java/latest/developer-guide/examples-s3-presign.html)
- [Amazon S3 Multipart Upload Overview](https://docs.aws.amazon.com/AmazonS3/latest/userguide/mpuoverview.html)
- [Spring Cloud AWS Documentation](https://awspring.io/)
- [ClamAV Antivirus Open Source Engine](https://www.clamav.net/)
