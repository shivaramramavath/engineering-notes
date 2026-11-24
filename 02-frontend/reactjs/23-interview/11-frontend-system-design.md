# Frontend System Design — Senior Interview Questions & Deep Dives

> Real-world staff/lead architectural interviews: Designing infinite feeds, collaborative real-time whiteboards, offline-first applications, and large-scale data tables.

---

## System Design Question: Design a Real-Time Collaborative Document Editor (e.g., Notion / Google Docs) in React

### 1. Requirements & Constraints
- **Functional**: Rich-text editing, multi-user cursor awareness (presence), conflict-free simultaneous edits, offline drafting with automatic sync.
- **Non-Functional**: Sub-50ms typing latency, zero document corruption on network partitions, sub-5MB client memory overhead.

### 2. High-Level Architecture
```
+-------------------------------------------------------------------+
|                         React Editor Shell                        |
|                                                                   |
|   [ Lexical / TipTap ContentEditable Engine ]                     |
|                           │                                       |
|                           ▼                                       |
|   [ Yjs CRDT (Conflict-Free Replicated Data Type) Document ]      |
|           │                                   │                   |
|           ▼                                   ▼                   |
|   [ y-indexeddb (Offline DB) ]         [ y-websocket Provider ]   |
|                                               │                   |
+-----------------------------------------------│-------------------+
                                                ▼
                                    [ WebSocket Server / Rooms ]
```

### 3. Key Design Trade-offs: OT vs CRDT
- **Operational Transformation (OT)**: Requires a single centralized server to order all operations (Google Docs legacy approach). Cannot resolve edits offline easily.
- **CRDT (Conflict-Free Replicated Data Types, e.g. Yjs / Automerge)**: Operations are mathematically commutative and associative. Any peer can edit offline, and trees converge deterministically upon reconnection without server locks.

### 4. React Optimization Strategy
- Avoid storing raw keystrokes in React state (`useState`)! Rerendering the component tree on every character keystroke introduces noticeable input latency.
- Bind the contenteditable DOM directly to the Yjs shared type using Lexical or TipTap. React manages only outer chrome (toolbars, participant avatar lists, and modal dialogs).
