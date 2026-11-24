# Real-Time Messaging with WebSockets and STOMP

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `spring-boot-starter-websocket`, `SimpMessagingTemplate` |

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
Traditional HTTP is fundamentally a request-response, half-duplex protocol initiated exclusively by the client. Building collaborative web applications (such as live trading dashboards, real-time chats, multiplayer gaming, or cooperative document editing) using HTTP polling wastes massive server resources and creates intolerable latency. Long-polling and HTTP streaming partially mitigate this, but still suffer from high header overhead (sending full cookie and header sets on every poll) and connection management complexity.

### The Evolution
The WebSocket protocol (RFC 6455) provides full-duplex, bidirectional, persistent communication channels over a single TCP connection. After an initial HTTP handshake (`Upgrade: websocket`), the connection transitions to lightweight binary or text framing. Because raw WebSockets lack application-level semantics (like message routing, topics, or subscriptions), Spring integrates **STOMP (Simple Text Oriented Messaging Protocol)** as a sub-protocol, providing standard frames (`CONNECT`, `SUBSCRIBE`, `SEND`, `MESSAGE`) and routing mechanisms.

### Relevance in Modern Engineering
Spring Boot provides enterprise-grade WebSocket support. Developers can use the lightweight in-memory simple broker for single-node setups or seamlessly delegate to full external message brokers (RabbitMQ, ActiveMQ) for horizontally clustered production deployments, complete with security, session management, and user-targeted messaging.

---

## 2. Core Concepts

- **WebSocket Protocol (RFC 6455)**: A persistent TCP connection upgraded from HTTP/1.1 allowing simultaneous two-way message exchange with minimal framing overhead (2-10 bytes per frame).
- **STOMP Sub-Protocol**: A text-based framing protocol operating over WebSockets defining message destinations (e.g., `/app/chat`, `/topic/orders`), subscription commands, and headers.
- **Message Broker**:
  - *Simple In-Memory Broker*: Built-in Spring broker suitable for single-node development (`enableSimpleBroker("/topic", "/queue")`).
  - *External Broker Relay (StompBrokerRelay)*: Delegates message distribution to a dedicated enterprise broker (like RabbitMQ) over TCP port 61613 for multi-node clustering.
- **`@MessageMapping`**: Annotation routing inbound STOMP messages to controller handler methods.
- **`@SendTo` / `@SendToUser`**: Directs method return values to a public topic or private user-specific destination queue.
- **`SimpMessagingTemplate`**: Spring bean used to programmatically send messages from any service or event listener to WebSocket destinations.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Spring STOMP over WebSocket Flow                           |
|                                                                                   |
| 1. HTTP Upgrade Handshake                                                         |
|    GET /ws-connect (Headers: Upgrade: websocket, Connection: Upgrade)             |
|                   | HandshakeHandler validates Origin & authenticates JWT         |
|                   v                                                               |
| 2. STOMP CONNECT Frame                                                            |
|    CONNECT / host: example.com / passcode: token                                 |
|                   | ChannelInterceptor intercepts inbound client channel          |
|                   v                                                               |
| 3. Message Routing Architecture                                                   |
|    +---------------------------------------------------------------------+        |
|    |                      ClientInboundChannel                           |        |
|    +---------------------------------------------------------------------+        |
|           |                                                      |                |
|           v (Destination: /app/**)                               v (Dest: /topic) |
|    +-----------------------------+                +-----------------------------+ |
|    | @MessageMapping Controller  |                | Message Broker (In-Memory   | |
|    | (Business Logic Processing) |                | or RabbitMQ Stomp Relay)    | |
|    +-----------------------------+                +-----------------------------+ |
|           |                                                      |                |
|           +-----------------> SimpMessagingTemplate ------------+                |
|                                                                  |                |
|                                                                  v                |
|    +---------------------------------------------------------------------+        |
|    |                     ClientOutboundChannel                           |        |
|    +---------------------------------------------------------------------+        |
|                                   |                                               |
|                                   v                                               |
| 4. Outbound STOMP MESSAGE Frame Delivered to Subscribed Browsers                  |
+-----------------------------------------------------------------------------------+
```

During initialization, Spring configures two primary channels: `ClientInboundChannel` and `ClientOutboundChannel`. When a browser sends a STOMP message, it travels through `ClientInboundChannel`. If the destination starts with `/app`, Spring's annotation method handler dispatches it to a `@MessageMapping` controller. If the destination starts with `/topic` or `/queue`, the message routes directly to the message broker. The broker distributes frames across `ClientOutboundChannel` to all active WebSocket sessions subscribed to that destination.

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-websocket</artifactId>
</dependency>
```

### Essential WebSocket Configuration
```java
package com.example.platform.websocket.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.messaging.simp.config.MessageBrokerRegistry;
import org.springframework.web.socket.config.annotation.EnableWebSocketMessageBroker;
import org.springframework.web.socket.config.annotation.StompEndpointRegistry;
import org.springframework.web.socket.config.annotation.WebSocketMessageBrokerConfigurer;

@Configuration
@EnableWebSocketMessageBroker
public class WebSocketConfig implements WebSocketMessageBrokerConfigurer {

    @Override
    public void configureMessageBroker(MessageBrokerRegistry config) {
        // Prefix for destinations handled by message broker
        config.enableSimpleBroker("/topic", "/queue");
        // Prefix for destinations routed to @MessageMapping controllers
        config.setApplicationDestinationPrefixes("/app");
        // Prefix for user-specific private messaging
        config.setUserDestinationPrefix("/user");
    }

    @Override
    public void registerStompEndpoints(StompEndpointRegistry registry) {
        // WebSocket connection endpoint with SockJS fallback support
        registry.addEndpoint("/ws")
                .setAllowedOriginPatterns("https://*.example.com", "http://localhost:3000")
                .withSockJS();
    }
}
```

---

## 5. Practical Examples

### Example 1: Chat and Broadcast Controller
```java
package com.example.platform.websocket.controller;

import org.springframework.messaging.handler.annotation.DestinationVariable;
import org.springframework.messaging.handler.annotation.MessageMapping;
import org.springframework.messaging.handler.annotation.Payload;
import org.springframework.messaging.handler.annotation.SendTo;
import org.springframework.stereotype.Controller;

import java.time.Instant;

public record ChatMessage(String sender, String content, String timestamp) {}

@Controller
public class LiveChatController {

    @MessageMapping("/rooms/{roomId}/send")
    @SendTo("/topic/rooms/{roomId}")
    public ChatMessage broadcastToRoom(
            @DestinationVariable String roomId,
            @Payload ChatMessage message) {
        
        return new ChatMessage(
                message.sender(),
                message.content(),
                Instant.now().toString()
        );
    }
}
```

### Example 2: Targeted Private Messaging with `SimpMessagingTemplate`
```java
package com.example.platform.websocket.service;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.stereotype.Service;

import java.time.Instant;

public record OrderNotification(String orderId, String status, String timestamp) {}

@Service
public class OrderNotificationDispatcher {

    private static final Logger log = LoggerFactory.getLogger(OrderNotificationDispatcher.class);
    private final SimpMessagingTemplate messagingTemplate;

    public OrderNotificationDispatcher(SimpMessagingTemplate messagingTemplate) {
        this.messagingTemplate = messagingTemplate;
    }

    public void notifyUserOfOrderStatus(String username, String orderId, String status) {
        log.info("Sending private WebSocket notification to user: {} for order: {}", username, orderId);

        OrderNotification notification = new OrderNotification(orderId, status, Instant.now().toString());

        // Routes to /user/{username}/queue/notifications
        messagingTemplate.convertAndSendToUser(
                username,
                "/queue/notifications",
                notification
        );
    }
}
```

### Example 3: External Message Broker Relay Configuration (RabbitMQ for Production)
```java
package com.example.platform.websocket.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.messaging.simp.config.MessageBrokerRegistry;
import org.springframework.web.socket.config.annotation.EnableWebSocketMessageBroker;
import org.springframework.web.socket.config.annotation.StompEndpointRegistry;
import org.springframework.web.socket.config.annotation.WebSocketMessageBrokerConfigurer;

@Configuration
@EnableWebSocketMessageBroker
public class ProductionRabbitMqWebSocketConfig implements WebSocketMessageBrokerConfigurer {

    @Override
    public void configureMessageBroker(MessageBrokerRegistry registry) {
        // Delegate message broadcasting to clustered RabbitMQ broker over STOMP
        registry.enableStompBrokerRelay("/topic", "/queue")
                .setRelayHost("rabbitmq.internal.example.com")
                .setRelayPort(61613)
                .setClientLogin("stomp_user")
                .setClientPasscode("stomp_secret")
                .setSystemLogin("stomp_admin")
                .setSystemPasscode("stomp_admin_secret");

        registry.setApplicationDestinationPrefixes("/app");
        registry.setUserDestinationPrefix("/user");
    }

    @Override
    public void registerStompEndpoints(StompEndpointRegistry registry) {
        registry.addEndpoint("/ws").setAllowedOrigins("https://app.example.com");
    }
}
```

### Example 4: Unauthenticated WebSocket Connections (Anti-Pattern Fix)
```java
// ANTI-PATTERN: Relying only on standard HTTP session security during WebSocket handshake,
// failing to validate JWT bearer tokens sent in STOMP CONNECT frame headers.

// FIX: Intercept the ClientInboundChannel and authenticate during the STOMP CONNECT phase
package com.example.platform.websocket.security;

import org.springframework.context.annotation.Configuration;
import org.springframework.core.Ordered;
import org.springframework.core.annotation.Order;
import org.springframework.messaging.Message;
import org.springframework.messaging.MessageChannel;
import org.springframework.messaging.simp.config.ChannelRegistration;
import org.springframework.messaging.simp.stomp.StompCommand;
import org.springframework.messaging.simp.stomp.StompHeaderAccessor;
import org.springframework.messaging.support.ChannelInterceptor;
import org.springframework.messaging.support.MessageHeaderAccessor;
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.web.socket.config.annotation.WebSocketMessageBrokerConfigurer;

import java.security.Principal;
import java.util.List;

@Configuration
@Order(Ordered.HIGHEST_PRECEDENCE + 99)
public class WebSocketSecurityChannelConfig implements WebSocketMessageBrokerConfigurer {

    @Override
    public void configureClientInboundChannel(ChannelRegistration registration) {
        registration.interceptors(new ChannelInterceptor() {
            @Override
            public Message<?> preSend(Message<?> message, MessageChannel channel) {
                StompHeaderAccessor accessor =
                        MessageHeaderAccessor.getAccessor(message, StompHeaderAccessor.class);

                if (accessor != null && StompCommand.CONNECT.equals(accessor.getCommand())) {
                    String authHeader = accessor.getFirstNativeHeader("Authorization");
                    if (authHeader != null && authHeader.startsWith("Bearer ")) {
                        String token = authHeader.substring(7);
                        // Validate JWT and extract subject
                        String username = "user_from_jwt"; // Simulated validation
                        Principal principal = new UsernamePasswordAuthenticationToken(
                                username, null, List.of(new SimpleGrantedAuthority("ROLE_USER")));
                        accessor.setUser(principal);
                    } else {
                        throw new IllegalArgumentException("Missing or invalid Authorization header in STOMP CONNECT");
                    }
                }
                return message;
            }
        });
    }
}
```

### Example 5: High-Performance WebSocket Session Life-Cycle Listener
```java
package com.example.platform.websocket.listener;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;
import org.springframework.web.socket.messaging.SessionConnectedEvent;
import org.springframework.web.socket.messaging.SessionDisconnectEvent;
import org.springframework.web.socket.messaging.SessionSubscribeEvent;

import java.util.concurrent.atomic.AtomicInteger;

@Component
public class WebSocketSessionEventListener {

    private static final Logger log = LoggerFactory.getLogger(WebSocketSessionEventListener.class);
    private final AtomicInteger activeConnections = new AtomicInteger(0);

    @EventListener
    public void handleSessionConnected(SessionConnectedEvent event) {
        int count = activeConnections.incrementAndGet();
        log.info("Client connected. Current active WebSocket sessions: {}", count);
    }

    @EventListener
    public void handleSessionSubscribe(SessionSubscribeEvent event) {
        log.info("Client subscribed to destination: {}",
                event.getMessage().getHeaders().get("simpDestination"));
    }

    @EventListener
    public void handleSessionDisconnect(SessionDisconnectEvent event) {
        int count = activeConnections.decrementAndGet();
        log.info("Client disconnected. SessionId: {}. Active sessions: {}",
                event.getSessionId(), count);
    }

    public int getActiveConnectionCount() {
        return activeConnections.get();
    }
}
```

---

## 6. Best Practices

1. **Use External Message Broker Relay in Multi-Node Clusters**: Never use `enableSimpleBroker` in a horizontally scaled cluster; memory brokers cannot forward messages across server instances without an external broker relay (RabbitMQ).
2. **Authenticate at STOMP CONNECT Frame**: Validate JWT bearer tokens directly inside the STOMP `CONNECT` frame via a `ChannelInterceptor` rather than only at the HTTP handshake.
3. **Restrict Allowed Origins**: Explicitly configure `setAllowedOriginPatterns` on WebSocket endpoints to prevent Cross-Site WebSocket Hijacking (CSWSH).
4. **Use SockJS as Fallback**: Provide `.withSockJS()` to ensure browser clients behind strict enterprise firewalls can fallback to HTTP long-polling seamlessly.
5. **Enforce Send Buffer Size and Inactivity Timeouts**: Configure `WebSocketTransportRegistration` with strict message size limits (e.g., 64KB) and send buffer limits to prevent slow consumers from exhausting server heap.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Using `enableSimpleBroker` in a multi-instance Kubernetes deployment | Messages sent to Instance A are invisible to clients connected to Instance B | Switch to `enableStompBrokerRelay` connected to RabbitMQ or ActiveMQ. |
| Forgetting to configure CORS / Allowed Origins | Browsers block the WebSocket connection due to CORS origin violation | Configure `.setAllowedOriginPatterns(...)` in `registerStompEndpoints()`. |
| Passing unauthenticated `Principal` to `@SendToUser` | Spring cannot resolve `/user/{username}/...`, dropping the message silently | Ensure `accessor.setUser(principal)` is assigned during STOMP CONNECT. |
| Sending multi-megabyte payloads over WebSockets | Saturation of WebSocket frames blocks other multiplexed messages | Upload large binaries to S3 and transmit only the URL reference over WebSockets. |
| Catching exceptions inside `@MessageMapping` without notification | Client UI hangs waiting for response with no indication of error | Use `@MessageExceptionHandler` to route error messages back to user queues. |
| Not handling client heartbeats | Dead connections remain open indefinitely on the server, leaking sockets | Configure STOMP heartbeats via `registry.enableSimpleBroker().setHeartbeatValue(...)`. |

---

## 8. Performance & Security Notes

- **CSWSH Defense (Cross-Site WebSocket Hijacking)**: WebSockets do not adhere to the Same-Origin Policy by default during the HTTP handshake. Validate the `Origin` header rigorously in `registerStompEndpoints()`.
- **Memory Consumption**: Each open WebSocket session maintains TCP buffers and session state. A server with 100,000 idle connections requires careful OS file descriptor (`ulimit -n`) and JVM heap tuning.
- **Backpressure & Slow Consumers**: Configure `setSendBufferSizeLimit` and `setSendTimeLimit` in `WebSocketTransportRegistration` to automatically disconnect slow clients that fail to consume messages.

---

## 9. Comparison / When to Use What

| Feature | WebSockets + STOMP | Server-Sent Events (SSE) | Long Polling |
|---|---|---|---|
| **Directionality** | Full Duplex (Bidirectional) | Half Duplex (Server-to-Client) | Half Duplex (Client-to-Server) |
| **Protocol** | WebSocket (ws:// / wss://) | Standard HTTP (text/event-stream) | Standard HTTP |
| **Framing Overhead** | Extremely low (2-10 bytes) | Low (data: prefix per event) | High (full HTTP headers) |
| **Reconnection** | Requires custom client code | Built-in browser auto-reconnect | Handled by application loops |
| **Best For** | Chat, collaborative editing, gaming | Live stock tickers, progress bars | Legacy environments |

---

## 10. Interview Questions

<details>
<summary>1. Why do enterprise applications use STOMP over raw WebSockets?</summary>
Raw WebSockets only establish a persistent stream of raw bytes or text frames without any higher-level protocol semantics. STOMP introduces standardized command frames (`CONNECT`, `SEND`, `SUBSCRIBE`, `UNSUBSCRIBE`), destination headers (`/topic`, `/queue`), and message routing, enabling seamless integration with enterprise message brokers and Spring controllers.
</details>

<details>
<summary>2. Why does the simple in-memory broker fail in a clustered multi-node deployment?</summary>
The simple broker (`enableSimpleBroker`) stores subscription lists and active client sessions exclusively in JVM memory. When an application scales horizontally, a message broadcast from Node 1 is only delivered to clients connected to Node 1; clients connected to Node 2 or Node 3 never receive it. Clustered environments require an external broker relay (like RabbitMQ).
</details>

<details>
<summary>3. How does Spring resolve user-specific destinations when using @SendToUser or convertAndSendToUser?</summary>
When a client subscribes to `/user/queue/notifications`, Spring's `UserDestinationMessageHandler` translates this destination using the authenticated user's `Principal.getName()` into a unique private session destination (e.g., `/queue/notifications-user123-session456`).
</details>

<details>
<summary>4. What is Cross-Site WebSocket Hijacking (CSWSH) and how is it prevented in Spring?</summary>
CSWSH occurs when a malicious website opens a WebSocket connection to a vulnerable backend using the victim's ambient browser cookies. Because WebSockets bypass standard SOP, the connection succeeds unless the server validates the `Origin` header during the HTTP handshake. Spring defends against this via `setAllowedOrigins()` or `setAllowedOriginPatterns()`.
</details>

<details>
<summary>5. How can you authenticate a client connecting via STOMP over WebSockets?</summary>
The recommended approach is to transmit the JWT access token in the STOMP `CONNECT` frame header (`Authorization: Bearer <token>`). A registered `ChannelInterceptor` on `ClientInboundChannel` intercepts the frame, parses and validates the JWT, and sets an authenticated `Principal` on `StompHeaderAccessor.setUser()`.
</details>

<details>
<summary>6. What is the role of SockJS in Spring WebSocket architecture?</summary>
SockJS is a client-side JavaScript library and server-side protocol that provides a fallback mechanism for environments where native WebSockets are blocked by restrictive enterprise proxies, firewalls, or legacy browsers. It emulates the WebSocket API using HTTP streaming, long-polling, or iframe techniques.
</details>

<details>
<summary>7. How does Spring handle slow consumers that do not read WebSocket messages quickly?</summary>
Spring provides `WebSocketTransportRegistration` where developers can configure `setSendBufferSizeLimit` (maximum byte buffer per session) and `setSendTimeLimit` (maximum time allowed for sending). If a slow consumer exceeds these limits, Spring terminates the session to protect the server from OutOfMemoryErrors.
</details>

<details>
<summary>8. What is the difference between @MessageMapping and @SubscribeMapping?</summary>
`@MessageMapping` is invoked when a client *sends* a message to a destination. `@SubscribeMapping` is invoked when a client *subscribes* to a destination, typically used to return an initial one-time state snapshot directly to that subscribing client without broadcasting it to other subscribers.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure standard WebSocket STOMP endpoint</summary>
Write a `@Configuration` class implementing `WebSocketMessageBrokerConfigurer` that enables a simple broker on `/topic` and registers `/ws-app` with SockJS fallback.
</details>

<details>
<summary>Exercise 2 (Easy): Build a broadcast controller</summary>
Implement a `@Controller` method annotated with `@MessageMapping("/broadcast")` and `@SendTo("/topic/announcements")` that transforms an incoming message string to uppercase.
</details>

<details>
<summary>Exercise 3 (Medium): Send private notifications to a specific user</summary>
Write a Spring service that uses `SimpMessagingTemplate` to send an order status update to a single user destination `/queue/orders`.
</details>

<details>
<summary>Exercise 4 (Medium): Intercept STOMP CONNECT frame for authentication</summary>
Implement a `ChannelInterceptor` on `ClientInboundChannel` that extracts an API key header from the STOMP `CONNECT` frame and rejects the connection if invalid.
</details>

<details>
<summary>Exercise 5 (Hard): Configure RabbitMQ Stomp Broker Relay with failover</summary>
Configure `StompBrokerRelay` pointing to an external RabbitMQ cluster with heartbeats enabled (10s send/receive) and system/client credentials.
</details>

<details>
<summary>Exercise 6 (Hard): Build an active user presence tracker</summary>
Implement event listeners for `SessionConnectedEvent`, `SessionDisconnectEvent`, and `SessionSubscribeEvent` that maintain an active online user roster stored in Redis.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                       Spring WebSocket & STOMP Summary                          |
+---------------------------------------------------------------------------------+
| Dependency:         spring-boot-starter-websocket                               |
| Config Interface:   WebSocketMessageBrokerConfigurer                            |
| Handshake Endpoint: registry.addEndpoint("/ws").withSockJS()                    |
| Destination Route:  config.setApplicationDestinationPrefixes("/app")           |
| Broker Channels:    config.enableSimpleBroker("/topic", "/queue")               |
| Production Broker:  config.enableStompBrokerRelay("/topic", "/queue") [RabbitMQ]|
| Controller:         @MessageMapping("/dest") + @SendTo("/topic/dest")           |
| Programmatic Send:  simpMessagingTemplate.convertAndSend("/topic/dest", body)   |
| Private Send:       simpMessagingTemplate.convertAndSendToUser(user, "/queue", b)|
| Security Invariant: Authenticate in STOMP CONNECT frame via ChannelInterceptor   |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring WebSocket Reference](https://docs.spring.io/spring-framework/reference/web/websocket.html)
- [RFC 6455: The WebSocket Protocol](https://datatracker.ietf.org/doc/html/rfc6455)
- [STOMP Protocol Specification (v1.2)](https://stomp.github.io/stomp-specification-1.2.html)
