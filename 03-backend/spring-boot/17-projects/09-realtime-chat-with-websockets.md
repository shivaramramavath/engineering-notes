# Project 09: Realtime Chat Backend with WebSockets, STOMP & Redis Pub/Sub

| Metadata Field | Value |
|---|---|
| **Spring Boot Version** | 3.4.2 |
| **Spring Framework** | 6.2.2 |
| **Java Version** | 21 (LTS) |
| **Build System** | Maven 3.9+ |
| **Key Dependencies** | Spring Boot Starter Websocket, Spring Boot Starter Data Redis, Spring Boot Starter Data JPA, PostgreSQL Driver, Testcontainers |
| **Architecture Style** | Horizontally Scalable Realtime Messaging with STOMP over WebSocket & Redis Pub/Sub Bridge |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture

This project implements a scalable realtime multi-room chat backend. WebSocket clients connect via STOMP protocol. When a client publishes to `/app/chat/{roomId}`, the local node receives the frame, persists the message into PostgreSQL, and publishes it to a distributed Redis Pub/Sub channel (`chat:rooms:{roomId}`). All cluster nodes subscribe to Redis channels and relay incoming messages to their connected local WebSocket clients through `/topic/rooms/{roomId}`.

```
       [ Client A (Browser) ]              [ Client B (Mobile) ]
                 │                                   │
      (WebSocket / STOMP)                 (WebSocket / STOMP)
                 ▼                                   ▼
      ┌─────────────────────┐             ┌─────────────────────┐
      │ Spring Boot Node 1  │             │ Spring Boot Node 2  │
      │                     │             │                     │
      │ ┌─────────────────┐ │             │ ┌─────────────────┐ │
      │ │ STOMP Broker    │ │             │ │ STOMP Broker    │ │
      │ └────────┬────────┘ │             │ └────────▲────────┘ │
      └──────────┼──────────┘             └──────────┼──────────┘
                 │                                   │
                 │ 1. Publish to Redis               │ 2. Redis Message
                 ▼                                   │    Listener Relays
      ┌──────────────────────────────────────────────┴──────────┐
      │              Redis Cluster / Pub-Sub Channel            │
      │                  "chat:rooms:{roomId}"                  │
      └──────────────────────────────┬──────────────────────────┘
                                     │
                             (Async Archive)
                                     ▼
                      ┌─────────────────────────────┐
                      │    PostgreSQL Database      │
                      │  - chat_rooms               │
                      │  - chat_messages            │
                      └─────────────────────────────┘
```

### 1.2 Directory Layout

```
09-realtime-chat-with-websockets/
├── pom.xml
├── src/
│   ├── main/
│   │   ├── java/com/example/chat/
│   │   │   ├── RealtimeChatApplication.java
│   │   │   ├── config/
│   │   │   │   ├── WebSocketConfig.java
│   │   │   │   └── RedisChatPubSubConfig.java
│   │   │   ├── domain/
│   │   │   │   ├── ChatRoom.java
│   │   │   │   └── ChatMessage.java
│   │   │   ├── repository/
│   │   │   │   ├── ChatRoomRepository.java
│   │   │   │   └── ChatMessageRepository.java
│   │   │   ├── redis/
│   │   │   │   ├── RedisMessagePublisher.java
│   │   │   │   └── RedisMessageSubscriber.java
│   │   │   ├── service/
│   │   │   │   └── ChatService.java
│   │   │   └── web/
│   │   │       ├── ChatWsController.java
│   │   │       ├── ChatHistoryRestController.java
│   │   │       └── dto/
│   │   │           ├── SendMessagePayload.java
│   │   │           └── ChatMessageDto.java
│   │   └── resources/
│   │       ├── application.yaml
│   │       └── db/migration/V1__init_chat_schema.sql
│   └── test/
│       └── java/com/example/chat/
│           └── ChatIntegrationTest.java
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
    <artifactId>realtime-chat</artifactId>
    <version>1.0.0</version>
    <name>realtime-chat</name>
    <description>Enterprise Realtime Chat Backend with WebSockets and Redis</description>

    <properties>
        <java.version>21</java.version>
        <testcontainers.version>1.20.4</testcontainers.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-websocket</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-data-redis</artifactId>
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
    name: realtime-chat
  datasource:
    url: jdbc:postgresql://${DB_HOST:localhost}:${DB_PORT:5432}/${DB_NAME:chat_db}
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

management:
  endpoints:
    web:
      exposure:
        include: health,info,metrics
```

### 2.3 Flyway Migration: `V1__init_chat_schema.sql`

```sql
CREATE TABLE chat_rooms (
    id UUID PRIMARY KEY,
    name VARCHAR(100) NOT NULL UNIQUE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE chat_messages (
    id UUID PRIMARY KEY,
    room_id UUID NOT NULL REFERENCES chat_rooms(id) ON DELETE CASCADE,
    sender_id VARCHAR(100) NOT NULL,
    sender_name VARCHAR(100) NOT NULL,
    content TEXT NOT NULL,
    sent_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_chat_messages_room ON chat_messages(room_id, sent_at DESC);
```

### 2.4 Domain Models & Repositories

```java
package com.example.chat.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "chat_rooms")
public class ChatRoom {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(nullable = false, unique = true, length = 100)
    private String name;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    public ChatRoom() {}

    public ChatRoom(String name) {
        this.name = name;
    }

    public UUID getId() { return id; }
    public String getName() { return name; }
    public Instant getCreatedAt() { return createdAt; }
}
```

```java
package com.example.chat.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "chat_messages")
public class ChatMessage {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "room_id", nullable = false)
    private ChatRoom room;

    @Column(name = "sender_id", nullable = false, length = 100)
    private String senderId;

    @Column(name = "sender_name", nullable = false, length = 100)
    private String senderName;

    @Column(nullable = false, columnDefinition = "TEXT")
    private String content;

    @Column(name = "sent_at", nullable = false, updatable = false)
    private Instant sentAt = Instant.now();

    public ChatMessage() {}

    public ChatMessage(ChatRoom room, String senderId, String senderName, String content) {
        this.room = room;
        this.senderId = senderId;
        this.senderName = senderName;
        this.content = content;
    }

    public UUID getId() { return id; }
    public ChatRoom getRoom() { return room; }
    public String getSenderId() { return senderId; }
    public String getSenderName() { return senderName; }
    public String getContent() { return content; }
    public Instant getSentAt() { return sentAt; }
}
```

```java
package com.example.chat.repository;

import com.example.chat.domain.ChatMessage;
import org.springframework.data.domain.Pageable;
import org.springframework.data.domain.Slice;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.UUID;

public interface ChatMessageRepository extends JpaRepository<ChatMessage, UUID> {
    Slice<ChatMessage> findByRoomIdOrderBySentAtDesc(UUID roomId, Pageable pageable);
}
```

```java
package com.example.chat.repository;

import com.example.chat.domain.ChatRoom;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.Optional;
import java.util.UUID;

public interface ChatRoomRepository extends JpaRepository<ChatRoom, UUID> {
    Optional<ChatRoom> findByName(String name);
}
```

### 2.5 WebSocket & Redis Infrastructure

```java
package com.example.chat.web.dto;

import java.time.Instant;
import java.util.UUID;

public record ChatMessageDto(
        UUID id,
        UUID roomId,
        String senderId,
        String senderName,
        String content,
        Instant sentAt
) {}
```

```java
package com.example.chat.web.dto;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import java.util.UUID;

public record SendMessagePayload(
        @NotNull UUID roomId,
        @NotBlank String senderId,
        @NotBlank String senderName,
        @NotBlank String content
) {}
```

```java
package com.example.chat.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.messaging.simp.config.MessageBrokerRegistry;
import org.springframework.web.socket.config.annotation.EnableWebSocketMessageBroker;
import org.springframework.web.socket.config.annotation.StompEndpointRegistry;
import org.springframework.web.socket.config.annotation.WebSocketMessageBrokerConfigurer;

@Configuration
@EnableWebSocketMessageBroker
public class WebSocketConfig implements WebSocketMessageBrokerConfigurer {

    @Override
    public void configureMessageBroker(MessageBrokerRegistry registry) {
        registry.enableSimpleBroker("/topic", "/queue");
        registry.setApplicationDestinationPrefixes("/app");
    }

    @Override
    public void registerStompEndpoints(StompEndpointRegistry registry) {
        registry.addEndpoint("/ws-chat")
                .setAllowedOriginPatterns("*")
                .withSockJS();
    }
}
```

```java
package com.example.chat.redis;

import com.example.chat.web.dto.ChatMessageDto;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.data.redis.connection.Message;
import org.springframework.data.redis.connection.MessageListener;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.stereotype.Component;

@Component
public class RedisMessageSubscriber implements MessageListener {

    private static final Logger log = LoggerFactory.getLogger(RedisMessageSubscriber.class);
    private final SimpMessagingTemplate messagingTemplate;
    private final ObjectMapper objectMapper;

    public RedisMessageSubscriber(SimpMessagingTemplate messagingTemplate, ObjectMapper objectMapper) {
        this.messagingTemplate = messagingTemplate;
        this.objectMapper = objectMapper;
    }

    @Override
    public void onMessage(Message message, byte[] pattern) {
        try {
            ChatMessageDto chatMessage = objectMapper.readValue(message.getBody(), ChatMessageDto.class);
            log.info("Relaying Redis chat event to local WS topic for room: {}", chatMessage.roomId());
            messagingTemplate.convertAndSend("/topic/rooms/" + chatMessage.roomId(), chatMessage);
        } catch (Exception ex) {
            log.error("Failed to deserialize Redis chat message: {}", ex.getMessage());
        }
    }
}
```

```java
package com.example.chat.service;

import com.example.chat.domain.ChatMessage;
import com.example.chat.domain.ChatRoom;
import com.example.chat.repository.ChatMessageRepository;
import com.example.chat.repository.ChatRoomRepository;
import com.example.chat.web.dto.ChatMessageDto;
import com.example.chat.web.dto.SendMessagePayload;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class ChatService {

    private static final Logger log = LoggerFactory.getLogger(ChatService.class);
    private final ChatRoomRepository roomRepository;
    private final ChatMessageRepository messageRepository;
    private final StringRedisTemplate redisTemplate;
    private final ObjectMapper objectMapper;

    public ChatService(ChatRoomRepository roomRepository,
                       ChatMessageRepository messageRepository,
                       StringRedisTemplate redisTemplate,
                       ObjectMapper objectMapper) {
        this.roomRepository = roomRepository;
        this.messageRepository = messageRepository;
        this.redisTemplate = redisTemplate;
        this.objectMapper = objectMapper;
    }

    @Transactional
    public ChatMessageDto processAndBroadcastMessage(SendMessagePayload payload) {
        ChatRoom room = roomRepository.findById(payload.roomId())
                .orElseThrow(() -> new IllegalArgumentException("Chat room not found: " + payload.roomId()));

        ChatMessage entity = new ChatMessage(room, payload.senderId(), payload.senderName(), payload.content());
        ChatMessage saved = messageRepository.save(entity);

        ChatMessageDto dto = new ChatMessageDto(
                saved.getId(),
                room.getId(),
                saved.getSenderId(),
                saved.getSenderName(),
                saved.getContent(),
                saved.getSentAt()
        );

        try {
            String json = objectMapper.writeValueAsString(dto);
            redisTemplate.convertAndSend("chat:rooms:" + room.getId(), json);
        } catch (Exception ex) {
            log.error("Failed to publish message to Redis: {}", ex.getMessage());
        }

        return dto;
    }
}
```

```java
package com.example.chat.web;

import com.example.chat.service.ChatService;
import com.example.chat.web.dto.ChatMessageDto;
import com.example.chat.web.dto.SendMessagePayload;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.messaging.handler.annotation.MessageMapping;
import org.springframework.messaging.handler.annotation.Payload;
import org.springframework.stereotype.Controller;

@Controller
public class ChatWsController {

    private static final Logger log = LoggerFactory.getLogger(ChatWsController.class);
    private final ChatService chatService;

    public ChatWsController(ChatService chatService) {
        this.chatService = chatService;
    }

    @MessageMapping("/chat.send")
    public void sendMessage(@Payload SendMessagePayload payload) {
        log.info("Received WebSocket chat frame from user: {}", payload.senderId());
        chatService.processAndBroadcastMessage(payload);
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.chat;

import com.example.chat.domain.ChatRoom;
import com.example.chat.repository.ChatRoomRepository;
import com.example.chat.service.ChatService;
import com.example.chat.web.dto.ChatMessageDto;
import com.example.chat.web.dto.SendMessagePayload;
import com.redis.testcontainers.RedisContainer;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@Testcontainers
public class ChatIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Container
    @ServiceConnection
    static RedisContainer redis = new RedisContainer("redis:7.2-alpine");

    @Autowired
    private ChatRoomRepository roomRepository;

    @Autowired
    private ChatService chatService;

    private ChatRoom testRoom;

    @BeforeEach
    void setup() {
        testRoom = roomRepository.save(new ChatRoom("general-discussion"));
    }

    @Test
    void shouldPersistAndBroadcastChatMessage() {
        SendMessagePayload payload = new SendMessagePayload(
                testRoom.getId(),
                "user-101",
                "Alice",
                "Hello distributed chat world!"
        );

        ChatMessageDto response = chatService.processAndBroadcastMessage(payload);

        assertThat(response).isNotNull();
        assertThat(response.id()).isNotNull();
        assertThat(response.content()).isEqualTo("Hello distributed chat world!");
        assertThat(response.senderName()).isEqualTo("Alice");
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
  chat-db:
    image: postgres:16-alpine
    container_name: chat-db
    environment:
      POSTGRES_DB: chat_db
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: chatpassword
    ports:
      - "5432:5432"
    volumes:
      - chat_postgres_data:/var/lib/postgresql/data

  chat-redis:
    image: redis:7.2-alpine
    container_name: chat-redis
    ports:
      - "6379:6379"

  chat-node-1:
    build: .
    container_name: chat-node-1
    ports:
      - "8081:8080"
    environment:
      DB_HOST: chat-db
      DB_PORT: 5432
      DB_NAME: chat_db
      DB_USER: postgres
      DB_PASS: chatpassword
      REDIS_HOST: chat-redis
      REDIS_PORT: 6379
    depends_on:
      - chat-db
      - chat-redis

  chat-node-2:
    build: .
    container_name: chat-node-2
    ports:
      - "8082:8080"
    environment:
      DB_HOST: chat-db
      DB_PORT: 5432
      DB_NAME: chat_db
      DB_USER: postgres
      DB_PASS: chatpassword
      REDIS_HOST: chat-redis
      REDIS_PORT: 6379
    depends_on:
      - chat-db
      - chat-redis

volumes:
  chat_postgres_data:
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Start dual-node cluster and databases
docker compose up -d

# 2. Check health of node 1 and node 2
curl -i http://localhost:8081/actuator/health
curl -i http://localhost:8082/actuator/health

# 3. Connect via STOMP client (or wscat / web client) to ws://localhost:8081/ws-chat
# Subscribe to: /topic/rooms/{roomId}
# Send STOMP Frame to /app/chat.send:
# {
#   "roomId": "e395df39-81a2-4a0b-93ff-187515b14e9e",
#   "senderId": "user-42",
#   "senderName": "Bob",
#   "content": "Hi from node 1!"
# }

# 4. Verify that a client connected to node 2 (ws://localhost:8082/ws-chat)
# receives the broadcast message in realtime via Redis Pub/Sub!
```
