# 13: Designing a High-Concurrency Seat Booking & Reservation System

| Metadata Field | Value |
|---|---|
| **Topic** | Concurrency Control, Seat Reservation & Double-Booking Prevention |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Key Middleware** | Redisson (Distributed Locks), PostgreSQL (Optimistic Locking `@Version`), Redis (Holds) |
| **Key Scale** | Sub-Second Contention for Identical Physical Seats (Airlines, Concerts, Hotels) |

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

In physical seat reservation domains (e.g., concert arenas, airline flights, train coaches, cinema theatres), inventory is discrete: Seat 12A on Flight 402 can only be booked by exactly one passenger. When a blockbuster show or holiday flight opens, thousands of concurrent users attempt to reserve the exact same premium seat simultaneously.

A booking architecture must solve two competing problems:
1. **Zero Double-Bookings**: Under no circumstances can two customers receive tickets for the same physical seat.
2. **Temporary Holds with Auto-Release**: Customers require 10 minutes to enter passenger names and complete credit card payments. If they abandon checkout, the seat must instantly return to the public pool without human intervention.

---

## 2. Core Concepts

### 2.1 The Two-Tier Reservation Protocol
1. **Tier 1: Distributed Mutex via Redisson**: Before opening a database transaction, acquire an exclusive Redis lock on `lock:seat:{seatId}` for 1 second. This serializes contention and eliminates deadlocks at the database level.
2. **Tier 2: Optimistic Locking with `@Version` in PostgreSQL**: In the rare event of distributed lock lease expiration, Hibernate's `@Version` verifies that the row has not changed. If concurrent writes collide, an `OptimisticLockException` is thrown and mapped to HTTP `409 Conflict`.

### 2.2 Seat Lifecycle State Machine
```
   ┌─────────────┐   Reserve (10m Hold)   ┌──────────────┐   Payment Confirmed   ┌──────────┐
   │  AVAILABLE  │ ─────────────────────> │   RESERVED   │ ────────────────────> │  BOOKED  │
   └─────────────┘                        └──────┬───────┘                       └──────────┘
          ▲                                      │
          │             TTL Expired              │
          └──────────────────────────────────────┘
```

---

## 3. Internals & Architecture

```
        [ 5,000 Concurrent Buyers Request Seat #12A ]
                             │
                             ▼
  ┌─────────────────────────────────────────────────────────┐
  │                 Spring Boot Booking Service             │
  │                                                         │
  │  ┌───────────────────────────────────────────────────┐  │
  │  │ Tier 1: Redisson Distributed Lock                 │  │
  │  │ RLock lock = redisson.getLock("lock:seat:12A")    │  │
  │  │ -> Exactly ONE thread acquires lock               │  │
  │  └─────────────────────────┬─────────────────────────┘  │
  │                            │ (Winner)                   │
  │                            ▼                            │
  │  ┌───────────────────────────────────────────────────┐  │
  │  │ Tier 2: PostgreSQL ACID Transaction               │  │
  │  │ - Verify status == 'AVAILABLE'                    │  │
  │  │ - Set status = 'RESERVED', hold_expires_at = +10m │  │
  │  │ - Increment @Version from 0 to 1                  │  │
  │  │ - Insert into reservations table                  │  │
  │  └─────────────────────────┬─────────────────────────┘  │
  └────────────────────────────┼────────────────────────────┘
                               │
                ┌──────────────┴──────────────┐
                ▼ (Winner)                    ▼ (All other 4,999 requests)
        [ Return HTTP 201 ]           [ Return HTTP 409 Conflict ]
        [ 10-Minute Hold Active ]     [ "Seat 12A already reserved" ]
```

---

## 4. Syntax & Practical Usage

### 4.1 Redisson Mutex Acquisition
```java
RLock lock = redisson.getLock("lock:seat:" + seatId);
boolean acquired = lock.tryLock(500, 5000, TimeUnit.MILLISECONDS);
if (!acquired) {
    throw new SeatUnavailableException("Seat is currently held by another user");
}
try {
    // Execute database transaction
} finally {
    if (lock.isHeldByCurrentThread()) lock.unlock();
}
```

---

## 5. 5 Practical Examples

### Example 1: Seat Entity with Optimistic Versioning

```java
package com.example.booking.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "event_seats")
public class EventSeat {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(name = "event_id", nullable = false)
    private String eventId;

    @Column(name = "seat_label", nullable = false, length = 16)
    private String seatLabel; // e.g. "ROW-A-12"

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    private SeatStatus status = SeatStatus.AVAILABLE;

    @Column(name = "hold_expires_at")
    private Instant holdExpiresAt;

    @Column(name = "reserved_by_user")
    private String reservedByUser;

    @Version
    @Column(nullable = false)
    private Long version;

    public enum SeatStatus { AVAILABLE, RESERVED, BOOKED }

    public EventSeat() {}

    public EventSeat(String eventId, String seatLabel) {
        this.eventId = eventId;
        this.seatLabel = seatLabel;
        this.status = SeatStatus.AVAILABLE;
    }

    public UUID getId() { return id; }
    public SeatStatus getStatus() { return status; }
    public void setStatus(SeatStatus status) { this.status = status; }
    public Instant getHoldExpiresAt() { return holdExpiresAt; }
    public void setHoldExpiresAt(Instant holdExpiresAt) { this.holdExpiresAt = holdExpiresAt; }
    public void setReservedByUser(String reservedByUser) { this.reservedByUser = reservedByUser; }
}
```

### Example 2: Distributed Locking Seat Reservation Service

```java
package com.example.booking.service;

import com.example.booking.domain.EventSeat;
import com.example.booking.repository.EventSeatRepository;
import org.redisson.api.RLock;
import org.redisson.api.RedissonClient;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.UUID;
import java.util.concurrent.TimeUnit;

@Service
public class SeatReservationService {

    private static final Logger log = LoggerFactory.getLogger(SeatReservationService.class);
    private final EventSeatRepository seatRepository;
    private final RedissonClient redisson;

    public SeatReservationService(EventSeatRepository seatRepository, RedissonClient redisson) {
        this.seatRepository = seatRepository;
        this.redisson = redisson;
    }

    public EventSeat reserveSeat(UUID seatId, String userId) {
        String lockKey = "lock:seat:" + seatId;
        RLock lock = redisson.getLock(lockKey);

        try {
            // Attempt to acquire lock for 500ms, leasing for max 5 seconds
            boolean acquired = lock.tryLock(500, 5000, TimeUnit.MILLISECONDS);
            if (!acquired) {
                throw new IllegalStateException("Seat is currently under checkout by another customer");
            }

            return executeReservationTransaction(seatId, userId);

        } catch (InterruptedException ex) {
            Thread.currentThread().interrupt();
            throw new IllegalStateException("Seat reservation interrupted", ex);
        } finally {
            if (lock.isHeldByCurrentThread()) {
                lock.unlock();
            }
        }
    }

    @Transactional
    protected EventSeat executeReservationTransaction(UUID seatId, String userId) {
        EventSeat seat = seatRepository.findById(seatId)
                .orElseThrow(() -> new IllegalArgumentException("Seat not found: " + seatId));

        Instant now = Instant.now();

        // Check if available or if prior hold has expired
        boolean isExpired = seat.getStatus() == EventSeat.SeatStatus.RESERVED &&
                            seat.getHoldExpiresAt() != null &&
                            seat.getHoldExpiresAt().isBefore(now);

        if (seat.getStatus() != EventSeat.SeatStatus.AVAILABLE && !isExpired) {
            throw new IllegalStateException("Seat is no longer available");
        }

        // Place 10-minute hold
        seat.setStatus(EventSeat.SeatStatus.RESERVED);
        seat.setHoldExpiresAt(now.plus(10, ChronoUnit.MINUTES));
        seat.setReservedByUser(userId);

        log.info("Seat {} reserved for user {} until {}", seat.getId(), userId, seat.getHoldExpiresAt());
        return seatRepository.save(seat);
    }
}
```

### Example 3: Booking Confirmation Service

```java
package com.example.booking.service;

import com.example.booking.domain.EventSeat;
import com.example.booking.repository.EventSeatRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.util.UUID;

@Service
public class BookingConfirmationService {

    private final EventSeatRepository seatRepository;

    public BookingConfirmationService(EventSeatRepository seatRepository) {
        this.seatRepository = seatRepository;
    }

    @Transactional
    public void confirmBooking(UUID seatId, String userId) {
        EventSeat seat = seatRepository.findById(seatId)
                .orElseThrow(() -> new IllegalArgumentException("Seat not found"));

        if (seat.getStatus() != EventSeat.SeatStatus.RESERVED) {
            throw new IllegalStateException("Seat is not currently reserved");
        }
        if (seat.getHoldExpiresAt().isBefore(Instant.now())) {
            throw new IllegalStateException("Hold has expired. Please reselect your seat.");
        }

        // Final transition to booked
        seat.setStatus(EventSeat.SeatStatus.BOOKED);
        seat.setHoldExpiresAt(null);
        seatRepository.save(seat);
    }
}
```

### Example 4: Scheduled Hold Reconciler Job

```java
package com.example.booking.scheduler;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;

@Component
public class ExpiredHoldReconciler {

    private static final Logger log = LoggerFactory.getLogger(ExpiredHoldReconciler.class);
    private final JdbcClient jdbcClient;

    public ExpiredHoldReconciler(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    @Scheduled(fixedRate = 30000) // Runs every 30 seconds
    @Transactional
    public void releaseExpiredHolds() {
        int updated = jdbcClient.sql("""
                UPDATE event_seats 
                SET status = 'AVAILABLE', hold_expires_at = NULL, reserved_by_user = NULL
                WHERE status = 'RESERVED' AND hold_expires_at < ?
                """)
                .param(Instant.now())
                .update();

        if (updated > 0) {
            log.info("Released {} expired seat holds back to AVAILABLE pool.", updated);
        }
    }
}
```

### Example 5: High-Concurrency Integration Test (Proving Zero Double-Bookings)

```java
package com.example.booking;

import com.example.booking.domain.EventSeat;
import com.example.booking.repository.EventSeatRepository;
import com.example.booking.service.SeatReservationService;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;

import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
public class ConcurrencyReservationTest {

    @Autowired
    private EventSeatRepository repository;

    @Autowired
    private SeatReservationService service;

    @Test
    void shouldGuaranteeExactlyOneWinnerUnderHighConcurrency() throws Exception {
        EventSeat seat = repository.save(new EventSeat("CONCERT-2026", "A-1"));

        int threads = 50;
        ExecutorService executor = Executors.newFixedThreadPool(threads);
        CountDownLatch startGate = new CountDownLatch(1);
        CountDownLatch endGate = new CountDownLatch(threads);

        AtomicInteger successes = new AtomicInteger(0);
        AtomicInteger failures = new AtomicInteger(0);

        for (int i = 0; i < threads; i++) {
            final String user = "user-" + i;
            executor.submit(() -> {
                try {
                    startGate.await(); // All threads burst together
                    service.reserveSeat(seat.getId(), user);
                    successes.incrementAndGet();
                } catch (Exception ex) {
                    failures.incrementAndGet();
                } finally {
                    endGate.countDown();
                }
            });
        }

        startGate.countDown();
        endGate.await(10, TimeUnit.SECONDS);

        assertThat(successes.get()).isEqualTo(1);
        assertThat(failures.get()).isEqualTo(threads - 1);
    }
}
```

---

## 6. Best Practices

- **Pair Distributed Locks with Database Versioning**: Do not rely solely on Redis locks; network partitions can cause lock leases to expire prematurely. JPA `@Version` guarantees safety as a second line of defense.
- **Short Distributed Lock Leases**: Keep Redisson lock leases short (e.g. 5 seconds) to prevent frozen resources if a worker pod crashes mid-execution.
- **Set Up a Visual Seat Map Cache**: Cache seat availability maps in Redis (Bitmaps or Hashes) so the 100,000 users browsing the seat map do not touch PostgreSQL.
- **Automatic Hold Release**: Always store `hold_expires_at` on the entity; clean up expired reservations with a scheduled worker.
- **Cart Timer Countdown on Frontend**: Display a live 10-minute countdown in the browser so users complete checkout before the hold expires.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Pessimistic Locking Across the Entire Arena** | Locking the whole `event_seats` table blocks all 50,000 seat reservations simultaneously. | Lock only the specific individual seat key (`lock:seat:{id}`). |
| **No Lease Timeout on Distributed Locks** | If a pod dies while holding the lock, the seat is permanently deadlocked. | Always set a lease time (e.g., 5 seconds) with automatic lock release. |
| **Relying Exclusively on Redis for Final State** | If Redis restarts or drops data, double-bookings occur in reality. | Redis manages the temporary lock/hold; PostgreSQL enforces final `@Version` ACID state. |
| **Long Database Transactions** | Keeping a DB transaction open for 10 minutes while the user types credit card details exhausts HikariCP instantly. | Commit the `RESERVED` status immediately (50ms); close the DB transaction; confirm later. |
| **No Index on `(event_id, status)`** | Searching for available seats requires full table scans of millions of seats. | Add composite index on `(event_id, status)`. |
| **Indefinite Holds** | Users reserve 6 seats and close their laptop, locking inventory away forever. | Enforce strict 10-minute hold expiration with automated reconciliation. |

---

## 8. Performance & Security Considerations

### 8.1 Redis Bitmaps for Fast Seat Map Rendering
Render seat availability maps for a 50,000-seat stadium in $< 1\text{ms}$ using a single 6 KB Redis Bitmap (`GETBIT stadium:event:123 seat_offset`).

### 8.2 Database Unique Constraints
Enforce unique constraints to prevent duplicate bookings:
```sql
CREATE UNIQUE INDEX uq_event_seat ON event_seats(event_id, seat_label);
```

---

## 9. Comparison / Decision Matrix

| Mechanism | Deadlock Risk | DB Connection Overhead | Double-Booking Risk |
|---|---|---|---|
| **DB Pessimistic (`FOR UPDATE`)** | High (Deadlocks under contention) | High (Threads hold DB connections) | Zero |
| **JPA `@Version` Optimistic** | None | Low | Zero (Throws `OptimisticLockException`) |
| **Redisson Lock + `@Version`** | Zero (Serialized at Redis) | Lowest (Only lock winner touches DB) | Zero (Dual-tier guarantee) |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. How do you guarantee two people never book the same concert seat?</strong></summary>

Employ a dual-tier concurrency control strategy: Tier 1 serializes requests using an exclusive Redisson distributed lock (`lock:seat:{id}`). Tier 2 enforces an atomic state transition in PostgreSQL with optimistic locking (`@Version`). Only the winner transitions the seat to `RESERVED`.
</details>

<details>
<summary><strong>2. Why can't we keep a database transaction open while the user enters credit card details?</strong></summary>

A user takes 5 to 10 minutes to pay. Keeping a database transaction open for 10 minutes holds a pooled connection open, exhausting HikariCP within seconds and causing total system failure. The transaction must commit the 10-minute hold within 50ms and release the connection.
</details>

<details>
<summary><strong>3. What happens if a user abandons checkout after reserving a seat?</strong></summary>

The reservation record stores a `hold_expires_at` timestamp (now + 10 mins). A background scheduler periodically executes an atomic SQL query resetting expired holds back to `AVAILABLE`.
</details>

<details>
<summary><strong>4. Why is JPA `@Version` optimistic locking needed if we already use Redisson locks?</strong></summary>

Defense in depth. If a network partition causes Redisson's lock lease to expire before the worker finishes (or Redis fails over), optimistic locking in PostgreSQL catches the concurrent update and throws an `OptimisticLockException`, preventing double booking.
</details>

<details>
<summary><strong>5. How do you render an interactive stadium map with 50,000 seats to 100,000 concurrent viewers?</strong></summary>

Do not query the database. Represent the stadium seat availability as a Redis Bitmap where each bit represents a seat offset (0 = available, 1 = occupied). A 50,000-seat bitmap occupies just 6.25 KB of memory and can be served from memory or edge CDNs.
</details>

<details>
<summary><strong>6. How do you prevent bots from monopolizing seats?</strong></summary>

Enforce CAPTCHA challenges prior to reservation, limit maximum simultaneous holds to 4 seats per authenticated User ID, and mandate phone/SMS identity verification.
</details>

<details>
<summary><strong>7. What HTTP status code should be returned when a seat reservation fails?</strong></summary>

Return HTTP `409 Conflict` with a JSON payload indicating that the seat has already been selected by another customer.
</details>

<details>
<summary><strong>8. How do you handle adjacent seat selection (e.g., booking 3 seats in a row)?</strong></summary>

Sort the requested seat IDs lexicographically before acquiring distributed locks to avoid deadlocks. Acquire locks for all 3 seats atomically. If any seat is unavailable, release all locks and roll back.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Redisson Spring Boot Starter Bean</strong></summary>

**Prompt**: Configure a `RedissonClient` bean pointing to Redis on localhost:6379.

**Solution**:
```java
@Bean
public RedissonClient redisson() {
    Config config = new Config();
    config.useSingleServer().setAddress("redis://localhost:6379");
    return Redisson.create(config);
}
```
</details>

<details>
<summary><strong>Exercise 2: Write Atomic Expired Hold Release Query</strong></summary>

**Prompt**: Write a Spring Data JPA `@Modifying` query resetting expired reservations to `AVAILABLE`.

**Solution**:
Inspect Example 4: `UPDATE event_seats SET status = 'AVAILABLE', hold_expires_at = NULL WHERE status = 'RESERVED' AND hold_expires_at < :now`.
</details>

<details>
<summary><strong>Exercise 3: Implement Multi-Seat Lexicographical Lock Sorter</strong></summary>

**Prompt**: Write a method that locks multiple seats in sorted order to avoid deadlock.

**Solution**:
```java
public void lockMultipleSeats(List<UUID> seatIds) {
    List<UUID> sorted = seatIds.stream().sorted().toList();
    for (UUID id : sorted) {
        RLock lock = redisson.getLock("lock:seat:" + id);
        lock.lock(5, TimeUnit.SECONDS);
    }
}
```
</details>

<details>
<summary><strong>Exercise 4: Test Optimistic Locking Exception in Spring Data</strong></summary>

**Prompt**: Simulate two transactions updating the same `EventSeat` with stale version and verify `ObjectOptimisticLockingFailureException` is thrown.

**Solution**:
Fetch seat entity in Thread A and Thread B. Thread A updates and saves. Thread B attempts to save stale entity -> assert exception is thrown.
</details>

<details>
<summary><strong>Exercise 5: Build Seat Map Bitmap Generator</strong></summary>

**Prompt**: Write a method setting bit offsets in Redis for occupied seats.

**Solution**:
```java
public void markSeatOccupied(String eventId, int seatNumber) {
    redis.opsForValue().setBit("bitmap:event:" + eventId, seatNumber, true);
}
```
</details>

<details>
<summary><strong>Exercise 6: Implement Frontend Countdown DTO</strong></summary>

**Prompt**: Return a DTO containing `seatId`, `holdSecondsRemaining`, and `expiresAt` timestamp.

**Solution**:
```java
public record ReservationDto(UUID seatId, long holdSecondsRemaining, Instant expiresAt) {}
```
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   SEAT BOOKING ARCHITECTURE CHEATSHEET                 │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Dual-Tier Locking: Redisson Mutex (Tier 1) + JPA @Version (Tier 2)  │
│ 2. Fast Hold: Commit 'RESERVED' in 50ms; never hold DB conns for 10m   │
│ 3. Auto-Release: Reconciler job resets expired holds every 30 seconds  │
│ 4. Map Display: Redis Bitmaps (6 KB for 50k seats) for sub-ms render   │
│ 5. Deadlock Prevention: Sort seat IDs lexicographically before locking │
│ 6. Response: HTTP 409 Conflict when a seat is taken                    │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Redisson Distributed Locks Documentation](https://redisson.org/docs/data-and-services/locks-and-semaphores/)
- [Hibernate ORM 6.6 Optimistic Locking Guide](https://docs.jboss.org/hibernate/orm/current/userguide/html_single/Hibernate_User_Guide.html#locking)
- [System Design: Ticketmaster / Seat Booking (Alex Xu)](https://bytebytego.com/)
- [Redis Bitmaps Official Tutorial](https://redis.io/docs/data-types/bitmaps/)
