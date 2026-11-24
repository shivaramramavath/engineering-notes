# Low-Level WebSockets in Node.js with `ws`

> High-throughput, low-overhead WebSocket server implementation using the bare `ws` library without Socket.IO abstractions.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Package** | `ws` v8+ |
| **Protocols** | RFC 6455 WebSocket Protocol |

---

## 1. When to Use Bare `ws` Over Socket.IO

- **Socket.IO**: Rich feature set (automatic fallback to long-polling, built-in rooms, automatic reconnection, custom binary packet serializer). Best for complex interactive user-facing dashboards.
- **Bare `ws`**: Minimal overhead, standard RFC 6455 compliance, zero proprietary handshakes. Best for high-frequency financial tickers, high-throughput microservice streams, or cross-language clients (Go, Rust, Python).

---

## 2. Implementing a Production `ws` Server

```javascript
import { createServer } from 'node:http';
import { WebSocketServer, WebSocket } from 'ws';

const server = createServer();
const wss = new WebSocketServer({ noServer: true });

// Handle HTTP Upgrade manually to authenticate before upgrading to WebSocket
server.on('upgrade', async (request, socket, head) => {
  try {
    const token = new URL(request.url, 'http://localhost').searchParams.get('token');
    if (!token || token !== 'valid_secret') {
      socket.write('HTTP/1.1 401 Unauthorized\r\n\r\n');
      socket.destroy();
      return;
    }

    wss.handleUpgrade(request, socket, head, (ws) => {
      wss.emit('connection', ws, request);
    });
  } catch {
    socket.destroy();
  }
});

// Heartbeat mechanism to detect dead clients
function heartbeat() {
  this.isAlive = true;
}

wss.on('connection', (ws, request) => {
  ws.isAlive = true;
  ws.on('pong', heartbeat);

  ws.on('message', (data, isBinary) => {
    const message = isBinary ? data : data.toString();
    console.log('Received:', message);

    // Broadcast to all active clients
    for (const client of wss.clients) {
      if (client.readyState === WebSocket.OPEN) {
        client.send(data, { binary: isBinary });
      }
    }
  });
});

// Check client connectivity every 30 seconds
const interval = setInterval(() => {
  for (const ws of wss.clients) {
    if (ws.isAlive === false) return ws.terminate();
    ws.isAlive = false;
    ws.ping();
  }
}, 30000);

wss.on('close', () => clearInterval(interval));

server.listen(8080);
```
