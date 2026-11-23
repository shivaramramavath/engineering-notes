# Forms & Constraint Validation

> Modern browser forms provide declarative user input handling, native Constraint Validation APIs, asynchronous submission pipelines via `FormData`, and secure file processing.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [The DOM](./01_dom.md), [DOM Events](./04_events.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **Standard** | HTML Living Standard / WHATWG FormData / File API / Baseline Widely Available |

---

## Table of Contents

1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

HTML forms are the primary mechanism for collecting, validating, and submitting user input across web applications. Historically, developers relied heavily on complex JavaScript regex validation scripts and manual input serialization to capture form state.

Modern HTML and DOM standards offer an enterprise-grade, built-in validation and serialization infrastructure:
- **Constraint Validation API**: Declarative attributes (`required`, `pattern`, `min`, `max`, `step`, `type`) evaluated natively in C++ before any JavaScript runs.
- **`FormData` Interface**: High-performance multi-part binary data serialization for asynchronous submission via `fetch()`.
- **`form.requestSubmit()`**: Programmatic submission that respects native validation constraints, submitter buttons, and submit event handlers (unlike legacy `form.submit()`).
- **File API**: Direct client-side inspection, preview, and chunking of user-selected files before network transmission.

---

## 2. Core Concepts

### Declarative Constraint Validation vs JavaScript API
HTML inputs declare validation rules directly in markup:
- `required`: Field must not be empty.
- `pattern="[A-Z0-9]{5}"`: Value must match the regular expression.
- `type="email"` / `type="url"`: Validates against standardized RFC grammar.
- `min`, `max`, `step`: Numeric, date, and range bounds.

The JavaScript **Constraint Validation API** exposes programmatic hooks:
- **`input.checkValidity()`**: Returns boolean indicating if input meets all rules.
- **`input.reportValidity()`**: Checks validity and displays the native browser error tooltip if invalid.
- **`input.validity`**: A `ValidityState` object containing detailed boolean failure flags (`valueMissing`, `typeMismatch`, `patternMismatch`, `tooShort`, `customError`).
- **`input.setCustomValidity(message)`**: Injects a custom error message, immediately marking the input as invalid until cleared with `""`.

### `form.submit()` vs `form.requestSubmit()`

| Method | Dispatches `submit` Event? | Runs HTML Constraint Validation? | Respects Submitter Button? |
| :--- | :---: | :---: | :---: |
| **`form.submit()`** | **NO** | **NO** (Submits invalid data directly) | **NO** |
| **`form.requestSubmit()`** | **YES** | **YES** (Aborts if invalid) | **YES** (Includes submitter name/value) |

---

## 3. How It Works Internally

### Form Submission and Validation Flow

```
 User triggers submit (clicks button or presses Enter)
                          │
                          ▼
 [1] Check all form controls against Constraint Validation rules
                          │
                          ▼
     Are all form controls valid?
         /                     \
   NO   /                       \ YES
       v                         v
 [ Display Browser Tooltip ]   [2] Dispatch 'submit' Event (cancelable)
 [ Focus first invalid input ]   Does event listener call e.preventDefault()?
 [ Halt submission ]                   /                     \
                                 YES  /                       \ NO
                                     v                         v
                         [ Custom JS Pipeline ]      [ Standard HTTP Post ]
                         [ e.g., fetch(FormData)]    [ Full Page Reload ]
```

---

## 4. Syntax & API Reference

### Form and Input Properties

```javascript
// Accessing elements via named properties
const form = document.forms['registration-form'];
const emailInput = form.elements['email']; // HTMLInputElement

// Submitting programmatically with validation
form.requestSubmit(); // Standard
// form.requestSubmit(specificSubmitButton);

// Resetting form to initial markup state
form.reset();

// Inspecting detailed validity state
const state = emailInput.validity;
if (state.valueMissing) console.log('Required field is empty');
if (state.typeMismatch) console.log('Not a valid email address');
if (state.patternMismatch) console.log('Regex pattern failed');
if (state.customError) console.log('Custom error was set');

// Custom error injection
emailInput.setCustomValidity('This email domain is not permitted.');
emailInput.reportValidity(); // Displays tooltip
emailInput.setCustomValidity(''); // Resets field to valid
```

### The `FormData` Interface

```javascript
// Construct from existing HTMLFormElement
const formData = new FormData(form);

// Querying and mutating entries
formData.get('username');          // First value matching key
formData.getAll('interests');      // Array of values for multi-select / checkboxes
formData.has('csrf_token');        // Boolean
formData.set('role', 'admin');     // Overwrite or create
formData.append('tags', 'news');   // Append additional value
formData.delete('temporary_field');

// Iteration
for (const [key, value] of formData.entries()) {
  console.log(`${key}:`, value); // value is string or File object!
}
```

---

## 5. Practical Examples

### Example 1: Asynchronous Form Submission with Native Validation
```javascript
const userForm = document.getElementById('user-form');

userForm.addEventListener('submit', async (event) => {
  event.preventDefault(); // Stop native page reload

  // Guard: Double-check validity
  if (!userForm.checkValidity()) {
    userForm.reportValidity();
    return;
  }

  const submitButton = userForm.querySelector('button[type="submit"]');
  submitButton.disabled = true;

  try {
    const formData = new FormData(userForm);

    const response = await fetch('/api/users', {
      method: 'POST',
      body: formData, // Browser sets multipart/form-data boundary automatically!
    });

    if (!response.ok) {
      throw new Error(`Server returned ${response.status}`);
    }

    const result = await response.json();
    console.log('User created:', result.id);
    userForm.reset();
  } catch (error) {
    console.error('Submission failed:', error.message);
  } finally {
    submitButton.disabled = false;
  }
});
```

### Example 2: Real-Time Password Confirmation Validation
```javascript
const passwordInput = document.getElementById('password');
const confirmInput = document.getElementById('confirm-password');

function validatePasswordsMatch() {
  if (confirmInput.value !== passwordInput.value) {
    confirmInput.setCustomValidity('Passwords must match.');
  } else {
    confirmInput.setCustomValidity(''); // Valid!
  }
}

passwordInput.addEventListener('input', validatePasswordsMatch);
confirmInput.addEventListener('input', validatePasswordsMatch);
```

### Example 3: Client-Side Image File Preview with `FileReader`
```javascript
const fileInput = document.getElementById('avatar-upload');
const previewImage = document.getElementById('avatar-preview');

fileInput.addEventListener('change', () => {
  const file = fileInput.files[0];
  if (!file) return;

  // Validate size (max 2MB) and type
  if (!file.type.startsWith('image/')) {
    alert('Please select an image file.');
    fileInput.value = ''; // Reset input
    return;
  }

  if (file.size > 2 * 1024 * 1024) {
    alert('File size cannot exceed 2MB.');
    fileInput.value = '';
    return;
  }

  // Modern object URL preview (preferred over FileReader for memory efficiency)
  const objectUrl = URL.createObjectURL(file);
  previewImage.src = objectUrl;

  previewImage.onload = () => {
    URL.revokeObjectURL(objectUrl); // Revoke once loaded to free memory!
  };
});
```

### Example 4: Converting `FormData` to JSON Object
```javascript
function formToJson(formElement) {
  const formData = new FormData(formElement);
  const data = {};

  for (const [key, value] of formData.entries()) {
    if (key in data) {
      // Convert repeated keys (e.g. checkboxes) to array
      if (!Array.isArray(data[key])) {
        data[key] = [data[key]];
      }
      data[key].push(value);
    } else {
      data[key] = value;
    }
  }

  return JSON.stringify(data);
}
```

### Example 5: Drag-and-Drop File Upload Zone
```javascript
const dropZone = document.getElementById('drop-zone');

dropZone.addEventListener('dragover', (e) => {
  e.preventDefault(); // Required to allow drop!
  dropZone.classList.add('drag-active');
});

dropZone.addEventListener('dragleave', () => {
  dropZone.classList.remove('drag-active');
});

dropZone.addEventListener('drop', (e) => {
  e.preventDefault();
  dropZone.classList.remove('drag-active');

  const files = e.dataTransfer.files;
  if (files.length > 0) {
    uploadFiles(files);
  }
});

async function uploadFiles(fileList) {
  const formData = new FormData();
  for (const file of fileList) {
    formData.append('documents', file, file.name);
  }

  await fetch('/api/upload', {
    method: 'POST',
    body: formData,
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`form.submit()` Skips All Validation**: Calling `form.submit()` via JavaScript submits the form immediately, bypassing HTML5 constraint validation and the `submit` event listener. Always use `form.requestSubmit()`.
2. **Missing `name` Attribute in `FormData`**: Any input element lacking a `name` attribute is completely ignored by `new FormData(form)`.
3. **Manual `Content-Type` Header Breakage in Fetch**: When sending `FormData` via `fetch()`, **never** manually set `headers: { 'Content-Type': 'multipart/form-data' }`! Doing so omits the browser-generated multipart MIME boundary parameter (`boundary=----WebKitFormBoundary...`), causing server-side body parsers to fail.
4. **`input` vs `change` Events**: The `input` event fires on every single keystroke. The `change` event fires only when the field loses focus (`blur`) after its value was modified, or when selecting a new option in a `<select>` tag.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Using `form.submit()`** | `form.submit()` | `form.requestSubmit()` | Bypasses `submit` event listeners and HTML constraint checks. |
| **Manual Multipart Header** | `headers: { 'Content-Type': 'multipart/form-data' }` | Leave headers empty when passing `FormData` | Strips the required boundary parameter, breaking file uploads. |
| **Ignoring Missing `name`** | `<input id="email" type="email">` | `<input id="email" name="email" type="email">` | `FormData` will completely omit inputs that lack `name`. |
| **Forgetting to Clear `setCustomValidity`**| `input.setCustomValidity('')` never called | Clear with `''` once condition is satisfied | Field remains permanently marked invalid and cannot be submitted. |
| **Using `change` for Live Validation** | `input.addEventListener('change', liveCheck)` | Use `input.addEventListener('input', ...)` | `change` does not fire until user leaves field (blur). |
| **Leaking `URL.createObjectURL`** | Creating previews without revoking URL | Revoke via `URL.revokeObjectURL(url)` in `onload` | Retains full file binary blob in memory until page reload. |

---

## 8. Performance Characteristics

- **Native Validation in C++**: Browser native validation attributes (`pattern`, `required`) execute directly in compiled C++ during token validation, running significantly faster than custom JavaScript regex loops.
- **`URL.createObjectURL` vs `FileReader`**: `URL.createObjectURL(file)` creates a lightweight internal pointer string in $O(1)$ time with zero memory duplication. `FileReader.readAsDataURL(file)` decodes the entire file into a massive Base64 string in memory (33% size bloat).

---

## 9. Engine Internals

1. **`ValidityState` Bitmask**: In Blink/Chromium, each form control contains a bitmask flag representing `blink::ValidityState`. Methods like `checkValidity()` perform a single fast bitwise AND operation across the flags.
2. **`FormData` Stream Serialization**: When passed to `fetch()`, Chromium streams `FormData` payloads directly from disk files via native IPC channels to the network process without loading entire multi-gigabyte files into V8 heap memory.

---

## 10. Best Practices & Design Guidelines

1. **Use `form.requestSubmit()` for Programmatic Submissions**: Always respect user validation constraints and submit button identity.
2. **Rely on Native HTML Constraints First**: Leverage `required`, `minlength`, `maxlength`, `type`, and `pattern` before writing custom JS validation logic.
3. **Never Set Explicit Multipart Headers on Fetch**: Let the browser manage `multipart/form-data` boundary formatting automatically.
4. **Clean Up Object URLs**: Always revoke preview URLs using `URL.revokeObjectURL()` once preview images have loaded.

---

## 11. Real-World Case Study

### Resilient Multi-Part Upload Form with Progress Tracking

An enterprise document management portal uploads large CAD drawings and PDF contracts with instant client-side validation, cancellation support, and real-time upload progress.

```javascript
class DocumentUploader {
  #form;
  #progressFill;
  #abortController;

  constructor(formElement, progressFillElement) {
    this.#form = formElement;
    this.#progressFill = progressFillElement;
    this.#init();
  }

  #init() {
    this.#form.addEventListener('submit', (e) => this.#handleSubmit(e));
  }

  async #handleSubmit(event) {
    event.preventDefault();

    if (!this.#form.checkValidity()) {
      this.#form.reportValidity();
      return;
    }

    const formData = new FormData(this.#form);
    const file = formData.get('document');

    if (file && file.size > 50 * 1024 * 1024) {
      alert('File exceeds maximum 50MB limit.');
      return;
    }

    this.#abortController = new AbortController();
    this.#setSubmittingState(true);

    try {
      // Using XMLHttpRequest for native upload progress monitoring
      await this.#uploadWithProgress('/api/documents/upload', formData);
      alert('Document uploaded successfully!');
      this.#form.reset();
    } catch (err) {
      if (err.name === 'AbortError') {
        console.warn('Upload was aborted by user');
      } else {
        alert(`Upload error: ${err.message}`);
      }
    } finally {
      this.#setSubmittingState(false);
    }
  }

  #uploadWithProgress(url, formData) {
    return new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest();
      xhr.open('POST', url);

      xhr.upload.addEventListener('progress', (e) => {
        if (e.lengthComputable) {
          const percent = Math.round((e.loaded / e.total) * 100);
          this.#progressFill.style.width = `${percent}%`;
        }
      });

      xhr.onload = () => {
        if (xhr.status >= 200 && xhr.status < 300) {
          resolve(JSON.parse(xhr.responseText));
        } else {
          reject(new Error(`Server error: ${xhr.status}`));
        }
      };

      xhr.onerror = () => reject(new Error('Network error during upload'));

      this.#abortController.signal.addEventListener('abort', () => {
        xhr.abort();
        reject(new DOMException('Upload aborted', 'AbortError'));
      }, { once: true });

      xhr.send(formData);
    });
  }

  #setSubmittingState(isSubmitting) {
    const btn = this.#form.querySelector('button[type="submit"]');
    btn.disabled = isSubmitting;
    if (!isSubmitting) this.#progressFill.style.width = '0%';
  }

  cancel() {
    this.#abortController?.abort();
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is the difference between <code>form.submit()</code> and <code>form.requestSubmit()</code>?</strong></summary>

`form.submit()` bypasses the HTML5 constraint validation checks, does not fire the `submit` event listener, and does not record which submit button triggered the action. `form.requestSubmit()` executes the full standard submission lifecycle: it runs constraint validation, dispatches the `submit` event (which can be cancelled via `preventDefault()`), and respects the submitter button.
</details>

<details>
<summary><strong>2. Why should you never set the <code>Content-Type</code> header manually when sending <code>FormData</code> via <code>fetch()</code>?</strong></summary>

When `fetch` receives a `FormData` body, the browser automatically configures `Content-Type: multipart/form-data; boundary=----WebKitFormBoundary...`. If you set `Content-Type: multipart/form-data` manually, the required `boundary` parameter is omitted, making it impossible for the server to parse the multi-part body fields.
</details>

<details>
<summary><strong>3. What is the difference between the <code>input</code> and <code>change</code> events on a text input?</strong></summary>

The `input` event fires synchronously on every value modification (each key typed, pasted, or deleted). The `change` event fires only when the field loses focus (`blur`) after its value was modified.
</details>

<details>
<summary><strong>4. How does <code>input.setCustomValidity(message)</code> work?</strong></summary>

It sets a custom error message on the input control, immediately marking the input as invalid (`validity.customError = true`). To restore the input to a valid state, you must explicitly call `input.setCustomValidity('')` with an empty string.
</details>

<details>
<summary><strong>5. What is the advantage of <code>URL.createObjectURL(file)</code> over <code>FileReader.readAsDataURL(file)</code> for image previews?</strong></summary>

`URL.createObjectURL` is instantaneous ($O(1)$) and generates a brief URL reference to the file in native storage without memory duplication. `FileReader.readAsDataURL` asynchronously reads and encodes the entire file into a massive Base64 string in JavaScript heap memory, causing memory bloat for large images.
</details>

<details>
<summary><strong>6. What happens if an input element in a form lacks a <code>name</code> attribute?</strong></summary>

The input's value will be completely omitted from `new FormData(form)` and will not be submitted to the server during standard form submission.
</details>

<details>
<summary><strong>7. How do you serialize multi-select dropdowns or checkboxes using <code>FormData</code>?</strong></summary>

Using `formData.getAll(name)`, which returns an array of all values associated with that key, whereas `formData.get(name)` only returns the first value.
</details>

<details>
<summary><strong>8. What is the purpose of the <code>novalidate</code> attribute on a form?</strong></summary>

It disables the browser's default native validation tooltips and blocks the browser from preventing submission when controls are invalid, allowing developers to manage validation reporting entirely in custom JavaScript.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an Auto-Saving Form Synchronizer
Create a class `AutoSaveForm` that listens to form inputs, debounces changes by 500ms, and serializes modified fields into `localStorage` so user input survives accidental page reloads.

<details>
<summary>View Solution</summary>

```javascript
class AutoSaveForm {
  #form;
  #storageKey;
  #timer = null;

  constructor(formElement, storageKey) {
    this.#form = formElement;
    this.#storageKey = storageKey;
    this.#restore();
    this.#bind();
  }

  #bind() {
    this.#form.addEventListener('input', () => {
      clearTimeout(this.#timer);
      this.#timer = setTimeout(() => this.#save(), 500);
    });

    this.#form.addEventListener('submit', () => {
      localStorage.removeItem(this.#storageKey); // Clear on submit
    });
  }

  #save() {
    const formData = new FormData(this.#form);
    const entries = Object.fromEntries(formData.entries());
    localStorage.setItem(this.#storageKey, JSON.stringify(entries));
    console.log('Form draft auto-saved');
  }

  #restore() {
    const raw = localStorage.getItem(this.#storageKey);
    if (!raw) return;

    try {
      const data = JSON.parse(raw);
      for (const [key, value] of Object.entries(data)) {
        const input = this.#form.elements[key];
        if (input && input.type !== 'file' && input.type !== 'password') {
          input.value = value;
        }
      }
    } catch (e) {
      console.warn('Failed to restore form draft', e);
    }
  }
}
```
</details>

### Challenge 2: Client-Side Credit Card Luhn Algorithm Validator
Implement a custom form validation handler using `input.setCustomValidity()` that validates credit card numbers using the mathematical Luhn checksum algorithm.

<details>
<summary>View Solution</summary>

```javascript
function attachLuhnValidator(cardInputElement) {
  function checkLuhn(numberStr) {
    const sanitized = numberStr.replace(/\D/g, '');
    if (sanitized.length < 13 || sanitized.length > 19) return false;

    let sum = 0;
    let shouldDouble = false;

    for (let i = sanitized.length - 1; i >= 0; i--) {
      let digit = parseInt(sanitized.charAt(i), 10);

      if (shouldDouble) {
        digit *= 2;
        if (digit > 9) digit -= 9;
      }

      sum += digit;
      shouldDouble = !shouldDouble;
    }

    return sum % 10 === 0;
  }

  cardInputElement.addEventListener('input', () => {
    const val = cardInputElement.value;
    if (val.length === 0) {
      cardInputElement.setCustomValidity('');
      return;
    }

    if (!checkLuhn(val)) {
      cardInputElement.setCustomValidity('Invalid credit card number checksum.');
    } else {
      cardInputElement.setCustomValidity('');
    }
  });
}
```
</details>
