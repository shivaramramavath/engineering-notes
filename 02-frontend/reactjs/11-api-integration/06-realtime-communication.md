# Realtime Communication

HTTP is request/response: the client asks, the server answers. **Realtime** features (chat, live dashboards, notifications, collaborative editing, progress bars) need the *server* to push data when something happens. There are four main techniques, and picking the simplest one that works matters more than picking the fanciest.

## The options

```text
Polling         client ──ask──► server   (repeat every N seconds)
Long polling    client ──ask──► server ···waits··· ──answer──► (ask again)
SSE             client ◄════ one-way stream of events ════ server
WebSocket       client ◄═══════ two-way connection ═══════► server
```

| | Direction | Transport | Auto-reconnect | Use for |
|---|---|---|---|---|
| **Polling** | Client → server | Plain HTTP | n/a | Data that changes slowly; "good enough" freshness |
| **SSE** (Server-Sent Events) | Server → client | HTTP stream | **Built in** | Notifications, live feeds, job progress, streaming text |
| **WebSocket** | Both ways | Dedicated connection | **You build it** | Chat, multiplayer, collaboration, anything chatty both ways |

**Decision rule:** start with polling. Move to SSE if you need server push and data flows one way. Use WebSockets when the client also sends frequent messages or you need very low latency in both directions.

## Polling

Often enough, and by far the simplest. With [TanStack Query](../12-server-state/03-tanstack-query.md) it's one option:

```tsx
useQuery({
  queryKey: ["job", jobId],
  queryFn: ({ signal }) => jobsApi.get(jobId, signal),
  refetchInterval: (query) => (query.state.data?.status === "done" ? false : 2000),
})
```

Returning `false` stops polling when the job finishes. By default polling pauses when the tab is in the background (you can change that with `refetchIntervalInBackground`). It reuses your existing auth, error handling, and caching ([04](./04-refresh-token-flow.md), [05](./05-api-error-handling.md)), with no new infrastructure.

Cost: latency up to the interval, plus wasted requests when nothing changed. Fine for dashboards and progress bars; wrong for chat.

## Server-Sent Events (SSE)

A long-lived HTTP response that the server keeps writing to (`Content-Type: text/event-stream`). The browser provides `EventSource`:

```ts
const source = new EventSource("/api/notifications/stream", { withCredentials: true })

source.addEventListener("notification", (e) => {
  const data = JSON.parse((e as MessageEvent).data)
  // handle
})

source.onerror = () => { /* the browser is already retrying */ }

source.close()   // when done
```

Strengths: works over plain HTTP (proxies, HTTP/2, CDNs are generally fine), **reconnects automatically**, and can resume from the last event via the `Last-Event-ID` header if the server supports it.

Limits to know:

- **One direction.** Client-to-server messages are ordinary HTTP requests.
- **`EventSource` can't set custom headers**, so you can't send `Authorization: Bearer …`. Use cookies (`withCredentials`), or stream with `fetch` instead (`res.body` is a `ReadableStream` you read and parse yourself; libraries wrap this).
- Over HTTP/1.1, browsers cap concurrent connections per domain (about six). Multiple tabs each holding an SSE stream can exhaust it. HTTP/2 largely removes this.
- Text only (UTF-8).

SSE is also the transport behind token-by-token streaming of AI responses.

## WebSockets

A persistent, bidirectional connection started with an HTTP upgrade:

```ts
const ws = new WebSocket("wss://api.example.com/ws")

ws.onopen    = () => ws.send(JSON.stringify({ type: "subscribe", room: "general" }))
ws.onmessage = (e) => handle(JSON.parse(e.data))
ws.onclose   = (e) => { /* e.code, e.wasClean */ }
ws.onerror   = () => { /* details are intentionally not exposed */ }
```

Use `wss://` (TLS) in production. The native API gives you **only the socket**. These come to you as your problem:

- **Reconnection** with backoff
- **Heartbeats** to detect dead connections (proxies can silently drop idle sockets)
- **Authentication**: browsers **can't set custom headers on a WebSocket**, so use a cookie, or fetch a **short-lived, single-use ticket** from your API and pass it as a query param or first message. Avoid putting your long-lived access token in the URL, because URLs get logged.
- **Message protocol** and typing
- **Re-subscribing** after a reconnect

### A typed message protocol

```ts
type ServerMessage =
  | { type: "message.created"; roomId: string; message: ChatMessage }
  | { type: "user.typing"; roomId: string; userId: string }
  | { type: "presence"; online: string[] }

type ClientMessage =
  | { type: "subscribe"; roomId: string }
  | { type: "message.send"; roomId: string; text: string }
```

Discriminated unions give you exhaustive `switch (msg.type)` handling and catch protocol drift at compile time. (As always with data from the network, validate with a schema if you can't trust the sender.)

### A reconnecting WebSocket hook

```tsx
function useWebSocket(url: string, onMessage: (msg: ServerMessage) => void) {
  const onMessageRef = useRef(onMessage)
  useEffect(() => { onMessageRef.current = onMessage })      // latest handler, no reconnect on change

  const [status, setStatus] = useState<"connecting" | "open" | "closed">("connecting")
  const sendRef = useRef<(m: ClientMessage) => void>(() => {})

  useEffect(() => {
    let ws: WebSocket | null = null
    let retry = 0
    let timer: ReturnType<typeof setTimeout>
    let stopped = false

    function connect() {
      setStatus("connecting")
      ws = new WebSocket(url)

      ws.onopen = () => { retry = 0; setStatus("open") }
      ws.onmessage = (e) => onMessageRef.current(JSON.parse(e.data))
      ws.onclose = () => {
        setStatus("closed")
        if (stopped) return
        const delay = Math.min(30_000, 1000 * 2 ** retry++) * (0.5 + Math.random() / 2)   // backoff + jitter
        timer = setTimeout(connect, delay)
      }
    }

    sendRef.current = (m) => { if (ws?.readyState === WebSocket.OPEN) ws.send(JSON.stringify(m)) }
    connect()

    return () => {                       // cleanup: also runs between StrictMode's double-mount
      stopped = true
      clearTimeout(timer)
      ws?.close()
    }
  }, [url])

  return { status, send: (m: ClientMessage) => sendRef.current(m) }
}
```

Key points:

- **Cleanup is essential.** Without it each mount leaks a connection. In development StrictMode mounts, unmounts, and remounts, so a leak shows up as duplicate messages immediately. That's useful, not annoying.
- **The `stopped` flag** stops the reconnect loop from resurrecting a socket you intentionally closed.
- **Backoff with jitter** prevents a "reconnect storm" when a server restarts and thousands of clients return at the same instant.
- **The latest-handler ref** means changing `onMessage` doesn't tear down the connection.
- After reconnecting you must **re-subscribe** and **catch up**: you've missed messages while disconnected. Refetch the data via your REST API (or request events since the last ID) rather than assuming the stream is gapless.

For production, consider a maintained library (`react-use-websocket`, or Socket.IO for rooms, acknowledgements, and fallbacks, which requires a Socket.IO server since it is **not** plain WebSocket). A solid hand-rolled hook is fine for simple cases.

### One connection, not one per component

Open the socket **once** (an app-level provider or a module singleton), and let components subscribe to the events they care about. A socket per component multiplies server load and ordering problems. A singleton can be exposed to React with [`useSyncExternalStore`](../16-advanced-react/02-external-stores.md).

## Wiring events into your data layer

The pattern that scales: **the socket doesn't own data; it tells the cache what changed.** Keep [TanStack Query](../12-server-state/03-tanstack-query.md) as the source of truth.

```tsx
function RealtimeBridge() {
  const queryClient = useQueryClient()

  useWebSocket(WS_URL, (msg) => {
    switch (msg.type) {
      case "message.created":
        queryClient.setQueryData<ChatMessage[]>(["messages", msg.roomId], (old = []) =>
          old.some((m) => m.id === msg.message.id) ? old : [...old, msg.message]   // dedupe
        )
        break
      case "presence":
        queryClient.setQueryData(["presence"], msg.online)
        break
    }
  })

  return null
}
```

Two strategies:

| Strategy | When |
|---|---|
| **`setQueryData`**: apply the event directly | Events carry the full new data; you want instant UI |
| **`invalidateQueries`**: mark stale, refetch | Events are just "something changed"; correctness over speed |

Invalidation is simpler and always correct, with an extra request per event. Patching is faster but you must handle ordering, duplicates, and missed events. Many apps patch hot paths (chat messages) and invalidate the rest.

Handle **duplicates and ordering**: your own optimistic send, the server echo, and a refetch can all deliver the same message, so key by ID. See [optimistic updates](../12-server-state/06-optimistic-updates.md).

## Choosing in practice

| Feature | Reasonable choice |
|---|---|
| Job / upload progress | Polling, or SSE |
| Notification badge | SSE (or polling every 30–60s) |
| Live dashboard | SSE or polling |
| Streaming AI/LLM text | SSE / streamed `fetch` |
| Chat, typing indicators, presence | WebSocket |
| Collaborative editing, multiplayer | WebSocket (often with a CRDT/OT layer) |
| "Is a new version deployed?" | Polling a version file |

A worked end-to-end example lives in [the realtime chat project](../22-projects/09-realtime-chat-app/).

## Production concerns

- **Auth expiry**: a socket outlives the access token. Decide what happens when it expires: close and reconnect with a new ticket, or have the server push a re-auth request.
- **Proxies and load balancers** need WebSocket/SSE support and sensible idle timeouts. Send heartbeats (ping/pong or SSE comment lines) to keep connections alive.
- **Scale**: each connection holds server resources. Backend fan-out (pub/sub across instances) is a server concern, but design clients to **tolerate reconnects**.
- **Backpressure**: a firehose of events can overwhelm rendering. Batch updates (apply once per animation frame or every ~100ms) and coalesce redundant ones. See [rendering performance](../14-performance/01-rendering-performance.md).
- **Background tabs and mobile**: browsers throttle timers and may kill connections. Reconnect and resync on `visibilitychange` / `online` events.
- **Security**: validate incoming messages, never trust client-supplied room IDs on the server, and don't send secrets over channels you haven't authorized.

## Debugging

- **Browser DevTools → Network → WS** shows frames in both directions; filter to "EventStream" for SSE.
- **Duplicate messages in dev** → missing cleanup (StrictMode exposes it).
- **Connects but never receives** → subscribe message not sent after reconnect, or auth ticket rejected silently.
- **Works locally, drops in production after ~60s** → a proxy idle timeout; add heartbeats.
- **`EventSource` loops on errors** → the server returned a non-200 or the wrong `Content-Type`; the browser keeps retrying.
- **CORS errors with SSE** → `withCredentials` requires explicit allowed origin and credentials headers, same as [fetch](./00-fetch.md#headers-credentials-and-cors).

## Common mistakes

- **Reaching for WebSockets first** when polling or SSE is enough.
- **No cleanup** (leaked sockets, duplicate handlers).
- **No reconnect logic**, so the UI silently goes stale after a network blip.
- **Reconnecting immediately in a tight loop**; always back off with jitter.
- **Assuming no messages were missed** across a reconnect.
- **Creating a socket per component** or inside render.
- **Long-lived tokens in the WebSocket URL.**
- **Storing realtime data in a separate store** that fights the query cache.
- **Trusting message shape** without a typed protocol or validation.
- **Reconnecting after intentional close**, or re-subscribing twice.
- **Updating React state per message** at high frequency without batching.

## Quick summary

- Start simple: **polling** → **SSE** (server push, one-way, auto-reconnect) → **WebSocket** (two-way, you handle the hard parts).
- WebSockets need reconnect with backoff + jitter, heartbeats, cleanup, a typed protocol, and an auth strategy that avoids headers.
- Use **one** shared connection; let the socket *update the query cache* (`setQueryData` or `invalidateQueries`) rather than owning data.
- After any reconnect, resubscribe and resync.
- Batch high-frequency updates and test behind real proxies.

## Next

Continue to [12 — Server state](../12-server-state/README.md).
