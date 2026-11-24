# System Design Blueprint: Video Streaming & Distributed Transcoding Pipeline (HLS / DASH)

## Metadata

- **Topic**: Video-on-Demand (VOD) Ingestion, Transcoding & Adaptive Bitrate Streaming
- **Scale Requirements**: 100,000 video uploads daily, 50 Million video streaming hours daily, 10 Terabits/sec peak CDN egress
- **Target SLA**: Upload to available streaming in < 3 minutes; zero buffering at 1080p/4K; 99.99% video playback availability
- **Tech Stack**: FastAPI, FFmpeg, Celery / Kubernetes Batch Jobs, AWS S3 / MinIO, AWS CloudFront, HLS (m3u8) / MPEG-DASH

---

## 1. Requirements & System Estimation

### 1.1 Functional Requirements
1. **Source Ingestion**: Resumable multi-gigabyte raw video upload directly to S3.
2. **Distributed Transcoding**: Convert master MP4/MOV into multiple Adaptive Bitrate (ABR) profiles: 1080p, 720p, 480p, 360p.
3. **Packaging & Segmentation**: Segment videos into 2-6 second `.ts` or `.m4s` chunks with Master `.m3u8` playlist index for Apple HLS and DASH.
4. **Content Security**: Signed CloudFront URLs / Cookies to prevent unauthorized hotlinking and scrape downloads.

### 1.2 Storage & Bandwidth Estimation
- 100,000 uploads / day $\times$ 500 MB average raw video $\approx 50\text{ TB / day}$ raw.
- Transcoded output: 1080p + 720p + 480p + 360p $\approx 1.5\text{x}$ master size $\approx 75\text{ TB / day}$.
- CDN Caching ratio: > 95% of video chunks served directly from CloudFront edge memory/SSD without hitting origin storage.

---

## 2. High-Level Architecture Diagram

```
                 +-----------------------------------------------+
                 |       Content Creator / Video Uploader        |
                 +-----------------------+-----------------------+
                                         |
                                         | 1. S3 Presigned Multipart Upload
                                         v
                 +-----------------------+-----------------------+
                 | AWS S3 Raw Ingest Bucket (s3://raw-videos)    |
                 +-----------------------+-----------------------+
                                         |
                                         | 2. S3 ObjectCreated Notification
                                         v
                 +-----------------------+-----------------------+
                 | AWS SQS / RabbitMQ Transcode Task Queue       |
                 +-----------------------+-----------------------+
                                         |
                                         | 3. Dequeue Transcoding Job
                                         v
                 +-----------------------+-----------------------+
                 | Distributed Worker Pods (FFmpeg on K8s)       |
                 | - Splits video into segments                  |
                 | - Encodes to 1080p, 720p, 480p H.264 / HEVC   |
                 | - Generates master.m3u8 and chunk-001.ts      |
                 +-----------------------+-----------------------+
                                         |
                                         | 4. Upload HLS Assets
                                         v
                 +-----------------------+-----------------------+
                 | AWS S3 Output Bucket (s3://stream-assets)     |
                 +-----------------------+-----------------------+
                                         |
                                         | 5. Global Edge Distribution
                                         v
                 +-----------------------+-----------------------+
                 | Global CDN (CloudFront / Fastly)              |
                 +-----------------------+-----------------------+
                                         |
                                         | 6. HLS Stream (master.m3u8)
                                         v
                 +-----------------------------------------------+
                 | End-User Video Player (Web / iOS / Android)   |
                 +-----------------------------------------------+
```

---

## 3. HLS Master Playlist Architecture (`master.m3u8`)

```
#EXTM3U
#EXT-X-VERSION:3

# 1080p Stream (5.0 Mbps)
#EXT-X-STREAM-INF:BANDWIDTH=5000000,RESOLUTION=1920x1080
1080p/index.m3u8

# 720p Stream (2.5 Mbps)
#EXT-X-STREAM-INF:BANDWIDTH=2500000,RESOLUTION=1280x720
720p/index.m3u8

# 480p Stream (1.0 Mbps)
#EXT-X-STREAM-INF:BANDWIDTH=1000000,RESOLUTION=854x480
480p/index.m3u8
```

---

## 4. FastAPI Production Implementation Blueprint

```python
import uuid
from fastapi import APIRouter, HTTPException, status
from pydantic import BaseModel
import redis.asyncio as aioredis

router = APIRouter(prefix="/videos", tags=["Video Streaming"])
redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)


class TranscodeJobRequest(BaseModel):
    video_id: uuid.UUID
    raw_s3_key: str


@router.post("/transcode/queue", status_code=status.HTTP_202_ACCEPTED)
async def queue_transcode_job(req: TranscodeJobRequest):
    # Enqueue job into distributed transcode stream/queue
    job_payload = {
        "video_id": str(req.video_id),
        "raw_s3_key": req.raw_s3_key,
        "profiles": "1080p,720p,480p",
        "segment_duration_sec": 4,
    }
    await redis_client.xadd("stream:transcode_jobs", {"job": str(job_payload)})
    return {"status": "QUEUED", "video_id": req.video_id}


@router.get("/{video_id}/playback-url")
async def get_secure_playback_url(video_id: uuid.UUID):
    # Generate CloudFront Signed URL with 4-hour playback window
    base_cdn_url = f"https://cdn.videoplatform.io/streams/{video_id}/master.m3u8"
    # In production: sign with private key and return signed query string
    return {"stream_url": base_cdn_url, "format": "HLS"}
```

---

## 5. Failure Modes, Scalability & Bottlenecks

| Failure Scenario | Impact | Mitigation Strategy |
|:---|:---|:---|
| **CPU Saturation during Long Video Transcoding (2-Hour 4K Movie)** | Single worker node takes hours to encode, blocking other jobs. | **Segment-Level Parallel Transcoding**: Pre-split raw file into 10-minute segments, transcode segments across 20 parallel K8s workers, and stitch final HLS manifests together. |
| **Origin Storage Overload during Viral Video Spike** | Millions of viewers request video chunks simultaneously. | Set aggressive CloudFront edge caching: `.ts` chunk files are immutable (cache TTL: 1 year). Only `master.m3u8` has short TTL (10s for live, 1 hour for VOD). |
| **Piracy & Unauthorized Video Hotlinking** | Third-party websites embed video streams directly. | Require **CloudFront Signed Cookies** validated on every `.m3u8` and `.ts` chunk request; restrict CORS origins. |
