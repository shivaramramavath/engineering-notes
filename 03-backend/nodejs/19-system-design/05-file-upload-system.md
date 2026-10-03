# File Upload System

Let users upload files — profile photos, documents, videos — and download or view them later. Handling a 5 KB avatar is easy (`06-express/07-file-upload.md`). Handling **many large files**, reliably, securely, and cheaply is a system design problem: routing every byte through your API servers doesn't scale.

## Requirements

**Functional**
- Upload files from web and mobile clients (images, documents, large videos)
- Download/view files later, with access control (public vs private)
- Post-processing: virus scan, thumbnails, video transcoding, metadata extraction
- Delete files; optionally versioning

**Non-functional**
- Support **large files** (hundreds of MB to GB) on unreliable connections — **resumable** uploads
- **Durable** — uploaded files must not be lost
- **Secure** — validate types, scan for malware, enforce access control
- API servers must not become the bottleneck for file bytes
- Cost-aware: storage and bandwidth are the big bills

**Out of scope:** collaborative editing, full-text search of contents.

## Scale estimate

Assumptions: 1 million uploads/day, average 2 MB, with 1% large videos averaging 500 MB.

| Quantity | Calculation | Result |
|----------|-------------|--------|
| Uploads/sec (avg) | 1M ÷ 86,400 | ~12/sec |
| Data/day, regular files | 990,000 × 2 MB | ~2 TB |
| Data/day, videos | 10,000 × 500 MB | ~5 TB |
| Total growth | | ~7 TB/day → ~2.5 PB/year |

Takeaways: request *count* is modest, but **data volume is huge** — that's why bytes must go to purpose-built object storage, not through Node processes and not into a relational database.

## Why not just use `multer` and disk?

Proxying uploads through the API (client → Node → storage) has real costs:

- Every byte occupies an API instance's bandwidth, memory/CPU, and **an open connection for the whole upload** — slow mobile clients tie up resources for minutes
- Files on local disk are lost when the container restarts and aren't visible to other instances (`01-scalable-api-and-rate-limiter.md` — stateless services)
- A request body size limit (`express.json`-style limits, proxy `client_max_body_size`) becomes a hard ceiling

## The standard design: pre-signed URLs + object storage

Instead of receiving the file, the API **authorizes** the upload and hands the client a short-lived, signed URL that lets it upload **directly to object storage** (S3 or compatible — `16-production/06-aws.md`).

```
 1. Client ──► API: "I want to upload report.pdf (3 MB, application/pdf)"
 2. API: authenticate, validate metadata, create file record (status=pending),
         generate a pre-signed upload URL
 3. API ──► Client: { fileId, uploadUrl }
 4. Client ──────────────► Object storage   (file bytes go directly here — API not involved)
 5. Storage ──► event notification ──► processing queue    (or client calls "complete")
 6. Worker: verify, scan, process → update record status=ready
 7. Client: polls / gets a socket or webhook notification that it's ready
```

API instances now handle small JSON requests only; storage handles the heavy lifting and scales virtually without limit.

## Step 1–3: requesting an upload URL

```js
import { S3Client, PutObjectCommand } from "@aws-sdk/client-s3";
import { getSignedUrl } from "@aws-sdk/s3-request-presigner";
import { randomUUID } from "node:crypto";

const s3 = new S3Client({ region: process.env.AWS_REGION });

const ALLOWED_TYPES = new Map([
  ["image/jpeg", 10 * 1024 * 1024],
  ["image/png", 10 * 1024 * 1024],
  ["application/pdf", 50 * 1024 * 1024],
]);

app.post("/api/uploads", requireAuth, async (req, res, next) => {
  try {
    const { filename, contentType, size } = req.body;

    // Validate BEFORE issuing a URL
    const maxSize = ALLOWED_TYPES.get(contentType);
    if (!maxSize) return res.status(415).json({ error: "File type not allowed" });
    if (!Number.isInteger(size) || size <= 0 || size > maxSize) {
      return res.status(413).json({ error: "File too large" });
    }

    const fileId = randomUUID();
    const key = `uploads/${req.user.id}/${fileId}`;          // server-chosen key — NEVER the client's filename

    await files.create({
      id: fileId, ownerId: req.user.id, key,
      originalName: filename, contentType, size, status: "pending",
    });

    const uploadUrl = await getSignedUrl(
      s3,
      new PutObjectCommand({ Bucket: process.env.BUCKET, Key: key, ContentType: contentType }),
      { expiresIn: 300 }                                      // valid for 5 minutes
    );

    res.status(201).json({ fileId, uploadUrl });
  } catch (err) {
    next(err);
  }
});
```

Key security points:

- **Generate the storage key server-side.** Using the client's filename invites path tricks (`../`), overwrites, and collisions. Store the original name as metadata only.
- **Validate type and size before signing.** A pre-signed URL is a capability — whoever holds it can use it until it expires. Keep expiry short.
- **Don't trust the declared `Content-Type`.** It's client-supplied. Verify actual content in the processing step (below).
- **Scope the URL** to one exact key and operation. Where your storage supports it, also enforce size limits in the signature/policy (e.g. S3 presigned POST policies can set a content-length range) rather than relying on the client.
- **Configure CORS** on the bucket so browsers may `PUT` from your origin (`05-http-web/04-cors.md`).

## Step 5: knowing the upload finished

Two common options:

| Approach | How | Notes |
|----------|-----|-------|
| **Storage event notification** | Bucket emits an event (e.g. "object created") to a queue | Authoritative — works even if the client crashes before reporting back |
| **Client "complete" call** | Client calls `POST /uploads/:id/complete` | Simple, but can't be trusted alone; the server should verify the object exists and matches |

Use the **storage event** as the source of truth, and optionally let the client call "complete" for faster UI feedback. Either way, the server confirms with a `HEAD` request that the object exists and its size matches expectations before marking it usable.

## Step 6: asynchronous post-processing

Heavy or risky work happens in background workers, never on the upload path (`11-async-processing/`, `06-job-processing-system.md`):

```
 object created ──► queue ──► worker pipeline:
                               1. verify size / sniff real type (magic bytes)
                               2. virus/malware scan
                               3. type-specific processing:
                                    images → resize, generate thumbnails, strip EXIF location data
                                    video  → transcode to streaming formats
                                    docs   → extract text / preview
                               4. write derived files back to storage
                               5. status = ready (or rejected)
```

- **Quarantine until scanned.** Newly uploaded objects should live in a "pending" location or state and be unavailable for download until they pass checks.
- **Verify the real type** by inspecting file contents (magic bytes), not the extension or declared header.
- **Strip sensitive metadata** — images often carry GPS coordinates in EXIF.
- Make workers **idempotent** — the same file may be processed twice after a retry.

## Large and unreliable uploads: multipart / resumable

A single `PUT` of a 2 GB file over mobile data will fail eventually and restart from zero. Use **multipart upload**:

```
 1. API: start multipart upload → uploadId
 2. API: return a pre-signed URL for each part (e.g. 8 MB chunks)
 3. Client uploads parts in parallel, retrying any that fail
 4. Client sends the list of completed parts; API completes the upload
 5. Storage assembles the final object
```

- Failed parts retry **individually** — no restart from the beginning
- Parallel parts improve throughput
- The client can **resume** after an app restart by asking which parts already succeeded
- **Clean up abandoned multipart uploads.** Incomplete uploads still consume storage and cost money — configure a lifecycle rule to abort them after a few days
- For a protocol designed around resumability, **tus** is an open standard with server and client libraries

## Downloads and access control

| File type | Approach |
|-----------|----------|
| **Public** (product images, avatars) | Serve through a **CDN** in front of the bucket — cached at the edge, fast, cheap (`05-http-web/05-caching-and-compression.md`) |
| **Private** (invoices, user documents) | API authorizes the requester, then returns a **short-lived pre-signed GET URL** (or signed CDN URL/cookie) |

```js
app.get("/api/files/:id/download", requireAuth, async (req, res, next) => {
  try {
    const file = await files.get(req.params.id);
    if (!file || file.status !== "ready") return res.status(404).end();
    if (file.ownerId !== req.user.id && !(await acl.canRead(req.user.id, file.id))) {
      return res.status(403).end();                       // authorize EVERY access
    }

    const url = await getSignedUrl(
      s3,
      new GetObjectCommand({ Bucket: process.env.BUCKET, Key: file.key }),
      { expiresIn: 60 }
    );
    res.json({ url });
  } catch (err) {
    next(err);
  }
});
```

- The authorization check lives in your API; storage never decides who may read what
- **Never make private buckets public** to "make downloads easy"
- Serve user-uploaded content from a **separate domain** (or with `Content-Disposition: attachment` and correct `Content-Type`) so an uploaded HTML or SVG file can't execute scripts in your app's origin (`08-authentication-security/05-common-vulnerabilities.md`)
- Remember signed URLs can be **shared** by whoever receives them until expiry — keep them short-lived for sensitive files

## Metadata

Store file *metadata* in your database, the *bytes* in object storage:

```
files
  id, owner_id, key, original_name, content_type, size,
  checksum, status (pending | processing | ready | rejected | deleted),
  created_at, ...
variants
  file_id, kind (thumb_200, hls_720p, ...), key, size
```

Never store file bytes in a relational database at this scale — it bloats the DB, slows backups, and gets expensive.

**Deduplication (optional):** compute a content hash and reuse an existing object for identical content, saving storage. It complicates deletion (reference counting) and has privacy implications — consider carefully.

## Deletion and lifecycle

- **Soft delete first** (mark `deleted`), physically remove later via a scheduled job (`11-async-processing/03-scheduled-jobs.md`)
- **Lifecycle rules** can move older, rarely accessed files to cheaper storage tiers and expire temporary files
- Honor **data-deletion obligations** (e.g. user account deletion) by removing both originals and derived variants
- Beware **orphans**: objects uploaded but never completed, or whose DB row failed to commit. A reconciliation job comparing storage with the database catches them.

## Scaling and cost

- **Bandwidth is usually the dominant cost.** A CDN reduces origin transfer and latency for popular files
- **Storage tiers:** hot for recent files, cold/archive for old data
- **Regional placement:** store near most users; replicate across regions where durability or latency demands it
- **Limits:** per-user storage quotas and upload rate limits (`01-scalable-api-and-rate-limiter.md`)
- **Backpressure:** if the processing queue backs up, uploads still succeed (files wait as `processing`) — the system degrades gracefully

## Failure scenarios

| Failure | Behavior |
|---------|----------|
| Client loses connection mid-upload | Multipart resumes remaining parts; pending record cleaned up if never completed |
| Worker dies mid-processing | Job retried (idempotent); file stays `processing` until done |
| Malicious file uploaded | Quarantine + scan rejects it; never served |
| Storage event lost | Reconciliation job finds `pending` records whose objects exist (or are stale) |
| Signed URL leaks | Short expiry limits the window; scoped to a single key |

## Trade-offs summary

| Decision | Trade-off |
|----------|-----------|
| Direct-to-storage via signed URL | Scalable and cheap vs more moving parts (callbacks, CORS, reconciliation) |
| Async processing | Fast uploads vs file isn't usable instantly |
| Multipart | Resilient for large files vs added client/server complexity |
| CDN for public files | Speed and cost vs cache invalidation concerns |
| Short-lived signed URLs | Safer vs clients must re-request when expired |

## Common mistakes

- **Proxying large uploads through the API** — ties up connections and bandwidth; use pre-signed URLs.
- **Using the client's filename as the storage key** — path traversal, overwrites, collisions.
- **Trusting the declared MIME type or extension** — inspect actual content.
- **Serving uploaded files from your main origin** — uploaded HTML/SVG can run scripts against your app.
- **Making buckets public for convenience** — authorize through your API and use signed URLs.
- **Skipping size limits and type allow-lists** — storage-cost and abuse risk.
- **Long-lived pre-signed URLs** — effectively permanent leaked access.
- **No cleanup of abandoned multipart uploads or orphaned objects** — silent, growing cost.
- **Processing synchronously in the request** — timeouts and blocked workers.
- **Storing file bytes in the database** — bloat and slow backups.

## Quick summary

- Don't stream files through your API: issue **short-lived pre-signed URLs** so clients upload **directly to object storage**
- Validate type and size *before* signing; generate the key **server-side**
- Treat the **storage event** as the source of truth for "upload finished"
- Process asynchronously — **verify real type, scan, quarantine until safe**, generate thumbnails/transcodes, strip metadata
- Use **multipart/resumable** uploads for large files; clean up abandoned parts
- Public files via **CDN**; private files via **authorized, short-lived signed URLs**
- Metadata in the database, bytes in object storage; plan lifecycle, deletion, and cost

## Next

**`06-job-processing-system.md`** designs the general-purpose background job platform that powers the workers used throughout this folder.
