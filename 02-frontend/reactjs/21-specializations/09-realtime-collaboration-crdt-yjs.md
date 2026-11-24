# 09 — Real-Time Collaboration with CRDTs & Yjs in React

## Metadata
- **React Version**: 19.0.0+
- **Prerequisites**: [11-api-integration/06-realtime-communication.md](../11-api-integration/06-realtime-communication.md), Distributed Systems Basics, WebSockets
- **Difficulty**: Expert
- **Estimated Read Time**: 30 minutes
- **Tags**: `collaboration`, `crdt`, `yjs`, `websockets`, `realtime`, `multi-cursor`, `local-first`

---

## Architecture Diagram

The diagram below illustrates how multiple concurrent users edit a shared document without merge conflicts using **Yjs (Conflict-free Replicated Data Types)**, synchronizing updates via `y-websocket` while broadcasting live cursor awareness states.

```mermaid
flowchart TD
    subgraph ClientA["Client A (Alice)"]
        ReactA["React UI (MultiCursorCanvas)"]
        YDocA["Local Y.Doc (CRDT State Engine)"]
        AwarenessA["Awareness: { cursor: { x: 120, y: 80 }, user: 'Alice' }"]
        ReactA <--> YDocA
        YDocA <--> AwarenessA
    end

    subgraph Transport["Realtime Sync Layer (WebSocket Server)"]
        WSServer["y-websocket Server (Relay & Optional Persistence)"]
    end

    subgraph ClientB["Client B (Bob)"]
        ReactB["React UI (MultiCursorCanvas)"]
        YDocB["Local Y.Doc (CRDT State Engine)"]
        AwarenessB["Awareness: { cursor: { x: 340, y: 220 }, user: 'Bob' }"]
        ReactB <--> YDocB
        YDocB <--> AwarenessB
    end

    YDocA -->|Binary Update Delta (14 bytes)| WSServer
    AwarenessA -->|Presence Delta| WSServer
    WSServer -->|Relay to peers| YDocB
    WSServer -->|Relay to peers| AwarenessB

    YDocB -->|Binary Update Delta| WSServer
    WSServer -->|Relay to peers| YDocA
```

---

## Concept Overview

Building multi-user real-time collaborative applications (such as Figma, Miro, Notion, or Google Docs) requires resolving **concurrent edit conflicts**:
- What happens if Alice and Bob type at character position 10 simultaneously?
- What happens if Alice deletes a Kanban card while Bob is editing its description?

Historically, collaborative software utilized **Operational Transformation (OT)**. OT was pioneered by Google Docs:
- **Centralized Master**: All client operations must travel to a single central authority server.
- **Transformation Complexity**: The central server mathematically transforms coordinates based on incoming timestamps. If the server loses connection, clients cannot edit offline safely.

The modern paradigm is **Conflict-free Replicated Data Types (CRDTs)**:
- **Mathematical Convergence**: A CRDT is a distributed data structure that can be updated concurrently across multiple machines without coordination.
- **Order-Independent**: Replicas can receive updates in any order or via high-latency networks. Once all updates arrive, all clients are **mathematically guaranteed to converge to the exact same state**.
- **Local-First & Offline Capable**: Users can make 1,000 edits on an airplane, reconnect hours later, and merge seamlessly with peers.

**Yjs** is the fastest and most widely adopted CRDT implementation in the JavaScript and React ecosystem.

---

## Practical Implementation

### Example 1: Initializing a Shared `Y.Doc` and WebSocket Provider

Connecting a React application to a collaborative room using `yjs` and `y-websocket`.

```ts
// src/lib/yjsClient.ts
import * as Y from 'yjs';
import { WebsocketProvider } from 'y-websocket';

export interface CollaborativeSession {
  doc: Y.Doc;
  provider: WebsocketProvider;
}

export function createCollaborativeSession(roomName: string, username: string, color: string): CollaborativeSession {
  // 1. Create a Yjs CRDT document
  const doc = new Y.Doc();

  // 2. Connect to a WebSocket sync server
  const provider = new WebsocketProvider(
    'wss://demos.yjs.dev', // Public demo server or your private ws://localhost:1234
    roomName,
    doc
  );

  // 3. Set local presence / awareness information
  provider.awareness.setLocalStateField('user', {
    name: username,
    color: color,
  });

  return { doc, provider };
}
```

*Expected Verification Output:*
```text
[Yjs WebSocket] Connected to room "project-canvas-alpha".
CRDT state initialized; local presence registered.
```

---

### Example 2: Reusable Hook for Shared Collaborative State (`Y.Map`)

A custom React hook that binds a `Y.Map` directly to React state, re-rendering automatically when any peer makes a modification.

```tsx
// src/hooks/useYMap.ts
import { useState, useEffect } from 'react';
import type * as Y from 'yjs';

export function useYMap<T extends Record<string, any>>(yMap: Y.Map<any>): [T, (key: string, value: any) => void] {
  const [state, setState] = useState<T>(() => yMap.toJSON() as T);

  useEffect(() => {
    // Observer triggered whenever local or remote peer changes the map
    const observer = () => {
      setState(yMap.toJSON() as T);
    };

    yMap.observe(observer);
    return () => yMap.unobserve(observer);
  }, [yMap]);

  const setItem = (key: string, value: any) => {
    yMap.set(key, value);
  };

  return [state, setItem];
}
```

*Expected Verification Output:*
```text
Calling setItem("theme", "dark") updates Y.Map.
Observer triggers in all connected peer browsers within 15ms.
```

---

### Example 3: Multi-User Collaborative Whiteboard with Live Cursors

Broadcasting mouse coordinates across all connected peers and rendering remote cursors with user name tags.

```tsx
// src/components/MultiCursorCanvas.tsx
import React, { useState, useEffect, useRef } from 'react';
import { createCollaborativeSession } from '../lib/yjsClient';

interface RemoteUser {
  clientId: number;
  user: { name: string; color: string };
  cursor?: { x: number; y: number };
}

export function MultiCursorCanvas({ roomName, username }: { roomName: string; username: string }) {
  const [remoteUsers, setRemoteUsers] = useState<RemoteUser[]>([]);
  const sessionRef = useRef<ReturnType<typeof createCollaborativeSession> | null>(null);

  useEffect(() => {
    const userColor = '#' + Math.floor(Math.random() * 16777215).toString(16);
    const session = createCollaborativeSession(roomName, username, userColor);
    sessionRef.current = session;

    const { provider } = session;
    const { awareness } = provider;

    const handleAwarenessChange = () => {
      const states = awareness.getStates();
      const users: RemoteUser[] = [];

      states.forEach((state, clientId) => {
        // Exclude our own cursor from remote display
        if (clientId !== doc.clientID && state.user && state.cursor) {
          users.push({
            clientId,
            user: state.user,
            cursor: state.cursor,
          });
        }
      });

      setRemoteUsers(users);
    };

    const { doc } = session;
    awareness.on('change', handleAwarenessChange);

    return () => {
      awareness.off('change', handleAwarenessChange);
      provider.destroy();
      doc.destroy();
    };
  }, [roomName, username]);

  const handleMouseMove = (e: React.MouseEvent<HTMLDivElement>) => {
    const rect = e.currentTarget.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    // Broadcast our mouse position to awareness
    sessionRef.current?.provider.awareness.setLocalStateField('cursor', { x, y });
  };

  return (
    <div
      onMouseMove={handleMouseMove}
      className="relative w-full h-96 border-2 border-dashed border-gray-300 rounded-xl bg-gray-50 overflow-hidden cursor-crosshair"
    >
      <div className="absolute top-3 left-3 text-xs font-semibold text-gray-500 pointer-events-none">
        Collaborative Room: <strong>{roomName}</strong> | Connected Peers: {remoteUsers.length}
      </div>

      {/* Render Remote User Cursors */}
      {remoteUsers.map((peer) => (
        <div
          key={peer.clientId}
          style={{
            transform: `translate3d(${peer.cursor?.x}px, ${peer.cursor?.y}px, 0)`,
            transition: 'transform 80ms ease-out',
          }}
          className="absolute top-0 left-0 pointer-events-none z-50 flex items-start"
        >
          {/* Custom SVG Cursor Arrow */}
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none">
            <path
              d="M5.65376 12.3673H5.46026L5.31717 12.4976L0.500002 16.8829L0.500002 1.19841L11.7841 12.3673H5.65376Z"
              fill={peer.user.color}
              stroke="white"
              strokeWidth="1.5"
            />
          </svg>
          <span
            style={{ backgroundColor: peer.user.color }}
            className="ml-1 px-1.5 py-0.5 rounded text-[10px] text-white font-bold whitespace-nowrap shadow"
          >
            {peer.user.name}
          </span>
        </div>
      ))}
    </div>
  );
}
```

*Expected Verification Output:*
```text
Open page in two separate browser tabs:
Tab A moves mouse across canvas -> Cursor in Tab B tracks smoothly in real time with colored name badge.
Closing Tab A removes cursor instantly from Tab B.
```

---

### Example 4: Collaborative Shared Todo List with `Y.Array`

Concurrent additions and deletions handled deterministically with zero merge conflicts.

```tsx
// src/components/CollaborativeTodoList.tsx
import React, { useState, useEffect } from 'react';
import * as Y from 'yjs';

export function CollaborativeTodoList({ doc }: { doc: Y.Doc }) {
  const [items, setItems] = useState<string[]>([]);
  const [input, setInput] = useState('');
  const yListRef = React.useRef<Y.Array<string>>(doc.getArray('todos'));

  useEffect(() => {
    const yList = yListRef.current;
    setItems(yList.toArray());

    const observer = () => {
      setItems(yList.toArray());
    };

    yList.observe(observer);
    return () => yList.unobserve(observer);
  }, []);

  const handleAdd = (e: React.FormEvent) => {
    e.preventDefault();
    if (!input.trim()) return;
    // Push into shared CRDT array
    yListRef.current.push([input.trim()]);
    setInput('');
  };

  const handleDelete = (index: number) => {
    // Delete item by index in CRDT array
    yListRef.current.delete(index, 1);
  };

  return (
    <div className="p-4 border rounded max-w-sm bg-white">
      <h4 className="font-bold mb-3">CRDT Shared Todo List</h4>
      <form onSubmit={handleAdd} className="flex gap-2 mb-3">
        <input
          type="text"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          placeholder="New shared item..."
          className="border p-1.5 text-sm rounded flex-1"
        />
        <button type="submit" className="px-3 py-1 bg-blue-600 text-white rounded text-sm font-semibold">
          Add
        </button>
      </form>
      <ul className="divide-y border rounded">
        {items.map((item, idx) => (
          <li key={idx} className="p-2 flex justify-between items-center text-sm">
            <span>{item}</span>
            <button
              onClick={() => handleDelete(idx)}
              className="text-xs text-red-500 hover:text-red-700 font-bold"
            >
              ✕
            </button>
          </li>
        ))}
      </ul>
    </div>
  );
}
```

*Expected Verification Output:*
```text
User A adds "Item 1". User B adds "Item 2" at the exact same millisecond.
CRDT resolves deterministically -> Both users see identical ordered lists ["Item 1", "Item 2"].
```

---

### Example 5: Collaborative Undo / Redo with `Y.UndoManager`

In collaborative applications, pressing `Ctrl+Z` must undo **only your own changes**, never undoing your teammate's edits!

```tsx
// src/components/ScopedUndoRedo.tsx
import React, { useEffect, useRef } from 'react';
import * as Y from 'yjs';

export function ScopedUndoRedo({ doc }: { doc: Y.Doc }) {
  const yText = doc.getText('document_text');
  const undoManagerRef = useRef<Y.UndoManager | null>(null);

  useEffect(() => {
    // Scoped Undo Manager tracks only edits made in this client session!
    const undoManager = new Y.UndoManager(yText, {
      trackedOrigins: new Set([doc.clientID]),
    });
    undoManagerRef.current = undoManager;

    return () => undoManager.destroy();
  }, [doc, yText]);

  return (
    <div className="flex gap-2 my-2">
      <button
        onClick={() => undoManagerRef.current?.undo()}
        className="px-3 py-1 bg-gray-100 hover:bg-gray-200 border rounded text-xs font-semibold"
      >
        ↺ Undo My Edit
      </button>
      <button
        onClick={() => undoManagerRef.current?.redo()}
        className="px-3 py-1 bg-gray-100 hover:bg-gray-200 border rounded text-xs font-semibold"
      >
        ↻ Redo My Edit
      </button>
    </div>
  );
}
```

*Expected Verification Output:*
```text
Alice types "Hello". Bob types "World".
Alice clicks "Undo My Edit":
"Hello" is reverted, while Bob's "World" remains intact!
```

---

## Mental Model & Analogies

### Git vs CRDTs

- **Git** is **asynchronous, manual conflict resolution**:
  - You edit on a branch, Bob edits on a branch.
  - When merging, if you edited the same lines, Git stops and screams: `<<<<<<< HEAD ======= >>>>>>>`. A human engineer must manually resolve the merge conflict.
- **Yjs (CRDT)** is **automatic mathematical convergence**:
  - Every character, node, and element has a unique, globally immutable identifier composed of `[clientID, clock]`.
  - Operations follow commutative mathematical rules ($A + B = B + A$).
  - No matter when or in what order operations arrive across the network, the mathematical formula guarantees that Alice and Bob's screens will calculate the exact same layout.

---

## Under the Hood / Internal Mechanics

### 1. How Yjs Achieves Extreme Performance

While older CRDT libraries (like original Automerge) were slow and memory-heavy due to maintaining deep tree structures for every character, Yjs invented the **Item Linked List Structure**:
- Consecutive typed characters are merged into single memory blocks ("Items").
- Typing 10,000 words produces a few hundred Items rather than 50,000 separate objects.
- Updates are serialized into ultra-compact **binary delta buffers** (`Uint8Array`) using variable-length integer encoding. An update typically weighs between **10 to 30 bytes**.

---

## Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Solution |
| :--- | :--- | :--- |
| **Using Array Indexes as Permanent IDs** | When peer inserts an item at index 0, all subsequent indexes shift, corrupting edits. | Rely on Yjs CRDT item IDs or use `Y.Map` indexed by UUIDs. |
| **Using Standard `useState` for Collaborative Documents**| React state does not know how to merge concurrent remote JSON payloads, causing last-write-wins overwrites. | Store collaborative data inside a `Y.Doc` and subscribe via observers. |
| **Forgetting to Call `provider.destroy()`** | Leaves zombie WebSocket connections and awareness listeners active in background memory. | Always destroy providers and documents in `useEffect` cleanup. |
| **Global Undo without Scoped Origins** | Pressing undo reverts another user's edits, enraging teammates. | Restrict `Y.UndoManager` using `trackedOrigins: new Set([doc.clientID])`. |
| **Transmitting Cursors Through the Main CRDT Document**| Every mouse movement (60 updates/sec) writes to the persistent document history, bloating memory. | Send high-frequency cursors through the **Awareness protocol**, which does not persist history. |
| **Mutating `Y.Doc` Inside Render** | Violates React purity; triggers infinite observer loops. | Mutate Yjs structures in event handlers or effects. |

---

## Performance & Optimization

### Local Document Compaction

Over weeks of continuous editing, a `Y.Doc` accumulates deleted tombstones. Periodically compact the document by encoding state into a fresh document snapshot:
```ts
const snapshot = Y.encodeStateAsUpdate(doc);
const cleanDoc = new Y.Doc();
Y.applyUpdate(cleanDoc, snapshot);
```

---

## Edge Cases & Debugging

### Inspecting Yjs Binary Updates
Yjs includes an inspection utility to decode binary byte updates into human-readable logs:
```ts
import { logUpdate } from 'yjs';
provider.on('sync', (isSynced) => {
  console.log('[Yjs Sync Status]:', isSynced);
});
```

---

## Ecosystem & Alternative Approaches

- **Automerge**: Outstanding CRDT library by Martin Kleppmann and Ink & Switch. Implemented in Rust with WebAssembly bindings.
- **Liveblocks (`@liveblocks/react`)**: Commercial backend-as-a-service providing turnkey presence, multiplayer storage, and comments.
- **PartyKit**: Cloudflare Workers platform designed specifically for hosting WebSocket multiplayer rooms.

---

## Exercises & Challenges

<details>
<summary><strong>Exercise 1: Peer Disconnect Timeout in Awareness</strong></summary>

**Challenge**: What happens to remote cursors if a peer loses internet connection abruptly without sending a disconnect packet?

**Answer**:
The Awareness protocol automatically includes a heartbeat timestamp with each presence packet. If the WebSocket server or peers do not receive a heartbeat update from a client within **30 seconds**, the client is considered timed out, and the Awareness state automatically purges the client and triggers an awareness `change` event.
</details>

<details>
<summary><strong>Exercise 2: Shared Sticky Notes Canvas State</strong></summary>

**Challenge**: Create a typed helper function to move a sticky note's position in a shared `Y.Map`.

```ts
export function moveStickyNote(notesMap: Y.Map<any>, noteId: string, x: number, y: number) {
  const current = notesMap.get(noteId);
  if (current) {
    notesMap.set(noteId, { ...current, x, y, updatedAt: Date.now() });
  }
}
```
</details>

---

## Frequently Asked Interview Questions

<details>
<summary><strong>1. What is a Conflict-free Replicated Data Type (CRDT), and why is it preferred over Operational Transformation (OT)?</strong></summary>

A CRDT is a distributed data structure that can be edited independently across multiple clients and merged automatically without a central server deciding the conflict resolution. 
Operational Transformation (OT) requires a centralized server to serialize and transform operation offsets. CRDTs use mathematical properties (commutativity, associativity, idempotency) guaranteeing that any two peers that have received the same set of updates will arrive at the exact same state, enabling true offline-first and peer-to-peer collaboration.
</details>

<details>
<summary><strong>2. How does Yjs represent deleted text or items?</strong></summary>

CRDTs cannot simply erase deleted data from memory immediately because a concurrent peer might insert a character relative to that deleted item. Yjs uses **tombstones**: an Item is marked as deleted (`deleted: true`), preserving its unique ID and coordinate position so incoming remote updates can resolve their positions. Tombstones can be compacted into a clean state snapshot during document saves.
</details>

<details>
<summary><strong>3. What is the difference between the Yjs Document and the Awareness Protocol?</strong></summary>

**The Yjs Document (`Y.Doc`)** stores persistent, conflict-free application state (text, shapes, tasks). All transactions are recorded in history and persisted to databases. 
**The Awareness Protocol** handles ephemeral, high-frequency presence information (user mouse coordinates, active selection highlights, typing status). Awareness updates are not persisted to document history and time out automatically when a client disconnects.
</details>

<details>
<summary><strong>4. Why should mouse cursor positions never be stored inside a `Y.Doc` directly?</strong></summary>

Mouse coordinates update at 60Hz. If stored in a `Y.Doc`, every pixel coordinate change is treated as an immutable CRDT transaction, creating thousands of historical items and bloating the document size by megabytes in minutes. Cursor coordinates must always be transmitted via the transient Awareness protocol.
</details>

<details>
<summary><strong>5. How does `Y.UndoManager` distinguish between local and remote user edits?</strong></summary>

`Y.UndoManager` tracks transaction origins. By configuring `trackedOrigins: new Set([doc.clientID])`, the UndoManager only records changes made by the local client. When the user triggers undo, only operations created by their specific client ID are inverted, leaving edits from remote collaborators completely untouched.
</details>

<details>
<summary><strong>6. Can Yjs be used with relational databases on the server?</strong></summary>

Yes. The server can listen to incoming binary updates from `y-websocket` and store them in PostgreSQL, Redis, or S3 using `y-leveldb` or custom database adapters. Documents can be loaded from database snapshots when the first user enters a room and saved back when idle.
</details>

<details>
<summary><strong>7. What transport protocols can Yjs use?</strong></summary>

Yjs is completely transport-agnostic. Official providers include:
- `y-websocket`: Standard client-server WebSockets.
- `y-webrtc`: Peer-to-peer WebRTC data channels with minimal server signaling.
- `y-indexeddb`: Local browser persistence without any network connection.
</details>

<details>
<summary><strong>8. How does Yjs prevent "Interleaving" when two users insert text at the same point?</strong></summary>

Yjs uses the **YATA** (Yet Another Transformation Approach) algorithm. When two characters are inserted at the same location, Yjs compares the unique client identifiers and logical clock numbers of the conflicting items to enforce a deterministic sorting order that preserves word continuity and prevents characters from alternating or interleaving.
</details>

---

## References & Further Reading

- [Yjs Official Documentation](https://docs.yjs.dev/)
- [Martin Kleppmann: Conflict-free Replicated Data Types (CRDTs)](https://martin.kleppmann.com/)
- [Kevin Jahns: High-Performance CRDT Architecture (Yjs internals)](https://github.com/yjs/yjs)
- [Local-First Software: You Own Your Data, in spite of the Cloud (Ink & Switch)](https://www.inkandswitch.com/local-first/)
- [Figma: How Figma's Multiplayer Technology Works](https://www.figma.com/blog/how-figmas-multiplayer-technology-works/)
