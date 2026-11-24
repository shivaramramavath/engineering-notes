# System Design Blueprint: Scalable File Storage, Direct Uploads & Global CDN

## Metadata

- **Topic**: Cloud Object Storage Service with Presigned Multipart Uploads & Global CDN Caching
- **Scale Requirements**: 500 Terabytes new media per month, 10,000 concurrent file uploads (video/images up to 5GB per file), 100Gbps CDN egress
- **Target SLA**: 99.999999999% (11 9s) object durability, sub-20ms TTFB for cached media via CDN Edge
- **Tech Stack**: FastAPI, AWS S3 / MinIO, AWS CloudFront / Cloudflare CDN, PostgreSQL (Metadata), SQS / EventBridge

---

## 1. Requirements & System Estimation

### 1.1 Functional Requirements
1. **Direct-to-S3 Uploads**: Client browsers and mobile apps upload directly to Object Storage via presigned URLs without piping multi-gigabyte streams through FastAPI memory.
2. **Resumable Multipart Uploads**: Files > 100MB upload in 5MB-50MB chunks in parallel, allowing retry on failed segments.
3. **Global Edge CDN Delivery**: Static assets and media files are cached and delivered globally via CloudFront/Cloudflare with Origin Access Identity (OAI/OAC) security.
4. **Asynchronous Post-Processing**: Automated thumbnail generation, virus scanning, and video transcoding triggered on upload completion.

### 1.2 Anti-Pattern vs. Production Architecture

```
ANTI-PATTERN (Piping File through API Memory):
Client ----[5 GB Video stream]----> [FastAPI Pod (OOM / Maxed Sockets)] ----> AWS S3

PRODUCTION ARCHITECTURE (Direct Presigned Upload):
Client ----[1. Request URL (1KB)]----> [FastAPI (Ultra lightweight)]
Client <---[2. Presigned URL (1KB)]--- [FastAPI]
Client ----[3. Direct Upload Stream]-----------------------------------> AWS S3 Bucket
Client ----[4. Notify Complete]------> [FastAPI] ----------------------> Event Queue
```

---

## 2. High-Level Architecture Diagram

```
                       +---------------------------------------+
                       |        Client / Browser / App         |
                       +-------+-----------------------+-------+
                               |                       |
        1. Request Presigned   |                       | 3. Direct Multipart
           Upload URL          |                       |    Chunk Uploads (PUT)
                               v                       |
                       +-------+-------+               |
                       | FastAPI App   |               |
                       +-------+-------+               |
                               |                       |
                               v                       v
                   +-----------+-----------+       +---+--------------------+
                   | PostgreSQL Metadata   |       | AWS S3 / MinIO Bucket  |
                   | Status: "INITIALIZED" |       | (Encrypted at Rest)    |
                   +-----------------------+       +---+--------------------+
                                                       |
                   4. Complete Multipart Ping          | S3 ObjectCreated Event
                      (Client confirms completion)     v
                               |               +-------+--------------------+
                               v               | AWS EventBridge / SQS      |
                       +-------+-------+       +-------+--------------------+
                       | FastAPI App   |               |
                       +-------+-------+               v
                               |               +-------+--------------------+
                               v               | Background Media Workers   |
                   +-----------+-----------+   | (Thumbnailer / Transcoder) |
                   | PostgreSQL Metadata   |   +-------+--------------------+
                   | Status: "READY"       |           |
                   +-----------------------+           v
                                               +-------+--------------------+
                                               | Global CDN (CloudFront)    |
                                               | Edge Cache for Downloads   |
                                               +----------------------------+
```

---

## 3. Data Model for File Metadata

```sql
CREATE TABLE media_files (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL,
    filename VARCHAR(255) NOT NULL,
    mime_type VARCHAR(100) NOT NULL,
    file_size_bytes BIGINT NOT NULL,
    s3_key VARCHAR(512) UNIQUE NOT NULL,
    upload_id VARCHAR(255) NULL, -- For S3 Multipart tracking
    status VARCHAR(32) NOT NULL DEFAULT 'PENDING', -- PENDING, UPLOADED, PROCESSING, READY
    sha256_hash VARCHAR(64) NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX ix_media_files_user ON media_files (user_id);
```

---

## 4. FastAPI Production Implementation Blueprint

```python
import uuid
import aioboto3
from fastapi import APIRouter, HTTPException, status
from pydantic import BaseModel, Field

router = APIRouter(prefix="/storage", tags=["Storage"])
s3_session = aioboto3.Session()
BUCKET_NAME = "prod-media-vault"


class PresignedRequest(BaseModel):
    filename: str
    mime_type: str = Field(..., pattern=r"^(image/|video/|application/pdf)")
    file_size_bytes: int = Field(..., gt=0, le=5 * 1024 * 1024 * 1024)  # 5GB max


class PresignedResponse(BaseModel):
    file_id: uuid.UUID
    s3_key: str
    upload_url: str
    expires_in_seconds: int = 900


@router.post("/presigned-upload", response_model=PresignedResponse, status_code=status.HTTP_201_CREATED)
async def generate_presigned_url(req: PresignedRequest):
    file_id = uuid.uuid4()
    s3_key = f"raw/{file_id}/{req.filename}"

    async with s3_session.client("s3") as s3:
        # Generate presigned PUT URL directly to S3
        url = await s3.generate_presigned_url(
            ClientMethod="put_object",
            Params={
                "Bucket": BUCKET_NAME,
                "Key": s3_key,
                "ContentType": req.mime_type,
            },
            ExpiresIn=900,  # 15 minutes
        )

    # In production: Record row in PostgreSQL metadata table as 'PENDING'
    return PresignedResponse(
        file_id=file_id,
        s3_key=s3_key,
        upload_url=url,
        expires_in_seconds=900,
    )
```

---

## 5. Failure Modes, Scalability & Bottlenecks

| Failure Scenario | Impact | Mitigation Strategy |
|:---|:---|:---|
| **Dangling Incomplete Uploads** | Clients abandon uploads halfway, costing storage fees for orphaned chunks. | Enable **S3 Lifecycle Rule**: `AbortIncompleteMultipartUpload` after 7 days automatically. |
| **Malicious File Masquerading (MIME spoofing)** | Attacker uploads executable `.exe` disguised as `.png`. | Inspect file magic bytes asynchronously in post-processing worker before setting status to `READY`; serve all user downloads with `Content-Disposition: attachment` or strict `Content-Security-Policy`. |
| **Hotspotting on S3 Key Prefixes** | > 5,500 requests per second to a single S3 partition. | Distribute keys using random UUID prefixes (e.g. `s3://bucket/u3f-91a/...`) to ensure automated partitioning across AWS S3 storage nodes. |
