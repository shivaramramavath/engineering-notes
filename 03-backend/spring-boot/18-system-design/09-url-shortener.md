# 09: Designing a Globally Scalable URL Shortener (1B Redirects/Day)

| Metadata Field | Value |
|---|---|
| **Topic** | High-Throughput Redirection, Base62 Encoding & Key Generation Services |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Scale** | 100:1 Read-to-Write Ratio, 1 Billion Redirects / Day ($\sim 11,500\text{ QPS}$ avg, $30,000\text{ QPS}$ peak) |
| **Key Middleware** | Redis Cluster (301/307 Cache), Key Generation Service (KGS), PostgreSQL, Kafka (Analytics) |

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

A URL shortener (e.g., TinyURL, Bitly) converts long, unwieldy web addresses (e.g. `https://example.com/products/electronics/laptops/2026/m3-pro?ref=summer_promo&source=email`) into compact, human-readable aliases (e.g. `https://sho.rt/xK9z2Q`). 

While conceptually simple, building a URL shortener that serves **1 billion redirects daily** with sub-10 millisecond latency requires solving critical distributed engineering problems:
- **Hash Collisions**: Traditional MD5/SHA-256 hashing produces long strings that, when truncated to 7 characters, suffer from collision vulnerabilities.
- **Read-Heavy Caching**: With a 100:1 read-to-write ratio, 99% of redirects must be resolved in Redis RAM or Edge CDNs without querying primary databases.
- **301 vs. 302/307 Redirection**: Balancing client-side browser caching (301 Moved Permanently) with backend click tracking and analytics requirements (307 Temporary Redirect).

---

## 2. Core Concepts

### 2.1 Base62 Encoding & Capacity Math
Using alphanumeric characters (`[0-9]`, `[a-z]`, `[A-Z]`), we have a base of 62 distinct characters.
- A 7-character string provides:
  $$62^7 = 3,521,614,606,208\text{ unique URLs} \approx 3.5\text{ Trillion URLs}$$
- At 10 million new URLs created daily, a 7-character Base62 namespace will last for:
  $$\frac{3.5\times 10^{12}}{10\times 10^6 \times 365} \approx 964\text{ Years}$$

### 2.2 Pre-Generated Key Generation Service (KGS)
Rather than hashing URLs on the fly (which causes race conditions and DB collision retry loops), a standalone **Key Generation Service (KGS)** pre-computes unique 7-character Base62 keys in the background, loads them into memory queues in batches (e.g., 5,000 keys), and dishes them out in microseconds upon request.

---

## 3. Internals & Architecture

```
        [ Client Read Request: GET /xK9z2Q ]
                          │
                          ▼
       ┌─────────────────────────────────────┐
       │     Cloudflare / CloudFront CDN     │
       │    (Edge Cache: 301 / 307 Redirect) │
       └──────────────────┬──────────────────┘
                          │ (Cache Miss)
                          ▼
       ┌─────────────────────────────────────┐
       │   Spring Boot URL Shortener Service │
       │   (Java 21 Virtual Threads)         │
       └──────────────────┬──────────────────┘
                          │
                          ▼
       ┌─────────────────────────────────────┐
       │       Redis Cluster (L2 Cache)      │
       │       Key: "short:xK9z2Q"           │
       └──────────────┬──────────────────────┘
                      │
        ┌─────────────┴─────────────┐
        ▼ (Hit: Return 307)         ▼ (Miss: Read DB)
   [ 307 Redirect ]         ┌────────────────────────┐
   [ Emit to Kafka Analytics│ PostgreSQL Primary DB │
   [ Stream: 'click-events' │ Table: url_mappings    │
                            └────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Base62 Algorithm Implementation
$$\text{Base62 Characters} = \text{"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"}$$
```java
public class Base62 {
    private static final String ALPHABET = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    public static String fromBase10(long n) {
        StringBuilder sb = new StringBuilder();
        while (n > 0) {
            sb.append(ALPHABET.charAt((int) (n % 62)));
            n /= 62;
        }
        return sb.reverse().toString();
    }
}
```

---

## 5. 5 Practical Examples

### Example 1: High-Performance Base62 Converter

```java
package com.example.shortener.util;

public final class Base62Converter {

    private static final String CHARS = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    private static final int BASE = CHARS.length();

    private Base62Converter() {}

    public static String encode(long value) {
        if (value <= 0) return "0";
        StringBuilder sb = new StringBuilder();
        while (value > 0) {
            sb.append(CHARS.charAt((int) (value % BASE)));
            value /= BASE;
        }
        return sb.reverse().toString();
    }

    public static long decode(String str) {
        long result = 0;
        for (int i = 0; i < str.length(); i++) {
            result = result * BASE + CHARS.indexOf(str.charAt(i));
        }
        return result;
    }
}
```

### Example 2: Distributed Key Generation Service (KGS) Range Allocator

```java
package com.example.shortener.kgs;

import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;

import java.util.concurrent.atomic.AtomicLong;

@Service
public class DistributedKeyGenerator {

    private final StringRedisTemplate redis;
    private final AtomicLong currentId = new AtomicLong(0);
    private final AtomicLong maxAllocatedId = new AtomicLong(0);
    private static final long BLOCK_SIZE = 100_000; // Allocate 100k IDs at once

    public DistributedKeyGenerator(StringRedisTemplate redis) {
        this.redis = redis;
    }

    public synchronized long getNextUniqueId() {
        if (currentId.get() >= maxAllocatedId.get()) {
            // Allocate new block from distributed Redis atomic counter
            Long endOfBlock = redis.opsForValue().increment("kgs:global:counter", BLOCK_SIZE);
            if (endOfBlock == null) endOfBlock = BLOCK_SIZE;
            maxAllocatedId.set(endOfBlock);
            currentId.set(endOfBlock - BLOCK_SIZE);
        }
        return currentId.incrementAndGet();
    }
}
```

### Example 3: URL Shortening & Redirection Service

```java
package com.example.shortener.service;

import com.example.shortener.domain.UrlMapping;
import com.example.shortener.kgs.DistributedKeyGenerator;
import com.example.shortener.repository.UrlMappingRepository;
import com.example.shortener.util.Base62Converter;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.cache.annotation.Cacheable;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Duration;

@Service
public class UrlShortenerService {

    private static final Logger log = LoggerFactory.getLogger(UrlShortenerService.class);
    private final UrlMappingRepository repository;
    private final DistributedKeyGenerator keyGenerator;
    private final StringRedisTemplate redisTemplate;
    private final KafkaTemplate<String, Object> kafkaTemplate;

    public UrlShortenerService(UrlMappingRepository repository,
                               DistributedKeyGenerator keyGenerator,
                               StringRedisTemplate redisTemplate,
                               KafkaTemplate<String, Object> kafkaTemplate) {
        this.repository = repository;
        this.keyGenerator = keyGenerator;
        this.redisTemplate = redisTemplate;
        this.kafkaTemplate = kafkaTemplate;
    }

    @Transactional
    public String shortenUrl(String originalUrl) {
        long uniqueId = keyGenerator.getNextUniqueId();
        String shortCode = Base62Converter.encode(uniqueId);

        UrlMapping mapping = new UrlMapping(shortCode, originalUrl);
        repository.save(mapping);

        // Warm Redis cache
        redisTemplate.opsForValue().set("short:" + shortCode, originalUrl, Duration.ofDays(30));
        return shortCode;
    }

    public String resolveUrl(String shortCode, String userAgent, String clientIp) {
        // Fast path: Check Redis cache
        String targetUrl = redisTemplate.opsForValue().get("short:" + shortCode);

        if (targetUrl == null) {
            log.info("Redis cache miss for {}. Querying database...", shortCode);
            targetUrl = repository.findByShortCode(shortCode)
                    .map(UrlMapping::getOriginalUrl)
                    .orElse(null);

            if (targetUrl != null) {
                redisTemplate.opsForValue().set("short:" + shortCode, targetUrl, Duration.ofDays(30));
            }
        }

        if (targetUrl != null) {
            // Asynchronously log analytics event
            kafkaTemplate.send("url-click-events", shortCode, targetUrl);
        }
        return targetUrl;
    }
}
```

### Example 4: Sub-10ms Redirection REST Controller

```java
package com.example.shortener.web;

import com.example.shortener.service.UrlShortenerService;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.net.URI;
import java.util.Map;

@RestController
public class UrlRedirectController {

    private final UrlShortenerService service;

    public UrlRedirectController(UrlShortenerService service) {
        this.service = service;
    }

    @PostMapping("/api/v1/shorten")
    public ResponseEntity<Map<String, String>> createShortUrl(@RequestBody Map<String, String> body) {
        String originalUrl = body.get("url");
        String shortCode = service.shortenUrl(originalUrl);
        return ResponseEntity.ok(Map.of("shortCode", shortCode, "shortUrl", "https://sho.rt/" + shortCode));
    }

    @GetMapping("/{shortCode:[0-9a-zA-Z]{1,7}}")
    public ResponseEntity<Void> redirect(@PathVariable String shortCode, HttpServletRequest request) {
        String longUrl = service.resolveUrl(shortCode, request.getHeader(HttpHeaders.USER_AGENT), request.getRemoteAddr());

        if (longUrl == null) {
            return ResponseEntity.notFound().build();
        }

        // Return HTTP 307 Temporary Redirect so browsers don't cache permanently,
        // ensuring future clicks hit the server for analytics tracking
        HttpHeaders headers = new HttpHeaders();
        headers.setLocation(URI.create(longUrl));
        headers.set(HttpHeaders.CACHE_CONTROL, "private, max-age=90"); // Allow brief 90s edge cache
        return new ResponseEntity<>(headers, HttpStatus.TEMPORARY_REDIRECT);
    }
}
```

### Example 5: PostgreSQL Database Schema (Flyway V1)

```sql
CREATE TABLE url_mappings (
    id BIGINT PRIMARY KEY,
    short_code VARCHAR(10) UNIQUE NOT NULL,
    original_url TEXT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP WITH TIME ZONE
);

CREATE INDEX idx_url_short_code ON url_mappings(short_code);
```

---

## 6. Best Practices

- **Use HTTP 307 Rather Than 301**: HTTP 301 (Moved Permanently) causes browsers to permanently cache the target URL, meaning subsequent clicks bypass your server, destroying analytics tracking. Use HTTP 307 (Temporary Redirect).
- **Pre-Allocate ID Ranges via KGS**: Let each Spring Boot pod reserve a block of 100,000 IDs from a central Redis counter. Pods generate short codes in local memory with zero network overhead.
- **Cache 100% of Hot Keys in Redis**: Apply the 80/20 rule: caching the top 20% of short links serves 80% of all redirect requests directly from RAM.
- **Asynchronously Ingest Click Analytics**: Never write click logs directly to PostgreSQL in the redirect request path. Emit an event to Kafka and process analytics in bulk.
- **Enforce Regular Expression Path Routing**: Use `@GetMapping("/{shortCode:[0-9a-zA-Z]{1,7}}")` to instantly reject malformed short codes before hitting Redis or the DB.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Using MD5 / SHA-256 Truncation** | Truncating a 128-bit MD5 to 7 characters creates high collision probabilities under birthday paradox. | Use unique sequential ID generation (KGS) encoded in Base62. |
| **HTTP 301 Permanent Redirect** | Browsers cache the destination permanently; you lose 90%+ of analytics click tracking. | Use HTTP 307 Temporary Redirect with a short `max-age=60` cache header. |
| **Writing Analytics to DB Synchronously** | Writing an audit row on every redirect reduces throughput by 90% and overwhelms the DB. | Stream click events asynchronously to Apache Kafka. |
| **Central Database Auto-Increment Counter** | A single DB `BIGSERIAL` becomes a bottleneck and single point of failure (SPOF) at high write rates. | Use distributed range allocation (e.g. KGS claiming 100k blocks via Redis `INCRBY`). |
| **No URL Expiration Policy** | Storing trillions of unused links forever leads to massive storage bills. | Enforce an optional 1-to-5 year TTL and move dead links to cold S3 storage. |
| **Missing Malicious URL Screening** | Attackers use your shortener to disguise phishing and malware links, getting your domain blacklisted. | Integrate Google Safe Browsing API during link creation. |

---

## 8. Performance & Security Considerations

### 8.1 Bloom Filters for Cache Penetration Defense
Malicious actors querying millions of non-existent short links will bypass Redis and hit PostgreSQL. A **Redis Bloom Filter** quickly identifies non-existent keys ($< 1\text{ms}$) and blocks DB queries:
```java
public boolean mightExist(String shortCode) {
    return redisTemplate.execute((RedisCallback<Boolean>) connection -> 
            connection.stringCommands().getBit("bloom:urls".getBytes(), hash(shortCode)));
}
```

### 8.2 Safe Browsing Phishing Verification
Before persisting a URL, verify against the Google Safe Browsing API to block phishing targets.

---

## 9. Comparison / Decision Matrix

| Strategy | Collision Risk | Latency | Complexity |
|---|---|---|---|
| **MD5 Truncation + DB Retry** | High | Unpredictable (Retries on collision) | High |
| **Snowflake ID + Base62** | Zero | Lowest ($< 1\mu\text{s}$ local CPU) | Low |
| **Redis KGS Block Allocator** | Zero | Lowest (1 Redis call per 100k IDs) | Medium |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. How do you generate 7-character short URLs without hash collisions?</strong></summary>

Avoid hashing. Instead, generate a unique 64-bit numerical ID using an atomic counter or Key Generation Service (KGS), and convert that base-10 number into Base62. Since every ID is unique, the Base62 string is mathematically guaranteed to be collision-free.
</details>

<details>
<summary><strong>2. What is the difference between HTTP 301 and HTTP 302/307 redirects?</strong></summary>

HTTP 301 is cached by the client's browser permanently; subsequent clicks never hit the server. HTTP 307 instructs the client to redirect temporarily, ensuring every future visit hits the shortener so analytics (click count, referrer, location) can be tracked.
</details>

<details>
<summary><strong>3. How does the Key Generation Service (KGS) scale across multiple servers?</strong></summary>

The KGS uses an atomic distributed counter (e.g. Redis `INCRBY`). When a server starts, it allocates a batch of 100,000 IDs (e.g., Server 1 takes IDs 1 to 100,000; Server 2 takes 100,001 to 200,000). Each server generates unique keys locally without coordination until its block is exhausted.
</details>

<details>
<summary><strong>4. How much memory is needed to cache the top 20% of URLs for 1 billion redirects daily?</strong></summary>

Assuming 1 billion redirects/day and 80/20 rule: 200 million distinct URLs are read. If each mapping consumes 500 bytes (short code + long URL + metadata):
$$200\text{M} \times 500\text{ bytes} = 100\text{ GB RAM}$$
A standard 3-node Redis cluster easily holds 100 GB in memory.
</details>

<details>
<summary><strong>5. How do you protect the system from cache penetration (queries for random invalid short codes)?</strong></summary>

Implement a Bloom Filter in front of the cache. If the Bloom filter reports the key does not exist, return HTTP 404 immediately without querying Redis or the database.
</details>

<details>
<summary><strong>6. How do you clean up expired short URLs without stalling the database?</strong></summary>

Run a lightweight lazy-deletion check on access: if `expires_at < now`, return 404 and delete. In parallel, run a nightly batch job during off-peak hours deleting expired records in chunks of 5,000 using `DELETE ... WHERE id IN (...)`.
</details>

<details>
<summary><strong>7. How do you prevent users from submitting malicious phishing URLs?</strong></summary>

On URL submission, asynchronously query the Google Safe Browsing API or a threat intelligence database. Reject known malware or phishing domains before creating the short link.
</details>

<details>
<summary><strong>8. Why is Base62 used instead of Base64?</strong></summary>

Base64 includes `+` and `/` characters, which have special meanings in URLs (query strings and path separators) and require escaping. Base62 consists strictly of alphanumeric `0-9`, `a-z`, `A-Z`, which are 100% URL-safe without encoding.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Implement Base62 Encoder & Decoder</strong></summary>

**Prompt**: Write a Java class encoding a `long` into Base62 and decoding it back to the original `long`. Verify with unit tests.

**Solution**:
Inspect Example 1: implement `encode(long)` and `decode(String)` using character mapping tables.
</details>

<details>
<summary><strong>Exercise 2: Build KGS Range Allocator</strong></summary>

**Prompt**: Create a Spring service that reserves 50,000 IDs from Redis using `opsForValue().increment()`.

**Solution**:
Inspect Example 2: manage `currentId` and `maxAllocatedId` with `AtomicLong`.
</details>

<details>
<summary><strong>Exercise 3: Configure Spring Boot HTTP 307 Redirect Controller</strong></summary>

**Prompt**: Write a controller method taking a 7-character short code, querying Redis, and returning an HTTP 307 response with `Location` header.

**Solution**:
Inspect Example 4: set `headers.setLocation(URI.create(longUrl))` and return `new ResponseEntity<>(headers, HttpStatus.TEMPORARY_REDIRECT)`.
</details>

<details>
<summary><strong>Exercise 4: Stream Click Analytics to Kafka</strong></summary>

**Prompt**: Publish click event records containing `shortCode`, `timestamp`, and `clientIp` to Kafka topic `url-clicks`.

**Solution**:
```java
kafkaTemplate.send("url-clicks", shortCode, Map.of(
    "code", shortCode,
    "ip", clientIp,
    "ts", System.currentTimeMillis()
));
```
</details>

<details>
<summary><strong>Exercise 5: Implement Redis Caching with Spring `@Cacheable`</strong></summary>

**Prompt**: Cache the `resolveUrl()` method in Redis using Spring Cache annotations with 30-day TTL.

**Solution**:
Annotate method with `@Cacheable(value = "urls", key = "#shortCode")` and configure `RedisCacheConfiguration.defaultCacheConfig().entryTtl(Duration.ofDays(30))`.
</details>

<details>
<summary><strong>Exercise 6: Test Shortener Redirection with MockMvc</strong></summary>

**Prompt**: Write a Spring MockMvc test asserting that `GET /abc1234` returns status 307 and expected `Location` header.

**Solution**:
```java
mockMvc.perform(get("/abc1234"))
        .andExpect(status().isTemporaryRedirect())
        .andExpect(header().string("Location", "https://example.com/target"));
```
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   URL SHORTENER DESIGN CHEATSHEET                      │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Key Generation: Sequential 64-bit ID encoded into Base62 (7 chars)  │
│ 2. Capacity: 62^7 = 3.5 Trillion unique collision-free URLs           │
│ 3. Redirection: HTTP 307 Temporary Redirect (preserves click tracking) │
│ 4. Cache: 100% of top 20% hot URLs cached in Redis (~100 GB RAM)       │
│ 5. Analytics: Non-blocking async event streaming to Kafka              │
│ 6. Protection: Bloom filter blocks invalid keys from touching database │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Designing a URL Shortener (Alex Xu System Design Volume 1)](https://bytebytego.com/)
- [RFC 7231: HTTP/1.1 Semantics - 307 Temporary Redirect](https://datatracker.ietf.org/doc/html/rfc7231#section-6.4.7)
- [Spring Data Redis Reference Documentation](https://docs.spring.io/spring-data/redis/reference/)
- [Google Safe Browsing API v4](https://developers.google.com/safe-browsing)
