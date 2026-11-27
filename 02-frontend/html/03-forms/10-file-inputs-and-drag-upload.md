# File Inputs, File System Access, and Drag-and-Drop Uploads

| Metadata | Details |
| :--- | :--- |
| **Specification** | WHATWG HTML Living Standard (§4.10.5.1.18 File Upload state) / File API / Drag and Drop |
| **Prerequisites** | `03-forms/01-form-basics-and-methods.md`, `03-forms/03-labels-fieldsets-and-legends.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`<input type="file">`, `accept`, `multiple`, drag-and-drop events) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, axe-core 4.9, NVDA 2024.1, VoiceOver macOS 14.4 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Basic Single Document Upload with MIME Filtering](#example-1-basic-single-document-upload-with-mime-filtering)
   - [Example 2: Production Multi-File Image Picker with Preview Grid](#example-2-production-multi-file-image-picker-with-preview-grid)
   - [Example 3: Direct Mobile Camera and Audio Capture](#example-3-direct-mobile-camera-and-audio-capture)
   - [Example 4: Accessible Drag-and-Drop Dropzone with Keyboard Fallback](#example-4-accessible-drag-and-drop-dropzone-with-keyboard-fallback)
   - [Example 5: Edge-Case Handling (Chunking, Cancelation, Oversized File Rejection)](#example-5-edge-case-handling-chunking-cancelation-oversized-file-rejection)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility, SEO & Security Notes](#8-performance-accessibility-seo--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

The `<input type="file">` element provides the native HTML gateway for users to select one or more files from their local storage, camera, or external media devices and stage them for processing or server transmission.

Historically, file uploading was notoriously clunky: browsers imposed un-stylable native file picker buttons, native forms lacked client-side file size or MIME validation, and custom drag-and-drop interfaces often broke keyboard accessibility or corrupted data transfer streams.

In modern web development, file inputs are fundamental to document portals, profile avatar uploaders, rich media editors, and cloud storage systems. Understanding native HTML file capabilities—including the `accept` attribute, `multiple`, the mobile `capture` attribute, the `FileList` / `Blob` API, and accessible drag-and-drop interactions—enables developers to construct resilient, secure, and accessible upload experiences without sacrificing browser security boundaries.

---

## 2. Core Concepts

### The File Upload State
When the `type` attribute of `<input>` is set to `file`, the element switches to the **File Upload state**. Key inherent attributes include:
- **`accept`**: A comma-separated list of allowed unique file type specifiers. Specifiers can be MIME types (e.g., `image/png`, `application/pdf`), wildcard MIME groups (e.g., `image/*`, `audio/*`, `video/*`), or explicit file extensions prefixed with a dot (e.g., `.png`, `.pdf`, `.docx`).
- **`multiple`**: A boolean attribute allowing the user to select more than one file simultaneously using OS selection keys (`Shift` or `Ctrl`/`Cmd`).
- **`capture`**: A mobile-specific directive indicating which device hardware sensor (e.g., `user` for front-facing camera, `environment` for rear camera) should be engaged directly.
- **`enctype="multipart/form-data"`**: Mandatory attribute on the parent `<form>` when submitting files over traditional HTTP POST requests. If omitted, standard `application/x-www-form-urlencoded` encoding transmits only the filename string, completely discarding binary payload data.

### DOM Interface: `HTMLInputElement.files`
The programmatic state of a file input is exposed via the read-only `files` property, which returns a `FileList` object (an array-like collection of `File` objects). Each `File` instance inherits from `Blob` and exposes:
- `name`: String containing the filename without local path information (path stripping is enforced for privacy).
- `size`: Unsigned 64-bit integer specifying the file size in exact bytes.
- `type`: Lowercase ASCII string representing the detected MIME type, or an empty string if unknown.
- `lastModified`: Milliseconds elapsed since the Unix epoch.

---

## 3. How It Works Internally

The browser isolates file system access behind strict sandboxing rules to prevent arbitrary drive reading.

```
+----------------------------------------------------------------------------------+
|                            Browser Sandbox & File API                            |
+----------------------------------------------------------------------------------+
  1. User Interaction (Click / Drag / Enter)
         |
         v
  +-----------------------+     OS File Picker Dialog
  | <input type="file">   | ----------------------------> [ User selects file.pdf ]
  +-----------------------+ <----------------------------
         |
  2. Security Sanitization:
     - Full path (e.g., "C:\Users\Secret\file.pdf") is stripped!
     - Exposed value: "C:\fakepath\file.pdf" (for legacy compat)
     - In-memory FileList created: [ File { name: "file.pdf", size: 409600 } ]
         |
         v
  3. Form Submission (or JS Fetch):
     - Parent Form requires: enctype="multipart/form-data" method="POST"
     - Browser streams binary chunks across boundary delimited multipart payload:
         |
         +--> Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryXYZ
         +--> Payload:
              ------WebKitFormBoundaryXYZ
              Content-Disposition: form-data; name="resume"; filename="file.pdf"
              Content-Type: application/pdf

              [Raw Binary Byte Stream ... ]
              ------WebKitFormBoundaryXYZ--
```

### Security Considerations: `fakepath` and File System Isolation
For privacy and cross-site scripting defense, browsers will never expose genuine host filesystem directories to JavaScript. Reading `inputElement.value` returns `C:\fakepath\<filename>` across all operating systems (even on Linux and macOS). JavaScript code must inspect `inputElement.files[0]` instead of attempting to parse directory strings.

---

## 4. Syntax & Usage

```html
<form action="/api/upload" method="POST" enctype="multipart/form-data">
  <label for="user-resume">Upload Resume (PDF only, max 5MB):</label>
  <input 
    type="file" 
    id="user-resume" 
    name="resume" 
    accept=".pdf,application/pdf"
    required
  />
  <button type="submit">Submit Document</button>
</form>
```

> **Cross-Browser & Baseline Status Box**
>
> - `<input type="file">`, `accept`, `multiple`: **Baseline Widely Available** across all major browsers since early HTML5.
> - `capture` attribute: **Baseline Widely Available** on mobile browsers (iOS Safari 6+, Android Chrome 25+). Desktop browsers gracefully ignore `capture` and open standard OS file pickers.
> - `DataTransfer.items` (Drag and Drop): **Baseline Widely Available**.

---

## 5. Practical Examples

### Example 1: Basic Single Document Upload with MIME Filtering
A clean, accessible single-file input with specific format guidance.

```html
<form action="/documents/upload" method="post" enctype="multipart/form-data" class="upload-form">
  <fieldset>
    <legend>Proof of Address</legend>
    
    <p id="upload-requirements">
      Please provide a recent utility bill or bank statement in PDF or PNG format. 
      Maximum file size is 10 MB.
    </p>

    <div class="field-group">
      <label for="address-document">Document File</label>
      <input 
        type="file" 
        id="address-document" 
        name="address_document" 
        accept=".pdf,.png,application/pdf,image/png"
        aria-describedby="upload-requirements file-error"
        required
      />
      <p id="file-error" class="error-msg" hidden></p>
    </div>

    <button type="submit">Upload and Verify</button>
  </fieldset>
</form>
```

---

### Example 2: Production Multi-File Image Picker with Preview Grid
Allows selecting multiple images, validates total file count and sizes via JavaScript, and renders miniature visual previews.

```html
<form id="gallery-form" action="/api/gallery" method="post" enctype="multipart/form-data">
  <label for="gallery-photos" class="picker-label">
    Select Photos (JPEG, WebP, AVIF — max 5 photos)
  </label>
  <input 
    type="file" 
    id="gallery-photos" 
    name="photos" 
    accept="image/jpeg,image/webp,image/avif" 
    multiple
    aria-describedby="gallery-status"
  />

  <!-- Status announcements for screen readers -->
  <div id="gallery-status" role="status" aria-live="polite"></div>

  <!-- Preview container -->
  <ul id="preview-list" class="preview-grid" aria-label="Selected image previews"></ul>

  <button type="submit" id="submit-btn" disabled>Upload Gallery</button>
</form>

<script>
  const fileInput = document.getElementById('gallery-photos');
  const previewList = document.getElementById('preview-list');
  const statusBox = document.getElementById('gallery-status');
  const submitBtn = document.getElementById('submit-btn');
  const MAX_FILES = 5;
  const MAX_BYTES = 5 * 1024 * 1024; // 5 MB per file

  fileInput.addEventListener('change', () => {
    previewList.innerHTML = '';
    statusBox.textContent = '';
    const files = Array.from(fileInput.files);

    if (files.length === 0) {
      submitBtn.disabled = true;
      return;
    }

    if (files.length > MAX_FILES) {
      statusBox.textContent = `Error: You selected ${files.length} images. Maximum allowed is ${MAX_FILES}.`;
      fileInput.setCustomValidity(`Select up to ${MAX_FILES} photos.`);
      submitBtn.disabled = true;
      return;
    }

    fileInput.setCustomValidity('');
    let totalValid = 0;

    files.forEach((file, index) => {
      if (file.size > MAX_BYTES) {
        statusBox.textContent = `Error: "${file.name}" exceeds 5 MB.`;
        return;
      }

      const li = document.createElement('li');
      li.className = 'preview-item';

      const img = document.createElement('img');
      img.src = URL.createObjectURL(file);
      img.alt = `Preview ${index + 1}: ${file.name}`;
      img.onload = () => URL.revokeObjectURL(img.src);

      const caption = document.createElement('span');
      caption.textContent = `${file.name} (${Math.round(file.size / 1024)} KB)`;

      li.append(img, caption);
      previewList.appendChild(li);
      totalValid++;
    });

    if (totalValid === files.length) {
      statusBox.textContent = `${files.length} file(s) ready for upload.`;
      submitBtn.disabled = false;
    }
  });
</script>
```

---

### Example 3: Direct Mobile Camera and Audio Capture
Enforces direct hardware activation on mobile devices.

```html
<fieldset>
  <legend>Identity Verification Biometrics</legend>

  <!-- Rear environmental camera for ID card photo -->
  <div class="field-row">
    <label for="id-card-capture">Take photo of ID Card (Rear camera):</label>
    <input 
      type="file" 
      id="id-card-capture" 
      name="id_card" 
      accept="image/*" 
      capture="environment"
      required
    />
  </div>

  <!-- Front camera for selfie confirmation -->
  <div class="field-row">
    <label for="selfie-capture">Take Selfie (Front camera):</label>
    <input 
      type="file" 
      id="selfie-capture" 
      name="selfie" 
      accept="image/*" 
      capture="user"
      required
    />
  </div>

  <!-- Voice verification note -->
  <div class="field-row">
    <label for="voice-note">Record Audio Declaration:</label>
    <input 
      type="file" 
      id="voice-note" 
      name="voice_signature" 
      accept="audio/*" 
      capture
      required
    />
  </div>
</fieldset>
```

---

### Example 4: Accessible Drag-and-Drop Dropzone with Keyboard Fallback
A drag-and-drop region that keeps the native `<input type="file">` reachable via keyboard `Tab` and `Space`/`Enter`.

```html
<div class="upload-dropzone" id="dropzone" tabindex="0" role="region" aria-label="File upload zone">
  <!-- Visually hidden native input overlaid or programmatically triggered -->
  <input 
    type="file" 
    id="native-file-picker" 
    class="sr-only" 
    accept=".csv,.xlsx" 
    aria-describedby="dropzone-instructions"
  />

  <div class="dropzone-content">
    <svg aria-hidden="true" width="48" height="48" viewBox="0 0 24 24">
      <path d="M19.35 10.04C18.67 6.59 15.64 4 12 4 9.11 4 6.6 5.64 5.35 8.04 2.34 8.36 0 10.91 0 14c0 3.31 2.69 6 6 6h13c2.76 0 5-2.24 5-5 0-2.64-2.05-4.78-4.65-4.96zM14 13v4h-4v-4H7l5-5 5 5h-3z"/>
    </svg>
    <p id="dropzone-instructions">
      <label for="native-file-picker" class="browse-link">Choose a spreadsheet</label>
      or drag and drop it here.
    </p>
    <p class="dropzone-meta">Accepted formats: .csv, .xlsx (up to 20 MB)</p>
  </div>
  
  <div id="dropzone-feedback" role="status" aria-live="polite"></div>
</div>

<style>
  .sr-only {
    position: absolute;
    width: 1px;
    height: 1px;
    padding: 0;
    margin: -1px;
    overflow: hidden;
    clip: rect(0, 0, 0, 0);
    border: 0;
  }
  .upload-dropzone {
    border: 2px dashed #005a9c;
    border-radius: 8px;
    padding: 2rem;
    text-align: center;
    cursor: pointer;
  }
  .upload-dropzone:focus-visible {
    outline: 3px solid #ffbf47;
    outline-offset: 2px;
  }
  .upload-dropzone.drag-active {
    background-color: #f0f7ff;
    border-color: #003366;
  }
  .browse-link {
    color: #005a9c;
    text-decoration: underline;
    font-weight: bold;
    cursor: pointer;
  }
</style>

<script>
  const dropzone = document.getElementById('dropzone');
  const fileInput = document.getElementById('native-file-picker');
  const feedback = document.getElementById('dropzone-feedback');

  ['dragenter', 'dragover'].forEach(eventName => {
    dropzone.addEventListener(eventName, (e) => {
      e.preventDefault();
      e.stopPropagation();
      dropzone.classList.add('drag-active');
    });
  });

  ['dragleave', 'drop'].forEach(eventName => {
    dropzone.addEventListener(eventName, (e) => {
      e.preventDefault();
      e.stopPropagation();
      dropzone.classList.remove('drag-active');
    });
  });

  dropzone.addEventListener('drop', (e) => {
    const droppedFiles = e.dataTransfer.files;
    if (droppedFiles.length > 0) {
      fileInput.files = droppedFiles; // Assign to native file input
      feedback.textContent = `Selected: ${droppedFiles[0].name}`;
    }
  });

  fileInput.addEventListener('change', () => {
    if (fileInput.files.length > 0) {
      feedback.textContent = `Selected: ${fileInput.files[0].name}`;
    }
  });

  // Enable keyboard trigger (Space or Enter activates picker)
  dropzone.addEventListener('keydown', (e) => {
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault();
      fileInput.click();
    }
  });
</script>
```

---

### Example 5: Edge-Case Handling (Chunking, Cancelation, Oversized File Rejection)
Handling client-side pre-validation, AbortController cancelation, and progress indication.

```html
<div class="uploader-container">
  <h2>Report Dispatcher</h2>
  
  <input type="file" id="large-file-input" accept=".zip,.tar.gz" />
  <div class="progress-wrap" hidden id="progress-wrap">
    <label for="upload-progress">Uploading file:</label>
    <progress id="upload-progress" max="100" value="0">0%</progress>
    <span id="progress-percent">0%</span>
    <button type="button" id="cancel-upload-btn">Cancel Upload</button>
  </div>
  
  <div id="upload-status" role="status" aria-live="assertive"></div>
</div>

<script>
  const input = document.getElementById('large-file-input');
  const progressWrap = document.getElementById('progress-wrap');
  const progressBar = document.getElementById('upload-progress');
  const progressPercent = document.getElementById('progress-percent');
  const cancelBtn = document.getElementById('cancel-upload-btn');
  const statusRegion = document.getElementById('upload-status');
  
  let abortController = null;
  const MAX_LIMIT = 50 * 1024 * 1024; // 50MB

  input.addEventListener('change', async () => {
    const file = input.files[0];
    if (!file) return;

    if (file.size > MAX_LIMIT) {
      statusRegion.textContent = `File rejected: ${file.name} is ${Math.round(file.size / 1024 / 1024)}MB. Limit is 50MB.`;
      input.value = ''; // Reset input
      return;
    }

    // Begin simulated or chunked upload via XHR or Fetch
    abortController = new AbortController();
    progressWrap.hidden = false;
    statusRegion.textContent = `Starting upload for ${file.name}...`;

    const formData = new FormData();
    formData.append('archive', file);

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/upload-archive');

    xhr.upload.onprogress = (event) => {
      if (event.lengthComputable) {
        const percent = Math.round((event.loaded / event.total) * 100);
        progressBar.value = percent;
        progressBar.textContent = `${percent}%`;
        progressPercent.textContent = `${percent}%`;
      }
    };

    xhr.onload = () => {
      if (xhr.status >= 200 && xhr.status < 300) {
        statusRegion.textContent = `Upload successful: ${file.name}`;
      } else {
        statusRegion.textContent = `Upload failed with status ${xhr.status}.`;
      }
      progressWrap.hidden = true;
    };

    xhr.onerror = () => {
      statusRegion.textContent = 'Network error during upload.';
      progressWrap.hidden = true;
    };

    xhr.onabort = () => {
      statusRegion.textContent = 'Upload canceled by user.';
      progressWrap.hidden = true;
    };

    cancelBtn.onclick = () => {
      xhr.abort();
      input.value = '';
    };

    xhr.send(formData);
  });
</script>
```

---

## 6. Best Practices

- [ ] **Always declare `enctype="multipart/form-data"` on forms submitting files natively**: Omitting this causes HTTP servers to receive the filename string only instead of binary data.
- [ ] **Combine extensions and MIME types in `accept`**: Operating systems like Windows determine MIME types by registry extension mappings, whereas macOS/Linux often inspect UTIs or content headers. Specifying `accept=".png,image/png"` ensures universal dialog filtering.
- [ ] **Never trust client-side MIME types or extensions for security**: Attackers can easily rename `malware.exe` to `invoice.pdf` or manipulate HTTP payload headers. Always validate file signatures (magic bytes) and scan files on the server backend.
- [ ] **Provide an accessible keyboard trigger when hiding native file inputs**: If wrapping `<input type="file">` inside a custom styled element or drag zone, retain `<label for="...">` associations or proxy keyboard events (`Enter`/`Space`) to `.click()`.
- [ ] **Release object URLs promptly**: Always call `URL.revokeObjectURL(url)` inside an `onload` callback or when replacing previews to prevent serious client memory leaks.
- [ ] **Announce upload status changes via `aria-live`**: File progress, completion, and error states must be exposed dynamically to assistive technology users using `role="status"` or `aria-live="polite"`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Forgetting `enctype="multipart/form-data"` on `<form>` | **Critical** | Server receives `key="filename.png"` string; no binary bytes are transmitted. | Add `enctype="multipart/form-data"` and `method="POST"` to `<form>`. |
| Hiding `<input type="file">` using `display: none` without a proper `<label>` or keyboard handler | **Critical** | Screen reader and keyboard users cannot trigger file selection. | Use an accessible `.sr-only` class or associate with a properly marked `<label for="id">`. |
| Relying exclusively on `accept` for security verification | **Critical** | Users can bypass client-side file dialog restrictions; security risk. | Validate magic bytes (file signatures) securely on the receiving server. |
| Reading `input.value` to determine full disk path | **Moderate** | Returns `C:\fakepath\file.ext` across all operating systems due to browser sandboxing. | Inspect `input.files[0].name` or `input.files[0].size` via JavaScript. |
| Forgetting to revoke `URL.createObjectURL` | **Moderate** | Browser retains raw binary image blobs in RAM for document lifetime; memory leaks. | Call `URL.revokeObjectURL(img.src)` immediately after the image has rendered. |
| Omitting file size limits before transmission | **High** | Browser attempts to upload multi-gigabyte files, freezing client thread or timing out. | Validate `file.size` against reasonable thresholds before initializing fetch/XHR. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Guideline 2.1.1 Keyboard**: Ensure custom drag-and-drop zones allow full keyboard file selection. Clicking a dropzone via keyboard `Enter` or `Space` must open the native file dialog.
- **Guideline 4.1.3 Status Messages**: Use `role="status"` or `aria-live="polite"` to announce the number of files selected, progress percentage, or validation errors.
- **Guideline 3.3.2 Labels or Instructions**: Clearly communicate allowed file formats, file count limits, and maximum file size upfront.

### Security
- **Magic Bytes Validation**: File extensions can be falsified. Validate file headers (e.g., `89 50 4E 47` for PNG, `25 50 44 46` for PDF) on backend servers.
- **File System Traversal Defenses**: Never write uploaded files to disk using the un-sanitized client-provided `file.name` to avoid directory traversal attacks (`../../etc/cron`). Generate random UUIDs for stored files.
- **Virus and Malware Scanning**: Quarantine uploaded files prior to serving them to other end users. Serve user files with `Content-Disposition: attachment` or from isolated, cookieless CDNs.

---

## 9. Comparison / When to Use What

| Technique | Baseline Status | Strengths | Limitations | When to Use |
| :--- | :--- | :--- | :--- | :--- |
| **Native `<input type="file">`** | Widely Available | Full OS integration, zero JS overhead, works without scripts. | Inconsistent styling across browsers. | Base requirement for all file uploading needs. |
| **Drag & Drop API** | Widely Available | Intuitive desktop desktop-to-web drag interaction. | Needs JavaScript, requires keyboard alternative. | Enhancing file pickers on desktop applications. |
| **File System Access API (`showOpenFilePicker`)** | Limited (Chromium-only) | Reads and writes directly to local user directories without download triggers. | Non-standard across Safari and Firefox; requires explicit user permission prompts. | Specialized web-based IDEs, CAD tools, or offline document editors. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does accessing the <code>value</code> property of a file input return <code>C:\fakepath\...</code> in modern browsers?</strong></summary>

Browsers intentionally mask real local file system paths to protect user privacy and prevent fingerprinting. In early web browsers, websites could glean sensitive username and directory structures (e.g., `C:\Users\JohnDoe\SecretProjects\resume.pdf`). The WHATWG standard mandates prepending `C:\fakepath\` to the bare filename for all platforms when querying the `.value` property for backward compatibility, while keeping the genuine filesystem path entirely hidden from client scripts.
</details>

<details>
<summary><strong>2. What happens if a user submits a form containing a file input when the form specifies <code>enctype="application/x-www-form-urlencoded"</code>?</strong></summary>

The browser URL-encodes the form fields. For the file input, it serializes only the string filename (e.g., `resume=document.pdf`). Zero binary file contents or MIME payload data are transmitted to the server. To upload the binary file contents, the form must explicitly set `enctype="multipart/form-data"` and `method="POST"`.
</details>

<details>
<summary><strong>3. Can client-side <code>accept="image/png"</code> guarantee that the user only uploads a valid PNG image?</strong></summary>

No. The `accept` attribute merely acts as a client-side filter hint for the operating system's native file selection picker. Users can easily toggle the dialog to "All Files (*.*)" and choose any arbitrary executable or script, or use automated HTTP tools (such as `curl`) to send non-PNG payloads. Strict server-side validation of magic bytes, image decoding, and antivirus scanning is mandatory.
</details>

<details>
<summary><strong>4. How does the <code>capture</code> attribute behave across mobile versus desktop operating systems?</strong></summary>

The `capture` attribute is designed specifically for mobile devices with embedded cameras and microphones. Setting `capture="environment"` directly engages the rear-facing camera, while `capture="user"` activates the front-facing selfie camera. On desktop operating systems without direct media picker bindings, browsers gracefully disregard the attribute and open the standard OS file explorer.
</details>

<details>
<summary><strong>5. How do you prevent memory leaks when creating thumbnail previews using <code>URL.createObjectURL()</code>?</strong></summary>

Each call to `URL.createObjectURL(blob)` stores an internal reference to the blob in browser memory for the lifetime of the document until explicitly released. If a user uploads dozens of images, this consumes significant RAM. You prevent memory leaks by calling `URL.revokeObjectURL(url)` inside the image element's `onload` callback (once rendering is complete) or immediately when an image is discarded.
</details>

<details>
<summary><strong>6. What is the difference between a <code>FileList</code>, a <code>File</code>, and a <code>Blob</code> in JavaScript?</strong></summary>

- `Blob` represents raw, immutable binary data with a byte `size` and MIME `type`.
- `File` inherits from `Blob`, adding file system metadata such as filename (`name`) and timestamp (`lastModified`).
- `FileList` is an array-like iterable collection of `File` objects returned by `HTMLInputElement.files` and `DataTransfer.files`.
</details>

<details>
<summary><strong>7. How can you make a custom visual drag-and-drop dropzone fully accessible to screen reader and keyboard users?</strong></summary>

You retain a real `<input type="file">` inside the DOM, associated with a `<label for="...">`. If the input is visually hidden, use a clipping utility class (`.sr-only`) rather than `display: none` so it remains focusable. Alternatively, ensure the dropzone container itself has `tabindex="0"`, a descriptive `role="region"`, explicit `aria-label`, instructions, and keyboard event handlers that simulate a click on the file input when `Enter` or `Space` is pressed.
</details>

<details>
<summary><strong>8. Can JavaScript programmatically write or set arbitrary file paths into <code>input.files</code>?</strong></summary>

No. For browser sandbox security, JavaScript cannot synthesize arbitrary local disk paths into `input.files`. However, modern browsers allow assigning a user-transferred `FileList` or programmatically constructing a `DataTransfer` object and setting `input.files = dataTransfer.files`, provided the underlying `File`/`Blob` objects were created in-memory or obtained legitimately via Drag-and-Drop events.
</details>

---

## 11. Practice Exercises

### Exercise 1: Accessible Resume File Picker (Easy)
- **Task**: Create a form containing an accessible file input that accepts only `.pdf` and `.docx` files up to 5 MB. Include explicit guidance text linked via `aria-describedby`.
- **Requirements**:
  1. Use `<form method="POST" enctype="multipart/form-data">`.
  2. Implement `accept=".pdf,.docx,application/pdf,application/vnd.openxmlformats-officedocument.wordprocessingml.document"`.
  3. Include a visible instruction block linked to the input with `aria-describedby`.

<details>
<summary>View Solution</summary>

```html
<form action="/apply" method="post" enctype="multipart/form-data">
  <div class="form-group">
    <label for="applicant-resume">Submit Resume</label>
    <p id="resume-instructions">
      Upload your resume as a PDF or Microsoft Word (.docx) document. Maximum size is 5 MB.
    </p>
    <input 
      type="file" 
      id="applicant-resume" 
      name="resume" 
      accept=".pdf,.docx,application/pdf,application/vnd.openxmlformats-officedocument.wordprocessingml.document"
      aria-describedby="resume-instructions"
      required
    />
  </div>
  <button type="submit">Send Application</button>
</form>
```
</details>

---

### Exercise 2: Client-Side File Size Validator (Medium)
- **Task**: Write HTML and JavaScript that intercepts the `change` event on a file input. If the user selects a file larger than 2 MB (2,097,152 bytes), display an error message, mark the input as invalid using `setCustomValidity`, and disable the submit button.
- **Requirements**:
  1. File input with `id="avatar-input"`.
  2. Submit button disabled when invalid.
  3. Dynamic error message linked to an accessible live region.

<details>
<summary>View Solution</summary>

```html
<form id="avatar-form" action="/profile/avatar" method="post" enctype="multipart/form-data">
  <label for="avatar-input">Profile Avatar (Max 2MB):</label>
  <input type="file" id="avatar-input" name="avatar" accept="image/png,image/jpeg" />

  <div id="avatar-error" role="alert" aria-live="assertive" class="error-msg"></div>

  <button type="submit" id="save-avatar-btn">Save Avatar</button>
</form>

<script>
  const avatarInput = document.getElementById('avatar-input');
  const errorMsg = document.getElementById('avatar-error');
  const saveBtn = document.getElementById('save-avatar-btn');
  const MAX_SIZE = 2 * 1024 * 1024; // 2MB

  avatarInput.addEventListener('change', () => {
    const file = avatarInput.files[0];
    if (!file) {
      avatarInput.setCustomValidity('');
      errorMsg.textContent = '';
      saveBtn.disabled = false;
      return;
    }

    if (file.size > MAX_SIZE) {
      const actualSizeMB = (file.size / (1024 * 1024)).toFixed(2);
      const message = `The selected file is ${actualSizeMB} MB. Maximum allowed size is 2 MB.`;
      avatarInput.setCustomValidity(message);
      errorMsg.textContent = message;
      saveBtn.disabled = true;
    } else {
      avatarInput.setCustomValidity('');
      errorMsg.textContent = '';
      saveBtn.disabled = false;
    }
  });
</script>
```
</details>

---

### Exercise 3: Full-Featured Multi-Image Drag-and-Drop Uploader (Hard)
- **Task**: Build an accessible, production-ready drag-and-drop file upload zone.
- **Requirements**:
  1. Accessible via mouse, touch, drag-and-drop, and keyboard (`Tab` + `Space`/`Enter`).
  2. Accepts multiple image files (`image/*`).
  3. Displays an accessible list of selected files with filename, formatted size, and an individual "Remove" button for each file using `DataTransfer`.
  4. Real-time screen reader announcements for added or removed files.

<details>
<summary>View Solution</summary>

```html
<div class="uploader-widget">
  <div 
    id="dropzone" 
    class="dropzone-box" 
    tabindex="0" 
    role="region" 
    aria-label="Image uploader drop zone"
    aria-describedby="zone-hint"
  >
    <p id="zone-hint">
      Drag and drop images here, or press Enter/Space to select files from your computer.
    </p>
    <input 
      type="file" 
      id="file-input" 
      class="sr-only" 
      accept="image/*" 
      multiple 
      tabindex="-1"
    />
  </div>

  <div id="live-announcer" role="status" aria-live="polite" class="sr-only"></div>

  <h3>Selected Files</h3>
  <ul id="file-list" class="file-list" aria-label="Selected file list"></ul>
</div>

<style>
  .sr-only {
    position: absolute;
    width: 1px;
    height: 1px;
    padding: 0;
    margin: -1px;
    overflow: hidden;
    clip: rect(0, 0, 0, 0);
    border: 0;
  }
  .dropzone-box {
    border: 2px dashed #0265dc;
    border-radius: 8px;
    padding: 2.5rem;
    text-align: center;
    background-color: #f7faff;
    cursor: pointer;
  }
  .dropzone-box:focus {
    outline: 3px solid #ffbf47;
  }
  .dropzone-box.dragover {
    background-color: #e3effd;
    border-color: #003e8a;
  }
  .file-list {
    list-style: none;
    padding: 0;
  }
  .file-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 0.5rem;
    border-bottom: 1px solid #ddd;
  }
</style>

<script>
  const dropzone = document.getElementById('dropzone');
  const fileInput = document.getElementById('file-input');
  const fileList = document.getElementById('file-list');
  const announcer = document.getElementById('live-announcer');

  let stagedFiles = [];

  function updateFileList() {
    fileList.innerHTML = '';
    const dt = new DataTransfer();

    stagedFiles.forEach((file, index) => {
      dt.items.add(file);
      const li = document.createElement('li');
      li.className = 'file-item';

      const label = document.createElement('span');
      label.textContent = `${file.name} (${(file.size / 1024).toFixed(1)} KB)`;

      const removeBtn = document.createElement('button');
      removeBtn.type = 'button';
      removeBtn.textContent = 'Remove';
      removeBtn.setAttribute('aria-label', `Remove ${file.name}`);
      removeBtn.onclick = () => {
        stagedFiles.splice(index, 1);
        announcer.textContent = `Removed ${file.name}`;
        updateFileList();
      };

      li.append(label, removeBtn);
      fileList.appendChild(li);
    });

    fileInput.files = dt.files;
  }

  function handleFiles(files) {
    const validImages = Array.from(files).filter(f => f.type.startsWith('image/'));
    stagedFiles = stagedFiles.concat(validImages);
    announcer.textContent = `${validImages.length} images added. Total files: ${stagedFiles.length}`;
    updateFileList();
  }

  dropzone.addEventListener('dragover', (e) => {
    e.preventDefault();
    dropzone.classList.add('dragover');
  });

  dropzone.addEventListener('dragleave', () => {
    dropzone.classList.remove('dragover');
  });

  dropzone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropzone.classList.remove('dragover');
    handleFiles(e.dataTransfer.files);
  });

  dropzone.addEventListener('click', () => fileInput.click());

  dropzone.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      fileInput.click();
    }
  });

  fileInput.addEventListener('change', () => {
    handleFiles(fileInput.files);
  });
</script>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                 HTML FILE INPUT CHEAT SHEET                                     |
+-------------------------------------------------------------------------------------------------+
| Attribute    | Value / Purpose                         | Example                                |
+-------------------------------------------------------------------------------------------------+
| type         | "file"                                  | <input type="file">                    |
| accept       | Comma-separated MIME / extensions       | accept=".pdf,.png,image/png"           |
| multiple     | Boolean for multi-selection             | <input type="file" multiple>           |
| capture      | "user" (front camera), "environment"    | <input type="file" capture="user">     |
| form enctype | MUST be "multipart/form-data"           | <form enctype="multipart/form-data">   |
+-------------------------------------------------------------------------------------------------+
| JS Properties                                                                                   |
| - input.files: FileList containing File objects                                                 |
| - file.name: Stripped filename (no host directories)                                            |
| - file.size: File size in bytes                                                                 |
| - file.type: Detected MIME type                                                                 |
| - URL.createObjectURL(file) -> Preview blob link                                                |
| - URL.revokeObjectURL(url) -> Garbage collect memory blob                                       |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [WHATWG HTML Living Standard: The File Upload state](https://html.spec.whatwg.org/multipage/input.html#file-upload-state-(type=file))
- [MDN Web Docs: `<input type="file">`](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/file)
- [W3C File API Specification](https://www.w3.org/TR/FileAPI/)
- [W3C HTML Media Capture Specification](https://www.w3.org/TR/html-media-capture/)
- [MDN Web Docs: Using files from web applications](https://developer.mozilla.org/en-US/docs/Web/API/File_API/Using_files_from_web_applications)

---

[← Previous: Autocomplete Tokens & Autofill](09-autocomplete-tokens-and-autofill.md) | [Section Index](README.md) | [Next: Form Submission & FormData →](11-form-submission-and-formdata.md)
