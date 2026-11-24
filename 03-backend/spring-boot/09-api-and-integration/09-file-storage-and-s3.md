# Cloud File Storage and AWS S3 / MinIO Integration

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `software.amazon.awssdk:s3:2.29.x`, `io.awspring.cloud:spring-cloud-aws-starter-s3:3.2.x` |

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
Storing user-uploaded files (profile pictures, legal documents, invoices, media assets) directly on local server filesystems introduces fatal scalability hazards. When services run in containerized environments (Docker, Kubernetes), local disk storage is ephemeral—restarting a pod destroys all saved files. Furthermore, storing files locally prevents horizontal scaling because multiple server instances cannot access each other's local disks. Uploading large multi-gigabyte files directly through application server threads exhausts JVM heap, saturates network interfaces, and ties up web worker threads for prolonged durations.

### The Evolution
Cloud object storage services like Amazon S3, Google Cloud Storage, Azure Blob Storage, and S3-compatible on-premise engines like MinIO provide virtually unlimited, highly durable (99.999999999% eleven-nines durability), horizontally accessible binary object storage. In modern Spring architectures, instead of proxying large files through application memory, systems generate cryptographically signed **Presigned URLs**, empowering clients to upload and download files directly to/from S3 with zero backend compute overhead.

### Relevance in Modern Engineering
Spring Boot 3 integrates with AWS SDK for Java v2 and Spring Cloud AWS S3 starter. This provides thread-safe S3 clients, reactive streaming transfers, automated multipart uploads, and secure presigning workflows for enterprise applications.

---

## 2. Core Concepts

- **Object Storage vs Block Storage**: Object storage treats files as discrete, immutable blobs identified by a unique key inside a flat bucket, rather than files within a hierarchical filesystem directory.
- **S3 Bucket & Object Key**: A bucket is a globally unique container; an object key is the unique identifier string (e.g., `tenants/102/invoices/inv-2026.pdf`).
- **AWS SDK for Java v2**: The non-blocking, modular AWS SDK built on top of Netty and HTTP client SPIs.
- **S3Presigner**: Component generating short-lived, pre-authenticated URLs (`PresignedPutObjectRequest`, `PresignedGetObjectRequest`) allowing clients to perform S3 operations without possessing AWS credentials.
- **Multipart Upload**: Dividing large objects (> 100MB) into parallel chunks for accelerated throughput and network fault resilience.
- **MinIO**: High-performance, S3-compatible open-source object storage engine frequently used for local development and integration testing via Testcontainers.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Secure Presigned S3 Upload Flow                            |
|                                                                                   |
| 1. Client Requests Upload Permission                                              |
|    POST /api/v1/files/upload-request (file: "statement.pdf", type: "app/pdf")    |
|                   |                                                               |
|                   v                                                               |
| 2. Spring Service Generates S3 Presigned URL                                      |
|    +-----------------------------+                                                |
|    | S3Presigner generates URL   | ---> Signs URL with AWS IAM Credentials        |
|    +-----------------------------+      Expires in: 15 minutes                    |
|                   |                                                               |
|                   v                                                               |
| 3. Backend Returns Upload URL to Client                                           |
|    { "uploadUrl": "https://bucket.s3.amazonaws.com/uuid.pdf?X-Amz-Signature=..." }|
|                   |                                                               |
|                   v                                                               |
| 4. Client Direct Binary Upload (Zero Backend CPU Load)                            |
|    PUT https://bucket.s3.amazonaws.com/uuid.pdf?... (Binary Stream)               |
|                   | AWS S3 validates cryptographic signature                      |
|                   v                                                               |
| 5. Direct 200 OK from Amazon S3 to Client                                         |
|                   |                                                               |
|                   v (Optional S3 Event Notification / Webhook)                    |
| 6. S3 triggers Lambda / SQS / Webhook -> Spring confirms file persistence         |
+-----------------------------------------------------------------------------------+
```

When an upload is initiated, the Spring Boot application authenticates the user and invokes `S3Presigner`. The presigner constructs an HTTP request matching the expected bucket, key, and content type, and computes an AWS Signature Version 4 (SigV4) HMAC token embedded directly in the URL query string. The client uses this presigned URL to stream the binary payload directly into Amazon S3 over HTTPS. The application server is completely bypassed during data transfer, preserving network bandwidth and JVM memory.

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>software.amazon.awssdk</groupId>
    <artifactId>s3</artifactId>
    <version>2.29.15</version>
</dependency>
```

### Application Properties
```properties
# AWS S3 Configuration
aws.s3.bucket-name=enterprise-app-storage
aws.s3.region=us-east-1
aws.s3.endpoint=http://localhost:9000 # Optional: MinIO endpoint for local dev
aws.s3.access-key=minioadmin
aws.s3.secret-key=minioadmin
```

---

## 5. Practical Examples

### Example 1: S3 Client Configuration Bean Supporting MinIO and AWS S3
```java
package com.example.platform.storage.config;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import software.amazon.awssdk.auth.credentials.AwsBasicCredentials;
import software.amazon.awssdk.auth.credentials.StaticCredentialsProvider;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.s3.S3ClientBuilder;
import software.amazon.awssdk.services.s3.presigner.S3Presigner;

import java.net.URI;

@Configuration
public class S3StorageConfig {

    @Value("${aws.s3.region:us-east-1}")
    private String region;

    @Value("${aws.s3.endpoint:#{null}}")
    private String endpoint;

    @Value("${aws.s3.access-key}")
    private String accessKey;

    @Value("${aws.s3.secret-key}")
    private String secretKey;

    @Bean
    public S3Client s3Client() {
        S3ClientBuilder builder = S3Client.builder()
                .region(Region.of(region))
                .credentialsProvider(StaticCredentialsProvider.create(
                        AwsBasicCredentials.create(accessKey, secretKey)));

        if (endpoint != null && !endpoint.isBlank()) {
            builder.endpointOverride(URI.create(endpoint))
                   .forcePathStyle(true); // Required for MinIO
        }

        return builder.build();
    }

    @Bean
    public S3Presigner s3Presigner() {
        var builder = S3Presigner.builder()
                .region(Region.of(region))
                .credentialsProvider(StaticCredentialsProvider.create(
                        AwsBasicCredentials.create(accessKey, secretKey)));

        if (endpoint != null && !endpoint.isBlank()) {
            builder.endpointOverride(URI.create(endpoint));
        }

        return builder.build();
    }
}
```

### Example 2: Presigned URL Generation Service
```java
package com.example.platform.storage.service;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import software.amazon.awssdk.services.s3.model.GetObjectRequest;
import software.amazon.awssdk.services.s3.model.PutObjectRequest;
import software.amazon.awssdk.services.s3.presigner.S3Presigner;
import software.amazon.awssdk.services.s3.presigner.model.GetObjectPresignRequest;
import software.amazon.awssdk.services.s3.presigner.model.PutObjectPresignRequest;

import java.time.Duration;
import java.util.UUID;

@Service
public class S3PresignedUrlService {

    private final S3Presigner presigner;
    private final String bucketName;

    public S3PresignedUrlService(S3Presigner presigner, @Value("${aws.s3.bucket-name}") String bucketName) {
        this.presigner = presigner;
        this.bucketName = bucketName;
    }

    public record UploadTicket(String objectKey, String uploadUrl, long expiresInMinutes) {}

    public UploadTicket generatePresignedUploadUrl(String originalFilename, String contentType) {
        String objectKey = "uploads/" + UUID.randomUUID() + "-" + originalFilename;

        PutObjectRequest objectRequest = PutObjectRequest.builder()
                .bucket(bucketName)
                .key(objectKey)
                .contentType(contentType)
                .build();

        PutObjectPresignRequest presignRequest = PutObjectPresignRequest.builder()
                .signatureDuration(Duration.ofMinutes(15))
                .putObjectRequest(objectRequest)
                .build();

        String presignedUrl = presigner.presignPutObject(presignRequest).url().toString();
        return new UploadTicket(objectKey, presignedUrl, 15);
    }

    public String generatePresignedDownloadUrl(String objectKey) {
        GetObjectRequest getRequest = GetObjectRequest.builder()
                .bucket(bucketName)
                .key(objectKey)
                .build();

        GetObjectPresignRequest presignRequest = GetObjectPresignRequest.builder()
                .signatureDuration(Duration.ofMinutes(10))
                .getObjectRequest(getRequest)
                .build();

        return presigner.presignGetObject(presignRequest).url().toString();
    }
}
```

### Example 3: Direct Streaming Upload with S3Client (Server-Side Ingestion)
```java
package com.example.platform.storage.service;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import software.amazon.awssdk.core.sync.RequestBody;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.s3.model.PutObjectRequest;

import java.io.InputStream;
import java.util.Map;

@Service
public class S3DirectStorageService {

    private final S3Client s3Client;
    private final String bucketName;

    public S3DirectStorageService(S3Client s3Client, @Value("${aws.s3.bucket-name}") String bucketName) {
        this.s3Client = s3Client;
        this.bucketName = bucketName;
    }

    public void uploadStream(String objectKey, InputStream inputStream, long contentLength, String contentType) {
        PutObjectRequest putRequest = PutObjectRequest.builder()
                .bucket(bucketName)
                .key(objectKey)
                .contentType(contentType)
                .metadata(Map.of("uploaded-by", "system-worker"))
                .build();

        s3Client.putObject(putRequest, RequestBody.fromInputStream(inputStream, contentLength));
    }
}
```

### Example 4: Loading Entire File into Memory (Anti-Pattern Fix)
```java
// ANTI-PATTERN: Reading byte[] entirely into JVM memory via MultipartFile.getBytes().
// Uploading a 500MB file creates immediate GC spikes and risks OutOfMemoryError under concurrent traffic.
// byte[] bytes = multipartFile.getBytes(); // MEMORY EXPLOSION!

// FIX: Stream the input stream directly or redirect client to S3 Presigned URL
package com.example.platform.storage.controller;

import com.example.platform.storage.service.S3DirectStorageService;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;

import java.io.IOException;
import java.util.UUID;

@RestController
@RequestMapping("/api/v1/files")
public class FileUploadController {

    private final S3DirectStorageService storageService;

    public FileUploadController(S3DirectStorageService storageService) {
        this.storageService = storageService;
    }

    @PostMapping("/direct-stream")
    public ResponseEntity<String> uploadDirectStream(@RequestParam("file") MultipartFile file) throws IOException {
        String key = "documents/" + UUID.randomUUID() + "-" + file.getOriginalFilename();
        
        // Streams directly to S3 without loading whole byte array into heap
        storageService.uploadStream(key, file.getInputStream(), file.getSize(), file.getContentType());
        
        return ResponseEntity.ok(key);
    }
}
```

### Example 5: High-Performance Multipart Upload Manager for Large Datasets
```java
package com.example.platform.storage.service;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import software.amazon.awssdk.core.sync.RequestBody;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.s3.model.*;

import java.io.ByteArrayInputStream;
import java.util.ArrayList;
import java.util.List;

@Service
public class S3MultipartUploadService {

    private static final Logger log = LoggerFactory.getLogger(S3MultipartUploadService.class);
    private final S3Client s3Client;
    private final String bucketName;

    public S3MultipartUploadService(S3Client s3Client, @Value("${aws.s3.bucket-name}") String bucketName) {
        this.s3Client = s3Client;
        this.bucketName = bucketName;
    }

    public void uploadLargeFileMultipart(String objectKey, List<byte[]> chunks) {
        // 1. Initiate Multipart Upload
        CreateMultipartUploadResponse initResponse = s3Client.createMultipartUpload(
                CreateMultipartUploadRequest.builder().bucket(bucketName).key(objectKey).build());
        String uploadId = initResponse.uploadId();

        List<CompletedPart> completedParts = new ArrayList<>();
        try {
            // 2. Upload individual chunks in parallel or sequence
            for (int i = 0; i < chunks.size(); i++) {
                int partNumber = i + 1;
                byte[] chunk = chunks.get(i);

                UploadPartResponse partResponse = s3Client.uploadPart(
                        UploadPartRequest.builder()
                                .bucket(bucketName)
                                .key(objectKey)
                                .uploadId(uploadId)
                                .partNumber(partNumber)
                                .build(),
                        RequestBody.fromInputStream(new ByteArrayInputStream(chunk), chunk.length)
                );

                completedParts.add(CompletedPart.builder()
                        .partNumber(partNumber)
                        .etag(partResponse.etag())
                        .build());
            }

            // 3. Complete Multipart Upload
            CompletedMultipartUpload completedUpload = CompletedMultipartUpload.builder()
                    .parts(completedParts)
                    .build();

            s3Client.completeMultipartUpload(CompleteMultipartUploadRequest.builder()
                    .bucket(bucketName)
                    .key(objectKey)
                    .uploadId(uploadId)
                    .multipartUpload(completedUpload)
                    .build());

            log.info("Successfully completed multipart upload for key: {}", objectKey);
        } catch (Exception e) {
            log.error("Multipart upload failed. Aborting uploadId: {}", uploadId, e);
            s3Client.abortMultipartUpload(AbortMultipartUploadRequest.builder()
                    .bucket(bucketName)
                    .key(objectKey)
                    .uploadId(uploadId)
                    .build());
            throw new RuntimeException("Multipart upload aborted", e);
        }
    }
}
```

---

## 6. Best Practices

1. **Prefer Presigned URLs Over Proxying Through Backend**: Uploading large binaries directly from client to S3 preserves application memory, CPU, and network throughput.
2. **Never Store AWS Credentials in Source Code**: Use AWS Default Credentials Provider Chain (environment variables, IAM roles for EC2/EKS) in production.
3. **Always Abort Failed Multipart Uploads**: Uncompleted multipart uploads accumulate hidden storage costs indefinitely. Implement bucket lifecycle rules to auto-abort incomplete multipart uploads after 7 days.
4. **Sanitize S3 Object Keys**: Prevent directory traversal or bucket-level collisions by prefixing object keys with UUIDs and sanitizing special characters.
5. **Enable Server-Side Encryption (SSE-S3 or SSE-KMS)**: Enforce encryption at rest by default across all S3 buckets storing customer data.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Hardcoding AWS access/secret keys in `application.properties` | Keys leaked into Git repositories and security scanners | Use IAM Roles, AWS Vault, or environment variables (`AWS_ACCESS_KEY_ID`). |
| Using `.forcePathStyle(true)` on Amazon S3 | AWS deprecated path-style URLs (`s3.amazonaws.com/bucket`) in favor of virtual-hosted | Only enable `forcePathStyle(true)` when connecting to local MinIO instances. |
| Neglecting to configure CORS on the S3 bucket | Browser blocks client direct uploads via Presigned URLs due to CORS policy | Configure CORS on S3 bucket allowing `PUT`, `GET` from authorized web origins. |
| Buffering entire `MultipartFile` in memory | Server crashes with JVM `OutOfMemoryError` during concurrent large uploads | Stream via `file.getInputStream()` or generate client presigned URLs. |
| Incomplete multipart uploads never aborted | AWS charges for partial orphan chunks forever | Configure S3 bucket lifecycle rule: `AbortIncompleteMultipartUploadAfterDays=7`. |
| Unrestricted file upload types | Attackers upload executable malware or HTML/JS causing stored XSS | Validate file magic bytes (MIME type inspection) and force `Content-Disposition: attachment`. |

---

## 8. Performance & Security Notes

- **SigV4 Presigning Expiry**: Keep presigned URL durations short (e.g., 5 to 15 minutes) to minimize exposure windows if URLs are intercepted.
- **Magic Byte Validation**: Never trust client-reported `Content-Type` headers or file extensions alone. Inspect initial file magic bytes using Apache Tika before processing.
- **MinIO Testcontainers**: In JUnit tests, use `org.testcontainers:minio` to test real S3 upload/download semantics without mocking or cloud egress fees.

---

## 9. Comparison / When to Use What

| Storage Approach | S3 Presigned URLs | S3 Backend Streaming | Local Filesystem |
|---|---|---|---|
| **Server CPU & RAM Load** | Zero (Client $\to$ S3 directly) | Moderate (Streamed through server) | High (Local I/O and disk locks) |
| **Network Bottleneck** | None on application server | Double bandwidth (Client $\to$ Server $\to$ S3) | High disk I/O |
| **Horizontal Scalability** | Infinite | High | Zero (Nodes have isolated disks) |
| **Pre-Upload Inspection** | None (Upload happens before inspect) | Full (Inspect stream as it passes) | Full |
| **Best For** | Large files, mobile/web uploads | Automated reports, backend worker ingestion | Temporary local scratchpad only |

---

## 10. Interview Questions

<details>
<summary>1. What are S3 Presigned URLs and why are they considered an enterprise best practice?</summary>
Presigned URLs grant temporary access to a specific S3 object using cryptographic signatures derived from server IAM credentials. They are an enterprise best practice because they allow clients (browsers, mobile apps) to upload or download files directly to/from S3, completely bypassing application servers, eliminating server memory pressure, CPU overhead, and network bandwidth saturation.
</details>

<details>
<summary>2. How do you configure the AWS SDK v2 for MinIO local testing?</summary>
MinIO requires two specific client builder configurations: overriding the endpoint URL (`.endpointOverride(URI.create("http://localhost:9000"))`) and enabling path-style access (`.forcePathStyle(true)`), because MinIO does not use DNS virtual-host subdomains like AWS.
</details>

<details>
<summary>3. Why must incomplete multipart uploads be explicitly aborted?</summary>
When a multipart upload is initiated, AWS S3 creates an upload ID and retains all uploaded parts until either `CompleteMultipartUpload` or `AbortMultipartUpload` is called. If an upload fails and is never aborted, the parts remain in S3 storage indefinitely, incurring storage fees without being visible as complete objects.
</details>

<details>
<summary>4. What is the difference between AWS SDK for Java v1 and v2?</summary>
AWS SDK v2 is a complete architectural rewrite. It is modularized (importing only `software.amazon.awssdk:s3`), built on non-blocking HTTP clients (Netty, CRT), supports Java 8+ / Java 17+ APIs (records, `Duration`, `Instant`), and provides non-blocking async clients (`S3AsyncClient`).
</details>

<details>
<summary>5. How do you defend against malicious file uploads in an S3 architecture?</summary>
1. Restrict bucket policies and enforce server-side encryption.
2. Validate file signatures (magic bytes) using libraries like Apache Tika rather than trusting client extensions.
3. Configure `Content-Disposition: attachment` to prevent browsers from executing uploaded HTML/SVG files.
4. Process uploads asynchronously through virus scanners (e.g., ClamAV) via S3 Event Notifications before releasing them to other users.
</details>

<details>
<summary>6. How does AWS SigV4 authentication work under the hood?</summary>
Signature Version 4 calculates a cryptographic HMAC-SHA256 digest across a canonical request string comprising HTTP method, URI, query parameters, headers, and payload hash, combined with date, region, and service scopes using secret signing keys.
</details>

<details>
<summary>7. What happens if a client uploads a file with a content type different from the Presigned URL request?</summary>
If the presigned URL was signed with a specific `Content-Type` header (e.g., `application/pdf`), S3 verifies that the client's `PUT` request carries the identical header. If the client sends a different content type, S3 rejects the upload with HTTP 403 Forbidden.
</details>

<details>
<summary>8. How do you handle large file downloads efficiently in Spring Boot without Presigned URLs?</summary>
By returning a `ResponseEntity<StreamingResponseBody>` or `org.springframework.core.io.Resource`. This streams the S3 `ResponseInputStream` directly to the client's HTTP response output stream without reading the entire file into a JVM byte array.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create S3Client bean with credentials</summary>
Write a Spring `@Configuration` class configuring an `S3Client` bean using AWS environment variable credentials.
</details>

<details>
<summary>Exercise 2 (Easy): Generate a presigned download URL</summary>
Implement a method `String getDownloadUrl(String key)` returning a presigned URL valid for 5 minutes.
</details>

<details>
<summary>Exercise 3 (Medium): Implement direct S3 file streaming download endpoint</summary>
Write a `@GetMapping` controller method that fetches an object stream from S3 and pipes it into `StreamingResponseBody`.
</details>

<details>
<summary>Exercise 4 (Medium): Build an S3 upload validation filter</summary>
Create a validation utility that inspects the first 16 bytes of an `InputStream` to verify that an uploaded image is genuinely a PNG (`89 50 4E 47`).
</details>

<details>
<summary>Exercise 5 (Hard): Implement automated S3 Multipart Upload with chunk retry</summary>
Write a service that slices a large stream into 10MB chunks, uploads them via `UploadPartRequest`, and aborts the multipart upload if any chunk fails after 3 retries.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test with MinIO Testcontainers</summary>
Author a complete JUnit 5 test spinning up a `MinIOContainer`, creating an S3 bucket, uploading a file, and asserting file contents.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                           AWS S3 / MinIO Cheat Sheet                            |
+---------------------------------------------------------------------------------+
| SDK:               software.amazon.awssdk:s3:2.x                                |
| Core Beans:        S3Client (sync), S3AsyncClient (reactive), S3Presigner       |
| MinIO Flags:       .endpointOverride(URI) + .forcePathStyle(true)               |
| Presigning:        presigner.presignPutObject(PutObjectPresignRequest)          |
| Invariant:         Never load byte[] into memory; use streams or presigned URLs |
| Incomplete Parts:  Configure S3 lifecycle rule to abort incomplete uploads       |
| Production IAM:    DefaultCredentialsProvider (IRSA / Instance Profiles)        |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [AWS SDK for Java 2.x S3 Developer Guide](https://docs.aws.amazon.com/sdk-for-java/latest/developer-guide/examples-s3.html)
- [Spring Cloud AWS Documentation](https://awspring.io/)
- [MinIO Object Storage Documentation](https://min.io/docs/minio/linux/index.html)
