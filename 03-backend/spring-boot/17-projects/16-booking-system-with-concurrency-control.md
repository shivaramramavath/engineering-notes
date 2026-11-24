# Project 16: High-Concurrency Ticket Reservation System with Distributed Locking

| Metadata Field | Value |
|---|---|
| **Spring Boot Version** | 3.4.2 |
| **Spring Framework** | 6.2.2 |
| **Java Version** | 21 (LTS) |
| **Build System** | Maven 3.9+ |
| **Key Dependencies** | Spring Boot Starter Web, Spring Boot Starter Data JPA, Spring Boot Starter Data Redis, Redisson Spring Boot Starter, PostgreSQL Driver, Testcontainers |
| **Architecture Style** | High-Concurrency Distributed Reservation System with Dual-Tier Locking (Redis Mutex + JPA `@Version` Optimistic Locking) |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture

This project implements a concert and event ticket booking engine subject to extreme concurrent burst traffic (flash sales). To prevent overselling and double booking of physical seats without collapsing database connection pools, it employs a **Dual-Tier Concurrency Strategy**:
1. **Tier 1 (Distributed Mutex)**: Redisson acquires an exclusive distributed lock on the resource key `lock:seat:{seatId}` with a 5-second lease time.
2. **Tier 2 (Optimistic Locking & State Validation)**: Inside PostgreSQL, the `Seat` entity contains a `@Version` field and an atomic state transition (`AVAILABLE` -> `RESERVED`). If two concurrent requests slip through, Hibernate throws `OptimisticLockException`, which is caught and mapped to HTTP `409 Conflict`.

```
        [ 10,000 Concurrent Seat Requests for Seat #42 ]
                               │
                               ▼
   ┌─────────────────────────────────────────────────────────┐
   │             Spring Boot Booking Service                 │
   │                                                         │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │ Tier 1: Redisson Distributed Lock                 │  │
   │  │ RLock lock = redisson.getLock("lock:seat:42")     │  │
   │  │ lock.tryLock(500ms, 5s)                           │  │
   │  └─────────────────────────┬─────────────────────────┘  │
   │                            │ (1 Thread wins lock)       │
   │                            ▼                            │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │ Tier 2: Spring Data JPA Transaction               │  │
   │  │ - SELECT seat WHERE id = 42                       │  │
   │  │ - Verify status == AVAILABLE                      │  │
   │  │ - seat.setStatus(RESERVED)                        │  │
   │  │ - UPDATE seat SET version = version + 1...        │  │
   │  └─────────────────────────┬─────────────────────────┘  │
   └────────────────────────────┼────────────────────────────┘
                                │
                                ▼
   ┌─────────────────────────────────────────────────────────┐
   │                 PostgreSQL Persistence                  │
   │  seats (id, event_id, seat_number, status, version)     │
   │  bookings (id, seat_id, user_id, reserved_at)           │
   └─────────────────────────────────────────────────────────┘
```

### 1.2 Directory Layout

```
16-booking-system-with-concurrency-control/
├── pom.xml
├── src/
│   ├── main/
│   │   ├── java/com/example/booking/
│   │   │   ├── BookingSystemApplication.java
│   │   │   ├── domain/
│   │   │   │   ├── Seat.java
│   │   │   │   ├── SeatStatus.java
│   │   │   │   └── Booking.java
│   │   │   ├── repository/
│   │   │   │   ├── SeatRepository.java
│   │   │   │   └── BookingRepository.java
│   │   │   ├── service/
│   │   │   │   └── ReservationService.java
│   │   │   └── web/
│   │   │       ├── BookingController.java
│   │   │       ├── dto/
│   │   │       │   ├── ReserveSeatRequest.java
│   │   │       │   └── BookingConfirmationResponse.java
│   │   │       └── exception/GlobalExceptionHandler.java
│   │   └── resources/
│   │       ├── application.yaml
│   │       └── db/migration/V1__init_booking_schema.sql
│   └── test/
│       └── java/com/example/booking/
│           └── ConcurrentSeatBookingIntegrationTest.java
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
    <artifactId>booking-concurrency-service</artifactId>
    <version>1.0.0</version>
    <name>booking-concurrency-service</name>
    <description>High-Concurrency Ticket Reservation Service with Distributed Locks</description>

    <properties>
        <java.version>21</java.version>
        <redisson.version>3.35.0</redisson.version>
        <testcontainers.version>1.20.4</testcontainers.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
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
            <groupId>org.redisson</groupId>
            <artifactId>redisson-spring-boot-starter</artifactId>
            <version>${redisson.version}</version>
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
            <groupId>com.redis</groupId>
            <artifactId>testcontainers-redis</artifactId>
            <version>2.2.2</version>
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
    name: booking-concurrency-service
  datasource:
    url: jdbc:postgresql://${DB_HOST:localhost}:${DB_PORT:5432}/${DB_NAME:booking_db}
    username: ${DB_USER:postgres}
    password: ${DB_PASS:postgres}
  jpa:
    hibernate:
      ddl-auto: validate
    open-in-view: false
  flyway:
    enabled: true
  data:
    redis:
      host: ${REDIS_HOST:localhost}
      port: ${REDIS_PORT:6379}
```

### 2.3 Flyway Migration: `V1__init_booking_schema.sql`

```sql
CREATE TABLE seats (
    id UUID PRIMARY KEY,
    event_id VARCHAR(64) NOT NULL,
    seat_number VARCHAR(32) NOT NULL,
    status VARCHAR(32) NOT NULL,
    version BIGINT NOT NULL DEFAULT 0,
    CONSTRAINT uq_event_seat UNIQUE (event_id, seat_number)
);

CREATE TABLE bookings (
    id UUID PRIMARY KEY,
    seat_id UUID NOT NULL REFERENCES seats(id),
    user_id VARCHAR(64) NOT NULL,
    reserved_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_seat_event ON seats(event_id, status);
```

### 2.4 Domain Entities & Repositories

```java
package com.example.booking.domain;

public enum SeatStatus {
    AVAILABLE, RESERVED, BOOKED
}
```

```java
package com.example.booking.domain;

import jakarta.persistence.*;
import java.util.UUID;

@Entity
@Table(name = "seats")
public class Seat {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(name = "event_id", nullable = false, length = 64)
    private String eventId;

    @Column(name = "seat_number", nullable = false, length = 32)
    private String seatNumber;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false, length = 32)
    private SeatStatus status = SeatStatus.AVAILABLE;

    @Version
    @Column(nullable = false)
    private Long version;

    public Seat() {}

    public Seat(String eventId, String seatNumber) {
        this.eventId = eventId;
        this.seatNumber = seatNumber;
        this.status = SeatStatus.AVAILABLE;
    }

    public UUID getId() { return id; }
    public String getEventId() { return eventId; }
    public String getSeatNumber() { return seatNumber; }
    public SeatStatus getStatus() { return status; }
    public void setStatus(SeatStatus status) { this.status = status; }
    public Long getVersion() { return version; }
}
```

```java
package com.example.booking.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "bookings")
public class Booking {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "seat_id", nullable = false)
    private Seat seat;

    @Column(name = "user_id", nullable = false, length = 64)
    private String userId;

    @Column(name = "reserved_at", nullable = false, updatable = false)
    private Instant reservedAt = Instant.now();

    public Booking() {}

    public Booking(Seat seat, String userId) {
        this.seat = seat;
        this.userId = userId;
    }

    public UUID getId() { return id; }
    public Seat getSeat() { return seat; }
    public String getUserId() { return userId; }
    public Instant getReservedAt() { return reservedAt; }
}
```

```java
package com.example.booking.repository;

import com.example.booking.domain.Seat;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.UUID;

public interface SeatRepository extends JpaRepository<Seat, UUID> {}
```

```java
package com.example.booking.repository;

import com.example.booking.domain.Booking;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.UUID;

public interface BookingRepository extends JpaRepository<Booking, UUID> {}
```

### 2.5 Reservation Service with Redisson Distributed Locks

```java
package com.example.booking.service;

import com.example.booking.domain.Booking;
import com.example.booking.domain.Seat;
import com.example.booking.domain.SeatStatus;
import com.example.booking.repository.BookingRepository;
import com.example.booking.repository.SeatRepository;
import org.redisson.api.RLock;
import org.redisson.api.RedissonClient;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.UUID;
import java.util.concurrent.TimeUnit;

@Service
public class ReservationService {

    private static final Logger log = LoggerFactory.getLogger(ReservationService.class);
    private final SeatRepository seatRepository;
    private final BookingRepository bookingRepository;
    private final RedissonClient redissonClient;

    public ReservationService(SeatRepository seatRepository,
                              BookingRepository bookingRepository,
                              RedissonClient redissonClient) {
        this.seatRepository = seatRepository;
        this.bookingRepository = bookingRepository;
        this.redissonClient = redissonClient;
    }

    public Booking reserveSeatWithDistributedLock(UUID seatId, String userId) {
        String lockKey = "lock:seat:" + seatId;
        RLock lock = redissonClient.getLock(lockKey);

        try {
            // Attempt to acquire lock for 1 second, holding for max 5 seconds
            boolean acquired = lock.tryLock(1000, 5000, TimeUnit.MILLISECONDS);
            if (!acquired) {
                throw new IllegalStateException("Seat is currently locked by another customer. Please retry.");
            }

            return executeReservationTransaction(seatId, userId);

        } catch (InterruptedException ex) {
            Thread.currentThread().interrupt();
            throw new IllegalStateException("Seat reservation thread interrupted", ex);
        } finally {
            if (lock.isHeldByCurrentThread()) {
                lock.unlock();
            }
        }
    }

    @Transactional
    protected Booking executeReservationTransaction(UUID seatId, String userId) {
        Seat seat = seatRepository.findById(seatId)
                .orElseThrow(() -> new IllegalArgumentException("Seat not found: " + seatId));

        if (seat.getStatus() != SeatStatus.AVAILABLE) {
            throw new IllegalStateException("Seat " + seat.getSeatNumber() + " is already " + seat.getStatus());
        }

        seat.setStatus(SeatStatus.RESERVED);
        seatRepository.save(seat);

        Booking booking = new Booking(seat, userId);
        return bookingRepository.save(booking);
    }
}
```

### 2.6 REST Controllers & Error Handling

```java
package com.example.booking.web.dto;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import java.util.UUID;

public record ReserveSeatRequest(
        @NotNull UUID seatId,
        @NotBlank String userId
) {}
```

```java
package com.example.booking.web.dto;

import java.time.Instant;
import java.util.UUID;

public record BookingConfirmationResponse(
        UUID bookingId,
        UUID seatId,
        String userId,
        Instant reservedAt
) {}
```

```java
package com.example.booking.web;

import com.example.booking.domain.Booking;
import com.example.booking.service.ReservationService;
import com.example.booking.web.dto.BookingConfirmationResponse;
import com.example.booking.web.dto.ReserveSeatRequest;
import jakarta.validation.Valid;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/bookings")
public class BookingController {

    private final ReservationService reservationService;

    public BookingController(ReservationService reservationService) {
        this.reservationService = reservationService;
    }

    @PostMapping("/reserve")
    @ResponseStatus(HttpStatus.CREATED)
    public BookingConfirmationResponse reserve(@Valid @RequestBody ReserveSeatRequest request) {
        Booking booking = reservationService.reserveSeatWithDistributedLock(request.seatId(), request.userId());
        return new BookingConfirmationResponse(
                booking.getId(),
                booking.getSeat().getId(),
                booking.getUserId(),
                booking.getReservedAt()
        );
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.booking;

import com.example.booking.domain.Seat;
import com.example.booking.domain.SeatStatus;
import com.example.booking.repository.BookingRepository;
import com.example.booking.repository.SeatRepository;
import com.example.booking.service.ReservationService;
import com.redis.testcontainers.RedisContainer;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@Testcontainers
public class ConcurrentSeatBookingIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Container
    @ServiceConnection
    static RedisContainer redis = new RedisContainer("redis:7.2-alpine");

    @Autowired
    private SeatRepository seatRepository;

    @Autowired
    private BookingRepository bookingRepository;

    @Autowired
    private ReservationService reservationService;

    private Seat targetSeat;

    @BeforeEach
    void setup() {
        bookingRepository.deleteAll();
        seatRepository.deleteAll();
        targetSeat = seatRepository.save(new Seat("EVT-ROCK-2026", "ROW-A-01"));
    }

    @Test
    void shouldPreventDoubleBookingUnderHeavyConcurrency() throws Exception {
        int threadCount = 20;
        ExecutorService executor = Executors.newFixedThreadPool(threadCount);
        CountDownLatch startSignal = new CountDownLatch(1);
        CountDownLatch doneSignal = new CountDownLatch(threadCount);

        AtomicInteger successCount = new AtomicInteger(0);
        AtomicInteger conflictCount = new AtomicInteger(0);

        for (int i = 0; i < threadCount; i++) {
            final String userId = "user-" + i;
            executor.submit(() -> {
                try {
                    startSignal.await(); // Ensure all threads burst simultaneously
                    reservationService.reserveSeatWithDistributedLock(targetSeat.getId(), userId);
                    successCount.incrementAndGet();
                } catch (Exception ex) {
                    conflictCount.incrementAndGet();
                } finally {
                    doneSignal.countDown();
                }
            });
        }

        startSignal.countDown(); // Fire!
        doneSignal.await(10, TimeUnit.SECONDS);
        executor.shutdown();

        // Exactly ONE customer must succeed, and (threadCount - 1) must be rejected
        assertThat(successCount.get()).isEqualTo(1);
        assertThat(conflictCount.get()).isEqualTo(threadCount - 1);
        assertThat(bookingRepository.count()).isEqualTo(1);

        Seat updatedSeat = seatRepository.findById(targetSeat.getId()).orElseThrow();
        assertThat(updatedSeat.getStatus()).isEqualTo(SeatStatus.RESERVED);
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
  booking-db:
    image: postgres:16-alpine
    container_name: booking-db
    environment:
      POSTGRES_DB: booking_db
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: bookingpassword
    ports:
      - "5432:5432"
    volumes:
      - booking_postgres_data:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5

  booking-redis:
    image: redis:7.2-alpine
    container_name: booking-redis
    ports:
      - "6379:6379"

  booking-service:
    build: .
    container_name: booking-service
    ports:
      - "8080:8080"
    environment:
      DB_HOST: booking-db
      DB_PORT: 5432
      DB_NAME: booking_db
      DB_USER: postgres
      DB_PASS: bookingpassword
      REDIS_HOST: booking-redis
      REDIS_PORT: 6379
    depends_on:
      booking-db:
        condition: service_healthy
      booking-redis:
        condition: service_started

volumes:
  booking_postgres_data:
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Start application stack
docker compose up -d

# 2. Check health
curl -i http://localhost:8080/actuator/health

# 3. Reserve a seat
curl -i -X POST http://localhost:8080/api/v1/bookings/reserve \
  -H "Content-Type: application/json" \
  -d '{
    "seatId": "e395df39-81a2-4a0b-93ff-187515b14e9e",
    "userId": "customer-alice"
  }'

# 4. Attempt to reserve the same seat concurrently or again
curl -i -X POST http://localhost:8080/api/v1/bookings/reserve \
  -H "Content-Type: application/json" \
  -d '{
    "seatId": "e395df39-81a2-4a0b-93ff-187515b14e9e",
    "userId": "customer-bob"
  }'
# Expected HTTP 500/409 with: "Seat is already RESERVED"
```
