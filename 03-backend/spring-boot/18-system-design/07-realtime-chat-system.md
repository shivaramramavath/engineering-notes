# 07: Designing a Real-Time Chat System at Scale (5M Concurrent Users)

| Metadata Field | Value |
|---|---|
| **Topic** | High-Concurrency Real-Time Messaging & Distributed WebSockets |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Scale** | 5,000,000 Concurrent Connections, Sub-50ms Message Delivery |
| **Key Middleware** | Spring WebSocket (STOMP), Redis Cluster (Pub/Sub & Presence), ScyllaDB / Cassandra (History), Kafka |

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

Real-time chat platforms (e.g., Slack, WhatsApp, Discord) require bidirectional, persistent, low-overhead communication channels between clients and servers. Standard HTTP polling or Long Polling introduces excessive network overhead and connection thrashing. 

Operating at a scale of 5,000,000 concurrent connected users introduces distinct engineering challenges:
- **Connection Density & Memory**: Maintaining 5M idle TCP connections consumes significant RAM and file descriptors.
- **Cross-Node Routing**: When Alice is connected to Gateway Node 1 and Bob is connected to Gateway Node 85, messages must bridge nodes instantly with sub-50ms latency.
- **Presence & Offline Fallbacks**: If Bob disconnects, the system must detect his absence via heartbeats and route the message to the Mobile Push Notification service (APNs/FCM).
- **High-Throughput Chat History**: Storing billions of messages requires an append-optimized distributed NoSQL database (ScyllaDB or Apache Cassandra).

---

## 2. Core Concepts

### 2.1 WebSockets & STOMP Protocol
- **WebSocket (RFC 6455)**: Upgrades an initial HTTP/1.1 handshake into a persistent, bidirectional, full-duplex TCP stream over port 80/443.
- **STOMP (Simple Text Oriented Messaging Protocol)**: An application-level framing protocol defining destination routing (`/app/chat.send`, `/topic/rooms/{id}`, `/user/queue/messages`).

### 2.2 Redis Cluster Pub/Sub Message Bridge
Because clients are distributed across hundreds of Spring Boot nodes, each node cannot know where all recipients are connected. Nodes publish messages to a centralized Redis Pub/Sub channel or Kafka topic. Every node subscribes to the channel; if the recipient's session lives on that node, it forwards the frame to the client's local WebSocket.

---

## 3. Internals & Architecture

```
        [ Alice (Mobile App) ]                  [ Bob (Web Client) ]
                  │                                       │
         (WebSocket / STOMP)                     (WebSocket / STOMP)
                  ▼                                       ▼
       ┌─────────────────────┐                 ┌─────────────────────┐
       │ Spring Gateway Pod 1│                 │Spring Gateway Pod 85│
       │ Local Session: Alice│                 │ Local Session: Bob  │
       └──────────┬──────────┘                 └──────────▲──────────┘
                  │                                       │
                  │ 1. Publish to Redis                   │ 3. Relay to Bob
                  ▼                                       │    over WebSocket
       ┌──────────────────────────────────────────────────┴──────────┐
       │             Distributed Redis Pub/Sub Cluster               │
       │                   Channel: "chat:rooms:42"                  │
       └──────────────────────────────┬──────────────────────────────┘
                                      │
                              2. Async Ingestion
                                      ▼
                       ┌──────────────────────────────┐
                       │   Apache Kafka Event Log     │
                       │   Topic: "chat-messages-raw" │
                       └──────────────┬───────────────┘
                                      │
                                      ▼
                       ┌──────────────────────────────┐
                       │  ScyllaDB / Cassandra Store  │
                       │  - Wide-column chat history  │
                       │  - Partition: room_id        │
                       │  - Cluster: message_id DESC  │
                       └──────────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Connection & Memory Math (5 Million Connections)
- 1 Linux Socket $\approx 3\text{ KB}$ kernel buffer memory.
- 1 Spring WebSocket session $\approx 10\text{ KB}$ JVM heap.
- Total memory for 5M connections $\approx 5\text{M} \times 13\text{ KB} \approx 65\text{ GB}$.
- Across 100 Spring Boot pods: each pod holds **50,000 connections**, consuming $\sim 650\text{ MB}$ session RAM.

---

## 5. 5 Practical Examples

### Example 1: STOMP WebSocket Configuration with Relay Broker

```java
package com.example.chat.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.messaging.simp.config.MessageBrokerRegistry;
import org.springframework.web.socket.config.annotation.EnableWebSocketMessageBroker;
import org.springframework.web.socket.config.annotation.StompEndpointRegistry;
import org.springframework.web.socket.config.annotation.WebSocketMessageBrokerConfigurer;

@Configuration
@EnableWebSocketMessageBroker
public class WebSocketBrokerConfig implements WebSocketMessageBrokerConfigurer {

    @Override
    public void configureMessageBroker(MessageBrokerRegistry registry) {
        // Enable external RabbitMQ STOMP broker relay for enterprise clustering
        registry.enableStompBrokerRelay("/topic", "/queue")
                .setRelayHost("rabbitmq-stomp-cluster")
                .setRelayPort(61613)
                .setClientLogin("guest")
                .setClientPasscode("guest");

        registry.setApplicationDestinationPrefixes("/app");
        registry.setUserDestinationPrefix("/user");
    }

    @Override
    public void registerStompEndpoints(StompEndpointRegistry registry) {
        registry.addEndpoint("/ws-chat")
                .setAllowedOriginPatterns("*")
                .withSockJS();
    }
}
```

### Example 2: Redis Pub/Sub Multi-Node Message Bridge

```java
package com.example.chat.redis;

import com.example.chat.dto.ChatMessageDto;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.data.redis.connection.Message;
import org.springframework.data.redis.connection.MessageListener;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.stereotype.Component;

@Component
public class RedisChatBridgeSubscriber implements MessageListener {

    private static final Logger log = LoggerFactory.getLogger(RedisChatBridgeSubscriber.class);
    private final SimpMessagingTemplate messagingTemplate;
    private final ObjectMapper objectMapper;

    public RedisChatBridgeSubscriber(SimpMessagingTemplate messagingTemplate, ObjectMapper objectMapper) {
        this.messagingTemplate = messagingTemplate;
        this.objectMapper = objectMapper;
    }

    @Override
    public void onMessage(Message message, byte[] pattern) {
        try {
            ChatMessageDto chat = objectMapper.readValue(message.getBody(), ChatMessageDto.class);
            log.debug("Bridging chat message for room {} to local subscribers", chat.roomId());
            messagingTemplate.convertAndSend("/topic/rooms/" + chat.roomId(), chat);
        } catch (Exception ex) {
            log.error("Failed to forward Redis chat frame: {}", ex.getMessage());
        }
    }
}
```

### Example 3: User Presence & Heartbeat Tracking in Redis

```java
package com.example.chat.presence;

import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;

import java.time.Duration;

@Service
public class UserPresenceService {

    private final StringRedisTemplate redis;

    public UserPresenceService(StringRedisTemplate redis) {
        this.redis = redis;
    }

    public void recordHeartbeat(String userId) {
        // Set user presence online with 60s TTL; heartbeats must arrive every 25s
        redis.opsForValue().set("presence:online:" + userId, "ONLINE", Duration.ofSeconds(60));
    }

    public boolean isUserOnline(String userId) {
        return Boolean.TRUE.equals(redis.hasKey("presence:online:" + userId));
    }

    public void markUserOffline(String userId) {
        redis.delete("presence:online:" + userId);
    }
}
```

### Example 4: Chat Inbound Controller with Offline Push Fallback

```java
package com.example.chat.web;

import com.example.chat.dto.ChatMessageDto;
import com.example.chat.presence.UserPresenceService;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.messaging.handler.annotation.MessageMapping;
import org.springframework.messaging.handler.annotation.Payload;
import org.springframework.stereotype.Controller;

@Controller
public class ChatMessageController {

    private static final Logger log = LoggerFactory.getLogger(ChatMessageController.class);
    private final StringRedisTemplate redisTemplate;
    private final KafkaTemplate<String, Object> kafkaTemplate;
    private final UserPresenceService presenceService;

    public ChatMessageController(StringRedisTemplate redisTemplate,
                                 KafkaTemplate<String, Object> kafkaTemplate,
                                 UserPresenceService presenceService) {
        this.redisTemplate = redisTemplate;
        this.kafkaTemplate = kafkaTemplate;
        this.presenceService = presenceService;
    }

    @MessageMapping("/chat.send")
    public void handleIncomingMessage(@Payload ChatMessageDto message) {
        log.info("Message received from {}: room {}", message.senderId(), message.roomId());

        // 1. Publish to Redis Pub/Sub for realtime delivery across active cluster nodes
        redisTemplate.convertAndSend("chat:channel:" + message.roomId(), message.content());

        // 2. Publish to Kafka for asynchronous persistence into ScyllaDB
        kafkaTemplate.send("chat-persistence-stream", message.roomId(), message);

        // 3. If recipient is offline, trigger push notification
        if (message.recipientId() != null && !presenceService.isUserOnline(message.recipientId())) {
            kafkaTemplate.send("notifications.critical", message.recipientId(), message);
        }
    }
}
```

### Example 5: ScyllaDB / Cassandra Chat History Schema (CQL)

```sql
CREATE KEYSPACE chat_system WITH replication = {
    'class': 'NetworkTopologyStrategy', 
    'us-east': 3
};

CREATE TABLE chat_system.messages_by_room (
    room_id text,
    bucket int, -- e.g. YearMonth: 202611
    message_id timeuuid,
    sender_id text,
    content text,
    PRIMARY KEY ((room_id, bucket), message_id)
) WITH CLUSTERING ORDER BY (message_id DESC);
```

---

## 6. Best Practices

- **Enforce WebSocket Heartbeats**: Configure ping/pong frames every 25 seconds (`setHeartbeatValue(new long[]{25000, 25000})`) to keep NAT routers and load balancers from closing idle connections.
- **Tune Linux File Descriptors**: Increase OS `nofile` limits (`ulimit -n 1000000`) on Gateway containers so each host can maintain 50,000+ open sockets.
- **Bucket Cassandra/ScyllaDB Partitions**: In high-velocity group chats, partition keys can exceed Cassandra's 100 MB per-partition threshold. Include a time bucket in the partition key: `(room_id, year_month)`.
- **Use TimeUUID for Monotonic Ordering**: Prevent client clock drift conflicts by generating Cassandra `TimeUUID` or Twitter Snowflake IDs on the server side.
- **Fallback to Push Notifications**: Check presence in Redis before abandoning undelivered direct messages; queue an APNs/FCM mobile alert if the user is offline.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Storing Open WebSockets in RDBMS** | Relational databases lock and collapse trying to track 5M active connection state updates. | Track presence in Redis with ephemeral TTLs and key-expiration listeners. |
| **Simple In-Memory Broker in Cluster** | Using `enableSimpleBroker()` in a multi-pod cluster fails because pods cannot forward messages to peers. | Use a distributed broker relay (RabbitMQ STOMP) or Redis Pub/Sub bridge. |
| **Unbounded Cassandra Partitions** | A group chat with 10M messages creates a gigantic partition that causes read timeouts and GC storms. | Bucket partition keys by room and month: `((room_id, bucket), message_id)`. |
| **No Backpressure on Slow Clients** | A client on poor cellular bandwidth causes the server to buffer thousands of outbound frames in RAM. | Terminate connections if client outbound buffers exceed limits. |
| **Relying on Client Timestamp for Order** | Out-of-sync client device clocks result in out-of-order message displays. | Generate monotonically increasing Snowflake IDs or `TimeUUID` on the server. |
| **No Load Balancer Session Stickiness** | WebSocket upgrades fail if initial HTTP handshake hits Node A and the upgrade hits Node B. | Enable cookie-based or IP-hash sticky sessions at the load balancer. |

---

## 8. Performance & Security Considerations

### 8.1 OS Socket Buffer Tuning
Reduce socket read/write buffer defaults in Linux to minimize RAM per connection:
```bash
# /etc/sysctl.conf
net.ipv4.tcp_rmem = 4096 87380 4194304
net.ipv4.tcp_wmem = 4096 65536 4194304
```

### 8.2 WebSocket Handshake Authentication
Validate JWTs during the initial HTTP handshake interceptor before the WebSocket upgrade completes:
```java
public class JwtHandshakeInterceptor implements HandshakeInterceptor {
    @Override
    public boolean beforeHandshake(ServerHttpRequest request, ServerHttpResponse response,
                                  WebSocketHandler wsHandler, Map<String, Object> attributes) {
        // Extract & validate Bearer token from query parameter or cookie
        return true;
    }
}
```

---

## 9. Comparison / Decision Matrix

| Technology | Latency | Overhead | Directionality | Scale Fit |
|---|---|---|---|---|
| **Short Polling** | High (500ms+) | Extreme (Headers per poll) | Unidirectional (Client -> Server) | Poor |
| **Long Polling** | Moderate | High (TCP teardown/reconnect) | Unidirectional | Fair (Fallback) |
| **Server-Sent Events (SSE)**| Low ($< 50\text{ms}$) | Low | Unidirectional (Server -> Client) | Good (Feed/Stock alerts) |
| **WebSocket (STOMP)**| Lowest ($< 10\text{ms}$) | Lowest (2-byte framing) | Bidirectional (Full duplex) | Best (Realtime Chat) |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. How do you scale WebSockets to 5 million concurrent connections?</strong></summary>

Distribute connections across 100 Spring Boot nodes (50k connections per node). Tune OS `nofile` limits, reduce TCP buffer sizes, and bridge nodes using an external message broker (RabbitMQ STOMP relay or Redis Pub/Sub cluster).
</details>

<details>
<summary><strong>2. How does Alice send a message to Bob if they are connected to different servers?</strong></summary>

Alice's server publishes the message to a distributed channel (Redis Pub/Sub or Kafka). Bob's server subscribes to the channel, recognizes that Bob holds an active local WebSocket session, and forwards the frame down Bob's TCP socket.
</details>

<details>
<summary><strong>3. How does the system detect when a user abruptly loses internet connectivity?</strong></summary>

WebSockets utilize periodic ping/pong heartbeats (every 25 seconds). If the server misses two consecutive heartbeats, it terminates the socket, cleans up local session state, and marks the user offline in Redis.
</details>

<details>
<summary><strong>4. Why is Cassandra or ScyllaDB ideal for chat history storage?</strong></summary>

Chat history is append-only and read by partition (`room_id`) ordered by time. Cassandra's LSM-tree storage engine provides high write throughput and low-latency range scans using clustering keys (`message_id DESC`).
</details>

<details>
<summary><strong>5. How do you guarantee strict message ordering in a group chat?</strong></summary>

Generate monotonically increasing 64-bit Snowflake IDs on the server side (combining timestamp, worker ID, and sequence number). Clients order messages by this ID rather than device clocks.
</details>

<details>
<summary><strong>6. What happens if a recipient is offline when a message is sent?</strong></summary>

The chat gateway queries the Redis presence key. If offline, the message is stored in ScyllaDB and an asynchronous event is published to Kafka to trigger a Push Notification via APNs/FCM.
</details>

<details>
<summary><strong>7. How do you prevent unauthorized users from eavesdropping on private chat rooms?</strong></summary>

Intercept STOMP `SUBSCRIBE` frames in a `ChannelInterceptor`. Check if the authenticated user's ID belongs to the `room_members` table before allowing the subscription to complete.
</details>

<details>
<summary><strong>8. Why should you avoid storing billions of chat messages in PostgreSQL?</strong></summary>

PostgreSQL uses B-Trees that incur high random I/O write amplification, VACUUM maintenance overhead, and complex sharding requirements at billions of rows. Cassandra scales horizontally with linear write performance.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Implement STOMP Channel Interceptor for Authorization</strong></summary>

**Prompt**: Write a `ChannelInterceptor` rejecting unauthorized subscriptions to `/topic/rooms/{id}`.

**Solution**:
```java
public class RoomAuthInterceptor implements ChannelInterceptor {
    @Override
    public Message<?> preSend(Message<?> message, MessageChannel channel) {
        StompHeaderAccessor accessor = StompHeaderAccessor.wrap(message);
        if (StompCommand.SUBSCRIBE.equals(accessor.getCommand())) {
            String destination = accessor.getDestination();
            Principal user = accessor.getUser();
            // Verify user belongs to room; throw AccessDeniedException if not
        }
        return message;
    }
}
```
</details>

<details>
<summary><strong>Exercise 2: Implement Redis Presence Heartbeat</strong></summary>

**Prompt**: Write a Spring service method recording user heartbeat and checking if a user has been active in the last 60 seconds.

**Solution**:
Inspect Example 3: use `redis.opsForValue().set(key, "ONLINE", Duration.ofSeconds(60))` and check `redis.hasKey(key)`.
</details>

<details>
<summary><strong>Exercise 3: Configure RabbitMQ STOMP Relay</strong></summary>

**Prompt**: Configure `WebSocketMessageBrokerConfigurer` with an external RabbitMQ STOMP broker relay on port 61613.

**Solution**:
Inspect Example 1: call `registry.enableStompBrokerRelay("/topic", "/queue").setRelayHost(...).setRelayPort(61613)`.
</details>

<details>
<summary><strong>Exercise 4: Generate Monotonic Snowflake ID</strong></summary>

**Prompt**: Implement a 64-bit Snowflake ID generator in Java using timestamp, worker ID, and atomic sequence.

**Solution**:
```java
public synchronized long nextId() {
    long timestamp = System.currentTimeMillis();
    long sequence = (seq++) & 4095;
    return (timestamp << 22) | (workerId << 12) | sequence;
}
```
</details>

<details>
<summary><strong>Exercise 5: Simulate Offline User Detection</strong></summary>

**Prompt**: Write a JUnit test verifying that an offline user triggers a Kafka event on `notifications.critical`.

**Solution**:
Mock `UserPresenceService.isUserOnline()` to return false, call `handleIncomingMessage()`, and verify `kafkaTemplate.send("notifications.critical", ...)` was invoked.
</details>

<details>
<summary><strong>Exercise 6: Query ScyllaDB Chat History with Paging</strong></summary>

**Prompt**: Write a Spring Data Cassandra repository query fetching the last 50 messages of a room before a specific `TimeUUID`.

**Solution**:
```java
@Query("SELECT * FROM messages_by_room WHERE room_id = ?0 AND bucket = ?1 AND message_id < ?2 LIMIT 50")
List<ChatMessageEntity> findHistory(String roomId, int bucket, UUID beforeMessageId);
```
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   REALTIME CHAT AT SCALE CHEATSHEET                    │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Transport: Full-duplex WebSockets with STOMP framing & heartbeats   │
│ 2. Node Bridging: External RabbitMQ STOMP relay or Redis Pub/Sub       │
│ 3. Connection Density: ~50k connections/pod; tune OS nofile to 1M      │
│ 4. History Storage: ScyllaDB / Cassandra partitioned by (room, bucket) │
│ 5. Offline Fallback: Check Redis presence; route to Push (APNs/FCM)    │
│ 6. Message Ordering: Monotonic server-side 64-bit Snowflake IDs        │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring WebSocket Reference Documentation](https://docs.spring.io/spring-framework/reference/web/websocket.html)
- [RFC 6455: The WebSocket Protocol](https://datatracker.ietf.org/doc/html/rfc6455)
- [STOMP Protocol Specification 1.2](https://stomp.github.io/stomp-specification-1.2.html)
- [Apache Cassandra Data Modeling Guide](https://cassandra.apache.org/_/quickstart.html)
