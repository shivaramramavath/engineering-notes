# socket.io-react-hook — Basic to Advanced

A learning guide for `socket.io-react-hook`, a React hooks library for Socket.IO 4.x. Intended for the React mastery notes (React frontend + NestJS backend).

> **Maintenance caution:** the latest listed version is 2.4.5, published about two years ago, and maintenance is reported as inactive. Evaluate it carefully before using it in production. Always cross-check option names against the package README on npm.

---

## Level 0 — Concepts

- **Socket.IO** is a real-time, bidirectional, event-based protocol (client and server emit and listen for named events).
- **socket.io-react-hook** wraps `socket.io-client` in hooks so React components can share a connection and subscribe to events.
- It gives you one provider (`IoProvider`) and two hooks (`useSocket`, `useSocketEvent`).
- Connections are **shared by namespace** between components. This is the single most important behavior to understand.

| API | Type | Purpose |
| --- | --- | --- |
| `IoProvider` | Component | Context provider that holds shared connections |
| `useSocket()` | Hook | Manage a connection: returns `socket`, `connected`, `error` |
| `useSocketEvent()` | Hook | Listen to an event and send messages: returns `lastMessage`, `sendMessage`, `socket` |

---

## Level 1 — Basic

### 1.1 Install

```bash
npm install socket.io-react-hook socket.io-client
```

### 1.2 Wrap the app with IoProvider

```tsx
import { IoProvider } from 'socket.io-react-hook';

export function AppProviders({ children }: { children: React.ReactNode }) {
  return <IoProvider>{children}</IoProvider>;
}
```

Place `AppProviders` around the whole app (usually in `main.tsx`) so any component can share connections.

### 1.3 Connect and listen

```tsx
import { useSocket, useSocketEvent } from 'socket.io-react-hook';

export function Notifications() {
  const { socket, connected, error } = useSocket('http://localhost:3000');

  const { lastMessage } = useSocketEvent<{ message: string }>(
    socket,
    'notification',
  );

  if (error) return <p>Socket connection error</p>;

  return (
    <div>
      <p>Status: {connected ? 'Connected' : 'Disconnected'}</p>
      <p>{lastMessage?.message ?? 'No notifications yet'}</p>
    </div>
  );
}
```

The server must emit a matching `notification` event, otherwise nothing arrives.

### 1.4 Emit an event

```ts
socket.emit('notification:read', { notificationId: '123' });
```

Your NestJS gateway must listen for `notification:read`.

### 1.5 Matching NestJS gateway (for testing)

```ts
import {
  WebSocketGateway,
  WebSocketServer,
  SubscribeMessage,
  MessageBody,
} from '@nestjs/websockets';
import { Server } from 'socket.io';

@WebSocketGateway({ cors: { origin: 'http://localhost:5173' } })
export class NotificationsGateway {
  @WebSocketServer() server: Server;

  @SubscribeMessage('notification:read')
  handleRead(@MessageBody() data: { notificationId: string }) {
    // mark as read, then push an update
    this.server.emit('notification', { message: 'Read: ' + data.notificationId });
  }
}
```

**Checkpoint:** you can render connection status and show the last received `notification`.

---

## Level 2 — Intermediate

### 2.1 What useSocket returns

```tsx
const { socket, connected, error } = useSocket('http://localhost:3000', {
  enabled: true,
  reconnection: true,
  reconnectionAttempts: 5,
});
```

- `socket`: the Socket.IO client instance.
- `connected`: boolean, whether the socket is currently connected.
- `error`: connection error information.
- `enabled`: whether this hook should initiate its connection.
- `reconnection`, `reconnectionAttempts`: passed through to the Socket.IO client.

### 2.2 What useSocketEvent returns

```tsx
interface Notification {
  id: string;
  message: string;
}

const { lastMessage, sendMessage } = useSocketEvent<Notification>(
  socket,
  'notification',
);
```

- `lastMessage`: the latest received payload.
- `sendMessage`: emits the event and can optionally await a server acknowledgement.
- `socket`: the underlying socket.

Useful options:

| Option | Meaning |
| --- | --- |
| `onMessage` | Callback invoked every time the event arrives |
| `keepPrevious` | Keep the previous event value when a subscriber remounts |

```tsx
useSocketEvent<Notification>(socket, 'notification', {
  onMessage: (n) => {
    console.log(n.message);
  },
});
```

### 2.3 lastMessage vs onMessage

- Use `lastMessage` when the UI only needs the **latest** value (status, a counter, a single toast).
- Use `onMessage` when you must handle **every** event (append to a list, play a sound, update a cache). `lastMessage` can skip values if events arrive faster than React renders.

```tsx
function useLiveFeed() {
  const { socket } = useSocket('http://localhost:3000');
  const [items, setItems] = useState<Notification[]>([]);

  useSocketEvent<Notification>(socket, 'notification', {
    onMessage: (n) => setItems((prev) => [n, ...prev].slice(0, 50)),
  });

  return items;
}
```

### 2.4 Conditional connection with enabled

```tsx
const isLoggedIn = Boolean(user);
const { socket } = useSocket('http://localhost:3000', {
  enabled: isLoggedIn,
});
```

Only connect after login, and avoid connecting on public pages.

**Checkpoint:** you can build a live feed that keeps all events and connects only for logged-in users.

---

## Level 3 — Connection lifecycle (important)

### 3.1 Mount = connect, unmount = disconnect?

```tsx
const { socket, connected, error } = useSocket('http://localhost:3000', {
  enabled: true,
});
```

`enabled: true` keeps the connection enabled while the hook is mounted. It does **not**, by itself, guarantee the underlying socket disconnects when this component unmounts, because the library **shares connections between components**. Unmounting one component must not be assumed to disconnect a socket still used by others.

### 3.2 Strict per-component lifecycle with socket.io-client

When a component must own its socket and disconnect on unmount, use `socket.io-client` directly:

```tsx
import { useEffect, useState } from 'react';
import { io, type Socket } from 'socket.io-client';

export function LiveDashboard() {
  const [connected, setConnected] = useState(false);

  useEffect(() => {
    const socket: Socket = io('http://localhost:3000', { autoConnect: false });

    const onConnect = () => setConnected(true);
    const onDisconnect = () => setConnected(false);

    socket.on('connect', onConnect);
    socket.on('disconnect', onDisconnect);
    socket.connect();

    return () => {
      socket.off('connect', onConnect);
      socket.off('disconnect', onDisconnect);
      socket.disconnect();
    };
  }, []);

  return <p>{connected ? 'Connected' : 'Disconnected'}</p>;
}
```

The cleanup function guarantees this component's own socket instance disconnects.

### 3.3 React Strict Mode

In development, Strict Mode intentionally runs setup, cleanup, then setup again. With the direct approach you may see connect, disconnect, connect in the network tab. This is normal and does not happen in production builds. Your cleanup must therefore be correct and idempotent.

### 3.4 Decision rule

| Situation | Use |
| --- | --- |
| Shared, app-wide connection (notifications, presence) | `socket.io-react-hook` |
| Each component must own and close its own socket | `socket.io-client` + `useEffect` cleanup |

**Do not mix both approaches for the same connection.**

---

## Level 4 — Authentication (React + NestJS)

### 4.1 Send a token in the handshake

Socket.IO supports an `auth` option in the client handshake. Pass it through the hook options (verify the exact option pass-through in the README for your installed version):

```tsx
const { socket } = useSocket('http://localhost:3000', {
  enabled: Boolean(accessToken),
  auth: { token: accessToken },
});
```

### 4.2 Validate in NestJS

```ts
@WebSocketGateway()
export class AppGateway implements OnGatewayConnection {
  handleConnection(client: Socket) {
    const token = client.handshake.auth?.token;
    if (!token || !this.authService.verify(token)) {
      client.disconnect(true);
    }
  }
}
```

### 4.3 Token refresh warning

If access tokens live in memory, **do not assume the package handles token refresh or re-authentication**. The handshake token is sent when the connection is established. When the token expires or changes you must:

1. Refresh the token through your normal auth flow.
2. Disconnect and reconnect so a new handshake carries the new token (for example, toggle `enabled`, or recreate the connection).
3. Handle the server rejecting an expired token (listen to the connection `error`).

---

## Level 5 — Advanced patterns

### 5.1 Organize into reusable hooks

```text
src/
├── providers/
│   └── SocketProvider.tsx
├── hooks/
│   ├── useAppSocket.ts
│   └── useSocketEvent.ts
└── features/
    ├── notifications/
    │   └── useNotifications.ts
    └── chat/
        └── useChat.ts
```

Centralize the URL and options in one hook so features never hardcode them:

```ts
// hooks/useAppSocket.ts
import { useSocket } from 'socket.io-react-hook';
import { useAuth } from '../features/auth/useAuth';

export function useAppSocket() {
  const { accessToken } = useAuth();
  return useSocket(import.meta.env.VITE_SOCKET_URL, {
    enabled: Boolean(accessToken),
    auth: { token: accessToken },
  });
}
```

```ts
// features/notifications/useNotifications.ts
import { useState } from 'react';
import { useSocketEvent } from 'socket.io-react-hook';
import { useAppSocket } from '../../hooks/useAppSocket';

export function useNotifications() {
  const { socket, connected, error } = useAppSocket();
  const [items, setItems] = useState<Notification[]>([]);

  const { sendMessage } = useSocketEvent<Notification>(socket, 'notification', {
    onMessage: (n) => setItems((prev) => [n, ...prev]),
  });

  const markRead = (id: string) =>
    socket.emit('notification:read', { notificationId: id });

  return { items, connected, error, markRead, sendMessage };
}
```

### 5.2 Shared connections and namespaces

- Components that call `useSocket` with the same URL and namespace share **one** connection.
- Different namespaces (for example `/chat`, `/notifications`) get separate connections.
- Benefit: fewer connections, consistent state. Risk: unmount does not mean disconnect.

### 5.3 Typed events

Define one type per event and reuse it on both sides (a shared package or a copied types file):

```ts
export interface ServerToClientEvents {
  notification: (payload: { id: string; message: string }) => void;
}
export interface ClientToServerEvents {
  'notification:read': (payload: { notificationId: string }) => void;
}
```

Use the generic on `useSocketEvent<T>` for the payload type of each event.

### 5.4 Acknowledgements (request and response)

`sendMessage` can optionally await a server acknowledgement. On the gateway, return a value from the handler:

```ts
@SubscribeMessage('chat:send')
handleSend(@MessageBody() body: { text: string }) {
  return { ok: true, receivedAt: Date.now() }; // sent back as the ack
}
```

Use acks for operations where the UI needs confirmation (message sent, item saved) instead of waiting for a separate event.

### 5.5 Reconnection and offline UX

- Tune `reconnection`, `reconnectionAttempts` and related client options.
- Show a banner when `connected` is false.
- After reconnect, **resync state** (refetch data over REST) because events emitted while offline are lost unless the server replays them.

### 5.6 Combining with data fetching

For server state, a common pattern is to fetch initial data over REST and apply live updates from socket events (merge into your cache or state). Treat the socket as the delta stream, not the source of truth.

### 5.7 Performance

- Subscribe only in components that need the event.
- Use `onMessage` plus functional state updates for high-frequency events; cap list sizes.
- Avoid creating new option objects every render if the library depends on them (memoize if you see reconnect loops).

---

## Level 6 — Pitfalls and debugging

| Symptom | Likely cause |
| --- | --- |
| Nothing is received | Server event name differs from the client name |
| CORS error | Gateway `cors.origin` does not include the frontend origin |
| Connects twice in dev | React Strict Mode setup and cleanup (normal) |
| Socket stays connected after unmount | Connection is shared; another component still uses it |
| Auth works once, then fails | Token expired; no automatic re-authentication |
| `lastMessage` skips events | Fast events batched by React; use `onMessage` |
| Connect then immediate disconnect | Server rejected the handshake (auth guard) |

Debug tip: enable Socket.IO client logs in the browser console with `localStorage.debug = 'socket.io-client:*'`, and check the Network tab (WS) for frames.

---

## Level 7 — Alternatives and when to leave the library

Because the package is reported as inactive, consider:

- Using `socket.io-client` directly inside your own `SocketProvider` and custom hooks (full control, no dependency risk).
- Keeping your hook API (`useAppSocket`, `useNotifications`) stable so you can swap the implementation later without touching features.

A good practice: wrap the library in your own hooks from day one.

---

## Cheat sheet

```tsx
// 1. Provider (once)
<IoProvider><App /></IoProvider>

// 2. Connect
const { socket, connected, error } = useSocket(URL, { enabled: true });

// 3. Listen
const { lastMessage, sendMessage } = useSocketEvent<T>(socket, 'event', {
  onMessage: (data) => {},
  keepPrevious: true,
});

// 4. Emit
socket.emit('event', payload);
```

Rules to remember:

1. One `IoProvider` at the root.
2. Event names must match on client and server.
3. Connections are shared per namespace; unmount is not disconnect.
4. Need strict lifecycle? Use `socket.io-client` + `useEffect` cleanup.
5. No automatic token refresh; reconnect with the new token.
6. Resync state after reconnect.

---

## Practice exercises

1. **Basic:** show connection status and the last notification.
2. **Intermediate:** build a live feed (max 50 items) with `onMessage`.
3. **Lifecycle:** build the same dashboard with `useEffect` and confirm disconnect on unmount in the Network tab.
4. **Auth:** add token handshake validation in NestJS and reject a bad token.
5. **Advanced:** make a chat feature with acknowledgements, reconnect banner and REST resync.
6. **Architecture:** wrap everything behind `useAppSocket` so the library can be replaced.

## Interview questions

- What does `IoProvider` do and why is it needed?
- Why might a socket stay connected after a component unmounts?
- `lastMessage` vs `onMessage`: when do you use each?
- How do you handle expired access tokens with a long-lived socket?
- Why does a socket connect twice in development?
- How would you design a reusable real-time layer so you can swap libraries?

## References

- socket.io-react-hook on npm (latest listed 2.4.5)
- Socket.IO client documentation
- NestJS WebSockets (gateways) documentation
