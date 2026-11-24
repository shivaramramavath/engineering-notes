# Backpressure & Slow Clients in Realtime Systems

> Protecting server memory from slow WebSocket consumers: `bufferedAmount` monitoring, high-water marks, and drop policies.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Problem** | Slow consumer socket buffering $\rightarrow$ Unbounded memory leak $\rightarrow$ Out-Of-Memory (OOM) Crash |

---

## 1. The Slow Consumer Threat

When a Node.js server broadcasts 1,000 messages per second to 10,000 WebSocket clients:
- Fast clients read the TCP stream immediately.
- A slow client (e.g. mobile device on 3G network) cannot drain its TCP receive buffer quickly enough.
- The Node.js process buffers unsent messages in memory for that socket.
- **The Result**: Process heap climbs from 100MB to 4GB within minutes, crashing the entire server.

---

## 2. Managing Backpressure via `ws.bufferedAmount`

The `ws` library exposes `ws.bufferedAmount`, indicating how many bytes are currently queued in Node.js memory waiting to be flushed to the OS kernel socket:

```javascript
const HIGH_WATER_MARK = 1024 * 1024; // 1MB threshold

export function broadcastSafe(wss, messagePayload) {
  const data = Buffer.from(JSON.stringify(messagePayload));

  for (const client of wss.clients) {
    if (client.readyState !== client.OPEN) continue;

    // Check if the client's send buffer is overwhelmed
    if (client.bufferedAmount > HIGH_WATER_MARK) {
      console.warn(`[Slow Client Dropped] Buffered bytes: ${client.bufferedAmount}`);
      // Strategy 1: Drop non-critical packet
      continue;
      
      // Strategy 2: For critical financial feeds, terminate the slow client:
      // client.terminate();
    }

    client.send(data);
  }
}
```
Applying backpressure thresholds guarantees that slow clients cannot degrade overall server performance or cause out-of-memory crashes.
