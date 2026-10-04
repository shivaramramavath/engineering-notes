# File Upload

File inputs behave differently from every other form control: they're always uncontrolled, they give you `File` objects instead of strings, and uploading them means `multipart/form-data`, progress reporting, and a stricter security model. This file covers selecting files, validating them, previewing, uploading with progress, drag-and-drop, and the server-side responsibilities you can't skip.

## Prerequisites

[`00-controlled-and-uncontrolled-inputs.md`](./00-controlled-and-uncontrolled-inputs.md), [`../03-hooks/04-useRef.md`](../03-hooks/04-useRef.md), and [`../03-hooks/02-useEffect.md`](../03-hooks/02-useEffect.md) (cleanup)

---

## 1. The file input

```tsx
function AvatarPicker() {
  const [file, setFile] = useState<File | null>(null);

  function handleChange(e: ChangeEvent<HTMLInputElement>) {
    setFile(e.target.files?.[0] ?? null);
  }

  return (
    <label>
      Avatar
      <input type="file" accept="image/png, image/jpeg" onChange={handleChange} />
      {file && <p>{file.name} ({Math.round(file.size / 1024)} KB)</p>}
    </label>
  );
}
```

Facts to know:

- **Always uncontrolled.** There's no `value` prop for file inputs — browsers don't allow scripts to set the selected file (security). Store the `File` yourself in state if you need it.
- `e.target.files` is a `FileList` (array-like, possibly `null`). Take `files?.[0]` for one file; use `Array.from(e.target.files ?? [])` for several.
- A `File` has `name`, `size` (bytes), `type` (MIME type, as reported by the browser), and `lastModified`.
- Add `multiple` to allow several files.
- **`accept`** (`image/*`, `.pdf`, `image/png`) filters the file picker, but it's a **hint**, not validation — users can pick other types or bypass it.
- `capture="environment"` hints mobile browsers to open the camera.

### Resetting the input

You can't set its value, but you can clear it:

```tsx
const inputRef = useRef<HTMLInputElement>(null);
function clear() {
  setFile(null);
  if (inputRef.current) inputRef.current.value = "";   // setting to "" is allowed
}
```

Or remount the input with a `key` ([`../02-state-and-rendering/05-state-preservation-and-reset.md`](../02-state-and-rendering/05-state-preservation-and-reset.md)). Without clearing, selecting the **same file again** after removing it doesn't fire `onChange`.

---

## 2. Client-side validation

Check files before uploading to give immediate feedback. Treat this as UX only; the server must repeat every check ([`01-form-validation.md`](./01-form-validation.md)).

```tsx
const MAX_SIZE = 5 * 1024 * 1024;                       // 5 MB
const ALLOWED = ["image/png", "image/jpeg", "image/webp"];

function validateFile(file: File): string | null {
  if (!ALLOWED.includes(file.type)) return "Use a PNG, JPEG, or WebP image";
  if (file.size > MAX_SIZE) return "The image must be under 5 MB";
  return null;
}
```

With Zod ([`../04-typescript-with-react/05-advanced-typing-patterns.md`](../04-typescript-with-react/05-advanced-typing-patterns.md)):

```tsx
const avatarSchema = z
  .instanceof(File, { message: "Choose a file" })
  .refine((f) => f.size <= MAX_SIZE, "The image must be under 5 MB")
  .refine((f) => ALLOWED.includes(f.type), "Use a PNG, JPEG, or WebP image");
```

Caveats:

- **`file.type` comes from the filename extension**, so it can be wrong or spoofed. Real type checking happens on the server by inspecting the file's actual bytes ("magic numbers").
- Validate **count** for multiple uploads and **total size**.
- For images you may also check dimensions (load into an `Image` and read `naturalWidth`/`naturalHeight`).
- With React Hook Form, register the input and read `FileList`, or use `Controller`; `z.instanceof(File)` fails on the server and in some SSR environments, since `File` may not exist there — use a different check for shared schemas ([`02-react-hook-form.md`](./02-react-hook-form.md)).

---

## 3. Previews

Show the image before uploading using an **object URL**, and **revoke it** to avoid leaking memory:

```tsx
function useObjectUrl(file: File | null) {
  const [url, setUrl] = useState<string | null>(null);

  useEffect(() => {
    if (!file) {
      setUrl(null);
      return;
    }
    const objectUrl = URL.createObjectURL(file);
    setUrl(objectUrl);
    return () => URL.revokeObjectURL(objectUrl);   // cleanup
  }, [file]);

  return url;
}

function Preview({ file }: { file: File | null }) {
  const url = useObjectUrl(file);
  return url ? <img src={url} alt="Selected avatar preview" width={96} height={96} /> : null;
}
```

- `URL.createObjectURL` is synchronous and cheap (it references the file, it doesn't copy it). `FileReader.readAsDataURL` works too but loads the whole file into a string.
- Revoking in the effect cleanup handles both file changes and unmounting ([`../03-hooks/02-useEffect.md`](../03-hooks/02-useEffect.md)).
- Give previews meaningful `alt` text, and for non-images show an icon with the filename.

---

## 4. Uploading

### With `FormData` and `fetch`

```tsx
async function upload(file: File) {
  const body = new FormData();
  body.append("avatar", file);
  body.append("userId", "123");

  const res = await fetch("/api/avatar", { method: "POST", body });
  if (!res.ok) throw new Error(`Upload failed (${res.status})`);
  return res.json();
}
```

**Do not set the `Content-Type` header yourself.** The browser must generate it, including the multipart boundary (`multipart/form-data; boundary=…`). Setting it manually breaks the request.

### With a progress bar

`fetch` does not report **upload** progress. Use `XMLHttpRequest`, or a client built on it such as Axios ([`../11-api-integration/01-axios.md`](../11-api-integration/01-axios.md)):

```tsx
// Axios
await axios.post("/api/avatar", body, {
  onUploadProgress: (e) => {
    if (e.total) setProgress(Math.round((e.loaded / e.total) * 100));
  },
  signal: controller.signal,           // for cancellation
});

// Plain XHR
function uploadWithProgress(file: File, onProgress: (pct: number) => void) {
  return new Promise<void>((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open("POST", "/api/avatar");
    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable) onProgress(Math.round((e.loaded / e.total) * 100));
    };
    xhr.onload = () => (xhr.status < 300 ? resolve() : reject(new Error(`HTTP ${xhr.status}`)));
    xhr.onerror = () => reject(new Error("Network error"));
    const body = new FormData();
    body.append("avatar", file);
    xhr.send(body);
  });
}
```

Progress UI: use `<progress value={progress} max={100}>` (accessible by default), not a styled `<div>`, or add `role="progressbar"` with `aria-valuenow`. Announce completion and failure.

### Cancel, retry, and states

Model each file's upload as a small state machine:

```tsx
type UploadState =
  | { status: "queued" }
  | { status: "uploading"; progress: number }
  | { status: "done"; url: string }
  | { status: "error"; message: string };
```

Support **cancel** (`AbortController`, `xhr.abort()`), **retry** on failure, and a **remove** action. For multiple files, upload with limited concurrency (2–4 at a time) instead of all at once.

---

## 5. Direct-to-storage uploads (presigned URLs)

Routing large files through your API server wastes bandwidth and memory. A common production pattern:

1. The client asks **your API** for permission: "I want to upload `avatar.png` (image/png, 200 KB)".
2. The server validates, then returns a **presigned URL** for object storage (S3, GCS, R2…) plus any required fields.
3. The client **uploads the file directly** to storage with `PUT`/`POST`.
4. The client tells your API the upload finished (or storage notifies the server), and the server records the file.

```tsx
async function uploadDirect(file: File) {
  const { uploadUrl, fileId } = await api.createUpload({ name: file.name, type: file.type, size: file.size });
  await fetch(uploadUrl, { method: "PUT", body: file, headers: { "Content-Type": file.type } });
  await api.completeUpload(fileId);
}
```

Benefits: scalability and resumable uploads for big files. Cost: more moving parts — the server must still limit size, types, and who may upload, and verify the result. (Here you **do** set `Content-Type` because the body is the raw file, and the presigned URL usually requires it to match.)

For very large files, look at **chunked/resumable uploads** (tus, multipart upload APIs).

---

## 6. Drag and drop

Use a library (`react-dropzone`) for production: it handles browser quirks, accessibility, and validation. To understand what it does:

```tsx
function DropZone({ onFiles }: { onFiles: (files: File[]) => void }) {
  const [dragging, setDragging] = useState(false);
  const inputRef = useRef<HTMLInputElement>(null);

  return (
    <div
      onDragOver={(e) => { e.preventDefault(); setDragging(true); }}   // preventDefault is required to allow dropping
      onDragLeave={() => setDragging(false)}
      onDrop={(e) => {
        e.preventDefault();
        setDragging(false);
        onFiles(Array.from(e.dataTransfer.files));
      }}
      style={{ border: dragging ? "2px solid blue" : "2px dashed gray", padding: 24 }}
    >
      <p>Drag files here, or</p>
      <button type="button" onClick={() => inputRef.current?.click()}>Browse…</button>
      <input
        ref={inputRef}
        type="file"
        multiple
        hidden
        onChange={(e) => onFiles(Array.from(e.target.files ?? []))}
      />
    </div>
  );
}
```

Accessibility: **drag-and-drop must have a keyboard-accessible alternative** — the "Browse" button (or a visible, labeled file input) is mandatory, not optional ([`../08-accessibility/02-keyboard-and-focus-management.md`](../08-accessibility/02-keyboard-and-focus-management.md)). Don't hide the only way to pick files behind a mouse-only gesture. Also handle pasted images and drags of non-file content defensively.

---

## 7. Server-side responsibilities (non-negotiable)

Everything above runs in the user's browser and can be bypassed. The server must:

- **Enforce size limits** (and set request body limits at the proxy/framework).
- **Check the real file type** by content, not the extension or client-reported MIME type.
- **Generate its own file names**; never trust `file.name` (path traversal like `../../etc/passwd`, collisions, unsafe characters).
- **Store uploads outside the web root** or in object storage with non-executable settings, and serve them with correct `Content-Type` and `Content-Disposition`.
- **Scan for malware** where the risk warrants.
- **Re-encode images** (strip metadata such as EXIF GPS data, defuse malformed files) when displaying user uploads.
- **Authorize**: only the right users can upload, replace, or read files.
- **Rate-limit** and quota uploads.
- Treat SVG uploads as potentially executable (they can contain scripts); sanitize or disallow.

See [`../19-production/05-security.md`](../19-production/05-security.md).

---

## 8. With React Hook Form

Register the input (it's uncontrolled, which suits RHF):

```tsx
const { register, handleSubmit } = useForm<{ avatar: FileList }>();

<input type="file" accept="image/*" {...register("avatar")} />;

async function onSubmit({ avatar }: { avatar: FileList }) {
  const file = avatar[0];
  if (file) await upload(file);
}
```

For previews, `watch("avatar")` (or `useWatch` in a child) gives you the `FileList`. For richer widgets (dropzone components), wrap them with `Controller` and store `File[]` as the field value ([`02-react-hook-form.md`](./02-react-hook-form.md)).

---

## 9. Accessibility checklist

- A **visible label** for the input; a styled custom button must still be associated with the real input (`label htmlFor` or `ref.click()` with an accessible name).
- **Announce** selection, validation errors, progress, success, and failure (`role="status"` / `role="alert"`).
- **Keyboard access** for choosing, removing, and retrying files.
- Don't use `display: none` on the input if you rely on it for focus; use a visually-hidden technique, or call `click()` from a real button.
- Communicate constraints in text (**"PNG or JPEG, up to 5 MB"**), not just via `accept`.

---

## Common mistakes

- **Trying to control a file input** with `value` — not possible; store the `File` in state.
- **Not clearing the input**, so re-selecting the same file doesn't fire `onChange`.
- **Trusting `accept` or `file.type`** — they're hints; validate on the server.
- **Setting `Content-Type: multipart/form-data` manually** when using `FormData` — breaks the boundary.
- **Expecting `fetch` upload progress** — use XHR or Axios.
- **Forgetting `URL.revokeObjectURL`** — memory leaks with many previews.
- **Uploading everything at once** — limit concurrency; support cancel and retry.
- **Using the client's file name on the server** — security risk.
- **Drag-and-drop without a keyboard alternative** — inaccessible.
- **Routing very large files through your API server** — use presigned direct uploads.
- **Ignoring client-side limits and server limits together** — enforce both.

## Quick summary

- File inputs are always uncontrolled; keep the `File` in state and clear with `ref.value = ""` or a `key`
- `accept` and `file.type` are hints; validate size and type client-side for UX, and re-validate on the server by content
- Preview with `URL.createObjectURL` and revoke on cleanup
- Upload with `FormData` (never set `Content-Type` yourself); use XHR/Axios for progress; model upload states; support cancel and retry
- For large files, use presigned direct-to-storage uploads or chunked uploads
- Drag-and-drop needs a keyboard-accessible alternative; consider `react-dropzone`
- The server enforces limits, type checks, safe naming, storage, and authorization

## Next

You've finished the forms chapter. Continue to **[`../07-styling/README.md`](../07-styling/README.md)** to style your components. For what to do with the data once it's submitted, see [`../12-server-state/05-mutations.md`](../12-server-state/05-mutations.md), and for pre-built form controls, [`../09-ui-components/06-form-controls.md`](../09-ui-components/06-form-controls.md).
