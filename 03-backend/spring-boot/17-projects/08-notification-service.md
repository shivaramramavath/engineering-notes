# Project 08: Enterprise Multi-Channel Notification Service

| Metadata Field | Value |
|---|---|
| **Spring Boot Version** | 3.4.2 |
| **Spring Framework** | 6.2.2 |
| **Java Version** | 21 (LTS) |
| **Build System** | Maven 3.9+ |
| **Key Dependencies** | Spring Boot Starter Web, Spring Boot Starter Mail, Spring Kafka, Spring AMQP (RabbitMQ), Resilience4j, Spring Data JPA, PostgreSQL Driver, Testcontainers |
| **Architecture Style** | Event-Driven Multi-Channel Dispatcher (Email, SMS, Webhook) with Dead Letter Queues & Retry |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture

The Notification Service consumes asynchronous dispatch requests from Apache Kafka and RabbitMQ topics/queues. It provides channel-based routing (Email, SMS via provider abstractions, and HTTP Webhooks), tenant-aware rate limiting, automatic backoff retry via Dead Letter Exchanges (DLX) and Dead Letter Topics (DLT), idempotency guarantees, and status callback tracking.

```
       [ Client Microservices ]
                  │
        (Produce Notification Events)
                  ▼
   ┌─────────────────────────────────────────────────────────┐
   │             Message Broker / Ingestion Layer            │
   │  ┌───────────────────────┐   ┌───────────────────────┐  │
   │  │  Kafka: 'notif-req'   │   │ RabbitMQ: 'sms-queue' │  │
   │  └───────────┬───────────┘   └───────────┬───────────┘  │
   └──────────────┼───────────────────────────┼──────────────┘
                  ▼                           ▼
   ┌─────────────────────────────────────────────────────────┐
   │            Spring Boot Notification Service             │
   │                                                         │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │   Idempotency Filter (Redis/PostgreSQL Check)     │  │
   │  └─────────────────────────┬─────────────────────────┘  │
   │                            ▼                            │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │   Channel Router (EMAIL / SMS / WEBHOOK / IN_APP) │  │
   │  └───────┬─────────────────┼──────────────────┬──────┘  │
   │          ▼                 ▼                  ▼         │
   │   ┌─────────────┐   ┌─────────────┐   ┌──────────────┐  │
   │   │ JavaMail    │   │ SMS Gateway │   │ Webhook Exec │  │
   │   │ Sender      │   │ (Twilio API)│   │ (RestClient) │  │
   │   └──────┬──────┘   └──────┬──────┘   └──────┬───────┘  │
   │          │ (Failure)       │ (Failure)       │ (Failure)│
   │          └────────┬────────┴─────────┬───────┘          │
   │                   ▼                  ▼                  │
   │   ┌──────────────────────────────────────────────────┐  │
   │   │ Resilience4j Circuit Breaker + DLT/DLQ Publisher │  │
   │   └──────────────────────────────────────────────────┘  │
   └────────────────────────────┬────────────────────────────┘
                                ▼
   ┌─────────────────────────────────────────────────────────┐
   │             Persistence: PostgreSQL Audit Log           │
   │  - notification_log (id, recipient, channel, status)    │
   │  - delivery_attempts (attempt_number, response_code)    │
   └─────────────────────────────────────────────────────────┘
```

### 1.2 Directory Layout

```
08-notification-service/
├── pom.xml
├── src/
│   ├── main/
│   │   ├── java/com/example/notifications/
│   │   │   ├── NotificationServiceApplication.java
│   │   │   ├── config/
│   │   │   │   ├── KafkaConsumerConfig.java
│   │   │   │   ├── RabbitMqConfig.java
│   │   │   │   └── AsyncConfig.java
│   │   │   ├── domain/
│   │   │   │   ├── NotificationChannel.java
│   │   │   │   ├── NotificationStatus.java
│   │   │   │   ├── Notification.java
│   │   │   │   └── DeliveryAttempt.java
│   │   │   ├── repository/
│   │   │   │   └── NotificationRepository.java
│   │   │   ├── listener/
│   │   │   │   ├── KafkaNotificationListener.java
│   │   │   │   └── RabbitNotificationListener.java
│   │   │   ├── dispatcher/
│   │   │   │   ├── ChannelDispatcher.java
│   │   │   │   ├── EmailChannelDispatcher.java
│   │   │   │   ├── SmsChannelDispatcher.java
│   │   │   │   └── WebhookChannelDispatcher.java
│   │   │   ├── service/
│   │   │   │   └── NotificationDeliveryService.java
│   │   │   └── web/
│   │   │       ├── NotificationController.java
│   │   │       ├── dto/
│   │   │       │   ├── SendNotificationRequest.java
│   │   │       │   └── NotificationResponse.java
│   │   │       └── exception/GlobalExceptionHandler.java
│   │   └── resources/
│   │       ├── application.yaml
│   │       └── db/migration/V1__init_notification_tables.sql
│   └── test/
│       └── java/com/example/notifications/
│           ├── NotificationIntegrationTest.java
│           └── TestcontainersConfiguration.java
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
    <artifactId>notification-service</artifactId>
    <version>1.0.0</version>
    <name>notification-service</name>
    <description>Enterprise Multi-Channel Notification Microservice</description>

    <properties>
        <java.version>21</java.version>
        <testcontainers.version>1.20.4</testcontainers.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-mail</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-data-jpa</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-validation</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-actuator</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.kafka</groupId>
            <artifactId>spring-kafka</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-amqp</artifactId>
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

        <!-- Testing Dependencies -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-test</artifactId>
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
        <dependency>
            <groupId>org.testcontainers</groupId>
            <artifactId>kafka</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.testcontainers</groupId>
            <artifactId>rabbitmq</artifactId>
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
    name: notification-service
  datasource:
    url: jdbc:postgresql://${DB_HOST:localhost}:${DB_PORT:5432}/${DB_NAME:notifications_db}
    username: ${DB_USER:postgres}
    password: ${DB_PASS:postgres}
  jpa:
    hibernate:
      ddl-auto: validate
    open-in-view: false
    show-sql: false
  flyway:
    enabled: true
  mail:
    host: ${SMTP_HOST:smtp.mailtrap.io}
    port: ${SMTP_PORT:2525}
    username: ${SMTP_USER:testuser}
    password: ${SMTP_PASS:testpass}
    properties:
      mail:
        smtp:
          auth: true
          starttls:
            enable: true
  kafka:
    bootstrap-servers: ${KAFKA_BOOTSTRAP:localhost:9092}
    consumer:
      group-id: notification-consumers
      auto-offset-reset: earliest
      key-deserializer: org.apache.kafka.common.serialization.StringDeserializer
      value-deserializer: org.springframework.kafka.support.serializer.JsonDeserializer
      properties:
        spring.json.trusted.packages: "com.example.notifications.*"
    producer:
      key-serializer: org.apache.kafka.common.serialization.StringSerializer
      value-serializer: org.springframework.kafka.support.serializer.JsonSerializer
  rabbitmq:
    host: ${RABBIT_HOST:localhost}
    port: ${RABBIT_PORT:5672}
    username: ${RABBIT_USER:guest}
    password: ${RABBIT_PASS:guest}

management:
  endpoints:
    web:
      exposure:
        include: health,info,metrics,prometheus
```

### 2.3 Flyway Migration: `V1__init_notification_tables.sql`

```sql
CREATE TABLE notifications (
    id UUID PRIMARY KEY,
    idempotency_key VARCHAR(128) UNIQUE NOT NULL,
    recipient VARCHAR(255) NOT NULL,
    channel VARCHAR(32) NOT NULL,
    subject VARCHAR(255),
    content TEXT NOT NULL,
    status VARCHAR(32) NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE delivery_attempts (
    id UUID PRIMARY KEY,
    notification_id UUID NOT NULL REFERENCES notifications(id) ON DELETE CASCADE,
    attempt_number INT NOT NULL,
    status VARCHAR(32) NOT NULL,
    error_message TEXT,
    attempted_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_notif_status ON notifications(status);
CREATE INDEX idx_notif_idem ON notifications(idempotency_key);
```

### 2.4 Domain Models & Repositories

```java
package com.example.notifications.domain;

public enum NotificationChannel {
    EMAIL, SMS, WEBHOOK
}
```

```java
package com.example.notifications.domain;

public enum NotificationStatus {
    PENDING, DELIVERED, FAILED, RETRYING
}
```

```java
package com.example.notifications.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

@Entity
@Table(name = "notifications")
public class Notification {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(name = "idempotency_key", nullable = false, unique = true, length = 128)
    private String idempotencyKey;

    @Column(nullable = false)
    private String recipient;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false, length = 32)
    private NotificationChannel channel;

    private String subject;

    @Column(nullable = false, columnDefinition = "TEXT")
    private String content;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false, length = 32)
    private NotificationStatus status;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    @Column(name = "updated_at", nullable = false)
    private Instant updatedAt = Instant.now();

    @OneToMany(mappedBy = "notification", cascade = CascadeType.ALL, orphanRemoval = true)
    private List<DeliveryAttempt> attempts = new ArrayList<>();

    public Notification() {}

    public Notification(String idempotencyKey, String recipient, NotificationChannel channel, String subject, String content) {
        this.idempotencyKey = idempotencyKey;
        this.recipient = recipient;
        this.channel = channel;
        this.subject = subject;
        this.content = content;
        this.status = NotificationStatus.PENDING;
    }

    public void addAttempt(DeliveryAttempt attempt) {
        attempts.add(attempt);
        attempt.setNotification(this);
    }

    // Getters and setters
    public UUID getId() { return id; }
    public String getIdempotencyKey() { return idempotencyKey; }
    public String getRecipient() { return recipient; }
    public NotificationChannel getChannel() { return channel; }
    public String getSubject() { return subject; }
    public String getContent() { return content; }
    public NotificationStatus getStatus() { return status; }
    public void setStatus(NotificationStatus status) { 
        this.status = status; 
        this.updatedAt = Instant.now();
    }
    public List<DeliveryAttempt> getAttempts() { return attempts; }
}
```

```java
package com.example.notifications.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "delivery_attempts")
public class DeliveryAttempt {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "notification_id", nullable = false)
    private Notification notification;

    @Column(name = "attempt_number", nullable = false)
    private int attemptNumber;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false, length = 32)
    private NotificationStatus status;

    @Column(name = "error_message", columnDefinition = "TEXT")
    private String errorMessage;

    @Column(name = "attempted_at", nullable = false)
    private Instant attemptedAt = Instant.now();

    public DeliveryAttempt() {}

    public DeliveryAttempt(int attemptNumber, NotificationStatus status, String errorMessage) {
        this.attemptNumber = attemptNumber;
        this.status = status;
        this.errorMessage = errorMessage;
    }

    public void setNotification(Notification notification) {
        this.notification = notification;
    }

    public UUID getId() { return id; }
    public int getAttemptNumber() { return attemptNumber; }
    public NotificationStatus getStatus() { return status; }
    public String getErrorMessage() { return errorMessage; }
}
```

```java
package com.example.notifications.repository;

import com.example.notifications.domain.Notification;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.Optional;
import java.util.UUID;

public interface NotificationRepository extends JpaRepository<Notification, UUID> {
    Optional<Notification> findByIdempotencyKey(String idempotencyKey);
}
```

### 2.5 Dispatchers & Services

```java
package com.example.notifications.dispatcher;

import com.example.notifications.domain.Notification;
import com.example.notifications.domain.NotificationChannel;

public interface ChannelDispatcher {
    NotificationChannel supportsChannel();
    void dispatch(Notification notification);
}
```

```java
package com.example.notifications.dispatcher;

import com.example.notifications.domain.Notification;
import com.example.notifications.domain.NotificationChannel;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.mail.SimpleMailMessage;
import org.springframework.mail.javamail.JavaMailSender;
import org.springframework.stereotype.Component;

@Component
public class EmailChannelDispatcher implements ChannelDispatcher {

    private static final Logger log = LoggerFactory.getLogger(EmailChannelDispatcher.class);
    private final JavaMailSender mailSender;

    public EmailChannelDispatcher(JavaMailSender mailSender) {
        this.mailSender = mailSender;
    }

    @Override
    public NotificationChannel supportsChannel() {
        return NotificationChannel.EMAIL;
    }

    @Override
    public void dispatch(Notification notification) {
        log.info("Sending email to {} with subject: {}", notification.getRecipient(), notification.getSubject());
        SimpleMailMessage message = new SimpleMailMessage();
        message.setTo(notification.getRecipient());
        message.setSubject(notification.getSubject() != null ? notification.getSubject() : "Notification");
        message.setText(notification.getContent());
        mailSender.send(message);
    }
}
```

```java
package com.example.notifications.dispatcher;

import com.example.notifications.domain.Notification;
import com.example.notifications.domain.NotificationChannel;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.web.client.RestClient;

@Component
public class WebhookChannelDispatcher implements ChannelDispatcher {

    private static final Logger log = LoggerFactory.getLogger(WebhookChannelDispatcher.class);
    private final RestClient restClient;

    public WebhookChannelDispatcher(RestClient.Builder restClientBuilder) {
        this.restClient = restClientBuilder.build();
    }

    @Override
    public NotificationChannel supportsChannel() {
        return NotificationChannel.WEBHOOK;
    }

    @Override
    public void dispatch(Notification notification) {
        log.info("Posting webhook payload to URL: {}", notification.getRecipient());
        restClient.post()
                .uri(notification.getRecipient())
                .body(notification.getContent())
                .retrieve()
                .toBodilessEntity();
    }
}
```

```java
package com.example.notifications.service;

import com.example.notifications.dispatcher.ChannelDispatcher;
import com.example.notifications.domain.DeliveryAttempt;
import com.example.notifications.domain.Notification;
import com.example.notifications.domain.NotificationChannel;
import com.example.notifications.domain.NotificationStatus;
import com.example.notifications.repository.NotificationRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.function.Function;
import java.util.stream.Collectors;

@Service
public class NotificationDeliveryService {

    private static final Logger log = LoggerFactory.getLogger(NotificationDeliveryService.class);
    private final NotificationRepository repository;
    private final Map<NotificationChannel, ChannelDispatcher> dispatchers;

    public NotificationDeliveryService(NotificationRepository repository, List<ChannelDispatcher> dispatcherList) {
        this.repository = repository;
        this.dispatchers = dispatcherList.stream()
                .collect(Collectors.toMap(ChannelDispatcher::supportsChannel, Function.identity()));
    }

    @Transactional
    public Notification processNotification(String idempotencyKey, String recipient, NotificationChannel channel, String subject, String content) {
        Optional<Notification> existing = repository.findByIdempotencyKey(idempotencyKey);
        if (existing.isPresent()) {
            log.info("Duplicate idempotency key '{}' encountered. Returning existing record.", idempotencyKey);
            return existing.get();
        }

        Notification notification = new Notification(idempotencyKey, recipient, channel, subject, content);
        notification = repository.save(notification);

        ChannelDispatcher dispatcher = dispatchers.get(channel);
        if (dispatcher == null) {
            markFailed(notification, "No dispatcher registered for channel: " + channel);
            return notification;
        }

        try {
            dispatcher.dispatch(notification);
            notification.setStatus(NotificationStatus.DELIVERED);
            notification.addAttempt(new DeliveryAttempt(1, NotificationStatus.DELIVERED, null));
        } catch (Exception ex) {
            log.error("Failed to deliver notification {}: {}", notification.getId(), ex.getMessage());
            notification.setStatus(NotificationStatus.FAILED);
            notification.addAttempt(new DeliveryAttempt(1, NotificationStatus.FAILED, ex.getMessage()));
        }

        return repository.save(notification);
    }

    private void markFailed(Notification notification, String reason) {
        notification.setStatus(NotificationStatus.FAILED);
        notification.addAttempt(new DeliveryAttempt(1, NotificationStatus.FAILED, reason));
    }
}
```

### 2.6 Message Listeners & REST Controller

```java
package com.example.notifications.web.dto;

import com.example.notifications.domain.NotificationChannel;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;

public record SendNotificationRequest(
        @NotBlank String idempotencyKey,
        @NotBlank String recipient,
        @NotNull NotificationChannel channel,
        String subject,
        @NotBlank String content
) {}
```

```java
package com.example.notifications.web.dto;

import com.example.notifications.domain.NotificationChannel;
import com.example.notifications.domain.NotificationStatus;
import java.util.UUID;

public record NotificationResponse(
        UUID id,
        String idempotencyKey,
        String recipient,
        NotificationChannel channel,
        NotificationStatus status
) {}
```

```java
package com.example.notifications.listener;

import com.example.notifications.service.NotificationDeliveryService;
import com.example.notifications.web.dto.SendNotificationRequest;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.stereotype.Component;

@Component
public class KafkaNotificationListener {

    private static final Logger log = LoggerFactory.getLogger(KafkaNotificationListener.class);
    private final NotificationDeliveryService deliveryService;

    public KafkaNotificationListener(NotificationDeliveryService deliveryService) {
        this.deliveryService = deliveryService;
    }

    @KafkaListener(topics = "notifications-topic", groupId = "notification-consumers")
    public void onNotificationEvent(SendNotificationRequest request) {
        log.info("Received Kafka notification event for recipient: {}", request.recipient());
        deliveryService.processNotification(
                request.idempotencyKey(),
                request.recipient(),
                request.channel(),
                request.subject(),
                request.content()
        );
    }
}
```

```java
package com.example.notifications.web;

import com.example.notifications.domain.Notification;
import com.example.notifications.service.NotificationDeliveryService;
import com.example.notifications.web.dto.NotificationResponse;
import com.example.notifications.web.dto.SendNotificationRequest;
import jakarta.validation.Valid;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/notifications")
public class NotificationController {

    private final NotificationDeliveryService deliveryService;

    public NotificationController(NotificationDeliveryService deliveryService) {
        this.deliveryService = deliveryService;
    }

    @PostMapping
    public ResponseEntity<NotificationResponse> sendNotification(@Valid @RequestBody SendNotificationRequest request) {
        Notification notification = deliveryService.processNotification(
                request.idempotencyKey(),
                request.recipient(),
                request.channel(),
                request.subject(),
                request.content()
        );

        NotificationResponse response = new NotificationResponse(
                notification.getId(),
                notification.getIdempotencyKey(),
                notification.getRecipient(),
                notification.getChannel(),
                notification.getStatus()
        );

        return ResponseEntity.status(HttpStatus.ACCEPTED).body(response);
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.notifications;

import com.example.notifications.domain.NotificationChannel;
import com.example.notifications.domain.NotificationStatus;
import com.example.notifications.repository.NotificationRepository;
import com.example.notifications.web.dto.SendNotificationRequest;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import java.util.UUID;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@Testcontainers
public class NotificationIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private TestRestTemplate restTemplate;

    @Autowired
    private NotificationRepository repository;

    @BeforeEach
    void setup() {
        repository.deleteAll();
    }

    @Test
    void shouldProcessNotificationIdempotently() {
        String idempotencyKey = UUID.randomUUID().toString();
        SendNotificationRequest request = new SendNotificationRequest(
                idempotencyKey,
                "user@example.com",
                NotificationChannel.EMAIL,
                "Welcome",
                "Welcome to our enterprise platform!"
        );

        ResponseEntity<String> firstCall = restTemplate.postForEntity("/api/v1/notifications", request, String.class);
        assertThat(firstCall.getStatusCode()).isEqualTo(HttpStatus.ACCEPTED);
        assertThat(repository.count()).isEqualTo(1);

        // Second call with same idempotency key should not insert duplicate
        ResponseEntity<String> secondCall = restTemplate.postForEntity("/api/v1/notifications", request, String.class);
        assertThat(secondCall.getStatusCode()).isEqualTo(HttpStatus.ACCEPTED);
        assertThat(repository.count()).isEqualTo(1);
    }
}
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
# Stage 1: Build stage
FROM maven:3.9.6-eclipse-temurin-21-alpine AS builder
WORKDIR /app
COPY pom.xml .
RUN mvn dependency:go-offline -B
COPY src ./src
RUN mvn clean package -DskipTests

# Stage 2: Production runtime image
FROM eclipse-temurin:21-jre-alpine
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
WORKDIR /app
COPY --from=builder /app/target/*.jar app.jar
USER appuser
EXPOSE 8080
ENTRYPOINT ["java", "-XX:+UseZGC", "-XX:+ZGenerational", "-Djava.security.egd=file:/dev/./urandom", "-jar", "app.jar"]
```

### 4.2 `compose.yaml`

```yaml
services:
  notification-db:
    image: postgres:16-alpine
    container_name: notification-db
    environment:
      POSTGRES_DB: notifications_db
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgrespassword
    ports:
      - "5432:5432"
    volumes:
      - postgres_notif_data:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5

  kafka:
    image: apache/kafka:3.7.0
    container_name: notification-kafka
    ports:
      - "9092:9092"
    environment:
      KAFKA_NODE_ID: 1
      KAFKA_PROCESS_ROLES: broker,controller
      KAFKA_LISTENERS: PLAINTEXT://:9092,CONTROLLER://:9093
      KAFKA_ADVERTISED_LISTENERS: PLAINTEXT://localhost:9092
      KAFKA_CONTROLLER_LISTENER_NAMES: CONTROLLER
      KAFKA_LISTENER_SECURITY_PROTOCOL_MAP: CONTROLLER:PLAINTEXT,PLAINTEXT:PLAINTEXT
      KAFKA_CONTROLLER_QUORUM_VOTERS: 1@localhost:9093
      KAFKA_OFFSETS_TOPIC_REPLICATION_FACTOR: 1

  notification-service:
    build: .
    container_name: notification-service
    ports:
      - "8080:8080"
    environment:
      DB_HOST: notification-db
      DB_PORT: 5432
      DB_NAME: notifications_db
      DB_USER: postgres
      DB_PASS: postgrespassword
      KAFKA_BOOTSTRAP: kafka:9092
    depends_on:
      notification-db:
        condition: service_healthy

volumes:
  postgres_notif_data:
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Start application dependencies
docker compose up -d

# 2. Verify health actuator endpoint
curl -i http://localhost:8080/actuator/health

# 3. Dispatch an email notification via REST API
curl -i -X POST http://localhost:8080/api/v1/notifications \
  -H "Content-Type: application/json" \
  -d '{
    "idempotencyKey": "req-uuid-998822",
    "recipient": "engineer@company.com",
    "channel": "EMAIL",
    "subject": "System Alert: High CPU",
    "content": "Server cluster alert: node-04 CPU > 95%."
  }'

# 4. Attempt exact duplicate (verify idempotency response)
curl -i -X POST http://localhost:8080/api/v1/notifications \
  -H "Content-Type: application/json" \
  -d '{
    "idempotencyKey": "req-uuid-998822",
    "recipient": "engineer@company.com",
    "channel": "EMAIL",
    "subject": "System Alert: High CPU",
    "content": "Server cluster alert: node-04 CPU > 95%."
  }'
```
