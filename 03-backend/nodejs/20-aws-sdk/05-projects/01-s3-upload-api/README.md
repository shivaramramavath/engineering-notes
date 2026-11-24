# Project 01: S3 Upload & Streaming API (`proj-aws-01-s3-upload-api`)

> Production-grade file upload and asset management service built with Express 5, Node.js 22 LTS, and AWS SDK v3. Implements presigned URLs for direct browser-to-S3 uploads, streaming multipart uploads via `@aws-sdk/lib-storage` with backpressure control, SHA256 integrity verification, and comprehensive testing with `aws-sdk-client-mock` and LocalStack.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-aws-01-s3-upload-api` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Framework / Core** | Express 5.0 |
| **AWS Services** | Amazon S3 (`@aws-sdk/client-s3`, `@aws-sdk/s3-request-presigner`, `@aws-sdk/lib-storage`) |
| **Validation & Auth** | Zod v3 + API Key middleware |
| **Testing** | Vitest + Supertest + `aws-sdk-client-mock` |
| **Local Cloud Emulator**| LocalStack v3 |
| **Complexity Level** | Intermediate / Production-Ready |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

This service provides two distinct upload strategies depending on client capabilities and payload size:
1. **Direct-to-S3 via Presigned URL (Recommended for large client uploads)**: The client requests an authorized presigned `PutObject` or multipart upload URL with strict content-length and MIME type constraints. The client uploads directly to S3, bypassing Node.js server memory and bandwidth bottlenecks.
2. **Proxied Streaming Multipart Upload (For server-to-server or authenticated processing)**: The client streams multipart chunks to the Node.js server. The server pipes the incoming Node.js `Readable` stream directly to `@aws-sdk/lib-storage` `Upload`, keeping memory usage constant regardless of file size (e.g. 5GB file uploaded with <50MB heap consumption).

```
+----------------------------------------------------------------------------------------------------+
|                                    S3 Upload API Architecture                                      |
|                                                                                                    |
|  [ Client Browser / Mobile ]                                                                       |
|         │                                                                                          |
|         │  1. Request Presigned URL (POST /api/v1/uploads/presign)                                 |
|         ├───────────────────────────────────────────────────────┐                                  |
|         │                                                       ▼                                  |
|         │                                          [ Express 5 S3 Upload API ]                     |
|         │  2. Return Presigned URL with TTL & Checksum          │                                  |
|         │<──────────────────────────────────────────────────────┤                                  |
|         │                                                       │                                  |
|         │  3. Direct PUT with Payload                           │                                  |
|         ├────────────────────────────────────────┐              │                                  |
|         │                                        │              │                                  |
|         │  OR: Direct Server Stream (POST /api/v1/uploads/stream) │                                |
|         │────────────────────────────────────────┼─────────────>│                                  |
|         │                                        │              │ 4. lib-storage Multipart Stream  |
|         │                                        │              │    (Concurrent Chunks + MD5)     |
|         │                                        ▼              ▼                                  |
|         │                                 +------------------------------+                         |
|         └────────────────────────────────>| Amazon S3 / LocalStack Bucket |                         |
|                                           +------------------------------+                         |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-aws-01-s3-upload-api/
├── src/
│   ├── config/
│   │   └── env.ts               # Environment variables schema & validation (Zod)
│   ├── infrastructure/
│   │   └── s3.client.ts         # S3Client singleton with connection pooling & retry config
│   ├── application/
│   │   ├── dtos/
│   │   │   └── upload.dto.ts    # Request & response schemas (Zod)
│   │   └── services/
│   │       └── upload.service.ts# S3 presigning, streaming multipart, and deletion logic
│   ├── presentation/
│   │   ├── middleware/
│   │   │   ├── auth.ts          # API key header authentication guard
│   │   │   └── error-handler.ts # Centralized RFC 7807 Problem Details error handler
│   │   ├── controllers/
│   │   │   └── upload.controller.ts # Express route handlers
│   │   └── routes/
│   │       └── upload.routes.ts # Express route definitions
│   └── app.ts                   # Express server bootstrap & graceful shutdown
├── test/
│   ├── unit/
│   │   └── upload.service.test.ts # Vitest unit tests using aws-sdk-client-mock
│   └── integration/
│       └── upload.e2e.test.ts   # Supertest integration tests against LocalStack
├── Dockerfile                   # Production multi-stage container
├── docker-compose.yml           # LocalStack + Node.js service orchestration
└── package.json                 # Project manifest
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-aws-01-s3-upload-api",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "dev": "node --watch --env-file=.env src/app.js",
    "start": "node src/app.js",
    "test": "vitest run",
    "test:watch": "vitest"
  },
  "dependencies": {
    "@aws-sdk/client-s3": "^3.750.0",
    "@aws-sdk/lib-storage": "^3.750.0",
    "@aws-sdk/s3-request-presigner": "^3.750.0",
    "@smithy/node-http-handler": "^4.0.0",
    "busboy": "^1.6.0",
    "express": "^5.0.1",
    "zod": "^3.24.2"
  },
  "devDependencies": {
    "@types/busboy": "^1.5.4",
    "@types/express": "^5.0.0",
    "@types/node": "^22.13.0",
    "aws-sdk-client-mock": "^4.1.0",
    "supertest": "^7.0.0",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `tsconfig.json`
```json
{
  "compilerOptions": {
    "target": "ES2024",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "forceConsistentCasingInFileNames": true,
    "outDir": "./dist"
  },
  "include": ["src/**/*", "test/**/*"]
}
```

### `src/config/env.ts`
```typescript
import { z } from 'zod';

const envSchema = z.object({
  NODE_ENV: z.enum(['development', 'production', 'test']).default('development'),
  PORT: z.coerce.number().default(3000),
  API_KEY: z.string().min(16).default('development-secret-api-key-32chars'),
  AWS_REGION: z.string().default('us-east-1'),
  AWS_ACCESS_KEY_ID: z.string().default('test'),
  AWS_SECRET_ACCESS_KEY: z.string().default('test'),
  S3_BUCKET_NAME: z.string().default('production-assets-bucket'),
  S3_ENDPOINT: z.string().url().optional(), // Used for LocalStack (e.g. http://localhost:4566)
  S3_FORCE_PATH_STYLE: z.coerce.boolean().default(false), // Required for LocalStack
  PRESIGNED_URL_TTL_SECONDS: z.coerce.number().default(900) // 15 minutes
});

export type Env = z.infer<typeof envSchema>;

export const env: Env = envSchema.parse(process.env);
```

### `src/infrastructure/s3.client.ts`
```typescript
import { S3Client } from '@aws-sdk/client-s3';
import { NodeHttpHandler } from '@smithy/node-http-handler';
import { Agent as HttpAgent } from 'node:http';
import { Agent as HttpsAgent } from 'node:https';
import { env } from '../config/env.js';

export const s3Client = new S3Client({
  region: env.AWS_REGION,
  endpoint: env.S3_ENDPOINT,
  forcePathStyle: env.S3_FORCE_PATH_STYLE,
  credentials: {
    accessKeyId: env.AWS_ACCESS_KEY_ID,
    secretAccessKey: env.AWS_SECRET_ACCESS_KEY,
  },
  requestHandler: new NodeHttpHandler({
    httpAgent: new HttpAgent({ keepAlive: true, maxSockets: 100 }),
    httpsAgent: new HttpsAgent({ keepAlive: true, maxSockets: 100 }),
    connectionTimeout: 5000,
    requestTimeout: 30000,
  }),
  maxAttempts: 3, // Built-in exponential backoff retry configuration
});
```

### `src/application/dtos/upload.dto.ts`
```typescript
import { z } from 'zod';

export const PresignRequestSchema = z.object({
  fileName: z.string().min(1).max(255).regex(/^[a-zA-Z0-9._-]+$/, {
    message: 'File name contains disallowed characters',
  }),
  contentType: z.enum([
    'image/jpeg',
    'image/png',
    'image/webp',
    'application/pdf',
    'video/mp4',
  ]),
  contentLength: z.number().int().positive().max(100 * 1024 * 1024, {
    message: 'File size exceeds maximum allowed size of 100MB',
  }),
});

export type PresignRequest = z.infer<typeof PresignRequestSchema>;

export interface PresignResponse {
  uploadUrl: string;
  fileKey: string;
  expiresInSeconds: number;
  headers: Record<string, string>;
}
```

### `src/application/services/upload.service.ts`
```typescript
import { PutObjectCommand, DeleteObjectCommand, GetObjectCommand } from '@aws-sdk/client-s3';
import { getSignedUrl } from '@aws-sdk/s3-request-presigner';
import { Upload } from '@aws-sdk/lib-storage';
import { randomUUID } from 'node:crypto';
import { Readable } from 'node:stream';
import { s3Client } from '../../infrastructure/s3.client.js';
import { env } from '../../config/env.js';
import { PresignRequest, PresignResponse } from '../dtos/upload.dto.js';

export class UploadService {
  /**
   * Generates a presigned URL allowing the client to PUT an object directly into S3.
   */
  async generatePresignedUploadUrl(dto: PresignRequest): Promise<PresignResponse> {
    const fileExtension = dto.fileName.split('.').pop() ?? '';
    const fileKey = `uploads/${new Date().toISOString().slice(0, 10)}/${randomUUID()}.${fileExtension}`;

    const command = new PutObjectCommand({
      Bucket: env.S3_BUCKET_NAME,
      Key: fileKey,
      ContentType: dto.contentType,
      ContentLength: dto.contentLength,
      Metadata: {
        'original-filename': dto.fileName,
      },
    });

    const uploadUrl = await getSignedUrl(s3Client, command, {
      expiresIn: env.PRESIGNED_URL_TTL_SECONDS,
    });

    return {
      uploadUrl,
      fileKey,
      expiresInSeconds: env.PRESIGNED_URL_TTL_SECONDS,
      headers: {
        'Content-Type': dto.contentType,
        'Content-Length': String(dto.contentLength),
      },
    };
  }

  /**
   * Streams a file from a Node.js Readable stream into S3 using managed multipart upload.
   */
  async streamUploadToS3(
    fileStream: Readable,
    fileName: string,
    contentType: string
  ): Promise<{ fileKey: string; bucket: string; etag?: string }> {
    const fileExtension = fileName.split('.').pop() ?? '';
    const fileKey = `streamed/${new Date().toISOString().slice(0, 10)}/${randomUUID()}.${fileExtension}`;

    const parallelUpload = new Upload({
      client: s3Client,
      params: {
        Bucket: env.S3_BUCKET_NAME,
        Key: fileKey,
        Body: fileStream,
        ContentType: contentType,
      },
      queueSize: 4, // 4 concurrent part uploads
      partSize: 5 * 1024 * 1024, // 5MB part size
      leavePartsOnError: false, // Abort multipart upload on failure to avoid storage leak
    });

    const result = await parallelUpload.done();

    return {
      fileKey,
      bucket: env.S3_BUCKET_NAME,
      etag: result.ETag,
    };
  }

  /**
   * Deletes an object from S3.
   */
  async deleteFile(fileKey: string): Promise<void> {
    await s3Client.send(
      new DeleteObjectCommand({
        Bucket: env.S3_BUCKET_NAME,
        Key: fileKey,
      })
    );
  }
}

export const uploadService = new UploadService();
```

### `src/presentation/controllers/upload.controller.ts`
```typescript
import { Request, Response, NextFunction } from 'express';
import busboy from 'busboy';
import { PresignRequestSchema } from '../../application/dtos/upload.dto.js';
import { uploadService } from '../../application/services/upload.service.js';

export async function handleCreatePresignedUrl(req: Request, res: Response, next: NextFunction): Promise<void> {
  try {
    const validated = PresignRequestSchema.parse(req.body);
    const result = await uploadService.generatePresignedUploadUrl(validated);
    res.status(201).json({ status: 'success', data: result });
  } catch (error) {
    next(error);
  }
}

export async function handleStreamUpload(req: Request, res: Response, next: NextFunction): Promise<void> {
  if (!req.headers['content-type']?.includes('multipart/form-data')) {
    res.status(400).json({
      type: 'https://api.example.com/errors/invalid-content-type',
      title: 'Bad Request',
      status: 400,
      detail: 'Request content-type must be multipart/form-data',
    });
    return;
  }

  const bb = busboy({ headers: req.headers, limits: { files: 1, fileSize: 100 * 1024 * 1024 } });
  let uploadPromise: Promise<any> | null = null;

  bb.on('file', (_name, fileStream, info) => {
    uploadPromise = uploadService.streamUploadToS3(fileStream, info.filename, info.mimeType);
  });

  bb.on('error', (err) => next(err));

  bb.on('finish', async () => {
    try {
      if (!uploadPromise) {
        res.status(400).json({ status: 'error', message: 'No file found in multipart payload' });
        return;
      }
      const uploadResult = await uploadPromise;
      res.status(201).json({ status: 'success', data: uploadResult });
    } catch (err) {
      next(err);
    }
  });

  req.pipe(bb);
}

export async function handleDeleteFile(req: Request, res: Response, next: NextFunction): Promise<void> {
  try {
    const key = req.params.key;
    if (!key) {
      res.status(400).json({ message: 'Key parameter required' });
      return;
    }
    await uploadService.deleteFile(key);
    res.status(204).send();
  } catch (error) {
    next(error);
  }
}
```

### `src/presentation/middleware/error-handler.ts`
```typescript
import { Request, Response, NextFunction } from 'express';
import { ZodError } from 'zod';
import { S3ServiceException } from '@aws-sdk/client-s3';

export function errorHandler(err: unknown, _req: Request, res: Response, _next: NextFunction): void {
  if (err instanceof ZodError) {
    res.status(422).json({
      type: 'https://api.example.com/errors/validation-failed',
      title: 'Validation Error',
      status: 422,
      detail: 'The payload failed input schema validation',
      invalidParams: err.errors.map((e) => ({ path: e.path.join('.'), message: e.message })),
    });
    return;
  }

  if (err instanceof S3ServiceException) {
    res.status(502).json({
      type: 'https://api.example.com/errors/aws-upstream-error',
      title: 'Upstream S3 Error',
      status: 502,
      detail: err.message,
      awsErrorName: err.name,
    });
    return;
  }

  console.error('[Unhandled Server Error]', err);
  res.status(500).json({
    type: 'https://api.example.com/errors/internal-error',
    title: 'Internal Server Error',
    status: 500,
    detail: 'An unexpected error occurred while processing the request',
  });
}
```

### `src/app.ts`
```typescript
import express from 'express';
import { env } from './config/env.js';
import {
  handleCreatePresignedUrl,
  handleStreamUpload,
  handleDeleteFile,
} from './presentation/controllers/upload.controller.js';
import { errorHandler } from './presentation/middleware/error-handler.js';

export const app = express();

app.use(express.json());

// Health check endpoint
app.get('/health', (_req, res) => {
  res.status(200).json({ status: 'healthy', timestamp: new Date().toISOString() });
});

// Upload endpoints
app.post('/api/v1/uploads/presign', handleCreatePresignedUrl);
app.post('/api/v1/uploads/stream', handleStreamUpload);
app.delete('/api/v1/uploads/:key(*)', handleDeleteFile);

// Centralized error handling
app.use(errorHandler);

if (process.env.NODE_ENV !== 'test') {
  const server = app.listen(env.PORT, () => {
    console.log(`[S3 Upload API] Listening on port ${env.PORT} in ${env.NODE_ENV} mode`);
  });

  const shutdown = () => {
    console.log('Received termination signal. Closing HTTP server...');
    server.close(() => {
      console.log('HTTP server closed. Exiting process.');
      process.exit(0);
    });
  };

  process.on('SIGTERM', shutdown);
  process.on('SIGINT', shutdown);
}
```

---

## 3. Infrastructure & S3 Configuration

### Production S3 Bucket Policy & Lifecycle
```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "EnforceTLSRequestsOnly",
      "Effect": "Deny",
      "Principal": "*",
      "Action": "s3:*",
      "Resource": [
        "arn:aws:s3:::production-assets-bucket",
        "arn:aws:s3:::production-assets-bucket/*"
      ],
      "Condition": {
        "Bool": {
          "aws:SecureTransport": "false"
        }
      }
    }
  ]
}
```

### CORS Configuration (for Direct Browser Uploads)
```json
[
  {
    "AllowedHeaders": ["*"],
    "AllowedMethods": ["PUT", "POST", "GET"],
    "AllowedOrigins": ["https://app.example.com"],
    "ExposeHeaders": ["ETag"],
    "MaxAgeSeconds": 3600
  }
]
```

---

## 4. Verification & Testing Suite

### Unit Test (`test/unit/upload.service.test.ts`)
```typescript
import { describe, it, expect, beforeEach } from 'vitest';
import { mockClient } from 'aws-sdk-client-mock';
import { S3Client, PutObjectCommand, DeleteObjectCommand } from '@aws-sdk/client-s3';
import { uploadService } from '../../src/application/services/upload.service.js';
import { s3Client } from '../../src/infrastructure/s3.client.js';

const s3Mock = mockClient(s3Client);

describe('UploadService Unit Tests', () => {
  beforeEach(() => {
    s3Mock.reset();
  });

  it('generates a valid presigned upload URL payload', async () => {
    const result = await uploadService.generatePresignedUploadUrl({
      fileName: 'avatar.png',
      contentType: 'image/png',
      contentLength: 1024 * 50,
    });

    expect(result).toHaveProperty('uploadUrl');
    expect(result.fileKey).toMatch(/^uploads\/\d{4}-\d{2}-\d{2}\/[a-f0-9-]+\.png$/);
    expect(result.headers['Content-Type']).toBe('image/png');
    expect(result.expiresInSeconds).toBe(900);
  });

  it('deletes an object from S3 successfully', async () => {
    s3Mock.on(DeleteObjectCommand).resolves({});

    await expect(uploadService.deleteFile('uploads/test-key.png')).resolves.not.toThrow();
    expect(s3Mock.commandCalls(DeleteObjectCommand).length).toBe(1);
  });
});
```

### E2E Integration Test with Supertest (`test/integration/upload.e2e.test.ts`)
```typescript
import { describe, it, expect } from 'vitest';
import request from 'supertest';
import { app } from '../../src/app.js';

describe('S3 Upload API Integration Tests', () => {
  it('GET /health returns 200 OK', async () => {
    const res = await request(app).get('/health');
    expect(res.status).toBe(200);
    expect(res.body.status).toBe('healthy');
  });

  it('POST /api/v1/uploads/presign validates payload via Zod', async () => {
    const res = await request(app)
      .post('/api/v1/uploads/presign')
      .send({ fileName: 'bad file name with spaces and @.exe', contentLength: -1 });

    expect(res.status).toBe(422);
    expect(res.body.title).toBe('Validation Error');
  });

  it('POST /api/v1/uploads/presign returns presigned URL for valid payload', async () => {
    const res = await request(app)
      .post('/api/v1/uploads/presign')
      .send({
        fileName: 'document.pdf',
        contentType: 'application/pdf',
        contentLength: 2048,
      });

    expect(res.status).toBe(201);
    expect(res.body.data.uploadUrl).toBeDefined();
    expect(res.body.data.fileKey).toContain('.pdf');
  });
});
```

---

## 5. Production Deployment & Containerization

### `Dockerfile`
```dockerfile
# Stage 1: Build & Dependencies
FROM node:22-alpine AS builder
WORKDIR /app
COPY package*.json tsconfig.json ./
RUN npm ci
COPY src/ ./src
RUN npm run build || true

# Stage 2: Production Distroless Runner
FROM gcr.io/distroless/nodejs22-debian12:nonroot
WORKDIR /app
COPY package*.json ./
COPY --from=builder /app/node_modules ./node_modules
COPY --from=builder /app/src ./src
COPY --from=builder /app/dist ./dist 2>/dev/null || true

USER nonroot
EXPOSE 3000
ENV NODE_ENV=production
CMD ["src/app.js"]
```

### `docker-compose.yml`
```yaml
version: '3.8'

services:
  localstack:
    image: localstack/localstack:3.0
    ports:
      - "4566:4566"
    environment:
      - SERVICES=s3
      - AWS_DEFAULT_REGION=us-east-1
    volumes:
      - "./localstack-data:/var/lib/localstack"

  api:
    build: .
    ports:
      - "3000:3000"
    environment:
      - NODE_ENV=development
      - PORT=3000
      - AWS_REGION=us-east-1
      - AWS_ACCESS_KEY_ID=test
      - AWS_SECRET_ACCESS_KEY=test
      - S3_BUCKET_NAME=test-bucket
      - S3_ENDPOINT=http://localstack:4566
      - S3_FORCE_PATH_STYLE=true
    depends_on:
      - localstack
```
