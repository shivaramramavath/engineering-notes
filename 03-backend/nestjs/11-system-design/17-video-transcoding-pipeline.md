---
title: "System Design: Asynchronous Video Transcoding and HLS Streaming"
description: "Designing a YouTube-style video processing engine: S3 chunking, FFmpeg transcoding workers, and HLS distribution"
tags: [nestjs, system-design, video-transcoding, ffmpeg, hls, bullmq]
---

# System Design: Asynchronous Video Transcoding and HLS Streaming

## Architecture
- **Ingestion**: S3 presigned multipart upload.
- **Worker Pipeline**: BullMQ dispatches jobs to GPU-enabled worker pods executing FFmpeg (generating 1080p, 720p, 480p `.m3u8` HLS playlists).
