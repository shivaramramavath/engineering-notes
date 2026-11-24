---
title: "System Design: Collaborative Realtime Document Editor"
description: "Designing a Google Docs / Notion-style collaborative editor using CRDTs (Yjs) and NestJS WebSockets"
tags: [nestjs, system-design, crdt, collaborative-editing, yjs, websockets]
---

# System Design: Collaborative Realtime Document Editor

## Architecture
- **Conflict Resolution**: Conflict-free Replicated Data Types (CRDTs) using Yjs binary update vectors.
- **Synchronization**: NestJS WebSocket Gateway broadcasting binary diffs across active document room peers.
