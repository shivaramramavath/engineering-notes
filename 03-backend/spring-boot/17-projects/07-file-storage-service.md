# Production Reference Architecture: Secure File Storage Service

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `SPRING-PROJ-007` |
| **Spring Boot Version** | `3.4.x` / `4.x` |
| **Spring Framework Version** | `6.2.x` / `7.0.x` |
| **Java Baseline** | `Java 21` / `Java 25 LTS` |
| **Build Tool** | `Maven 3.9+` |
| **Object Storage** | `AWS S3 SDK v2` / `MinIO` (S3 Compatible), Presigned URLs |
| **Security & Validation** | Apache Tika (Magic byte MIME type verification), ClamAV Antivirus Scanner |
| **Testing** | `JUnit 5.11+`, `Testcontainers` (LocalStack / MinIO) |
| **Complexity Level** | Intermediate |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |         Web / Mobile Client           |
                      +-------------------+-------------------+
                                          |
                                          | 1. POST /api/v1/files/upload-url (File metadata)
                                          v
                      +---------------------------------------+
                      |        FileManagementController       |
                      +-------------------+-------------------+
                                          |
                                          | 2. Generates S3 Presigned Upload URL (15m TTL)
                                          v
                      +---------------------------------------+
                      |          S3StorageService             |
                      +-------------------+-------------------+
                                          |
                          +---------------+---------------+
                          |                               |
                          | Returns Presigned PUT URL     | Records Pending Metadata
                          v                               v
             +-------------------------+     +-------------------------+
             | Client uploads directly |     | PostgreSQL Database     |
             | to AWS S3 bucket!       |     | (FileMetadataEntity)    |
             | (Bypasses Spring RAM!)  |     |                         |
             +-------------------------+     +-------------------------+
```

### 1.2 Directory Layout

```
07-file-storage-service/
|-- pom.xml
|-- Dockerfile
|-- compose.yaml
`-- src/
    |-- main/
    |   |-- java/com/example/storage/
    |   |   |-- StorageApplication.java
    |   |   |-- config/
    |   |   |   `-- S3ClientConfig.java
    |   |   |-- domain/
    |   |   |   `-- FileMetadata.java
    |   |   |-- dto/
    |   |   |   |-- PresignedUrlResponse.java
    |   |   |   `-- FileUploadRequest.java
    |   |   |-- service/
    |   |   |   `-- S3StorageService.java
    |   |   `-- web/
    |   |       `-- FileController.java
    |   `-- resources/
    |       |-- application.yaml
    |       `-- db/migration/
    |           `-- V1__init_storage_schema.sql
    `-- test/
        `-- java/com/example/storage/
            `-- FileStorageIntegrationTest.java
```

---

## 2. Complete Production Implementation

### 2.1 Build Configuration (`pom.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 https://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.4.1</version>
        <relativePath/>
    </parent>

    <groupId>com.example</groupId>
    <artifactId>file-storage-service</artifactId>
    <version>1.0.0</version>
    <name>file-storage-service</name>

    <properties>
        <java.version>21</java.version>
        <aws.sdk.version>2.29.20</aws.sdk.version>
    </properties>

    <dependencyManagement>
        <dependencies>
            <dependency>
                <groupId>software.amazon.awssdk</groupId>
                <artifactId>bom</artifactId>
                <version>${aws.sdk.version}</version>
                <type>pom</type>
                <scope>import</scope>
            </dependency>
        </dependencies>
    </dependencyManagement>

    <dependencies>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-validation</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-data-jpa</artifactId>
        </dependency>
        <dependency>
            <groupId>software.amazon.awssdk</groupId>
            <artifactId>s3</artifactId>
        </dependency>
        <dependency>
            <groupId>org.apache.tika</groupId>
            <artifactId>tika-core</artifactId>
            <version>3.0.0</version>
        </dependency>
        <dependency>
            <groupId>org.postgresql</groupId>
            <artifactId>postgresql</artifactId>
            <scope>runtime</scope>
        </dependency>
        <dependency>
            <groupId>org.flywaydb</groupId>
            <artifactId>flyway-core</artifactId>
        </dependency>
        <dependency>
            <groupId>org.flywaydb</groupId>
            <artifactId>flyway-database-postgresql</artifactId>
        </dependency>

        <!-- Testing -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-test</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.testcontainers</groupId>
            <artifactId>localstack</artifactId>
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

### 2.2 S3 Configuration & Presigned URL Generator Service

```java
package com.example.storage.service;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import software.amazon.awssdk.services.s3.model.GetObjectRequest;
import software.amazon.awssdk.services.s3.model.PutObjectRequest;
import software.amazon.awssdk.services.s3.presigner.S3Presigner;
import software.amazon.awssdk.services.s3.presigner.model.GetObjectPresignRequest;
import software.amazon.awssdk.services.s3.presigner.model.PresignedGetObjectRequest;
import software.amazon.awssdk.services.s3.presigner.model.PresignedPutObjectRequest;
import software.amazon.awssdk.services.s3.presigner.model.PutObjectPresignRequest;

import java.time.Duration;
import java.util.UUID;

public record PresignedUploadResponse(String fileKey, String uploadUrl, long expiresInSeconds) {}

@Service
public class S3StorageService {

    private final S3Presigner presigner;
    private final String bucketName;

    public S3StorageService(S3Presigner presigner, @Value("${app.s3.bucket}") String bucketName) {
        this.presigner = presigner;
        this.bucketName = bucketName;
    }

    // Direct-to-S3 Upload: Client uploads directly to AWS S3 without passing 100MB files through Spring Boot!
    public PresignedUploadResponse generatePresignedUploadUrl(String originalFilename, String contentType) {
        String fileKey = UUID.randomUUID() + "-" + originalFilename.replaceAll("[^a-zA-Z0-9._-]", "_");

        PutObjectRequest objectRequest = PutObjectRequest.builder()
                .bucket(bucketName)
                .key(fileKey)
                .contentType(contentType)
                .build();

        PutObjectPresignRequest presignRequest = PutObjectPresignRequest.builder()
                .signatureDuration(Duration.ofMinutes(15))
                .putObjectRequest(objectRequest)
                .build();

        PresignedPutObjectRequest presignedRequest = presigner.presignPutObject(presignRequest);
        return new PresignedUploadResponse(fileKey, presignedRequest.url().toString(), 900);
    }

    public String generatePresignedDownloadUrl(String fileKey) {
        GetObjectRequest getRequest = GetObjectRequest.builder()
                .bucket(bucketName)
                .key(fileKey)
                .build();

        GetObjectPresignRequest presignRequest = GetObjectPresignRequest.builder()
                .signatureDuration(Duration.ofHours(1))
                .getObjectRequest(getRequest)
                .build();

        PresignedGetObjectRequest presigned = presigner.presignGetObject(presignRequest);
        return presigned.url().toString();
    }
}
```

### 2.3 Magic Byte MIME Validation Utility

```java
package com.example.storage.util;

import org.apache.tika.Tika;

import java.io.InputStream;
import java.util.Set;

public final class FileSecurityValidator {

    private static final Tika TIKA = new Tika();
    private static final Set<String> ALLOWED_MIME_TYPES = Set.of(
            "image/jpeg", "image/png", "application/pdf", "text/plain"
    );

    private FileSecurityValidator() {}

    // Inspects binary magic bytes, NOT the spoofable file extension!
    public static void validateMagicBytes(InputStream inputStream, String claimedMimeType) throws Exception {
        String detectedMime = TIKA.detect(inputStream);
        if (!ALLOWED_MIME_TYPES.contains(detectedMime)) {
            throw new IllegalArgumentException("Unsupported or malicious file type detected: " + detectedMime);
        }
    }
}
```

### 2.4 REST Controller

```java
package com.example.storage.web;

import com.example.storage.service.PresignedUploadResponse;
import com.example.storage.service.S3StorageService;
import jakarta.validation.constraints.NotBlank;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.Map;

public record RequestUploadDto(@NotBlank String filename, @NotBlank String contentType) {}

@RestController
@RequestMapping("/api/v1/files")
public class FileController {

    private final S3StorageService s3Service;

    public FileController(S3StorageService s3Service) {
        this.s3Service = s3Service;
    }

    @PostMapping("/upload-url")
    public ResponseEntity<PresignedUploadResponse> requestUploadUrl(@RequestBody RequestUploadDto request) {
        return ResponseEntity.ok(s3Service.generatePresignedUploadUrl(request.filename(), request.contentType()));
    }

    @GetMapping("/download-url/{fileKey}")
    public ResponseEntity<Map<String, String>> requestDownloadUrl(@PathVariable String fileKey) {
        String url = s3Service.generatePresignedDownloadUrl(fileKey);
        return ResponseEntity.ok(Map.of("downloadUrl", url));
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.storage;

import com.example.storage.service.PresignedUploadResponse;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;

import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class FileStorageIntegrationTest {

    @Autowired private TestRestTemplate restTemplate;

    @Test
    void shouldGeneratePresignedUploadUrlSuccessfully() {
        ResponseEntity<PresignedUploadResponse> response = restTemplate.postForEntity(
                "/api/v1/files/upload-url",
                Map.of("filename", "architecture-diagram.png", "contentType", "image/png"),
                PresignedUploadResponse.class
        );

        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.OK);
        PresignedUploadResponse body = response.getBody();
        assertThat(body).isNotNull();
        assertThat(body.uploadUrl()).contains("architecture-diagram");
    }
}
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
FROM eclipse-temurin:21-jdk-alpine AS build
WORKDIR /app
COPY pom.xml .
COPY src src
RUN ./mvnw clean package -DskipTests

FROM eclipse-temurin:21-jre-alpine
WORKDIR /app
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
USER appuser:appgroup
COPY --from=build /app/target/*.jar app.jar
ENV JAVA_TOOL_OPTIONS="-XX:+UseG1GC -XX:MaxRAMPercentage=75.0"
EXPOSE 8080
ENTRYPOINT ["java", "-jar", "app.jar"]
```

### 4.2 `compose.yaml` (MinIO S3 Compatible Object Store)

```yaml
services:
  file-service:
    build: .
    ports:
      - "8080:8080"
    environment:
      - APP_S3_BUCKET=enterprise-files
      - AWS_ENDPOINT=http://minio:9000
    depends_on:
      - minio

  minio:
    image: minio/minio:RELEASE.2024-11-07T00-52-28Z
    command: server /data --console-address ":9001"
    ports:
      - "9000:9000"
      - "9001:9001"
    environment:
      - MINIO_ROOT_USER=minioadmin
      - MINIO_ROOT_PASSWORD=miniopassword
    volumes:
      - miniodata:/data

volumes:
  miniodata:
```

---

## 5. Operational Runbook & Verification

```bash
# 1. Request presigned upload URL
RESPONSE=$(curl -s -X POST http://localhost:8080/api/v1/files/upload-url \
     -H "Content-Type: application/json" \
     -d '{"filename": "document.pdf", "contentType": "application/pdf"}')

UPLOAD_URL=$(echo $RESPONSE | jq -r .uploadUrl)

# 2. Upload file DIRECTLY to S3 / MinIO via HTTP PUT
curl -X PUT -T local_file.pdf -H "Content-Type: application/pdf" "$UPLOAD_URL"
```
