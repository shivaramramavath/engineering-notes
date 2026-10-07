# File Upload

Accepting files means accepting untrusted bytes from the internet. This note shows the three practical approaches, with the security checks each one needs.

> Verified against the Next.js 16.4 docs for the Route Handler and Server Action parts. Storage examples use the AWS SDK v3 packages; adapt them to your provider.

## Choose an approach

| Approach | How it works | Good for | Limits |
|---|---|---|---|
| **A. Multipart to your server** (Route Handler or Server Action) | Browser sends `multipart/form-data`; your code reads `File`s | Small files (avatars, documents), simple apps | Whole body goes through your server; platform body and time limits apply |
| **B. Direct to storage with a signed URL/form** | Server authorizes and signs; browser uploads straight to the bucket | Large files, many users, serverless hosting | Needs a storage service; two-step flow |
| **C. Resumable / chunked** | Library uploads in chunks and resumes after failures | Very large files, flaky networks | Extra library and server support |

Rule of thumb: **small and simple → A, anything big or at scale → B.** Sending large bodies through a serverless function wastes memory and hits timeouts.

## A. Multipart upload through your server

### Client

```tsx
"use client";

import { useState } from "react";

export function AvatarUpload() {
  const [status, setStatus] = useState<string>("");

  async function onSubmit(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const formData = new FormData(e.currentTarget);

    setStatus("Uploading…");
    const res = await fetch("/api/upload", { method: "POST", body: formData });
    // Do NOT set the Content-Type header yourself; the browser adds the multipart boundary.

    setStatus(res.ok ? "Done" : (await res.json()).error ?? "Failed");
  }

  return (
    <form onSubmit={onSubmit}>
      <input type="file" name="avatar" accept="image/png,image/jpeg,image/webp" required />
      <button>Upload</button>
      <p aria-live="polite">{status}</p>
    </form>
  );
}
```

Setting `Content-Type: multipart/form-data` manually breaks the request, because the required `boundary` value is missing. Leave it unset and let the browser fill it in.

### Route Handler

```ts
// app/api/upload/route.ts
import { NextResponse } from "next/server";
import { randomUUID } from "node:crypto";
import { auth } from "@/lib/auth";
import { saveToStorage } from "@/lib/storage";

const MAX_BYTES = 2 * 1024 * 1024; // 2 MB
const ALLOWED: Record<string, string> = {
  "image/png": "png",
  "image/jpeg": "jpg",
  "image/webp": "webp",
};

export async function POST(request: Request) {
  const session = await auth();
  if (!session?.user) return NextResponse.json({ error: "Unauthorized" }, { status: 401 });

  // cheap early rejection (the header can be wrong or missing, so this is not the real check)
  const declared = Number(request.headers.get("content-length") ?? 0);
  if (declared > MAX_BYTES + 10_000) {
    return NextResponse.json({ error: "File too large" }, { status: 413 });
  }

  const form = await request.formData();
  const file = form.get("avatar");

  if (!(file instanceof File) || file.size === 0) {
    return NextResponse.json({ error: "No file" }, { status: 400 });
  }
  if (file.size > MAX_BYTES) {
    return NextResponse.json({ error: "File too large" }, { status: 413 });
  }

  const bytes = new Uint8Array(await file.arrayBuffer());
  const type = sniffImageType(bytes);                // check the real content, not file.type
  if (!type || !(type in ALLOWED)) {
    return NextResponse.json({ error: "Unsupported file type" }, { status: 415 });
  }

  const key = `avatars/${session.user.id}/${randomUUID()}.${ALLOWED[type]}`; // our own name
  await saveToStorage(key, bytes, type);

  return NextResponse.json({ key }, { status: 201 });
}

function sniffImageType(b: Uint8Array): string | null {
  if (b[0] === 0x89 && b[1] === 0x50 && b[2] === 0x4e && b[3] === 0x47) return "image/png";
  if (b[0] === 0xff && b[1] === 0xd8 && b[2] === 0xff) return "image/jpeg";
  if (b[0] === 0x52 && b[1] === 0x49 && b[2] === 0x46 && b[3] === 0x46 && b[8] === 0x57 && b[9] === 0x45 && b[10] === 0x42 && b[11] === 0x50) return "image/webp";
  return null;
}
```

`request.formData()` reads the **entire body into memory**. That is fine for 2 MB, not for 500 MB.

### Server Action version

A form can post files straight to a Server Action:

```tsx
<form action={uploadAvatar}>
  <input type="file" name="avatar" accept="image/*" />
  <button>Upload</button>
</form>
```

```ts
"use server";

export async function uploadAvatar(formData: FormData) {
  const session = await auth();
  if (!session?.user) throw new Error("Unauthorized");

  const file = formData.get("avatar");
  if (!(file instanceof File) || file.size === 0 || file.size > 2 * 1024 * 1024) {
    return { error: "Choose an image under 2 MB" };
  }
  // same checks and storage as above
}
```

Server Action requests are capped at **1 MB by default**. Raise `serverActions.bodySizeLimit` in `next.config.ts` if you need more (see [Server Actions](../07-server-actions/00-server-actions.md)). Actions are dispatched one at a time, so a slow upload blocks later actions from the same client. Use an action for small forms and a Route Handler (or approach B) for heavier uploads.

## B. Direct upload with a signed form

The browser uploads to storage itself. Your server only **authorizes** and **records** the result. This keeps big files out of your functions.

```text
1. browser → POST /api/upload/sign   { filename, type, size }
2. server  → checks auth, type, size → returns a short-lived signed upload
3. browser → uploads the file directly to the bucket
4. browser → POST /api/upload/complete { key }
5. server  → verifies the object exists → saves it in the database
```

### Step 2: sign (with a size limit)

A pre-signed **PUT URL** cannot enforce a maximum size by itself. A pre-signed **POST** with conditions can, so prefer it when size matters:

```ts
// app/api/upload/sign/route.ts
import { S3Client } from "@aws-sdk/client-s3";
import { createPresignedPost } from "@aws-sdk/s3-presigned-post";
import { randomUUID } from "node:crypto";
import { NextResponse } from "next/server";
import { auth } from "@/lib/auth";

const s3 = new S3Client({ region: process.env.AWS_REGION });
const ALLOWED = new Set(["image/png", "image/jpeg", "image/webp", "application/pdf"]);
const MAX_BYTES = 10 * 1024 * 1024;

export async function POST(request: Request) {
  const session = await auth();
  if (!session?.user) return NextResponse.json({ error: "Unauthorized" }, { status: 401 });

  const { type } = (await request.json()) as { type?: string };
  if (!type || !ALLOWED.has(type)) {
    return NextResponse.json({ error: "Unsupported type" }, { status: 415 });
  }

  const key = `uploads/${session.user.id}/${randomUUID()}`;   // server chooses the key

  const { url, fields } = await createPresignedPost(s3, {
    Bucket: process.env.S3_BUCKET!,
    Key: key,
    Conditions: [
      ["content-length-range", 1, MAX_BYTES],   // enforced by S3
      ["eq", "$Content-Type", type],
    ],
    Fields: { "Content-Type": type },
    Expires: 60,                                // seconds
  });

  return NextResponse.json({ url, fields, key });
}
```

### Step 3: browser uploads

```ts
async function upload(file: File) {
  const sign = await fetch("/api/upload/sign", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ type: file.type }),
  }).then((r) => r.json());

  const form = new FormData();
  Object.entries(sign.fields).forEach(([k, v]) => form.append(k, v as string));
  form.append("file", file);                    // the file field must come LAST

  const res = await fetch(sign.url, { method: "POST", body: form });
  if (!res.ok) throw new Error("Upload failed");
  return sign.key as string;
}
```

The bucket must allow your site's origin in its **CORS** rules for this browser upload to work.

### Step 5: confirm

Do not trust the client's "I uploaded it". Check the object, then record it:

```ts
// in a Server Action or Route Handler
import { HeadObjectCommand } from "@aws-sdk/client-s3";

const head = await s3.send(new HeadObjectCommand({ Bucket, Key: key }));
// check head.ContentLength and head.ContentType, and that key starts with `uploads/${session.user.id}/`
await db.file.create({ data: { key, ownerId: session.user.id, size: head.ContentLength ?? 0 } });
```

For stronger guarantees, inspect the file after upload (magic bytes, antivirus, image re-encoding) before making it available.

## Upload progress

`fetch` does not report upload progress. Use `XMLHttpRequest` when you need a progress bar:

```ts
function uploadWithProgress(url: string, body: FormData, onProgress: (pct: number) => void) {
  return new Promise<void>((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open("POST", url);
    xhr.upload.onprogress = (e) => e.lengthComputable && onProgress((e.loaded / e.total) * 100);
    xhr.onload = () => (xhr.status < 300 ? resolve() : reject(new Error(`HTTP ${xhr.status}`)));
    xhr.onerror = () => reject(new Error("Network error"));
    xhr.send(body);
  });
}
```

## Security checklist

| Risk | Defense |
|---|---|
| Anonymous uploads | Authenticate before accepting or signing |
| Huge files | Enforce a size limit on the server; for signed uploads use `content-length-range` |
| Fake file types | `file.type` and the extension are client-supplied. Check the real bytes (magic numbers) or re-encode images |
| Path traversal (`../../x`) | Never use the client filename as a path; generate your own key |
| Overwriting others' files | Put the user ID and a random ID in the key |
| Executable or active content (HTML, SVG, scripts) served from your origin | Serve uploads from a separate domain or with `Content-Disposition: attachment`; restrict allowed types |
| Malware | Scan uploads where users share files with each other |
| Leaking private files | Keep the bucket private; hand out short-lived signed GET URLs |
| Storage abuse | Per-user quotas and rate limits |
| Writing to local disk on serverless | The filesystem is ephemeral or read-only; use object storage |
| Metadata leaks | Strip EXIF (GPS data) from user photos if you re-publish them |

Keep the original filename only as display metadata in your database, escaped when shown.

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Setting `Content-Type: multipart/form-data` manually | Server cannot parse the body | Omit the header with `FormData` |
| Trusting `file.type` | Disguised scripts or oversized junk accepted | Sniff magic bytes |
| Using `file.name` in the storage path | Path traversal or overwrites | Generate a random key |
| Pushing big files through a serverless function | Timeouts, memory errors, platform "payload too large" | Direct-to-storage upload |
| Action upload over 1 MB fails | Request rejected | Raise `bodySizeLimit` or use a Route Handler / signed upload |
| Pre-signed PUT with no size control | Users upload anything | Pre-signed POST with `content-length-range` |
| Trusting the client's "upload complete" | Records point at missing or wrong files | `HeadObject` before saving |
| File field before other fields in a signed S3 POST | Upload rejected | Append `file` last |
| Browser upload blocked | Bucket CORS missing | Allow your origin on the bucket |
| Public bucket by default | Anyone can read private files | Private bucket + signed URLs |

## Quick Summary

- Small files: multipart to a Route Handler or Server Action. Large files: signed direct-to-storage upload.
- `request.formData()` buffers the whole body; Server Actions cap at 1 MB by default.
- Never trust the filename, extension or `file.type`; check the real bytes and generate your own storage key.
- Authenticate first, enforce size limits on the server (or in the signed policy), and verify uploads before recording them.
- Do not set `Content-Type` yourself when sending `FormData`.
- Keep storage private and serve through signed URLs.

## Next

- [09 · Styling and Assets](../09-styling-and-assets/README.md)
- [Route Handlers](./00-route-handlers.md)
- [Server Actions](../07-server-actions/00-server-actions.md)
