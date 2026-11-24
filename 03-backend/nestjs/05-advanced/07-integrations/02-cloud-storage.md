# Cloud Storage Integration: AWS S3 and Cloudflare R2

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 07-integrations
- **Prerequisites**: [01-file-upload](./01-file-upload.md), [03-core-concepts/03-configuration/01-config-module](../../03-core-concepts/03-configuration/01-config-module.md)

---

## 1. Context & Purpose

Ephemeral application containers (Docker, Kubernetes pods, AWS ECS) must never store uploaded files on local container filesystems. When a pod restarts, scales down, or crashes, all local files are permanently lost.

**Cloud Object Storage** (AWS S3, Cloudflare R2, Google Cloud Storage, Azure Blob Storage) provides durable, highly available, virtually infinite storage accessible over HTTPS. Beyond standard server-side uploads, high-throughput architectures offload uploads entirely by issuing **Pre-Signed URLs**, allowing frontend clients to upload large files directly to S3 without saturating backend network bandwidth or consuming Node.js memory.

---

## 2. Theoretical Foundations

### 2.1 Server-Side Upload vs Direct Pre-Signed Upload
1. **Server-Side Upload**:
   Client $\xrightarrow{\text{Upload 50MB}}$ Backend Server $\xrightarrow{\text{Upload 50MB}}$ S3 Bucket.
   *Problem*: Consumes 100MB of network transfer and holds backend sockets open.
2. **Pre-Signed URL (Direct Upload)**:
   - Client requests pre-signed upload URL from backend (`POST /files/presign`).
   - Backend generates a cryptographically signed S3 `PUT` URL with a 5-minute expiry.
   - Client uploads binary bytes directly: Client $\xrightarrow{\text{Direct PUT}}$ S3 Bucket.
   - *Result*: Zero backend bandwidth or memory consumed!

---

## 3. Architecture & Internal Mechanics

```
PRE-SIGNED UPLOAD FLOW:
Frontend Client (React/Next.js)                 NestJS API               AWS S3 Bucket
      │                                             │                         │
      ├─ 1. POST /files/presign { filename } ──────>│                         │
      │                                             ├─ s3.getSignedUrlPromise │
      │<─ 2. Returns: { uploadUrl, key } ───────────┤                         │
      │                                                                       │
      ├─ 3. HTTP PUT <binary bytes> directly to uploadUrl ───────────────────>│
      │<─ 4. Returns: 200 OK (S3 verifies signature) ─────────────────────────┤
      │                                             │                         │
      ├─ 5. POST /files/confirm { key } ───────────>│                         │
      │                                             ├─ Verify in S3 & save DB │
      │<─ 6. Returns 200 OK (Confirmed) ────────────┤                         │
```

---

## 4. Concrete Implementation

### 4.1 Installing AWS SDK v3 Dependencies

```bash
npm install --save @aws-sdk/client-s3 @aws-sdk/s3-request-presigner
```

---

### 4.2 Building an Injectable S3 / R2 Storage Service

```typescript
// src/storage/storage.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import {
  S3Client,
  PutObjectCommand,
  GetObjectCommand,
  DeleteObjectCommand,
} from '@aws-sdk/client-s3';
import { getSignedUrl } from '@aws-sdk/s3-request-presigner';
import { randomUUID } from 'crypto';
import { extname } from 'path';

@Injectable()
export class StorageService {
  private readonly s3Client: S3Client;
  private readonly bucketName: string;
  private readonly logger = new Logger(StorageService.name);

  constructor(private readonly config: ConfigService) {
    this.bucketName = this.config.getOrThrow<string>('S3_BUCKET_NAME');

    // S3Client supports AWS S3, Cloudflare R2, MinIO, and DigitalOcean Spaces
    this.s3Client = new S3Client({
      region: this.config.get<string>('S3_REGION', 'auto'),
      endpoint: this.config.get<string>('S3_ENDPOINT'), // e.g. https://<id>.r2.cloudflarestorage.com
      credentials: {
        accessKeyId: this.config.getOrThrow<string>('S3_ACCESS_KEY_ID'),
        secretAccessKey: this.config.getOrThrow<string>('S3_SECRET_ACCESS_KEY'),
      },
    });
  }

  // 1. Generate Pre-Signed Upload URL for Direct Client Uploads
  async generatePresignedUploadUrl(originalFilename: string, mimeType: string) {
    const ext = extname(originalFilename);
    const key = `uploads/${randomUUID()}${ext}`;

    const command = new PutObjectCommand({
      Bucket: this.bucketName,
      Key: key,
      ContentType: mimeType,
    });

    // URL expires in 15 minutes (900 seconds)
    const uploadUrl = await getSignedUrl(this.s3Client, command, { expiresIn: 900 });

    this.logger.log(`Generated pre-signed upload URL for key: ${key}`);
    return { uploadUrl, key };
  }

  // 2. Direct Server-Side Upload (For generated reports/buffers)
  async uploadBuffer(key: string, buffer: Buffer, mimeType: string) {
    const command = new PutObjectCommand({
      Bucket: this.bucketName,
      Key: key,
      Body: buffer,
      ContentType: mimeType,
    });

    await this.s3Client.send(command);
    this.logger.log(`Uploaded buffer to key: ${key}`);
    return { key };
  }

  // 3. Generate Pre-Signed Download URL (For Private Files)
  async generatePresignedDownloadUrl(key: string, expiresInSeconds = 3600) {
    const command = new GetObjectCommand({
      Bucket: this.bucketName,
      Key: key,
    });

    return getSignedUrl(this.s3Client, command, { expiresIn: expiresInSeconds });
  }

  // 4. Delete Object
  async deleteFile(key: string) {
    const command = new DeleteObjectCommand({
      Bucket: this.bucketName,
      Key: key,
    });

    await this.s3Client.send(command);
    this.logger.log(`Deleted object from storage: ${key}`);
  }
}
```

---

### 4.3 Storage Controller Exposing Pre-Signed Endpoints

```typescript
// src/storage/storage.controller.ts
import { Controller, Post, Body, UseGuards, Req } from '@nestjs/common';
import { StorageService } from './storage.service';
import { JwtAuthGuard } from '../auth/guards/jwt-auth.guard';

@Controller('storage')
@UseGuards(JwtAuthGuard)
export class StorageController {
  constructor(private readonly storageService: StorageService) {}

  @Post('presign')
  async getPresignedUrl(
    @Body() body: { filename: string; mimeType: string },
    @Req() req: any,
  ) {
    return this.storageService.generatePresignedUploadUrl(
      body.filename,
      body.mimeType,
    );
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 S3 Bucket CORS Failures
When uploading directly from frontend browsers to S3 via pre-signed URLs, the browser sends an `OPTIONS` preflight request. If S3 CORS is unconfigured, the browser blocks the upload.
- **Handling**: Configure CORS rules on the S3 bucket:
```json
[
  {
    "AllowedHeaders": ["*"],
    "AllowedMethods": ["PUT", "POST", "GET"],
    "AllowedOrigins": ["https://app.example.com", "http://localhost:3000"],
    "ExposeHeaders": ["ETag"]
  }
]
```

### 5.2 Content-Type Mismatch in Pre-Signed URLs
If the backend signs the URL with `ContentType: 'image/png'`, but the frontend client sends `Content-Type: 'application/octet-stream'`, S3 rejects the upload with `403 SignatureDoesNotMatch`.
- **Rule**: The client's `Content-Type` header must exactly match the `mimeType` passed during pre-signing.

---

## 6. Performance & Memory Implications

- **Bandwidth Savings**: Pre-signed URLs reduce backend server network egress by 100% for file uploads, completely eliminating upload proxy bottlenecks.
- **SDK v3 Modularity**: AWS SDK v3 packages (`@aws-sdk/client-s3`) are modular and tree-shakeable, adding minimal overhead to production bundle sizes.

---

## 7. Anti-Patterns & Common Mistakes

- **Hardcoding AWS Root Credentials in Source Code**: Committing access keys to Git repositories. Always load credentials via IAM Roles (on AWS ECS/EKS) or environment variables.
- **Making the Entire Bucket Publicly Writable**: Disabling all S3 block-public-access settings. Keep the bucket completely private; serve private assets via pre-signed download URLs or public assets via a CloudFront CDN.

---

## 8. Production Checklist & Operational Readiness

- [ ] IAM credentials follow the Principle of Least Privilege (scoped strictly to target bucket).
- [ ] Pre-signed upload URLs configured with short lifetimes ($\le 15\text{ minutes}$).
- [ ] S3 CORS configuration allows authorized frontend domains.
- [ ] S3 Lifecycle rules configured to automatically abort incomplete multipart uploads.

---

## 9. Migration & Version History

- **AWS SDK v2 to v3**: Migrated from monolithic `aws-sdk` to modular `@aws-sdk/client-s3`.

---

## 10. Verification & Testing

Unit testing pre-signed URL generation:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { StorageService } from './storage.service';
import { ConfigService } from '@nestjs/config';

describe('StorageService (S3)', () => {
  let service: StorageService;

  beforeEach(async () => {
    const mockConfig = {
      getOrThrow: vi.fn((key: string) => {
        if (key === 'S3_BUCKET_NAME') return 'test-bucket';
        if (key === 'S3_ACCESS_KEY_ID') return 'key';
        if (key === 'S3_SECRET_ACCESS_KEY') return 'secret';
        return 'test';
      }),
      get: vi.fn().mockReturnValue('us-east-1'),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        StorageService,
        { provide: ConfigService, useValue: mockConfig },
      ],
    }).compile();

    service = module.get<StorageService>(StorageService);
  });

  it('generates presigned url with unique uuid key', async () => {
    const res = await service.generatePresignedUploadUrl('avatar.png', 'image/png');
    expect(res.uploadUrl).toBeDefined();
    expect(res.key).toMatch(/^uploads\/[a-f0-9-]+\.png$/);
  });
});
```

---

## 11. Comprehensive Examples

### Client-Side Pre-Signed Direct Upload (Vanilla JS)

```javascript
// client/upload-helper.js
export async function uploadFileDirectlyToS3(file) {
  // Step 1: Request pre-signed URL from NestJS backend
  const presignRes = await fetch('/storage/presign', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${token}` },
    body: JSON.stringify({ filename: file.name, mimeType: file.type }),
  });
  const { uploadUrl, key } = await presignRes.json();

  // Step 2: Upload file directly to S3 / Cloudflare R2
  const s3UploadRes = await fetch(uploadUrl, {
    method: 'PUT',
    headers: { 'Content-Type': file.type },
    body: file,
  });

  if (!s3UploadRes.ok) {
    throw new Error('Direct S3 upload failed');
  }

  console.log('Upload complete! S3 Key:', key);
  return key;
}
```

---

## 12. Cross-References & Further Reading

- [01-file-upload.md](./01-file-upload.md) - Multer file processing
- [03-core-concepts/03-configuration/01-config-module.md](../../03-core-concepts/03-configuration/01-config-module.md) - Secret configuration
- [AWS SDK v3 S3 Client Documentation](https://docs.aws.amazon.com/AWSJavaScriptSDK/v3/latest/clients/client-s3/)
