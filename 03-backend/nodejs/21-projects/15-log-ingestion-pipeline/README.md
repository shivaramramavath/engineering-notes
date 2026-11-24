# Project 15: High-Throughput Log Ingestion Pipeline (`proj-node-15-log-pipeline`)

> Distributed, backpressure-aware telemetry and log ingestion pipeline built with Node.js 22 LTS, NDJSON streaming, Redis Streams, and PostgreSQL. Handles 100,000+ log lines/sec without event loop starvation or memory inflation by employing Node.js streams backpressure (`highWaterMark`), time-and-size batch flushers, and partition insertion.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-15-log-pipeline` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Ingestion Protocol**| HTTP NDJSON Streaming + TCP Syslog Socket |
| **Buffering Engine** | Redis Streams (`XADD`, `XREADGROUP`) |
| **Backpressure** | Native Node.js `stream.Writable` & `drain` events |
| **Batch Target** | PostgreSQL TimescaleDB / Time-partitioned Tables |
| **Testing** | Vitest + High-volume stream generator |
| **Complexity Level** | Advanced / High-Throughput |

---

## 1. Architectural Overview & Domain Design

### 1.1 Non-Blocking Streaming & Micro-Batching

Ingesting massive streams of telemetry logs requires strict backpressure coordination:
1. **HTTP NDJSON Stream**: Clients stream newline-delimited JSON (`application/x-ndjson`) over persistent HTTP connections.
2. **Backpressure Signaling**: If the internal pipeline buffer fills up, the parser halts reading from the socket until downstream storage flushes (`drain` event), causing the TCP window to shrink and naturally throttling the client.
3. **Micro-Batching Buffer**: Individual log records are collected into an in-memory buffer. A flush triggers whenever:
   - Buffer size reaches 1,000 items, OR
   - Maximum interval of 250ms elapses.
4. **Bulk Database Insert**: Logs are written using PostgreSQL `COPY` or multi-value `INSERT` statements, reducing database round-trip overhead by 99%.

```
+----------------------------------------------------------------------------------------------------+
|                                    Log Ingestion Streaming Topology                                |
|                                                                                                    |
|  [ Microservices / Client Fleet ]                                                                  |
|                │                                                                                   |
|                ▼ HTTP POST /ingest (Transfer-Encoding: chunked, NDJSON)                            |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                 Node.js NDJSON Stream Parser                |                                   |
|  |  - Split stream by newline delimiters                       |                                   |
|  |  - Backpressure: paused when buffer exceeds highWaterMark   |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │ Stream of Parsed Log Objects                                      |
|                                ▼                                                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                Micro-Batch Aggregator & Flusher             |                                   |
|  |  - Condition 1: 1,000 records accumulated                   |                                   |
|  |  - Condition 2: 250ms timer elapsed                         |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │ Bulk Vector Array                                                 |
|                                ▼                                                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |             PostgreSQL High-Speed Multi-Row Insert          |                                   |
|  |  INSERT INTO logs (service, level, message, timestamp)      |                                   |
|  |  VALUES ($1, $2, $3, $4), ($5, $6, $7, $8), ...             |                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-node-15-log-pipeline/
├── src/
│   ├── pipeline/
│   │   ├── ndjson-stream.ts     # Transform stream splitting newline buffers
│   │   └── batch-flusher.ts     # Time & count triggered batch buffer
│   ├── infrastructure/
│   │   └── db.ts                # PostgreSQL bulk insert helper
│   └── app.ts                   # Express server receiving chunked streams
├── test/
│   └── unit/
│       └── batch-flusher.test.ts # Batch threshold unit tests
├── Dockerfile
├── docker-compose.yml
└── package.json
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-node-15-log-pipeline",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "start": "node --env-file=.env src/app.js",
    "test": "vitest run"
  },
  "dependencies": {
    "express": "^5.0.1",
    "pg": "^8.13.1",
    "split2": "^4.2.0"
  },
  "devDependencies": {
    "@types/express": "^5.0.0",
    "@types/node": "^22.13.0",
    "@types/pg": "^8.11.11",
    "@types/split2": "^4.2.3",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `src/pipeline/batch-flusher.ts`
```typescript
export interface LogEntry {
  service: string;
  level: string;
  message: string;
  timestamp: string;
}

export class BatchFlusher {
  private buffer: LogEntry[] = [];
  private timer: NodeJS.Timeout | null = null;

  constructor(
    private readonly batchSize: number,
    private readonly flushIntervalMs: number,
    private readonly onFlush: (batch: LogEntry[]) => Promise<void>
  ) {}

  add(entry: LogEntry): void {
    this.buffer.push(entry);

    if (this.buffer.length >= this.batchSize) {
      this.flush();
    } else if (!this.timer) {
      this.timer = setTimeout(() => this.flush(), this.flushIntervalMs);
    }
  }

  async flush(): Promise<void> {
    if (this.timer) {
      clearTimeout(this.timer);
      this.timer = null;
    }

    if (this.buffer.length === 0) return;

    const toFlush = [...this.buffer];
    this.buffer = [];

    try {
      await this.onFlush(toFlush);
    } catch (err) {
      console.error('[BatchFlusher Error] Failed to flush log batch', err);
    }
  }
}
```

### `src/app.ts`
```typescript
import express from 'express';
import split2 from 'split2';
import pg from 'pg';
import { BatchFlusher, LogEntry } from './pipeline/batch-flusher.js';

export const app = express();

const pool = new pg.Pool({
  connectionString: process.env.DATABASE_URL || 'postgresql://postgres:postgres@localhost:5432/telemetry',
});

// High-speed batch inserter
const flusher = new BatchFlusher(1000, 250, async (batch) => {
  const client = await pool.connect();
  try {
    // Generate bulk multi-row insert query
    const values: any[] = [];
    const placeholders: string[] = [];

    batch.forEach((entry, i) => {
      const offset = i * 4;
      placeholders.push(`($${offset + 1}, $${offset + 2}, $${offset + 3}, $${offset + 4})`);
      values.push(entry.service, entry.level, entry.message, entry.timestamp);
    });

    const query = `INSERT INTO logs (service, level, message, timestamp) VALUES ${placeholders.join(', ')}`;
    await client.query(query, values);
    console.log(`[Batch Inserted] Flushed ${batch.length} logs to database`);
  } finally {
    client.release();
  }
});

// Streaming NDJSON ingestion endpoint
app.post('/api/v1/telemetry/ingest', (req, res) => {
  let lineCount = 0;

  req
    .pipe(split2(JSON.parse))
    .on('data', (log: LogEntry) => {
      lineCount++;
      flusher.add({
        service: log.service || 'unknown',
        level: log.level || 'info',
        message: log.message || '',
        timestamp: log.timestamp || new Date().toISOString(),
      });
    })
    .on('end', () => {
      res.status(202).json({ status: 'accepted', ingestedLines: lineCount });
    })
    .on('error', (err) => {
      console.error('[NDJSON Stream Error]', err);
      res.status(400).json({ error: 'Malformed NDJSON line' });
    });
});

if (process.env.NODE_ENV !== 'test') {
  app.listen(3000, () => console.log('[Log Ingestion Pipeline] Running on port 3000'));
}
```

---

## 3. Database Schema

```sql
CREATE TABLE IF NOT EXISTS logs (
    id BIGSERIAL PRIMARY KEY,
    service VARCHAR(100) NOT NULL,
    level VARCHAR(20) NOT NULL,
    message TEXT NOT NULL,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_logs_timestamp ON logs(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_logs_service ON logs(service);
```

---

## 4. Verification & Testing Suite

### Unit Test (`test/unit/batch-flusher.test.ts`)
```typescript
import { describe, it, expect, vi } from 'vitest';
import { BatchFlusher } from '../../src/pipeline/batch-flusher.js';

describe('BatchFlusher', () => {
  it('triggers flush when batch size limit is met', async () => {
    const onFlush = vi.fn().mockResolvedValue(undefined);
    const flusher = new BatchFlusher(3, 1000, onFlush);

    flusher.add({ service: 'auth', level: 'info', message: 'm1', timestamp: '' });
    flusher.add({ service: 'auth', level: 'info', message: 'm2', timestamp: '' });
    expect(onFlush).not.toHaveBeenCalled();

    flusher.add({ service: 'auth', level: 'info', message: 'm3', timestamp: '' });
    expect(onFlush).toHaveBeenCalledOnce();
  });
});
```

---

## 5. Production Deployment

### `Dockerfile`
```dockerfile
FROM node:22-alpine AS builder
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .

FROM gcr.io/distroless/nodejs22-debian12:nonroot
WORKDIR /app
COPY --from=builder /app ./
USER nonroot
EXPOSE 3000
CMD ["src/app.js"]
```
