# WebRTC Signaling Server in Node.js

> Orchestrating peer-to-peer audio, video, and data channels: SDP exchange (offer/answer) and ICE candidate routing via WebSockets.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Protocols** | WebRTC, SDP (Session Description Protocol), ICE (Interactive Connectivity Establishment) |

---

## 1. Role of the Signaling Server in WebRTC

WebRTC enables direct peer-to-peer (P2P) communication between browsers for low-latency audio/video streaming.
However, two browsers behind NAT routers and firewalls cannot discover each other directly:
- **Signaling Server (Node.js)**: Acts as the intermediary "matchmaker".
- Peers connect to the Node.js signaling server over WebSockets to exchange:
  1. **SDP Offers and Answers**: Media capabilities (codecs, resolutions).
  2. **ICE Candidates**: Network routing possibilities (IP addresses, ports).
- Once the signaling handshake completes, the peers communicate **directly P2P**, bypassing the Node.js server entirely.

---

## 2. Signaling Server Implementation

```javascript
import { WebSocketServer, WebSocket } from 'ws';

const wss = new WebSocketServer({ port: 8080 });
const rooms = new Map(); // roomId -> Set<WebSocket>

wss.on('connection', (ws) => {
  let currentRoom = null;

  ws.on('message', (messageText) => {
    const data = JSON.parse(messageText.toString());

    switch (data.type) {
      case 'join':
        currentRoom = data.roomId;
        if (!rooms.has(currentRoom)) rooms.set(currentRoom, new Set());
        rooms.get(currentRoom).add(ws);
        break;

      case 'offer':
      case 'answer':
      case 'ice-candidate':
        // Forward signaling payload to all other peers in the room
        if (currentRoom && rooms.has(currentRoom)) {
          for (const peer of rooms.get(currentRoom)) {
            if (peer !== ws && peer.readyState === WebSocket.OPEN) {
              peer.send(JSON.stringify(data));
            }
          }
        }
        break;
    }
  });

  ws.on('close', () => {
    if (currentRoom && rooms.has(currentRoom)) {
      rooms.get(currentRoom).delete(ws);
    }
  });
});
```
