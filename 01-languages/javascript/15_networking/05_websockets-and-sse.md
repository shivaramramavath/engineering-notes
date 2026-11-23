# Real-Time Networking: WebSockets & SSE

> Real-time communication architectures in JavaScript: bidirectional full-duplex messaging via WebSockets, unidirectional push streaming via Server-Sent Events (SSE), and modern WebTransport.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Event Loop](../12_event-loop/01_event-loop.md), [HTTP Fundamentals](./01_http-fundamentals.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | IETF RFC 6455 (WebSocket Protocol) / WHATWG HTML Server-Sent Events / W3C WebTransport |

---

## Table of Contents

1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Standard HTTP is fundamentally half-duplex and request-driven: the client initiates a request, and the server returns a response. For applications requiring instant server-to-client updates—such as financial trading terminals, collaborative document editors, live chat rooms, and multiplayer games—traditional HTTP polling (`setInterval` + `fetch`) is grossly inefficient. Polling introduces latency, wastes bandwidth by continuously retransmitting HTTP headers, and generates millions of useless empty requests.

The web platform provides two specialized real-time primitives:
1. **WebSockets (RFC 6455)**: A full-duplex, bidirectional, persistent TCP connection enabling simultaneous client $\leftrightarrow$ server message transmission with minimal frame overhead (2–10 bytes).
2. **Server-Sent Events (SSE)**: A lightweight, unidirectional server $\rightarrow$ client push stream operating over standard HTTP with automatic reconnection and native event ID tracking.

---

## 2. Core Concepts

### Architectural Comparison: WebSockets vs SSE vs Long Polling

| Feature | WebSockets (`WebSocket`) | Server-Sent Events (`EventSource`) | HTTP Long Polling |
| :--- | :--- | :--- | :--- |
| **Directionality** | Full-Duplex (Bidirectional) | Unidirectional (Server $\rightarrow$ Client) | Unidirectional (Simulated via repeated requests) |
| **Underlying Protocol**| TCP (Upgraded from HTTP/1.1) | Standard HTTP/1.1 or HTTP/2 | Standard HTTP/1.1 |
| **Data Format** | UTF-8 Text or Binary (Blob/ArrayBuffer) | Plaintext UTF-8 Stream (`text/event-stream`)| Arbitrary HTTP payloads (JSON, Text) |
| **Reconnection** | Must be hand-rolled in JavaScript | Built-in native auto-reconnection | Hand-rolled in JavaScript loop |
| **Header Overhead** | $\approx 2 - 10$ bytes per frame | Zero after initial HTTP handshake | Full HTTP request/response headers on every tick |
| **Firewall / Proxy** | May require special proxy configuration | Works through standard corporate proxies | Works everywhere standard HTTP works |

```text
WebSocket (Full-Duplex):
 Client <══════════════ [ Persistent TCP Socket ] ══════════════> Server
   (Client sends messages freely)            (Server pushes messages freely)

Server-Sent Events (Unidirectional Push):
 Client <──────────────── [ Open HTTP Stream ] ────────────────── Server
   (Client sends data via separate POST)     (Server pushes live events)
```

---

## 3. How It Works Internally

### 1. The WebSocket Opening Handshake
A WebSocket begins as a standard HTTP/1.1 request that requests a protocol upgrade:

```
 Client sends:
 GET /chat HTTP/1.1
 Host: server.example.com
 Upgrade: websocket
 Connection: Upgrade
 Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
 Sec-WebSocket-Version: 13
                  │
                  ▼
 Server verifies key, computes SHA-1 hash + GUID, and replies:
 HTTP/1.1 101 Switching Protocols
 Upgrade: websocket
 Connection: Upgrade
 Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
                  │
                  ▼
 [ Connection Upgraded: Raw TCP Socket switched to WebSocket Framing Protocol ]
```

### 2. SSE Stream Format
Server-Sent Events operate over a persistent HTTP connection returning `Content-Type: text/event-stream`. The server sends newline-delimited event blocks:

```text
id: 101\n
event: price-update\n
data: {"symbol": "NVDA", "price": 140.25}\n\n
```

---

## 4. Syntax & API Reference

### 1. WebSocket API

```javascript
// Connect to WebSocket server (use wss:// in production!)
const socket = new WebSocket('wss://api.example.com/live');

// Configure binary data format: 'blob' (default) or 'arraybuffer'
socket.binaryType = 'arraybuffer';

// Lifecycle Events
socket.onopen = (event) => {
  console.log('Connected! ReadyState:', socket.readyState);
  socket.send(JSON.stringify({ type: 'AUTH', token: 'jwt_123' }));
};

socket.onmessage = (event) => {
  if (typeof event.data === 'string') {
    const payload = JSON.parse(event.data);
    console.log('Received message:', payload);
  } else {
    // ArrayBuffer binary frame
    const buffer = new Uint8Array(event.data);
    console.log('Binary bytes received:', buffer.length);
  }
};

socket.onerror = (error) => console.error('Socket error:', error);

socket.onclose = (event) => {
  console.log(`Disconnected. Code: ${event.code}, Reason: ${event.reason}`);
};

// ReadyState Constants:
// 0: CONNECTING, 1: OPEN, 2: CLOSING, 3: CLOSED
```

### 2. Server-Sent Events (`EventSource`) API

```javascript
// Initialize unidirectional push connection
const eventSource = new EventSource('/api/live-stream', {
  withCredentials: true, // Send cookies cross-origin
});

// Default 'message' event listener
eventSource.onmessage = (event) => {
  console.log('Data:', JSON.parse(event.data));
  console.log('Last Event ID:', event.lastEventId);
};

// Custom named event listener (matches 'event: alert' from server)
eventSource.addEventListener('alert', (event) => {
  console.warn('Custom Alert:', event.data);
});

eventSource.onerror = (err) => {
  console.error('SSE Error. Browser will automatically retry reconnection...');
};

// Explicit termination
eventSource.close();
```

---

## 5. Practical Examples

### Example 1: Resilient WebSocket with Auto-Reconnect & Heartbeat (Ping/Pong)
```javascript
class RobustWebSocket {
  #url;
  #socket = null;
  #heartbeatInterval = null;
  #reconnectAttempts = 0;
  #maxReconnectDelay = 30000;
  #isTerminated = false;

  constructor(url) {
    this.#url = url;
    this.#connect();
  }

  #connect() {
    if (this.#isTerminated) return;

    this.#socket = new WebSocket(this.#url);

    this.#socket.onopen = () => {
      console.log('[WS] Connected successfully');
      this.#reconnectAttempts = 0;
      this.#startHeartbeat();
    };

    this.#socket.onmessage = (e) => {
      if (e.data === 'pong') return; // Heartbeat response
      this.onMessage?.(JSON.parse(e.data));
    };

    this.#socket.onclose = () => {
      this.#stopHeartbeat();
      if (!this.#isTerminated) {
        this.#scheduleReconnect();
      }
    };
  }

  #startHeartbeat() {
    this.#heartbeatInterval = setInterval(() => {
      if (this.#socket?.readyState === WebSocket.OPEN) {
        this.#socket.send('ping');
      }
    }, 25000);
  }

  #stopHeartbeat() {
    clearInterval(this.#heartbeatInterval);
  }

  #scheduleReconnect() {
    this.#reconnectAttempts++;
    const delay = Math.min(this.#maxReconnectDelay, 1000 * 2 ** this.#reconnectAttempts);
    console.log(`[WS] Reconnecting in ${delay}ms (Attempt ${this.#reconnectAttempts})...`);
    setTimeout(() => this.#connect(), delay);
  }

  send(data) {
    if (this.#socket?.readyState === WebSocket.OPEN) {
      this.#socket.send(JSON.stringify(data));
    } else {
      console.warn('[WS] Cannot send: Socket not open');
    }
  }

  close() {
    this.#isTerminated = true;
    this.#stopHeartbeat();
    this.#socket?.close();
  }
}
```

### Example 2: Node.js SSE Stream Producer (Vanilla HTTP)
```javascript
import http from 'node:http';

http.createServer((req, res) => {
  if (req.url === '/events') {
    // 1. Send SSE Headers
    res.writeHead(200, {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache',
      'Connection': 'keep-alive',
      'Access-Control-Allow-Origin': '*',
    });

    let messageId = 0;

    // 2. Stream events periodically
    const timer = setInterval(() => {
      messageId++;
      const payload = JSON.stringify({ cpu: Math.random() * 100 });

      // SSE Wire Format: id: <id>\nevent: <name>\ndata: <json>\n\n
      res.write(`id: ${messageId}\n`);
      res.write(`event: metric\n`);
      res.write(`data: ${payload}\n\n`);
    }, 2000);

    // 3. Clean up when client disconnects
    req.on('close', () => {
      clearInterval(timer);
      console.log('Client closed SSE connection');
    });
  }
}).listen(5000);
```

### Example 3: Consuming Binary WebSockets with `ArrayBuffer`
```javascript
function setupAudioStreamSocket(wsUrl, audioContext) {
  const socket = new WebSocket(wsUrl);
  socket.binaryType = 'arraybuffer'; // Raw binary frames

  socket.onmessage = async (event) => {
    if (event.data instanceof ArrayBuffer) {
      // Decode raw PCM / WAV audio buffer
      const audioBuffer = await audioContext.decodeAudioData(event.data);
      playBuffer(audioBuffer, audioContext);
    }
  };
}

function playBuffer(buffer, ctx) {
  const source = ctx.createBufferSource();
  source.buffer = buffer;
  source.connect(ctx.destination);
  source.start();
}
```

### Example 4: Handling SSE Automatic Reconnection with `Last-Event-ID`
```javascript
// When EventSource reconnects after a dropout, it automatically transmits the
// 'Last-Event-ID' HTTP header containing the last received 'id:' field!
// The server reads this header to replay missed events seamlessly.

// Server implementation:
function handleSseConnection(req, res) {
  const lastId = req.headers['last-event-id'];
  if (lastId) {
    console.log(`Client reconnected. Replaying missed events starting after ID: ${lastId}`);
    replayEventsFrom(parseInt(lastId, 10), res);
  }
}
```

### Example 5: Backpressure Detection on WebSockets (`bufferedAmount`)
```javascript
function sendLargeDatasetSafely(socket, dataChunks) {
  // socket.bufferedAmount indicates bytes queued in memory waiting for TCP send
  const MAX_BUFFER = 1024 * 1024; // 1MB threshold

  function pump() {
    while (dataChunks.length > 0) {
      if (socket.bufferedAmount > MAX_BUFFER) {
        // Backpressure detected! Wait for socket buffer to drain before sending more
        console.warn('Socket buffer congested. Throttling sender...');
        setTimeout(pump, 50);
        return;
      }
      const chunk = dataChunks.shift();
      socket.send(chunk);
    }
  }

  pump();
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Proxy and Load Balancer Timeouts**: Corporate proxies, NGINX, and AWS ALBs close idle TCP sockets after a timeout (default 60 seconds). You **must implement application-level heartbeats (ping/pong)** every 25–45 seconds to prevent silent disconnections.
2. **HTTP/1.1 6-Connection Limit on SSE**: Under HTTP/1.1, opening an `EventSource` in 6 different browser tabs consumes the maximum allowed connections per domain, completely blocking all subsequent `fetch()` and `<img>` requests in that browser! **Always serve SSE over HTTP/2 or HTTP/3**, where connections are multiplexed.
3. **CORS on WebSockets**: WebSockets do **not** follow the standard Same-Origin Policy! WebSockets can be opened to any external origin (`wss://evil.com`) without a CORS preflight. The server must explicitly validate the `Origin` header during the initial HTTP upgrade handshake to prevent Cross-Site WebSocket Hijacking (CSWSH).
4. **Closing Codes**: Standard WebSocket close codes:
   - `1000`: Normal Closure (clean shutdown).
   - `1001`: Going Away (tab navigating away).
   - `1006`: Abnormal Closure (network dropped abruptly, no close frame received).

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Missing Heartbeat** | No ping/pong mechanism on WebSocket | Send heartbeat every 30 seconds | Proxies and mobile carriers silently terminate idle sockets. |
| **Using `ws://` in Production** | `new WebSocket('ws://api.com')` | Always use `wss://` (TLS) | Unencrypted `ws://` is blocked by corporate firewalls and modern HTTPS pages. |
| **SSE over HTTP/1.1 across Tabs** | Multiple `EventSource` instances | Use HTTP/2 or single `SharedWorker` | Exhausts browser 6-connection per-domain limit. |
| **Recreating Socket without Backoff**| Reconnecting immediately in `onclose` | Use exponential backoff + jitter | Overwhelms recovering backend with a thundering herd. |
| **Ignoring `socket.bufferedAmount`**| Blasting gigabytes into `socket.send()` | Monitor `socket.bufferedAmount` | Causes client-side memory blowup if network is slow. |
| **Assuming SSE is Bidirectional** | Trying to send data via `eventSource.send()` | Send client data via `fetch(POST)` | SSE is strictly unidirectional (server to client). |

---

## 8. Performance Characteristics

- **Frame Framing Overhead**: WebSocket frames carry only 2 bytes for small payloads ($< 126$ bytes), compared to 500–1000 bytes of HTTP headers per request in polling.
- **Connection Scalability**: A single Node.js or Go server can maintain 100,000+ idle WebSocket connections, provided kernel file descriptor (`ulimit`) and TCP socket buffer settings are tuned.
- **Battery Impact on Mobile**: Maintaining an active WebSocket keeps the mobile cellular radio energized in high-power mode; SSE over HTTP/2 integrates better with system sleep cycles.

---

## 9. Engine Internals

1. **Chromium WebSocket Masking**: Under RFC 6455, all frames sent from client to server **must be XOR-masked** using a random 32-bit key to prevent cache-poisoning attacks on intermediate proxies. Chromium's C++ network layer generates this mask and applies the XOR transformation natively before socket transmission.
2. **`EventSource` Parser**: Chromium's SSE parser buffers incoming chunks until a double newline (`\n\n`) is encountered, emitting a parsed `blink::MessageEvent` to JavaScript.

---

## 10. Best Practices & Design Guidelines

1. **Use SSE for News Feeds, Notifications, and LLM Token Streams**: If the client only receives updates (like OpenAI ChatGPT completions or dashboards), choose Server-Sent Events over WebSockets for built-in reconnection and native HTTP/2 multiplexing.
2. **Use WebSockets for Two-Way Interactive Applications**: Choose WebSockets for games, real-time whiteboards, and instant chat.
3. **Always Implement Exponential Backoff with Jitter**: Avoid synchronized reconnect loops when recovering from server restarts.
4. **Validate the `Origin` Header on the Server**: Prevent Cross-Site WebSocket Hijacking by enforcing origin checks during the handshake.

---

## 11. Real-World Case Study

### Real-Time Financial Stock Ticker Terminal with Hybrid SSE & WebSocket

A financial platform displays live trade updates for 500 stocks. Market ticks are broadcast to 50,000 clients, while users execute limit orders interactively.

```javascript
class FinancialTradingSession {
  #tickerSource;
  #orderSocket;

  init() {
    // 1. Unidirectional Market Price Updates via SSE over HTTP/2
    // Takes advantage of native reconnection, automatic Last-Event-ID, and HTTP/2 multiplexing
    this.#tickerSource = new EventSource('/api/market/ticks');

    this.#tickerSource.addEventListener('trade', (event) => {
      const trade = JSON.parse(event.data);
      this.updatePriceDisplay(trade.symbol, trade.price);
    });

    this.#tickerSource.onerror = () => {
      console.warn('Ticker stream interrupted. Browser auto-reconnecting...');
    };

    // 2. Bidirectional Trading Connection via Secure WebSocket
    // Reserved for interactive order submissions requiring sub-10ms latency
    this.#orderSocket = new RobustWebSocket('wss://trading.platform.com/orders');
    this.#orderSocket.onMessage = (orderAck) => {
      this.handleOrderConfirmation(orderAck);
    };
  }

  submitOrder(symbol, quantity, price) {
    this.#orderSocket.send({
      action: 'BUY',
      symbol,
      quantity,
      price,
      timestamp: Date.now(),
    });
  }

  updatePriceDisplay(symbol, price) { /* ... DOM updates ... */ }
  handleOrderConfirmation(ack) { /* ... UI feedback ... */ }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between WebSockets and Server-Sent Events (SSE)?</strong></summary>

WebSockets provide a full-duplex, bidirectional persistent connection over a single TCP socket that supports both text and binary frames. Server-Sent Events provide a unidirectional (server-to-client) text stream operating over standard HTTP with native automatic reconnection and event IDs.
</details>

<details>
<summary><strong>2. How does the initial WebSocket handshake work?</strong></summary>

The client sends a standard HTTP/1.1 `GET` request with `Upgrade: websocket` and `Connection: Upgrade` headers along with a `Sec-WebSocket-Key`. If the server accepts, it responds with HTTP status `101 Switching Protocols` and an accept hash. The underlying TCP socket then switches to the binary WebSocket framing protocol.
</details>

<details>
<summary><strong>3. Why is HTTP/2 essential when using Server-Sent Events?</strong></summary>

Under HTTP/1.1, browsers limit concurrent connections to 6 per domain. Because an SSE stream remains open indefinitely, opening multiple tabs exhausts this limit, blocking all other HTTP traffic. HTTP/2 multiplexes unlimited streams over a single connection, bypassing this bottleneck.
</details>

<details>
<summary><strong>4. Why do client-to-server WebSocket frames require XOR masking?</strong></summary>

RFC 6455 requires clients to XOR-mask all frame payloads with a random 32-bit key to prevent cache-poisoning attacks on intermediate proxy servers that might misinterpret framed data as cached HTTP responses.
</details>

<details>
<summary><strong>5. What does the <code>socket.bufferedAmount</code> property indicate?</strong></summary>

It indicates the number of bytes that have been queued using `socket.send()` but have not yet been transmitted to the operating system's network socket buffer. It is used to detect and handle network backpressure.
</details>

<details>
<summary><strong>6. Does the Same-Origin Policy protect WebSockets?</strong></summary>

No. The Same-Origin Policy does not block cross-origin WebSocket connections. A web page on `evil.com` can establish a WebSocket connection directly to `bank.com`. The server must validate the `Origin` header during the initial HTTP handshake to prevent Cross-Site WebSocket Hijacking.
</details>

<details>
<summary><strong>7. How does SSE handle automatic reconnection?</strong></summary>

The `EventSource` API handles reconnection natively in the browser. When the connection drops, it waits (default 3 seconds, customizable via the server `retry: <ms>` field) and reconnects, automatically transmitting the `Last-Event-ID` header so the server can resume missed events.
</details>

<details>
<summary><strong>8. What is WebTransport?</strong></summary>

WebTransport is an emerging modern web API running over HTTP/3 (QUIC) that provides bidirectional, multiplexed, low-latency transport supporting both reliable streams and **unreliable datagrams** (similar to UDP), ideal for gaming and live media.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an SSE Reconnection Parser
Write a vanilla JavaScript function `parseSseChunk(rawText)` that parses raw SSE text chunks into structured event objects: `{ id, event, data }`.

<details>
<summary>View Solution</summary>

```javascript
function parseSseChunk(rawText) {
  const lines = rawText.split('\n');
  let id = null;
  let event = 'message';
  let dataLines = [];

  for (const line of lines) {
    if (line.startsWith('id:')) {
      id = line.slice(3).trim();
    } else if (line.startsWith('event:')) {
      event = line.slice(6).trim();
    } else if (line.startsWith('data:')) {
      dataLines.push(line.slice(5).trim());
    }
  }

  if (dataLines.length === 0) return null;

  return {
    id,
    event,
    data: dataLines.join('\n'),
  };
}
```
</details>

### Challenge 2: Implement a Safe WebSocket Queue
Create a class `QueuedWebSocket` that allows calling `send(data)` at any time; if the socket is still in the `CONNECTING` state, messages are buffered in memory and flushed automatically once `onopen` fires.

<details>
<summary>View Solution</summary>

```javascript
class QueuedWebSocket {
  #socket;
  #queue = [];

  constructor(url) {
    this.#socket = new WebSocket(url);

    this.#socket.onopen = () => {
      console.log('Socket opened. Flushing queued messages...');
      while (this.#queue.length > 0) {
        const msg = this.#queue.shift();
        this.#socket.send(msg);
      }
    };
  }

  send(data) {
    const payload = typeof data === 'string' ? data : JSON.stringify(data);

    if (this.#socket.readyState === WebSocket.OPEN) {
      this.#socket.send(payload);
    } else if (this.#socket.readyState === WebSocket.CONNECTING) {
      this.#queue.push(payload);
    } else {
      throw new Error(`Cannot send message: Socket is in state ${this.#socket.readyState}`);
    }
  }
}
```
</details>
