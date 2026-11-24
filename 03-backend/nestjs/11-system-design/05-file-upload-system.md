---
title: "System Design: Scalable File Ingestion and Processing Pipeline"
description: "Designing a multi-gigabyte file upload system with Amazon S3 presigned URLs, chunked multipart uploads, and virus scanning"
tags: [nestjs, system-design, file-upload, s3, multipart]
---

# System Design: Scalable File Ingestion and Processing Pipeline

## Architecture
- **Direct-to-S3 Upload**: Clients request presigned multipart URLs from NestJS; files upload directly to S3 without saturating backend memory.
- **Asynchronous Processing**: S3 triggers SQS event $	o$ NestJS worker downloads, executes ClamAV virus scan and Sharp image resizing.
