# File Uploads and Multipart Processing in NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 07-integrations
- **Prerequisites**: [02-fundamentals/07-request-data](../../02-fundamentals/07-request-data.md), [02-fundamentals/11-platform-express-vs-fastify](../../02-fundamentals/11-platform-express-vs-fastify.md)

---

## 1. Context & Purpose

User avatars, PDF contracts, image attachments, and CSV bulk imports all require handling `multipart/form-data` uploads. Unlike standard JSON payloads, multipart streams interleave binary file chunks and form fields separated by boundary delimiters.

In NestJS on Express, file uploads are handled via **Multer** and dedicated interceptors (`FileInterceptor`, `FilesInterceptor`, `FileFieldsInterceptor`). NestJS provides built-in validation pipes (`ParseFilePipe`, `MaxFileSizeValidator`, `FileTypeValidator`) to validate MIME types and byte sizes before files touch disk or cloud storage.

---

## 2. Theoretical Foundations

### 2.1 Multipart/Form-Data Encoding
When a client uploads a file:
```http
POST /upload HTTP/1.1
Content-Type: multipart/form-data; boundary=---------------------------974767299852498929531610575

-----------------------------974767299852498929531610575
Content-Disposition: form-data; name="title"

Annual Report
-----------------------------974767299852498929531610575
Content-Disposition: form-data; name="file"; filename="report.pdf"
Content-Type: application/pdf

<binary PDF bytes>
-----------------------------974767299852498929531610575--
```

### 2.2 Storage Engines: Memory vs Disk
- **`memoryStorage()`**: Buffers file bytes directly into a Node.js `Buffer` in RAM (`file.buffer`). Fast and ideal for immediately forwarding files to AWS S3 / Cloudflare R2, but dangerous for multi-gigabyte files.
- **`diskStorage()`**: Streams chunks directly to temporary files on the local filesystem (`file.path`). Safe against memory exhaustion, but requires subsequent disk cleanup.

---

## 3. Architecture & Internal Mechanics

```
Client HTTP Request (multipart/form-data)
                 │
                 ▼
          FileInterceptor('avatar')
                 │
                 ├─> Multer parses multipart stream
                 ├─> Buffers or writes to disk
                 │
                 ▼
          ParseFilePipe
                 │
                 ├─> MaxFileSizeValidator (<= 5MB)
                 ├─> FileTypeValidator (image/jpeg|image/png)
                 │
         ┌───────┴───────┐
         ▼               ▼
     [Valid]         [Invalid]
         │               │
         ▼          400 Bad Request
  Controller Handler
  @UploadedFile() file: Express.Multer.File
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save-dev @types/multer
```

---

### 4.2 Single File Upload with Strict Validation

```typescript
// src/users/users-avatar.controller.ts
import {
  Controller,
  Post,
  UseInterceptors,
  UploadedFile,
  ParseFilePipe,
  MaxFileSizeValidator,
  FileTypeValidator,
  HttpStatus,
} from '@nestjs/common';
import { FileInterceptor } from '@nestjs/platform-express';
import { diskStorage } from 'multer';
import { extname } from 'path';
import { randomUUID } from 'crypto';

@Controller('users')
export class UsersAvatarController {
  @Post('avatar')
  @UseInterceptors(
    FileInterceptor('file', {
      storage: diskStorage({
        destination: './uploads/avatars',
        filename: (req, file, callback) => {
          // Generate unique, sanitized filename
          const uniqueSuffix = randomUUID();
          const ext = extname(file.originalname);
          callback(null, `avatar-${uniqueSuffix}${ext}`);
        },
      }),
      limits: {
        fileSize: 5 * 1024 * 1024, // 5MB hard limit
      },
    }),
  )
  uploadAvatar(
    @UploadedFile(
      new ParseFilePipe({
        errorHttpStatusCode: HttpStatus.UNPROCESSABLE_ENTITY,
        validators: [
          // Enforce 5MB limit
          new MaxFileSizeValidator({ maxSize: 5 * 1024 * 1024 }),
          // Enforce MIME type strictly via regex
          new FileTypeValidator({ fileType: '.(png|jpeg|jpg)' }),
        ],
      }),
    )
    file: Express.Multer.File,
  ) {
    return {
      message: 'Avatar uploaded successfully',
      filename: file.filename,
      size: file.size,
      mimetype: file.mimetype,
    };
  }
}
```

---

### 4.3 Multiple File Uploads (`FilesInterceptor` and `FileFieldsInterceptor`)

#### Uploading an Array of Files:
```typescript
@Post('photos')
@UseInterceptors(FilesInterceptor('photos', 10)) // Max 10 files
uploadMultiplePhotos(@UploadedFiles() files: Array<Express.Multer.File>) {
  return { uploadedCount: files.length };
}
```

#### Uploading Multiple Named Fields:
```typescript
@Post('profile-bundle')
@UseInterceptors(
  FileFieldsInterceptor([
    { name: 'avatar', maxCount: 1 },
    { name: 'documents', maxCount: 5 },
  ]),
)
uploadBundle(
  @UploadedFiles()
  files: {
    avatar?: Express.Multer.File[];
    documents?: Express.Multer.File[];
  },
) {
  return {
    hasAvatar: !!files.avatar,
    docCount: files.documents?.length || 0,
  };
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 MIME Type Spoofing
A malicious user renames an executable (`malware.exe`) to `document.pdf`. The browser reports `file.mimetype = 'application/pdf'`. Standard `FileTypeValidator` checks the client-provided header, passing the malicious file!
- **Handling**: Use "Magic Byte" inspection (using packages like `file-type`) to verify binary signatures:
```typescript
import { fromBuffer } from 'file-type';
const detected = await fromBuffer(file.buffer);
if (detected?.mime !== 'application/pdf') throw new BadRequestException('Spoofed MIME type!');
```

### 5.2 Disk Storage Orphaned Files on Validation Failure
If `diskStorage()` writes the file to `./uploads` and a downstream service throws an error, the temporary file remains on disk forever.
- **Handling**: Use `fs.promises.unlink(file.path)` in an exception filter to clean up abandoned files.

---

## 6. Performance & Memory Implications

- **Memory Storage Limits**: Buffering a 200MB video in RAM (`memoryStorage()`) consumes 200MB of V8 heap per concurrent user. For large files, use `diskStorage()` or direct S3 multipart pre-signed uploads.
- **Garbage Collection**: Buffers are freed immediately once the request finishes if not retained in global variables.

---

## 7. Anti-Patterns & Common Mistakes

- **Preserving Original Filenames on Disk**: Saving files as `file.originalname`. Attackers upload `../../etc/passwd` to perform Path Traversal attacks. Always generate unique random filenames (`randomUUID()`).
- **Storing Uploaded Files Inside the Git Repository**: Saving uploads inside the local project folder. Container redeployments wipe local storage! Always offload uploads to S3, GCS, or Azure Blob.

---

## 8. Production Checklist & Operational Readiness

- [ ] File size limits configured on both Multer and reverse proxy (Nginx `client_max_body_size`).
- [ ] Uploaded files saved under randomized UUID filenames.
- [ ] Magic byte inspection used to verify genuine binary signatures.
- [ ] Files forwarded to durable Cloud Object Storage.

---

## 9. Migration & Version History

- **NestJS 9.x**: Introduced `ParseFilePipe`, `MaxFileSizeValidator`, and `FileTypeValidator`.
- **NestJS 11.x**: Validated with Express 5 multipart streaming pipelines.

---

## 10. Verification & Testing

E2E testing file uploads using Supertest:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { INestApplication } from '@nestjs/common';
import * as request from 'supertest';
import { UsersAvatarController } from './users-avatar.controller';

describe('UsersAvatarController (File Upload)', () => {
  let app: INestApplication;

  beforeAll(async () => {
    const moduleFixture: TestingModule = await Test.createTestingModule({
      controllers: [UsersAvatarController],
    }).compile();

    app = moduleFixture.createNestApplication();
    await app.init();
  });

  afterAll(async () => {
    await app.close();
  });

  it('rejects upload exceeding max size', async () => {
    const largeBuffer = Buffer.alloc(6 * 1024 * 1024); // 6MB

    return request(app.getHttpServer())
      .post('/users/avatar')
      .attach('file', largeBuffer, 'large.png')
      .expect(422);
  });
});
```

---

## 11. Comprehensive Examples

### Custom Magic-Number File Type Validator

```typescript
// src/common/validators/magic-byte-file.validator.ts
import { FileValidator } from '@nestjs/common';
import { fileTypeFromBuffer } from 'file-type';

export class MagicByteValidator extends FileValidator<{ allowedMimes: string[] }> {
  async isValid(file?: Express.Multer.File): Promise<boolean> {
    if (!file?.buffer) return false;

    // Inspect actual binary bytes
    const type = await fileTypeFromBuffer(file.buffer);
    if (!type) return false;

    return this.validationOptions.allowedMimes.includes(type.mime);
  }

  buildErrorMessage(): string {
    return `Uploaded file header does not match authorized MIME types: ${this.validationOptions.allowedMimes.join(', ')}`;
  }
}
```

---

## 12. Cross-References & Further Reading

- [02-cloud-storage.md](./02-cloud-storage.md) - Uploading to AWS S3 and Cloudflare R2
- [02-fundamentals/07-request-data.md](../../02-fundamentals/07-request-data.md) - Request decorators
- [Multer Official GitHub](https://github.com/expressjs/multer)
