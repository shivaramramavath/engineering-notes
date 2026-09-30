# File Upload

Handling files uploaded from a client — images, documents, avatars — using `multer`, the standard Express middleware for parsing `multipart/form-data` requests.

## Why this needs special handling

```js
app.use(express.json()); // only parses JSON bodies
```

`express.json()`/`express.urlencoded()` don't handle file uploads at all — a file upload uses a different request encoding, `multipart/form-data`, which can mix binary file content with regular text fields in a single request body. Parsing that format is exactly what `multer` does.

```bash
npm install multer
```

---

## Basic single file upload

```js
import multer from "multer";

const upload = multer({ dest: "uploads/" });

app.post("/avatar", upload.single("avatar"), (req, res) => {
  console.log(req.file);
  // {
  //   fieldname: 'avatar',
  //   originalname: 'photo.jpg',
  //   filename: 'a1b2c3d4e5f6',   // multer generates a random name to avoid collisions
  //   path: 'uploads/a1b2c3d4e5f6',
  //   size: 204800,
  //   mimetype: 'image/jpeg'
  // }
  res.json({ file: req.file });
});
```

`upload.single("avatar")` expects exactly one file, sent under the form field name `"avatar"` — this must match the field name the client actually used when constructing the form data.

```html
<form action="/avatar" method="post" enctype="multipart/form-data">
  <input type="file" name="avatar" />
</form>
```

```js
// client-side JS equivalent
const formData = new FormData();
formData.append("avatar", fileInput.files[0]);
fetch("/avatar", { method: "POST", body: formData });
```

---

## Multiple files

```js
app.post("/gallery", upload.array("photos", 5), (req, res) => {
  console.log(req.files); // an array of file objects, up to 5
  res.json({ count: req.files.length });
});
```

### Multiple different fields

```js
const uploadFields = upload.fields([
  { name: "avatar", maxCount: 1 },
  { name: "documents", maxCount: 3 },
]);

app.post("/profile", uploadFields, (req, res) => {
  console.log(req.files.avatar); // array with 1 item
  console.log(req.files.documents); // array with up to 3 items
});
```

---

## Storage: disk vs memory

```js
// disk storage — writes directly to a folder (default behavior)
const upload = multer({ dest: "uploads/" });
```

```js
// memory storage — keeps the file as a Buffer in memory, doesn't touch disk
const upload = multer({ storage: multer.memoryStorage() });

app.post("/avatar", upload.single("avatar"), (req, res) => {
  console.log(req.file.buffer); // the raw file content — see 02-core-modules/06-buffer.md
});
```

Memory storage is common when the file is immediately being forwarded elsewhere (e.g. uploaded to S3/a cloud storage bucket) rather than kept on the app server's own disk — avoids an unnecessary write-then-read cycle, but means large files consume request-handling memory directly.

### Custom disk storage: controlling filenames

```js
const storage = multer.diskStorage({
  destination: (req, file, cb) => cb(null, "uploads/"),
  filename: (req, file, cb) => {
    const uniqueSuffix = `${Date.now()}-${Math.round(Math.random() * 1e9)}`;
    cb(null, `${uniqueSuffix}-${file.originalname}`);
  },
});

const upload = multer({ storage });
```

Never trust `file.originalname` alone as a filename to write to disk — see the validation section below for why.

---

## Limiting file size and type

```js
const upload = multer({
  dest: "uploads/",
  limits: {
    fileSize: 5 * 1024 * 1024, // 5MB
  },
  fileFilter: (req, file, cb) => {
    const allowedTypes = ["image/jpeg", "image/png", "image/webp"];
    if (!allowedTypes.includes(file.mimetype)) {
      return cb(new Error("Only JPEG, PNG, and WebP images are allowed"));
    }
    cb(null, true);
  },
});
```

- `limits.fileSize` — rejects anything over the size cap, protecting against a client uploading an enormous file that exhausts disk/memory
- `fileFilter` — rejects disallowed file types **before** the file is fully written to disk, based on the declared MIME type

**Important caveat:** `file.mimetype` is reported by the _client_ and isn't verified by multer itself — a malicious client can lie about it. For genuinely security-sensitive validation (not just a basic UX-level check), verify the actual file content server-side after upload (e.g. checking a file's magic bytes, or using a dedicated library), rather than trusting the declared MIME type alone.

---

## Handling multer's errors

```js
app.post("/avatar", (req, res, next) => {
  upload.single("avatar")(req, res, (err) => {
    if (err instanceof multer.MulterError) {
      return res.status(400).json({ error: err.message }); // e.g. file too large
    } else if (err) {
      return res.status(400).json({ error: err.message }); // e.g. fileFilter rejection
    }
    next();
  });
});
```

Or, more simply, let it flow into your centralized error handler (`04-error-handling.md`):

```js
app.post("/avatar", upload.single("avatar"), (req, res) => {
  res.json({ file: req.file });
});

app.use((err, req, res, next) => {
  if (err instanceof multer.MulterError || err.message?.includes("Only JPEG")) {
    return res.status(400).json({ error: err.message });
  }
  next(err);
});
```

---

## Security: never trust the original filename

```js
// ❌ dangerous — file.originalname is fully attacker-controlled
fs.writeFileSync(`uploads/${file.originalname}`, file.buffer);
```

A malicious filename like `../../etc/passwd` could allow writing outside the intended uploads directory entirely — the same path traversal risk covered in `02-core-modules/02-path.md`. Always generate your own safe filename (as the custom storage example above does) rather than using `originalname` directly in a filesystem path; if you need to preserve it for display purposes, store it as separate metadata instead.

---

## After upload: what usually comes next

In a real app, uploaded files are rarely kept only on the local app server's disk — that doesn't survive a redeploy, doesn't scale across multiple instances (`02-core-modules/10-cluster-and-worker-threads.md`, `05-load-balancing.md` in the Nginx docs), and has no built-in redundancy. The typical next step is uploading the file to object storage (like AWS S3) immediately after receiving it, and storing only the resulting URL/key in your database.

## Common mistakes

- **Trusting `file.mimetype` as a genuine security check** — it's client-reported and can be spoofed; fine for basic UX filtering, not sufficient alone for security-sensitive validation.
- **Using `file.originalname` directly as a filesystem path** — a path traversal risk; always generate your own safe filename.
- **No `limits.fileSize`** — leaves the app open to a trivial denial-of-service via an enormous upload.
- **Storing uploaded files only on local disk in production** — doesn't survive redeploys or scale across multiple instances; move to object storage for anything beyond local development.
- **Forgetting the field name must match between client and `upload.single("fieldName")`** — a mismatch results in `req.file` being `undefined` with no obvious error.

## Quick summary

- `multer` parses `multipart/form-data`, which `express.json()`/`urlencoded()` don't handle
- `upload.single()`/`.array()`/`.fields()` cover single-file, multi-file-same-field, and multi-field upload shapes
- Disk storage writes directly to a folder; memory storage keeps the file as a `Buffer`, useful when forwarding straight to cloud storage
- `limits.fileSize` and `fileFilter` guard against oversized/wrong-type uploads — but verify file type server-side for real security, not just the client-reported MIME type
- Never build a filesystem path from `file.originalname` directly — generate your own safe filename instead

## Section complete

That covers building a real Express application: setup, routing, middleware, controllers, error handling, validation, auth/authorization, and file uploads. **`07-databases`** covers connecting all of this to an actual data store.
