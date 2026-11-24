# 14: Designing a Scalable Social Feed System (Fan-Out on Write vs. Read)

| Metadata Field | Value |
|---|---|
| **Topic** | Social Newsfeed Generation, Fan-Out Strategies & Timeline Caching |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Scale** | 500M Active Users, Sub-50ms Feed Delivery, Handling Celebrity Accounts |
| **Key Middleware** | Redis Sorted Sets (ZSET Timelines), Apache Kafka (Fan-Out Pipeline), ScyllaDB, PostgreSQL |

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

A social newsfeed (e.g., Twitter/X timeline, Instagram home feed, LinkedIn stream) generates a personalized, reverse-chronological stream of posts from accounts that a user follows.

Delivering this experience to 500 million users involves contrasting architectural challenges:
- **Read Latency**: Users expect instant feed loading ($< 50\text{ms}$). Querying relational databases on every feed load (`SELECT * FROM posts WHERE author_id IN (SELECT following_id FROM followers WHERE user_id = ?)...`) requires joining tables across millions of rows, which does not scale.
- **The Celebrity Problem ("Hotspot Fan-Out")**: When a celebrity with 100 million followers publishes a post, attempting to write that post into 100 million follower feeds simultaneously crashes background message queues and creates severe write amplification.

A resilient system resolves this by employing a **Hybrid Fan-Out Architecture**: pushing posts for regular users (Fan-out on Write) and pulling posts on demand for high-follower celebrities (Fan-out on Read).

---

## 2. Core Concepts

### 2.1 Fan-Out on Write (Push Model)
When an author posts:
- A background worker fetches all followers.
- The new `post_id` is appended to each follower's pre-computed timeline in Redis (using Redis `ZSET`).
- **Advantage**: Feed reads are $O(1)$ operations from Redis.
- **Disadvantage**: Immense write amplification when an account has millions of followers.

### 2.2 Fan-Out on Read (Pull Model)
When a user requests their feed:
- The system queries recent posts published by authors they follow and merges them in memory.
- **Advantage**: Zero write amplification when publishing posts.
- **Disadvantage**: Heavy read-time latency and database load.

### 2.3 The Hybrid Model (Industry Standard)
- **Regular Users ($< 25,000$ followers)**: Use Fan-out on Write (push).
- **Celebrities ($> 25,000$ followers)**: Use Fan-out on Read (pull).
- During feed generation, the user's pre-computed Redis timeline is merged in memory with recent posts fetched from the small set of followed celebrities.

---

## 3. Internals & Architecture

```
                       [ Author Creates Post ]
                                  │
                                  ▼
   ┌─────────────────────────────────────────────────────────────┐
   │                  Spring Boot Post Service                   │
   │   - Save post to ScyllaDB / PostgreSQL                      │
   │   - Publish PostCreatedEvent to Apache Kafka                │
   └──────────────────────────────┬──────────────────────────────┘
                                  │
                                  ▼
   ┌─────────────────────────────────────────────────────────────┐
   │                  Kafka Fan-Out Workers                      │
   │  Is author a celebrity (>25k followers)?                    │
   └──────────────┬───────────────────────────────┬──────────────┘
                  │ No (Regular User)             │ Yes (Celebrity)
                  ▼                               ▼
   ┌──────────────────────────────┐ ┌───────────────────────────┐
   │ Push to Followers' Redis     │ │ DO NOT PUSH!              │
   │ Timelines via ZADD           │ │ Save only to celebrity's  │
   │ Key: "timeline:{followerId}" │ │ author post list          │
   └──────────────────────────────┘ └───────────────────────────┘

                       [ User Loads Feed ]
                                  │
                                  ▼
   ┌─────────────────────────────────────────────────────────────┐
   │                 Spring Boot Feed Aggregator                 │
   │  1. Fetch pre-computed timeline from Redis (ZREVRANGEBYSCORE│
   │  2. Fetch recent posts from followed celebrities (Pull)     │
   │  3. Merge & sort top 20 items in memory                     │
   │  4. Hydrate post content from Cache / ScyllaDB              │
   └─────────────────────────────────────────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Redis Sorted Set (ZSET) Timeline Syntax
```bash
# Add post to user 42's timeline (Score = Epoch Milliseconds)
ZADD timeline:42 1732489200000 "post_987654"

# Fetch newest 20 posts from user 42's timeline
ZREVRANGEBYSCORE timeline:42 +inf -inf LIMIT 0 20

# Trim timeline to retain only the newest 800 posts
ZREMRANGEBYRANK timeline:42 0 -801
```

---

## 5. 5 Practical Examples

### Example 1: Redis Timeline Service with ZSET

```java
package com.example.feed.service;

import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;

import java.util.Collections;
import java.util.List;
import java.util.Set;

@Service
public class TimelineCacheService {

    private final StringRedisTemplate redis;
    private static final int MAX_TIMELINE_SIZE = 800;

    public TimelineCacheService(StringRedisTemplate redis) {
        this.redis = redis;
    }

    public void addPostToTimeline(String userId, String postId, long timestampMillis) {
        String key = "timeline:" + userId;
        // ZADD timeline:{userId} score=timestamp postId
        redis.opsForZSet().add(key, postId, (double) timestampMillis);

        // Cap timeline size at 800 items to prevent unbounded memory growth
        Long size = redis.opsForZSet().size(key);
        if (size != null && size > MAX_TIMELINE_SIZE) {
            redis.opsForZSet().removeRange(key, 0, size - MAX_TIMELINE_SIZE - 1);
        }
    }

    public List<String> getTimelinePostIds(String userId, int page, int size) {
        String key = "timeline:" + userId;
        long start = (long) page * size;
        long end = start + size - 1;

        // Fetch reverse chronological order (highest score first)
        Set<String> postIds = redis.opsForZSet().reverseRange(key, start, end);
        return postIds != null ? List.copyOf(postIds) : Collections.emptyList();
    }
}
```

### Example 2: Fan-Out Kafka Consumer with Celebrity Detection

```java
package com.example.feed.worker;

import com.example.feed.service.TimelineCacheService;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.stereotype.Component;

import java.util.List;

@Component
public class PostFanOutWorker {

    private static final Logger log = LoggerFactory.getLogger(PostFanOutWorker.class);
    private final TimelineCacheService timelineCacheService;

    public PostFanOutWorker(TimelineCacheService timelineCacheService) {
        this.timelineCacheService = timelineCacheService;
    }

    public record PostCreatedEvent(String postId, String authorId, long timestamp, boolean isCelebrity) {}

    @KafkaListener(topics = "social-post-created", groupId = "feed-fanout-workers", concurrency = "10")
    public void onPostCreated(PostCreatedEvent event) {
        if (event.isCelebrity()) {
            log.info("Author {} is a celebrity (>25k followers). Skipping fan-out push.", event.authorId());
            return;
        }

        // Fetch followers from database or graph service
        List<String> followerIds = fetchFollowers(event.authorId());
        log.info("Fanning out post {} to {} followers...", event.postId(), followerIds.size());

        for (String followerId : followerIds) {
            timelineCacheService.addPostToTimeline(followerId, event.postId(), event.timestamp());
        }
    }

    private List<String> fetchFollowers(String authorId) {
        // Simulating follower retrieval
        return List.of("user_101", "user_102", "user_103");
    }
}
```

### Example 3: Hybrid Feed Aggregator Service

```java
package com.example.feed.service;

import com.example.feed.dto.FeedPostDto;
import org.springframework.stereotype.Service;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

@Service
public class HybridFeedAggregatorService {

    private final TimelineCacheService timelineCache;

    public HybridFeedAggregatorService(TimelineCacheService timelineCache) {
        this.timelineCache = timelineCache;
    }

    public List<FeedPostDto> getHomeFeed(String userId, int page, int size) {
        // 1. Pull pre-computed post IDs from user's Redis timeline (Fan-out on Write)
        List<String> pushPostIds = timelineCache.getTimelinePostIds(userId, page, size);

        // 2. Pull recent posts from followed celebrities (Fan-out on Read)
        List<String> followedCelebrities = getFollowedCelebrities(userId);
        List<FeedPostDto> celebrityPosts = fetchRecentCelebrityPosts(followedCelebrities);

        // 3. Hydrate push post IDs from cache/DB
        List<FeedPostDto> regularPosts = hydratePosts(pushPostIds);

        // 4. Merge, sort by timestamp DESC, and return top 'size'
        List<FeedPostDto> combined = new ArrayList<>();
        combined.addAll(regularPosts);
        combined.addAll(celebrityPosts);
        combined.sort(Comparator.comparing(FeedPostDto::timestamp).reversed());

        return combined.stream().limit(size).toList();
    }

    private List<String> getFollowedCelebrities(String userId) {
        return List.of("celeb_elon", "celeb_taylor");
    }

    private List<FeedPostDto> fetchRecentCelebrityPosts(List<String> celebs) {
        return List.of(); // Demo stub
    }

    private List<FeedPostDto> hydratePosts(List<String> ids) {
        return ids.stream()
                .map(id -> new FeedPostDto(id, "author_1", "Sample Post Content", System.currentTimeMillis()))
                .toList();
    }
}
```

### Example 4: Feed REST Controller with Cursor-Based Pagination

```java
package com.example.feed.web;

import com.example.feed.dto.FeedPostDto;
import com.example.feed.service.HybridFeedAggregatorService;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/v1/feed")
public class NewsFeedController {

    private final HybridFeedAggregatorService feedService;

    public NewsFeedController(HybridFeedAggregatorService feedService) {
        this.feedService = feedService;
    }

    @GetMapping
    public List<FeedPostDto> getFeed(
            @RequestHeader("X-User-Id") String userId,
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "20") int size) {

        return feedService.getHomeFeed(userId, page, size);
    }
}
```

### Example 5: Post DTO

```java
package com.example.feed.dto;

public record FeedPostDto(
        String postId,
        String authorId,
        String content,
        long timestamp
) {}
```

---

## 6. Best Practices

- **Cap Timeline Depth**: Never store an infinite timeline in Redis. Cap each user's timeline at the newest 800 items (`ZREMRANGEBYRANK`) to control memory costs.
- **Use Cursor-Based Pagination**: Avoid `page=500` offset pagination. Use timestamp cursors (`max_id` or `until_timestamp`) to prevent duplicate items when new posts are published during scrolling.
- **Isolate Inactive Users**: Do not fan out posts to users who haven't logged in for 30 days. Only build their timeline when they log back in.
- **Store IDs in Timelines, Not Full Post Payloads**: Keep Redis ZSETs lightweight by storing only 64-bit `post_id`s. Hydrate rich content (text, image URLs, author profile) from an L1/L2 cache or ScyllaDB.
- **Tune Kafka Partitions for Fan-Out**: Partition fan-out workers by `follower_id` to parallelize timeline writes across hundreds of worker nodes.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **SQL `JOIN` on Read** | `SELECT * FROM posts WHERE author IN (SELECT following...)` collapses at 500M users. | Pre-compute feeds into Redis Sorted Sets. |
| **Pushing Celebrity Posts to 100M Followers** | Pushing 1 post across 100M Redis keys takes 10+ minutes and creates catastrophic write spikes. | Use Fan-out on Read for accounts with $> 25\text{k}$ followers. |
| **Storing Full Post Text in Redis ZSET** | Storing 5 KB post JSON in 800 timeline entries per user consumes terabytes of unnecessary RAM. | Store only lightweight 8-byte `post_id`s in the ZSET; hydrate on demand. |
| **Offset Pagination (`LIMIT 10 OFFSET 200`)** | If new posts are inserted while the user scrolls, posts shift and duplicate across pages. | Use Cursor-based pagination (`max_timestamp` / `since_id`). |
| **Fanning Out to Inactive Accounts** | 60% of registered accounts are dormant; wasting disk and CPU updating dead feeds is costly. | Check `last_active_at`; skip fan-out for accounts dormant $>30$ days. |
| **No TTL on Stale Timelines** | Abandoned timelines sit in Redis indefinitely, exhausting cluster memory. | Set a 7-day TTL on timeline keys, refreshed on user activity. |

---

## 8. Performance & Security Considerations

### 8.1 Memory Sizing for 500 Million Users
Assuming 50M Daily Active Users (DAU), each maintaining 800 post IDs in Redis ZSET:
$$\text{Memory per user} = 800\text{ IDs} \times 16\text{ bytes} \approx 12.8\text{ KB}$$
$$\text{Total Active RAM} = 50,000,000 \times 12.8\text{ KB} \approx 640\text{ GB RAM}$$
A 10-node Redis Cluster (64 GB per node) easily holds all active timelines in memory.

### 8.2 Private Account Authorization
When hydrating posts, verify that the viewer is authorized to view private author content before returning the feed.

---

## 9. Comparison / Decision Matrix

| Metric | Fan-Out on Write (Push) | Fan-Out on Read (Pull) | Hybrid Model |
|---|---|---|---|
| **Read Latency** | Ultra-Fast ($< 5\text{ms}$) | Slow ($100-500\text{ms}$) | Fast ($< 25\text{ms}$) |
| **Write Amplification**| Severe for celebrities | Zero | Low & predictable |
| **Storage Cost** | High (Duplicated post IDs) | Minimal (Posts stored once) | Optimized (Only active users) |
| **Best For** | Regular users ($< 25\text{k}$ followers)| Accounts with millions of followers| Large-scale production |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. What is the difference between Fan-Out on Write and Fan-Out on Read?</strong></summary>

Fan-Out on Write pushes a newly published post ID into every follower's pre-computed timeline in Redis at write time. Fan-Out on Read pulls and merges posts from followed accounts in real time when the user loads their feed.
</details>

<details>
<summary><strong>2. How does the Hybrid Model solve the "Celebrity Problem"?</strong></summary>

When a user with 50M followers posts, pushing to 50M Redis keys exhausts system resources. In the hybrid model, posts from users with $>25\text{k}$ followers are NOT pushed. Instead, when a follower loads their feed, the system pulls the celebrity's recent posts and merges them with the pre-computed feed in memory.
</details>

<details>
<summary><strong>3. Why should Redis Timelines only store `post_id`s rather than full JSON documents?</strong></summary>

Storing full JSON documents duplicates data thousands of times and rapidly exhausts RAM. Storing 8-byte `post_id`s in a Redis ZSET keeps timelines ultra-compact. Content is hydrated from an in-memory document cache or ScyllaDB on demand.
</details>

<details>
<summary><strong>4. How do you prevent inactive users from consuming Redis memory?</strong></summary>

Track `last_login_timestamp`. If a user hasn't logged in for 30 days, skip pushing posts to their timeline. When an inactive user opens the app, reconstruct their timeline on demand.
</details>

<details>
<summary><strong>5. Why is cursor-based pagination superior to offset-based pagination for social feeds?</strong></summary>

Offset pagination (`OFFSET 20`) causes duplicate or skipped posts when new items are added at the top of the feed during scrolling. Cursor-based pagination (`WHERE post_id < last_seen_id`) provides deterministic, stable pagination.
</details>

<details>
<summary><strong>6. Which database is ideal for storing raw post content and follower graphs?</strong></summary>

ScyllaDB / Cassandra for append-only raw posts (keyed by `post_id`), and Neo4j or distributed relational/B-Tree stores (PostgreSQL / MySQL) for follower relationship graphs.
</details>

<details>
<summary><strong>7. How do you handle unfollowing an account?</strong></summary>

Asynchronously remove the unfollowed user's post IDs from the follower's Redis timeline (`ZREM`), or filter them out during feed hydration.
</details>

<details>
<summary><strong>8. What is the optimal data structure in Redis for newsfeeds?</strong></summary>

Redis Sorted Set (`ZSET`). The score is the publication epoch timestamp, and the value is the `post_id`. This allows $O(\log N + M)$ range queries sorted chronologically.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Implement Redis ZSET Timeline Ingestion</strong></summary>

**Prompt**: Write a method adding a post ID with current timestamp to a user's timeline and trimming to 500 items.

**Solution**:
Inspect Example 1: call `redis.opsForZSet().add(key, postId, timestamp)` followed by `removeRange()`.
</details>

<details>
<summary><strong>Exercise 2: Implement In-Memory Feed Merger</strong></summary>

**Prompt**: Write a Java function merging two lists of posts and returning the top 20 sorted by timestamp descending.

**Solution**:
```java
public List<FeedPostDto> mergeFeeds(List<FeedPostDto> regular, List<FeedPostDto> celeb) {
    return Stream.concat(regular.stream(), celeb.stream())
            .sorted(Comparator.comparing(FeedPostDto::timestamp).reversed())
            .limit(20)
            .toList();
}
```
</details>

<details>
<summary><strong>Exercise 3: Configure Spring Kafka Fan-Out Listener</strong></summary>

**Prompt**: Configure `@KafkaListener` with 10 concurrent threads for parallelizing timeline fan-out writes.

**Solution**:
Inspect Example 2: set `concurrency = "10"` on `@KafkaListener`.
</details>

<details>
<summary><strong>Exercise 4: Test Cursor-Based Pagination</strong></summary>

**Prompt**: Write a unit test simulating scrolling through a timeline with a timestamp cursor.

**Solution**:
Query `opsForZSet().reverseRangeByScore(key, 0, maxScore, 0, 10)` and assert that all returned scores are $\le$ `maxScore`.
</details>

<details>
<summary><strong>Exercise 5: Build Inactive User Eviction Filter</strong></summary>

**Prompt**: Write a method checking user activity before enqueuing a fan-out task.

**Solution**:
```java
public boolean isUserActive(String userId) {
    Instant lastSeen = userActivityRepo.findLastSeen(userId);
    return lastSeen.isAfter(Instant.now().minus(30, ChronoUnit.DAYS));
}
```
</details>

<details>
<summary><strong>Exercise 6: Test Feed Endpoint with MockMvc</strong></summary>

**Prompt**: Write a MockMvc integration test verifying that `GET /api/v1/feed` returns HTTP 200 with 20 items.

**Solution**:
```java
mockMvc.perform(get("/api/v1/feed").header("X-User-Id", "user_1"))
        .andExpect(status().isOk())
        .andExpect(jsonPath("$.length()").value(20));
```
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   SOCIAL NEWSFEED DESIGN CHEATSHEET                    │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Storage: Store only post_ids in Redis ZSET (Score = Timestamp)      │
│ 2. Hybrid Model: Fan-out on write (<25k followers) + read (celebrities)│
│ 3. Memory Cap: Trim timelines to newest 800 items (ZREMRANGEBYRANK)    │
│ 4. Dormant Accounts: Skip fan-out for users inactive > 30 days         │
│ 5. Pagination: Use Cursor-based pagination (timestamp / last_seen_id)  │
│ 6. Hydration: Hydrate post text/images from ScyllaDB/Redis cache       │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Designing a Newsfeed System (Alex Xu System Design Volume 1)](https://bytebytego.com/)
- [Redis Sorted Sets Documentation](https://redis.io/docs/data-types/sorted-sets/)
- [The Architecture of Twitter's Feed System](https://blog.twitter.com/engineering/en_us/topics/infrastructure/2017/the-infrastructure-behind-twitter-scale)
- [Instagram Feed Architecture Overview](https://instagram-engineering.com/)
