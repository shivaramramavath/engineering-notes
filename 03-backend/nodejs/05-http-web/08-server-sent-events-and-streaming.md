# Server-Sent Events (SSE) & Streaming Uploads in Node.js

> Building real-time one-way data feeds using Server-Sent Events (`text/event-stream`) and handling high-volume chunked multipart uploads without disk buffering.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Protocol** | HTTP/1.1 Chunked Transfer, SSE (`text/event-stream`) |
| **Level** | Core Web Protocols |

---

## 1. Server-Sent Events (SSE) vs WebSockets

| Feature | Server-Sent Events (SSE) | WebSockets (`ws`) |
| :--- | :--- | :--- |
| **Direction** | One-way (Server $\rightarrow$ Client) | Full Duplex (Bi-directional) |
| **Protocol** | Standard HTTP (`text/event-stream`) | Upgraded TCP protocol (`ws://`) |
| **Reconnection** | Built-in browser auto-reconnect | Requires manual client retry logic |
| **Proxy & Firewall Friendly** | 100% standard HTTP (passes through CDNs) | Requires specialized WebSocket proxying |
| **Best Used For** | AI LLM token streaming, notifications, price feeds | Collaborative editing, real-time gaming, chat |

---

## 2. Implementing an SSE Stream in Raw Node.js

```javascript
import http from 'node:http';

const server = http.createServer((req, res) => {
  if (req.url === '/api/live-metrics') {
    // 1. Mandatory SSE Response Headers
    res.writeHead(200, {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache, no-transform',
      'Connection': 'keep-alive',
      'Access-Control-Allow-Origin': '*',
    });

    // 2. Format: 'id: <id>\nevent: <name>\ndata: <json>\n\n'
    let counter = 0;
    const intervalId = setInterval(() => {
      counter++;
      const payload = JSON.stringify({ cpu: Math.random() * 100, counter });
      res.write(`id: ${counter}\nevent: metric\ndata: ${payload}\n\n`);
    }, 1000);

    // 3. Clean up timer on client disconnect!
    req.on('close', () => {
      clearInterval(intervalId);
      console.log('SSE Client disconnected; stopped metric emitter');
    });
  } else {
    res.writeHead(404);
    res.end();
  }
});

server.listen(3000);
```

---

## 3. Streaming Multipart Uploads to S3 without Disk Buffering

Using `busboy` or streams to pipe directly from client HTTP socket to object storage:

```javascript
import http from 'node:http';
import { Upload } from '@aws-sdk/lib-storage';
import { S3Client } from '@aws-sdk/client-s3';
import busboy from 'busboy';

const s3 = new S3Client({ region: 'us-east-1' });

export function handleStreamingUpload(req, res) {
  const bb = busboy({ headers: req.headers });

  bb.on('file', (name, fileStream, info) => {
    const { filename, mimeType } = info;

    // Stream directly to S3 without writing to local disk!
    const parallelUploads3 = new Upload({
      client: s3,
      params: {
        Bucket: 'my-upload-bucket',
        Key: `uploads/${Date.now()}-${filename}`,
        Body: fileStream,
        ContentType: mimeType,
      },
    });

    parallelUploads3.done().then(() => {
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ success: true, filename }));
    });
  });

  req.pipe(bb);
}
```
