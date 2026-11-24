# Project 06: Cloud Storage & Ingestion Gateway (`06-file-upload-service`)

> Enterprise file upload gateway: Presigned Amazon S3 URLs, Multer streaming, MIME-type validation, and asynchronous image thumbnailing with BullMQ.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-06-file-upload-service` |
| **Storage** | AWS S3 / MinIO (S3-compatible) |
| **Processing** | Sharp (Image Processing) + BullMQ Worker |
| **Complexity Level** | Intermediate Production Reference |

---

## 1. S3 Presigned URL Generator

```typescript
// src/storage/s3.service.ts
import { Injectable } from '@nestjs/common';
import { S3Client, PutObjectCommand } from '@aws-sdk/client-s3';
import { getSignedUrl } from '@aws-sdk/s3-request-presigner';
import { randomUUID } from 'node:crypto';

@Injectable()
export class S3Service {
  private readonly s3 = new S3Client({ region: process.env.AWS_REGION });

  async generatePresignedUploadUrl(filename: string, mimeType: string) {
    const key = `uploads/${randomUUID()}-${filename}`;
    const command = new PutObjectCommand({
      Bucket: process.env.S3_BUCKET_NAME,
      Key: key,
      ContentType: mimeType,
    });

    const presignedUrl = await getSignedUrl(this.s3, command, { expiresIn: 300 });
    return { uploadUrl: presignedUrl, fileKey: key };
  }
}
```
