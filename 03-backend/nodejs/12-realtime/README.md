# Module 12: Realtime Architecture & WebSockets in Node.js

> Building low-latency bidirectional systems: Socket.IO, bare `ws`, Redis scaling adapters, WebRTC signaling, and backpressure management.

---

## Module Overview

Realtime backends enable instant updates for live dashboards, collaborative canvases, multiplayer games, and chat platforms. This module explores both high-level ecosystems (Socket.IO with Redis adapters) and low-level RFC 6455 protocols (`ws`), WebRTC signaling, and memory protection against slow consumers.

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-websocket-and-socketio.md](./01-websocket-and-socketio.md) | WebSocket protocol handshake, Socket.IO architecture, connection lifecycle. |
| **02** | [02-auth-and-rooms.md](./02-auth-and-rooms.md) | Authenticating WebSocket connections via JWT/session, dynamic room joining. |
| **03** | [03-scaling-with-redis-adapter.md](./03-scaling-with-redis-adapter.md) | Multi-node WebSocket clustering using `@socket.io/redis-adapter` pub/sub. |
| **04** | [04-ws-library-from-scratch.md](./04-ws-library-from-scratch.md) | Low-level WebSocket server using the bare `ws` package without overhead. |
| **05** | [05-webrtc-signalling-server.md](./05-webrtc-signalling-server.md) | WebRTC peer-to-peer signaling, SDP exchange, ICE candidate routing. |
| **06** | [06-backpressure-in-realtime-systems.md](./06-backpressure-in-realtime-systems.md) | Managing slow consumers, `bufferedAmount` monitoring, memory drop policies. |

---

## Next Steps
Proceed to [13-testing](../13-testing/README.md) to explore unit testing, integration testing, Vitest, and Testcontainers.
