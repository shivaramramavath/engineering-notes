# HTTP/2, HTTP/3 & Connection Keep-Alive in Node.js

> Optimizing backend network latency: HTTP connection reuse, TCP head-of-line blocking, HTTP/2 multiplexing via `node:http2`, and reverse proxy offloading.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Modules** | `node:http`, `node:https`, `node:http2` |
| **Concepts** | HTTP Keep-Alive, TCP 3-way Handshake, Multiplexed Streams |

---

## 1. HTTP Keep-Alive & Connection Pooling

Every new TCP connection requires a 3-way handshake (`SYN`, `SYN-ACK`, `ACK`) plus TLS negotiation ($2\times \text{RTT}$).
Without Keep-Alive, sequential HTTP calls incur massive network latency penalties.

```javascript
import http from 'node:http';

// Idiomatic HTTP Agent with persistent Keep-Alive pooling
export const keepAliveAgent = new http.Agent({
  keepAlive: true,
  keepAliveMsecs: 30000, // Send TCP keepalive probes every 30s
  maxSockets: 50,        // Max concurrent sockets per origin host
  maxFreeSockets: 10,    // Keep up to 10 idle sockets ready
  timeout: 60000,        // Socket timeout
});

// Outgoing request using pooled agent
http.get('http://internal-service/api', { agent: keepAliveAgent }, (res) => {
  // Sockets are reused automatically across consecutive calls!
});
```

---

## 2. HTTP/2 Multiplexing in Node.js (`node:http2`)

Unlike HTTP/1.1 (which requires multiple parallel TCP sockets to handle concurrent requests), HTTP/2 multiplexes hundreds of concurrent requests over a **single TCP socket** via binary frames:

```javascript
import http2 from 'node:http2';
import fs from 'node:fs';

const server = http2.createSecureServer({
  key: fs.readFileSync('server.key'),
  cert: fs.readFileSync('server.crt'),
});

server.on('stream', (stream, headers) => {
  const method = headers[':method'];
  const path = headers[':path'];

  stream.respond({
    'content-type': 'application/json',
    ':status': 200,
  });

  stream.end(JSON.stringify({ message: `Handled ${method} ${path} via HTTP/2 stream` }));
});

server.listen(8443);
```

---

## 3. Production Architecture: Edge vs Node Protocol Termination

In real-world enterprise architectures:
- **At the Edge**: Cloudflare, AWS CloudFront, or Nginx terminates public HTTP/2 and HTTP/3 (QUIC/UDP) from clients.
- **Inside the Private Network**: The reverse proxy proxies requests to Node.js backend pods over persistent HTTP/1.1 keep-alive pools or gRPC/HTTP/2.
