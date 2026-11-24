# Streaming Files and StreamableFile

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 02-fundamentals
- **Prerequisites**: [08-response-handling](./08-response-handling.md), [11-platform-express-vs-fastify](./11-platform-express-vs-fastify.md), [00-prerequisites/03-nodejs-async-and-event-loop](../00-prerequisites/03-nodejs-async-and-event-loop.md)

---

## 1. Context & Purpose

Loading large files into memory buffers (`fs.readFileSync` or `Buffer.from()`) before sending them across the network is one of the quickest ways to crash a Node.js process with an `OutOfMemory` (OOM) error. If multiple concurrent users request a 200MB export, gigabytes of heap memory are instantly consumed.

NestJS solves this natively with the `StreamableFile` class. `StreamableFile` wraps standard Node.js readable streams, typed arrays, or file buffers, automatically configuring chunked transfer encodings, content disposition headers, and pipe backpressure across both Express and Fastify platforms without exposing raw platform response objects.

---

## 2. Theoretical Foundations

Streaming allows chunk-by-chunk data transmission between data producers and network consumers:
- **Backpressure**: When a slow mobile client downloads data from a fast SSD, backpressure pauses the disk read stream when the network socket's internal buffer fills up, resuming when the buffer drains.
- **Constant Memory Footprint**: Streaming a 5GB video file consumes only a few kilobytes of buffer memory at any instant, regardless of total file size.
- **HTTP Chunked Encoding**: When file length is unknown upfront (e.g., dynamically generated ZIP or CSV streams), HTTP/1.1 sends `Transfer-Encoding: chunked`. When the file size is known, `Content-Length` enables client download progress indicators.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   Controller Method                    │
│      const stream = createReadStream('large-file.pdf') │
│      return new StreamableFile(stream, options)        │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
               NestJS Response Serialization
                            │
                            ├─> Inspects return value instanceof StreamableFile
                            ├─> Sets Content-Type & Content-Disposition
                            ├─> Sets Content-Length (if known)
                            │
              ┌─────────────┴─────────────┐
              ▼                           ▼
       Express Adapter              Fastify Adapter
       stream.pipe(res)             reply.send(stream)
              │                           │
              └─────────────┬─────────────┘
                            ▼
              Client HTTP Stream (Chunked)
```

By returning `StreamableFile`, controllers remain 100% platform-agnostic, avoiding direct manipulation of Express `res` or Fastify `reply`.

---

## 4. Concrete Implementation

### 4.1 Basic File Streaming

```typescript
// src/files/files.controller.ts
import {
  Controller,
  Get,
  StreamableFile,
  Header,
  NotFoundException,
} from '@nestjs/common';
import { createReadStream, existsSync } from 'fs';
import { join } from 'path';

@Controller('files')
export class FilesController {
  @Get('report')
  @Header('Content-Type', 'application/pdf')
  @Header('Content-Disposition', 'attachment; filename="annual-report.pdf"')
  downloadReport(): StreamableFile {
    const filePath = join(process.cwd(), 'assets', 'annual-report.pdf');
    if (!existsSync(filePath)) {
      throw new NotFoundException('Report file not found');
    }

    const fileStream = createReadStream(filePath);
    return new StreamableFile(fileStream);
  }
}
```

---

### 4.2 Dynamic File Metadata via `StreamableFileOptions`

NestJS allows passing options directly into the `StreamableFile` constructor to set headers dynamically:

```typescript
// src/files/export.controller.ts
import { Controller, Get, Param, StreamableFile } from '@nestjs/common';
import { ExportService } from './export.service';

@Controller('exports')
export class ExportController {
  constructor(private readonly exportService: ExportService) {}

  @Get(':dataset/csv')
  async exportCsv(@Param('dataset') dataset: string): Promise<StreamableFile> {
    const { stream, size, filename } =
      await this.exportService.generateCsvStream(dataset);

    return new StreamableFile(stream, {
      type: 'text/csv',
      disposition: `attachment; filename="${filename}"`,
      length: size, // Enables client progress bar
    });
  }
}
```

---

### 4.3 Streaming Dynamic In-Memory Generators

Streaming dynamically generated content without writing to disk:

```typescript
// src/files/generator.service.ts
import { Injectable } from '@nestjs/common';
import { Readable } from 'stream';

@Injectable()
export class GeneratorService {
  createRowStream(): Readable {
    let rowCount = 0;
    const maxRows = 100_000;

    return new Readable({
      read() {
        if (rowCount >= maxRows) {
          this.push(null); // End of stream
          return;
        }

        const chunk = `Row_${rowCount},Data_${Date.now()}\n`;
        rowCount++;
        this.push(chunk);
      },
    });
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Premature Socket Disconnects
If a user closes their browser tab or cancels the download midway, the network socket destroys. Unhandled read streams may continue reading from disk or database cursors.
- **Handling**: Listen for stream `'close'` and `'error'` events to ensure database cursors and file descriptors are destroyed:
```typescript
fileStream.on('close', () => {
  // Cleanup cursor or temp file
});
```

### 5.2 Stream Errors After Headers Are Sent
If an error occurs midway through generating or reading the stream, HTTP headers (e.g., `200 OK`) have already been sent to the client. A standard NestJS exception filter cannot change the status code to 500.
- **Handling**: Ensure validation and availability checks occur **before** initiating the stream. Handle errors on the stream by destroying it with an error event.

---

## 6. Performance & Memory Implications

- **Heap Stability**: Memory usage remains flat (typically under 30MB) even when simultaneously streaming hundreds of gigabytes across concurrent requests.
- **Event Loop Health**: Node's libuv thread pool handles disk reads asynchronously, preventing the V8 execution thread from stalling.

---

## 7. Anti-Patterns & Common Mistakes

- **Reading Entire File with `fs.promises.readFile`**: Returning `new StreamableFile(await fs.readFile(path))`. This defeats the entire purpose of streaming by buffering the whole file in RAM before sending.
- **Bypassing `StreamableFile` with `res.pipe()`**: Injecting `@Res() res` and piping manually. This locks the code to Express and bypasses NestJS interceptors and exception filters.
- **Missing Content-Disposition**: Forgetting `Content-Disposition: attachment; ...`, causing the browser to attempt inline rendering of binary files instead of triggering a download.

---

## 8. Production Checklist & Operational Readiness

- [ ] File reads utilize `createReadStream` directly without intermediary RAM buffering.
- [ ] Appropriate `Content-Type` and `Content-Disposition` headers are defined.
- [ ] Temp files generated for exports are removed via stream `'close'` listeners.
- [ ] Nginx / Cloudflare configured with appropriate client download timeouts.
- [ ] Disk paths sanitized against Path Traversal attacks (`join()` with root boundary validation).

---

## 9. Migration & Version History

- **NestJS 8.x**: Introduced `StreamableFile` in `@nestjs/common`.
- **NestJS 9.x**: Added `StreamableFileOptions` for type, disposition, and length configuration.
- **NestJS 11.x**: Optimized stream piping for Express 5 and Fastify 5 response lifecycles.

---

## 10. Verification & Testing

Testing file download streams via Supertest:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { INestApplication } from '@nestjs/common';
import * as request from 'supertest';
import { FilesController } from './files.controller';

describe('FilesController (StreamableFile e2e)', () => {
  let app: INestApplication;

  beforeAll(async () => {
    const moduleFixture: TestingModule = await Test.createTestingModule({
      controllers: [FilesController],
    }).compile();

    app = moduleFixture.createNestApplication();
    await app.init();
  });

  afterAll(async () => {
    await app.close();
  });

  it('streams report with correct headers and body chunks', async () => {
    const response = await request(app.getHttpServer())
      .get('/files/report')
      .expect(200)
      .expect('Content-Type', 'application/pdf');

    expect(response.headers['content-disposition']).toContain('annual-report.pdf');
    expect(response.body).toBeDefined();
  });
});
```

---

## 11. Comprehensive Examples

### Zip Archiving on the Fly (Zero Disk Writes)

```typescript
// src/files/zip.service.ts
import { Injectable, StreamableFile } from '@nestjs/common';
import archiver from 'archiver';
import { PassThrough } from 'stream';

@Injectable()
export class ZipArchiveService {
  createZipStream(files: Array<{ name: string; content: string }>): StreamableFile {
    const archive = archiver('zip', { zlib: { level: 6 } });
    const passThrough = new PassThrough();

    archive.pipe(passThrough);

    for (const file of files) {
      archive.append(file.content, { name: file.name });
    }

    archive.finalize();

    return new StreamableFile(passThrough, {
      type: 'application/zip',
      disposition: 'attachment; filename="bundle.zip"',
    });
  }
}
```

---

## 12. Cross-References & Further Reading

- [08-response-handling.md](./08-response-handling.md) - Response handling patterns
- [11-platform-express-vs-fastify.md](./11-platform-express-vs-fastify.md) - Platform differences
- [00-prerequisites/03-nodejs-async-and-event-loop.md](../00-prerequisites/03-nodejs-async-and-event-loop.md) - Node streams and backpressure
- [NestJS Streaming Files Documentation](https://docs.nestjs.com/techniques/streaming-files)
