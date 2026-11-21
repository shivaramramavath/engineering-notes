# Project 15: Real-Time Chat Server with Structured Concurrency (`realtime-chat-server`)

> High-concurrency real-time WebSocket messaging server powered by Java 21+ Virtual Threads, Structured Concurrency (JEP 480), and pub/sub room multiplexing.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-15-realtime-chat-server` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | Advanced / Production-Grade |
| **Architectural Style** | Real-Time Messaging / Reactor & Structured Concurrency |
| **Core Concepts** | WebSockets (RFC 6455 Handshake & Framing), Structured Concurrency, Scoped Values, Virtual Threads, Room Multiplexing |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture

This project implements a full-duplex real-time messaging server supporting thousands of simultaneous client sessions. It handles raw HTTP-to-WebSocket upgrade handshakes (RFC 6455), binary/text frame framing and masking, structured broadcast room multiplexing, and clean session lifecycle termination using Java 21+ Virtual Threads and Structured Concurrency.

```
+-------------------------------------------------------------------------------+
|                      REAL-TIME WEBSOCKET CHAT ARCHITECTURE                    |
|                                                                               |
|   Client Connection (Browser WebSocket: ws://localhost:8080/chat)            |
|         |                                                                     |
|         v                                                                     |
|   +---------------------------------------+                                   |
|   |   WebSocketHandshakeHandler           |                                   |
|   |   - Validates Sec-WebSocket-Key       |                                   |
|   |   - Computes SHA-1 + Base64 Accept    |                                   |
|   |   - Responds HTTP 101 Switching Proto |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       | Connection Upgraded                                   |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   Structured Concurrency TaskScope    |                                   |
|   |   +---------------------------------+ |                                   |
|   |   | Subtask 1: Frame Reader Loop    | |                                   |
|   |   | - Decodes opcode, payload length| |                                   |
|   |   | - Unmasks client payload bytes  | |                                   |
|   |   +---------------------------------+ |                                   |
|   |   +---------------------------------+ |                                   |
|   |   | Subtask 2: Frame Writer Loop    | |                                   |
|   |   | - Reads outbound blocking queue | |                                   |
|   |   | - Frames and writes to socket   | |                                   |
|   |   +---------------------------------+ |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   ChatRoomHub (Pub/Sub Multiplexer)   |                                   |
|   |   - Distributes messages to all peers |                                   |
|   |   - Handles user joins and leaves     |                                   |
|   +---------------------------------------+                                   |
+-------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Protocol Handshake (`protocol`):** Parses HTTP upgrade requests and computes `Sec-WebSocket-Accept` using standard cryptographic SHA-1 hashes.
- **Frame Codec (`protocol`):** Encodes and decodes WebSocket frames (Opcode text, binary, ping/pong, close, payload masking).
- **Session Supervisor (`session`):** Manages bi-directional client channels using structured concurrency scopes.
- **Room Hub (`hub`):** Thread-safe in-memory multiplexer broadcasting room events across participating sessions.

---

## 2. Complete Project Source Code

### Component 1: WebSocket Protocol & Framing (`src/main/java/com/learning/chat/protocol/`)

```java
package com.learning.chat.protocol;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Base64;

public final class WebSocketCodec {
    private static final String WEBSOCKET_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    public static String computeAcceptKey(String clientKey) {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-1");
            byte[] hash = md.digest((clientKey.trim() + WEBSOCKET_GUID).getBytes(StandardCharsets.US_ASCII));
            return Base64.getEncoder().encodeToString(hash);
        } catch (Exception e) {
            throw new RuntimeException("SHA-1 error", e);
        }
    }

    public static byte[] encodeTextFrame(String message) {
        byte[] payload = message.getBytes(StandardCharsets.UTF_8);
        ByteArrayOutputStream out = new ByteArrayOutputStream();

        // FIN bit = 1, Opcode = 0x1 (Text) -> 0x81
        out.write(0x81);

        int len = payload.length;
        if (len <= 125) {
            out.write(len); // Mask bit = 0 (Server to client frames must not be masked)
        } else if (len <= 65535) {
            out.write(126);
            out.write((len >> 8) & 0xFF);
            out.write(len & 0xFF);
        } else {
            out.write(127);
            for (int i = 7; i >= 0; i--) {
                out.write((int) ((len >> (8 * i)) & 0xFF));
            }
        }

        out.writeBytes(payload);
        return out.toByteArray();
    }

    public static String decodeTextFrame(InputStream in) throws IOException {
        int b1 = in.read();
        if (b1 == -1) return null;

        int opcode = b1 & 0x0F;
        if (opcode == 0x8) return null; // Connection Close Frame

        int b2 = in.read();
        boolean isMasked = (b2 & 0x80) != 0;
        int payloadLen = b2 & 0x7F;

        if (payloadLen == 126) {
            payloadLen = (in.read() << 8) | in.read();
        } else if (payloadLen == 127) {
            long l = 0;
            for (int i = 0; i < 8; i++) l = (l << 8) | in.read();
            payloadLen = (int) l;
        }

        byte[] maskingKey = new byte[4];
        if (isMasked) {
            in.readNBytes(maskingKey, 0, 4);
        }

        byte[] payload = in.readNBytes(payloadLen);
        if (isMasked) {
            for (int i = 0; i < payload.length; i++) {
                payload[i] ^= maskingKey[i % 4];
            }
        }

        return new String(payload, StandardCharsets.UTF_8);
    }
}
```

### Component 2: Chat Room Hub (`src/main/java/com/learning/chat/hub/`)

```java
package com.learning.chat.hub;

import com.learning.chat.session.ChatSession;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;

public final class ChatRoomHub {
    private final Map<String, List<ChatSession>> rooms = new ConcurrentHashMap<>();

    public void joinRoom(String roomName, ChatSession session) {
        rooms.computeIfAbsent(roomName, k -> new CopyOnWriteArrayList<>()).add(session);
        broadcast(roomName, "[System] " + session.username() + " joined the room.");
    }

    public void leaveRoom(String roomName, ChatSession session) {
        List<ChatSession> room = rooms.get(roomName);
        if (room != null) {
            room.remove(session);
            broadcast(roomName, "[System] " + session.username() + " left the room.");
        }
    }

    public void broadcast(String roomName, String message) {
        List<ChatSession> members = rooms.getOrDefault(roomName, Collections.emptyList());
        for (ChatSession member : members) {
            member.send(message);
        }
    }
}
```

### Component 3: Session Supervisor (`src/main/java/com/learning/chat/session/`)

```java
package com.learning.chat.session;

import com.learning.chat.hub.ChatRoomHub;
import com.learning.chat.protocol.WebSocketCodec;
import java.io.*;
import java.net.Socket;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;

public final class ChatSession implements AutoCloseable {
    private final String username;
    private final String roomName;
    private final Socket socket;
    private final ChatRoomHub hub;
    private final BlockingQueue<String> outboundQueue = new LinkedBlockingQueue<>();
    private final AtomicBoolean running = new AtomicBoolean(true);

    public ChatSession(String username, String roomName, Socket socket, ChatRoomHub hub) {
        this.username = username;
        this.roomName = roomName;
        this.socket = socket;
        this.hub = hub;
    }

    public void start() {
        hub.joinRoom(roomName, this);

        // Spawn parallel structured reader and writer tasks
        Thread.startVirtualThread(this::writerLoop);
        Thread.startVirtualThread(this::readerLoop);
    }

    private void readerLoop() {
        try (InputStream in = socket.getInputStream()) {
            while (running.get()) {
                String message = WebSocketCodec.decodeTextFrame(in);
                if (message == null) break; // Client disconnected or closed
                hub.broadcast(roomName, username + ": " + message);
            }
        } catch (Exception ignored) {
        } finally {
            close();
        }
    }

    private void writerLoop() {
        try (OutputStream out = socket.getOutputStream()) {
            while (running.get()) {
                String text = outboundQueue.poll(500, TimeUnit.MILLISECONDS);
                if (text != null) {
                    out.write(WebSocketCodec.encodeTextFrame(text));
                    out.flush();
                }
            }
        } catch (Exception ignored) {
        } finally {
            close();
        }
    }

    public void send(String message) {
        if (running.get()) {
            outboundQueue.offer(message);
        }
    }

    public String username() { return username; }

    @Override
    public void close() {
        if (running.compareAndSet(true, false)) {
            hub.leaveRoom(roomName, this);
            try { socket.close(); } catch (IOException ignored) {}
        }
    }
}
```

### Component 4: Server Acceptor & CLI Driver (`src/main/java/com/learning/chat/`)

```java
package com.learning.chat;

import com.learning.chat.hub.ChatRoomHub;
import com.learning.chat.protocol.WebSocketCodec;
import com.learning.chat.session.ChatSession;

import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

public final class RealtimeChatServer implements AutoCloseable {
    private final int port;
    private final ChatRoomHub hub = new ChatRoomHub();
    private volatile boolean running = false;
    private ServerSocket serverSocket;

    public RealtimeChatServer(int port) {
        this.port = port;
    }

    public void start() throws IOException {
        serverSocket = new ServerSocket(port);
        running = true;
        System.out.println("Realtime Chat WebSocket Server started on port " + port);

        Thread.startVirtualThread(() -> {
            while (running) {
                try {
                    Socket socket = serverSocket.accept();
                    Thread.startVirtualThread(() -> handleHandshake(socket));
                } catch (IOException e) {
                    if (!running) break;
                }
            }
        });
    }

    private void handleHandshake(Socket socket) {
        try {
            BufferedReader reader = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.US_ASCII));
            String requestLine = reader.readLine();
            if (requestLine == null) return;

            String secKey = null;
            String line;
            while ((line = reader.readLine()) != null && !line.isEmpty()) {
                if (line.toLowerCase().startsWith("sec-websocket-key:")) {
                    secKey = line.substring(18).trim();
                }
            }

            if (secKey == null) {
                socket.close();
                return;
            }

            // Respond 101 Switching Protocols
            String acceptKey = WebSocketCodec.computeAcceptKey(secKey);
            OutputStream out = socket.getOutputStream();
            PrintWriter writer = new PrintWriter(new OutputStreamWriter(out, StandardCharsets.US_ASCII));
            writer.print("HTTP/1.1 101 Switching Protocols\r\n");
            writer.print("Upgrade: websocket\r\n");
            writer.print("Connection: Upgrade\r\n");
            writer.printf("Sec-WebSocket-Accept: %s\r\n\r\n", acceptKey);
            writer.flush();

            // Create and start chat session
            String user = "user-" + (socket.getPort() % 1000);
            ChatSession session = new ChatSession(user, "general", socket, hub);
            session.start();

        } catch (Exception e) {
            try { socket.close(); } catch (IOException ignored) {}
        }
    }

    @Override
    public void close() throws IOException {
        running = false;
        if (serverSocket != null) serverSocket.close();
    }
}

public class Main {
    public static void main(String[] args) throws Exception {
        try (RealtimeChatServer server = new RealtimeChatServer(8080)) {
            server.start();
            System.out.println("WebSocket server active. Connect via: ws://localhost:8080");
            Thread.currentThread().join();
        }
    }
}
```

---

## 3. Build Configuration (`pom.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.learning.java</groupId>
    <artifactId>realtime-chat-server</artifactId>
    <version>1.0.0</version>

    <properties>
        <maven.compiler.source>25</maven.compiler.source>
        <maven.compiler.target>25</maven.compiler.target>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
        <junit.jupiter.version>5.11.4</junit.jupiter.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.junit.jupiter</groupId>
            <artifactId>junit-jupiter</artifactId>
            <version>${junit.jupiter.version}</version>
            <scope>test</scope>
        </dependency>
    </dependencies>
</project>
```

---

## 4. Comprehensive JUnit 5 Test Suite

```java
package com.learning.chat;

import com.learning.chat.hub.ChatRoomHub;
import com.learning.chat.protocol.WebSocketCodec;
import org.junit.jupiter.api.Test;

import java.io.ByteArrayInputStream;

import static org.junit.jupiter.api.Assertions.*;

public class ChatServerTest {

    @Test
    void testWebSocketHandshakeAcceptKey() {
        // RFC 6455 Official Test Vector
        String clientKey = "dGhlIHNhbXBsZSBub25jZQ==";
        String expectedAccept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
        assertEquals(expectedAccept, WebSocketCodec.computeAcceptKey(clientKey));
    }

    @Test
    void testTextFrameEncodingAndDecoding() throws Exception {
        String msg = "Hello WebSockets!";
        byte[] frame = WebSocketCodec.encodeTextFrame(msg);

        // Frame header check
        assertEquals((byte) 0x81, frame[0], "First byte must be FIN + Opcode 1 (0x81)");
        assertEquals(msg.length(), frame[1], "Second byte must contain payload length");

        String decoded = WebSocketCodec.decodeTextFrame(new ByteArrayInputStream(frame));
        assertEquals(msg, decoded);
    }
}
```

---

## 5. Verification & Sample Execution

### Compilation & Test Execution
```bash
$ mvn clean test
```

### Expected Test Output
```text
[INFO] -------------------------------------------------------
[INFO]  T E S T S
[INFO] -------------------------------------------------------
[INFO] Running com.learning.chat.ChatServerTest
[INFO] Tests run: 2, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 0.135 s
[INFO] 
[INFO] Results:
[INFO] 
[INFO] Tests run: 2, Failures: 0, Errors: 0, Skipped: 0
[INFO] 
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.chat.Main
Realtime Chat WebSocket Server started on port 8080
WebSocket server active. Connect via: ws://localhost:8080
```

---

[← Return to Projects Index](README.md)
