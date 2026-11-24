# Serving Static Assets and Server-Sent Events (SSE)

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 02-fundamentals
- **Prerequisites**: [03-controllers](./03-controllers.md), [08-response-handling](./08-response-handling.md), [00-prerequisites/03-nodejs-async-and-event-loop](../00-prerequisites/03-nodejs-async-and-event-loop.md)

---

## 1. Context & Purpose

Backend applications frequently need to deliver two non-standard HTTP payloads:
1. **Static Assets**: Serving client-side assets (images, documentation builds, uploaded avatars, Single Page Application bundles).
2. **Server-Sent Events (SSE)**: Pushing real-time, unidirectional event streams from the server to client browsers over standard HTTP without the overhead, handshake complexity, or firewall traversal issues of WebSockets.

NestJS provides dedicated, idiomatic modules for both: `@nestjs/serve-static` for asset caching and SPA routing, and the `@Sse()` decorator powered by RxJS `Observable` streams for reactive event push.

---

## 2. Theoretical Foundations

### 2.1 Static Asset Delivery
Serving static files involves caching headers (`Cache-Control`, `ETag`, `Last-Modified`), conditional GET requests (`If-None-Match`), and Single Page Application (SPA) fallback routing (redirecting unmatched routes to `index.html`).

### 2.2 Server-Sent Events (SSE)
SSE is a standard HTTP protocol (`text/event-stream`) defined in the HTML5 specification:
- **Unidirectional**: Server pushes events to the browser.
- **Transport**: Standard persistent HTTP connection (HTTP/1.1 or multiplexed over HTTP/2).
- **Format**: Plain text lines formatted as `data: ...\n\n`, with optional `id`, `event`, and `retry` fields.
- **Client Auto-Reconnect**: Browser `EventSource` handles reconnections natively without client libraries.

---

## 3. Architecture & Internal Mechanics

```
Static Asset Request:
Client ──> [ GET /assets/logo.png ] ──> [ ServeStaticModule ] ──> [ File System / Disk Cache ]
Client ──> [ GET /dashboard/orders ] ──> [ ServeStaticModule ] ──> [ Render index.html (SPA) ]

Server-Sent Events (SSE):
Client ──> [ GET /notifications/stream ] (Accept: text/event-stream)
                │
                ▼
      @Sse() Route Handler
                │
                ▼
   Returns Observable<MessageEvent>
                │
         interval / event emit
                │
                ▼
   Chunked HTTP Response Stream
   data: {"message":"Order created"}\n\n
```

---

## 4. Concrete Implementation

### 4.1 Serving Static Assets with `@nestjs/serve-static`

Install the package:
```bash
npm install --save @nestjs/serve-static
```

Configure `ServeStaticModule` in `app.module.ts`:

```typescript
// src/app.module.ts
import { Module } from '@nestjs/common';
import { ServeStaticModule } from '@nestjs/serve-static';
import { join } from 'path';

@Module({
  imports: [
    ServeStaticModule.forRoot({
      rootPath: join(__dirname, '..', 'public'),
      serveRoot: '/static', // Mount path: http://localhost:3000/static/...
      exclude: ['/api*'],    // Do not intercept backend API routes
      serveStaticOptions: {
        maxAge: 86400000,    // 1 day browser cache in ms
        etag: true,
      },
    }),
  ],
})
export class AppModule {}
```

---

### 4.2 Streaming Events with `@Sse()`

The `@Sse()` decorator transforms a controller method returning an `Observable<MessageEvent>` into a continuous `text/event-stream` endpoint.

```typescript
// src/notifications/notifications.controller.ts
import { Controller, Sse, MessageEvent } from '@nestjs/common';
import { Observable, interval } from 'rxjs';
import { map } from 'rxjs/operators';

@Controller('notifications')
export class NotificationsController {
  // Streams a heartbeat or ticker event every 2 seconds
  @Sse('stream')
  streamEvents(): Observable<MessageEvent> {
    return interval(2000).pipe(
      map((sequence) => ({
        data: {
          sequence,
          timestamp: new Date().toISOString(),
          status: 'SYSTEM_OK',
        },
        id: String(sequence),
        type: 'heartbeat',
        retry: 5000, // Client re-attempts connection after 5s if dropped
      })),
    );
  }
}
```

#### Frontend Consumer (Browser EventSource)
```javascript
const eventSource = new EventSource('/notifications/stream');

eventSource.addEventListener('heartbeat', (event) => {
  const data = JSON.parse(event.data);
  console.log('Received Heartbeat:', data);
});

eventSource.onerror = (err) => {
  console.error('SSE connection error:', err);
};
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Route Shadowing in Static Serving
If `serveRoot` is omitted or configured as `'/'`, and `exclude` does not cover API endpoints, static middleware intercepts unmatched API requests and serves `index.html` with a 200 OK status instead of returning a 404 JSON response.
- **Prevention**: Always configure `serveRoot: '/static'` or specify `exclude: ['/api/(.*)']`.

### 5.2 Reverse Proxy Buffer Truncation (Nginx / Cloudflare)
Reverse proxies buffer HTTP responses by default. In an SSE stream, buffering blocks events until the proxy buffer (usually 4KB or 16KB) fills up, destroying real-time behavior.
- **Remedy**: Disable buffering via response headers:
```typescript
res.setHeader('X-Accel-Buffering', 'no'); // Nginx
res.setHeader('Cache-Control', 'no-cache');
```

### 5.3 Memory Leaks with Unsubscribed SSE Streams
If the client disconnects, the underlying RxJS `Observable` must terminate to avoid memory leaks. NestJS automatically unsubscribes from the observable when the underlying HTTP socket closes. However, if using custom Subject pipelines, ensure proper teardown.

---

## 6. Performance & Memory Implications

- **Static Assets in Production**: While NestJS can serve static files, production architectures should offload static assets to Object Storage (AWS S3, Cloudflare R2) and a CDN (Cloudflare, CloudFront) to free Node.js event loop capacity.
- **SSE Connection Density**: Each open SSE connection holds an open TCP socket and file descriptor. On Node.js, 10,000 idle SSE connections consume ~250–350 MB of RAM.

---

## 7. Anti-Patterns & Common Mistakes

- **Using WebSockets When SSE is Sufficient**: Introducing bidirectional WebSocket complexity (heartbeats, reconnect logic, state management) for purely server-to-client notifications (e.g., job progress bars, stock tickers).
- **Neglecting Cache-Control on Static Assets**: Serving static files without ETags or max-age headers, causing repeated disk reads on every request.
- **Blocking the Event Loop in SSE Pipes**: Performing heavy synchronous computations inside the RxJS `map` operator of an SSE pipeline.

---

## 8. Production Checklist & Operational Readiness

- [ ] `ServeStaticModule` configured with explicit `serveRoot` and API exclusion rules.
- [ ] Reverse proxy (Nginx / ALB) configured with `proxy_buffering off;` for SSE routes.
- [ ] HTTP/2 enabled on load balancers to allow multiplexing SSE connections over a single TCP connection.
- [ ] SSE streams include heartbeat messages every 15–30 seconds to prevent NAT timeout drops.

---

## 9. Migration & Version History

- **NestJS 7.x**: Introduced the official `@Sse()` decorator.
- **NestJS 9.x**: Enhanced `ServeStaticModule` with Fastify adapter support.
- **NestJS 11.x**: Fully validated against Express 5 stream pipelines and Node 22 event loop mechanics.

---

## 10. Verification & Testing

Unit testing an SSE controller method:

```typescript
import { Test } from '@nestjs/testing';
import { NotificationsController } from './notifications.controller';
import { firstValueFrom } from 'rxjs';

describe('NotificationsController (SSE)', () => {
  let controller: NotificationsController;

  beforeEach(async () => {
    const module = await Test.createTestingModule({
      controllers: [NotificationsController],
    }).compile();

    controller = module.get<NotificationsController>(NotificationsController);
  });

  it('emits properly formatted MessageEvent objects', async () => {
    const stream$ = controller.streamEvents();
    const event = await firstValueFrom(stream$);

    expect(event).toBeDefined();
    expect(event.type).toBe('heartbeat');
    expect(event.retry).toBe(5000);
  });
});
```

---

## 11. Comprehensive Examples

### Live Job Progress Stream via Subject

```typescript
// src/jobs/jobs.controller.ts
import { Controller, Sse, Param, MessageEvent } from '@nestjs/common';
import { Observable, Subject } from 'rxjs';
import { map, filter } from 'rxjs/operators';

interface JobProgressEvent {
  jobId: string;
  progress: number;
  status: 'RUNNING' | 'COMPLETED' | 'FAILED';
}

@Controller('jobs')
export class JobsController {
  private readonly progress$ = new Subject<JobProgressEvent>();

  // External services push progress updates here
  emitProgress(event: JobProgressEvent) {
    this.progress$.next(event);
  }

  @Sse(':id/progress')
  streamJobProgress(@Param('id') jobId: string): Observable<MessageEvent> {
    return this.progress$.pipe(
      filter((e) => e.jobId === jobId),
      map((e) => ({
        data: e,
        id: `${e.jobId}-${e.progress}`,
        type: 'job_progress',
      })),
    );
  }
}
```

---

## 12. Cross-References & Further Reading

- [08-response-handling.md](./08-response-handling.md) - Standard HTTP responses
- [14-streaming-files.md](./14-streaming-files.md) - File download streaming
- [05-advanced/04-realtime/01-websockets-gateways.md](../05-advanced/04-realtime/01-websockets-gateways.md) - Bidirectional WebSockets
- [MDN Server-Sent Events Guide](https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events)
