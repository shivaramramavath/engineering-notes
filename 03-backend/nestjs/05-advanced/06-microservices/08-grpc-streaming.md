# gRPC Streaming Patterns in NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 06-microservices
- **Prerequisites**: [06-grpc-fundamentals-and-protobuf](./06-grpc-fundamentals-and-protobuf.md), [07-grpc-server-and-client](./07-grpc-server-and-client.md)

---

## 1. Context & Purpose

While unary RPC (one request $\rightarrow$ one response) satisfies standard CRUD queries, high-throughput data transfer often requires continuous streams:
- Large dataset export (streaming 100,000 database rows without loading them into memory).
- Live financial ticker updates or telemetry streams.
- Client file chunk uploads over a single HTTP/2 multiplexed stream.

gRPC supports three streaming modes beyond simple unary calls: **Server Streaming**, **Client Streaming**, and **Bi-directional Streaming**. In NestJS, gRPC streams integrate natively with **RxJS `Observable`** pipelines and `@GrpcStreamMethod()`.

---

## 2. Theoretical Foundations

### 2.1 The Four gRPC Communication Models
1. **Unary RPC**: Single request $\rightarrow$ Single response.
   `rpc GetUser (UserRequest) returns (UserResponse);`
2. **Server Streaming RPC**: Single request $\rightarrow$ Stream of responses.
   `rpc StreamPrices (MarketRequest) returns (stream PricePoint);`
3. **Client Streaming RPC**: Stream of requests $\rightarrow$ Single aggregated response.
   `rpc UploadMetrics (stream Metric) returns (UploadSummary);`
4. **Bi-directional Streaming RPC**: Stream of requests $\leftrightarrow$ Stream of responses independently.
   `rpc Chat (stream ChatMessage) returns (stream ChatMessage);`

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   gRPC Server Stream                   │
│   Client sends: { symbol: "BTC" }                      │
│                                                        │
│   Server returns RxJS Observable:                      │
│   ──(Price 60k)──(Price 61k)──(Price 59k)──| (Complete)│
└───────────────────────────┬────────────────────────────┘
                            │
               Continuous HTTP/2 Data Frames
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                   gRPC Client Consumer                 │
│      client.streamPrices({ symbol: "BTC" })            │
│      .subscribe({ next: (chunk) => ... })              │
└────────────────────────────────────────────────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Protobuf Definition with Streams

```protobuf
// src/proto/analytics.proto
syntax = "proto3";

package analytics;

service AnalyticsService {
  // Server streaming RPC
  rpc StreamTransactions (TransactionFilter) returns (stream Transaction);

  // Bi-directional streaming RPC
  rpc LiveExchange (stream PriceRequest) returns (stream PriceResponse);
}

message TransactionFilter {
  string accountId = 1;
}

message Transaction {
  string id = 1;
  double amount = 2;
  string timestamp = 3;
}

message PriceRequest {
  string symbol = 1;
}

message PriceResponse {
  string symbol = 1;
  double price = 2;
}
```

---

### 4.2 Implementing Server Streaming with RxJS

In the gRPC server controller, return an RxJS `Observable`:

```typescript
// src/analytics/analytics.controller.ts
import { Controller } from '@nestjs/common';
import { GrpcMethod } from '@nestjs/microservices';
import { Observable, from } from 'rxjs';
import { map } from 'rxjs/operators';

@Controller()
export class AnalyticsController {
  // Server Streaming RPC: returns an Observable stream of chunks
  @GrpcMethod('AnalyticsService', 'StreamTransactions')
  streamTransactions(filter: { accountId: string }): Observable<any> {
    console.log(`Streaming transactions for account: ${filter.accountId}`);

    const transactions = [
      { id: 'tx_1', amount: 150.0, timestamp: new Date().toISOString() },
      { id: 'tx_2', amount: 24.5, timestamp: new Date().toISOString() },
      { id: 'tx_3', amount: 99.0, timestamp: new Date().toISOString() },
    ];

    // Emits each transaction item as an individual gRPC HTTP/2 frame
    return from(transactions);
  }
}
```

---

### 4.3 Implementing Bi-Directional Streaming with `@GrpcStreamMethod()`

When both client and server stream simultaneously:

```typescript
// src/analytics/analytics.controller.ts (Continued)
import { GrpcStreamMethod } from '@nestjs/microservices';
import { Observable } from 'rxjs';

@Controller()
export class BiDirectionalAnalyticsController {
  // Bi-directional streaming: receives an Observable of requests and returns an Observable of responses
  @GrpcStreamMethod('AnalyticsService', 'LiveExchange')
  liveExchange(messages$: Observable<{ symbol: string }>): Observable<{ symbol: string; price: number }> {
    return new Observable((subscriber) => {
      // Listen to incoming client request stream
      const subscription = messages$.subscribe({
        next: (req) => {
          console.log(`Client requested price for: ${req.symbol}`);
          // Send price back immediately
          subscriber.next({
            symbol: req.symbol,
            price: Number((Math.random() * 100).toFixed(2)),
          });
        },
        error: (err) => subscriber.error(err),
        complete: () => subscriber.complete(),
      });

      return () => subscription.unsubscribe();
    });
  }
}
```

---

### 4.4 Consuming a gRPC Stream on the Client

```typescript
// src/analytics/analytics-client.service.ts
import { Injectable, OnModuleInit } from '@nestjs/common';
import { ClientGrpc } from '@nestjs/microservices';
import { Observable } from 'rxjs';

interface AnalyticsGrpcClient {
  streamTransactions(filter: { accountId: string }): Observable<any>;
}

@Injectable()
export class AnalyticsClientService implements OnModuleInit {
  private analyticsClient!: AnalyticsGrpcClient;

  constructor(private readonly client: ClientGrpc) {}

  onModuleInit() {
    this.analyticsClient = this.client.getService<AnalyticsGrpcClient>('AnalyticsService');
  }

  listenToTransactions(accountId: string) {
    // Consume stream chunk-by-chunk without buffering entire payload into RAM
    this.analyticsClient.streamTransactions({ accountId }).subscribe({
      next: (tx) => {
        console.log(`Received transaction chunk: ${tx.id} for $${tx.amount}`);
      },
      error: (err) => console.error('Stream error:', err),
      complete: () => console.log('Stream completed by server.'),
    });
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Unhandled Stream Errors
If the server's RxJS stream emits an error, the gRPC connection terminates abruptly with status `13 INTERNAL` or `2 UNKNOWN`.
- **Handling**: Catch errors inside RxJS operator pipes (`catchError`) and emit a clean gRPC status code.

### 5.2 Client Disconnect Midway Through Stream
If a client terminates its connection while the server is streaming 100,000 items, the server must stop reading from the database.
- **Handling**: In RxJS, always return a teardown logic function in `new Observable(...)` to close database cursors when the client unsubscribes.

---

## 6. Performance & Memory Implications

- **Flat Memory Usage**: Streaming 10GB of data via chunks consumes only a few kilobytes of Node.js RAM at any given moment.
- **Backpressure**: HTTP/2 window updates automatically throttle the server stream if the client cannot read as fast as the server produces.

---

## 7. Anti-Patterns & Common Mistakes

- **Buffering Entire Data Arrays Before Emitting**: Gathering 100,000 database entities in an array and doing `from(hugeArray)`. This loads the entire dataset into memory before streaming, defeating the memory advantage of streams. Use database cursors (e.g. TypeORM query runner stream).
- **Forgetting `stream` Keyword in Protobuf**: Writing `returns (Transaction)` instead of `returns (stream Transaction)`. Without `stream`, gRPC expects a single response.

---

## 8. Production Checklist & Operational Readiness

- [ ] Database cursors used for large server streams.
- [ ] Client unsubscriptions cleanly tear down server database queries.
- [ ] Maximum receive and send message sizes configured on gRPC channel options if chunks exceed 4MB.

---

## 9. Migration & Version History

- **NestJS 7/8**: Introduced `@GrpcStreamMethod()` decorator for bi-directional streaming.

---

## 10. Verification & Testing

Testing a server stream with RxJS `toArray()`:

```typescript
import { AnalyticsController } from './analytics.controller';
import { firstValueFrom } from 'rxjs';
import { toArray } from 'rxjs/operators';

describe('AnalyticsController (gRPC Stream)', () => {
  it('streams all transaction items sequentially', async () => {
    const controller = new AnalyticsController();
    const stream$ = controller.streamTransactions({ accountId: 'acc_1' });

    const allTransactions = await firstValueFrom(stream$.pipe(toArray()));
    expect(allTransactions.length).toBe(3);
    expect(allTransactions[0].id).toBe('tx_1');
  });
});
```

---

## 11. Comprehensive Examples

### Database Cursor Streaming Pattern

```typescript
// src/reports/cursor-stream.service.ts
import { Injectable } from '@nestjs/common';
import { DataSource } from 'typeorm';
import { Observable } from 'rxjs';

@Injectable()
export class CursorStreamService {
  constructor(private readonly dataSource: DataSource) {}

  streamLargeReport(): Observable<any> {
    return new Observable((subscriber) => {
      const queryRunner = this.dataSource.createQueryRunner();

      (async () => {
        await queryRunner.connect();
        const stream = await queryRunner.stream('SELECT id, amount, created_at FROM orders');

        stream.on('data', (row) => subscriber.next(row));
        stream.on('end', async () => {
          subscriber.complete();
          await queryRunner.release();
        });
        stream.on('error', async (err) => {
          subscriber.error(err);
          await queryRunner.release();
        });
      })();

      return () => {
        // Cleanup if client cancels stream
        queryRunner.release();
      };
    });
  }
}
```

---

## 12. Cross-References & Further Reading

- [06-grpc-fundamentals-and-protobuf.md](./06-grpc-fundamentals-and-protobuf.md) - Protobuf syntax
- [07-grpc-server-and-client.md](./07-grpc-server-and-client.md) - gRPC clients
- [00-prerequisites/03-nodejs-async-and-event-loop.md](../../00-prerequisites/03-nodejs-async-and-event-loop.md) - Node streams
- [gRPC Streaming Best Practices](https://grpc.io/docs/guides/concepts/)
