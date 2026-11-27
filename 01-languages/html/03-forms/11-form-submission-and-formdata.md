# Form Submission Lifecycle and the FormData API

| Metadata | Details |
| :--- | :--- |
| **Specification** | WHATWG HTML Living Standard (§4.10.21 Form submission) / XMLHttpRequest Level 2 |
| **Prerequisites** | `03-forms/01-form-basics-and-methods.md`, `03-forms/07-built-in-constraint-validation.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`form.requestSubmit()`, `FormData`, `formaction`, `formmethod`) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, axe-core 4.9, NVDA 2024.1, VoiceOver macOS 14.4 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Traditional Multi-Button Form Submission with Attribute Overrides](#example-1-traditional-multi-button-form-submission-with-attribute-overrides)
   - [Example 2: Intercepted AJAX Submission via `FormData` and `fetch()`](#example-2-intercepted-ajax-submission-via-formdata-and-fetch)
   - [Example 3: Safe Programmatic Submission with `requestSubmit()`](#example-3-safe-programmatic-submission-with-requestsubmit)
   - [Example 4: JSON Payload Transformation from `FormData`](#example-4-json-payload-transformation-from-formdata)
   - [Example 5: File Progress Tracking and Upload Abort](#example-5-file-progress-tracking-and-upload-abort)
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

Form submission is the mechanism through which user-entered data is gathered, serialized, validated, and dispatched to a server or client-side processing script. 

Historically, web forms operated exclusively via synchronous HTTP POST or GET roundtrips, causing full-page reloads and resetting browser scroll positions. With modern single-page applications and reactive frameworks, developers frequently intercept form submissions using JavaScript. However, bypassing native submission behavior often leads to severe regressions: broken accessibility, loss of constraint validation, failed password-manager autofill captures, and non-functional keyboard `Enter` handlers.

Mastering the native submission lifecycle—along with button override attributes (`formaction`, `formmethod`, `formnovalidate`), modern `form.requestSubmit()`, and the `FormData` API—allows engineers to build high-performance, asynchronous web interfaces that retain 100% of native HTML's resilience, accessibility, and validation infrastructure.

---

## 2. Core Concepts

### The Submission Algorithm Lifecycle
When a form is submitted (whether by clicking `<button type="submit">`, pressing `Enter` in a text input, or calling `form.requestSubmit()`), the browser initiates an atomic multi-step pipeline:
1. **Identify Submitter**: The browser identifies the specific submit button triggering the action.
2. **Constraint Validation**: The browser validates all submittable elements unless the form or button specifies `novalidate` / `formnovalidate`. If any element fails, submission halts, an `invalid` event fires, and the browser displays native validation UI.
3. **Dispatch `submit` Event**: A cancelable `submit` event fires on the `<form>` element. JavaScript can intercept this using `event.preventDefault()`.
4. **Data Construction (`formdata` event)**: The browser constructs the entry list of submittable elements (only elements with a `name` attribute that are not `disabled`).
5. **Encoding & Transmission**: The payload is serialized according to `enctype` and transmitted via HTTP.

### Button Override Attributes
Child buttons inside a form can dynamically override form-level configuration attributes when clicked:
- **`formaction`**: Overrides the parent form's `action` URL.
- **`formmethod`**: Overrides `method` (e.g., switches POST to GET or dialog).
- **`formenctype`**: Overrides `enctype` (e.g., switches to `multipart/form-data`).
- **`formnovalidate`**: Bypasses constraint validation (crucial for "Save Draft" or "Cancel" actions).
- **`formtarget`**: Overrides `target` (e.g., opens results in `_blank`).

### `form.requestSubmit()` vs `form.submit()`
- `form.submit()`: **Bypasses everything.** Does NOT run constraint validation, does NOT fire the `submit` event, does NOT identify a submitter button, and cannot be intercepted by `event.preventDefault()`.
- `form.requestSubmit(submitter)`: **Simulates a real user submission.** Runs constraint validation, fires the `submit` event, allows cancellation, and tracks button overrides.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                              Native Form Submission Engine                              |
+-----------------------------------------------------------------------------------------+
  User clicks Submit / presses Enter / calls form.requestSubmit()
                              |
                              v
                [ Is formnovalidate active? ]
                   /                     \
                 YES                      NO
                 /                         \
                |                 [ Run Constraint Validation ]
                |                   /                     \
                |                PASSED                  FAILED
                |                  /                        \
                |                 /              [ Halt! Fire 'invalid' on elements ]
                |                /               [ Scroll & focus first invalid input ]
                v               v
    +-------------------------------------------------------+
    | Fire cancelable 'submit' event on <form>               |
    +-------------------------------------------------------+
                |
       [ event.defaultPrevented? ]
          /                 \
        YES                  NO (Standard HTTP Navigation)
        /                     \
  +---------------+   +---------------------------------------------------+
  | JS handles it |   | Construct Entry List (name + value pairs)         |
  | (e.g. Fetch)  |   | Fire 'formdata' event for programmatic mutations  |
  +---------------+   | Serialize payload via selected enctype:           |
                      |   - application/x-www-form-urlencoded            |
                      |   - multipart/form-data                           |
                      |   - text/plain                                    |
                      | Dispatch HTTP Request & Navigate Window           |
                      +---------------------------------------------------+
```

---

## 4. Syntax & Usage

```html
<form action="/api/account" method="POST" enctype="multipart/form-data">
  <label for="username">Username:</label>
  <input type="text" id="username" name="username" required />

  <!-- Standard submission -->
  <button type="submit">Save Changes</button>

  <!-- Button overrides: saves draft without triggering constraint validation -->
  <button 
    type="submit" 
    formaction="/api/drafts" 
    formnovalidate
  >
    Save as Draft
  </button>
</form>
```

> **Cross-Browser & Baseline Status Box**
>
> - `form.requestSubmit()`: **Baseline Widely Available** (Chrome 76+, Firefox 75+, Safari 16+, Edge 79+).
> - `FormData`: **Baseline Widely Available** across all desktop and mobile browsers.
> - `formaction`, `formmethod`, `formnovalidate`: **Baseline Widely Available** since early HTML5.

---

## 5. Practical Examples

### Example 1: Traditional Multi-Button Form Submission with Attribute Overrides
Demonstrating how a single HTML form cleanly routes to multiple server endpoints without JavaScript.

```html
<form action="/invoices/send" method="post" class="invoice-form">
  <fieldset>
    <legend>Invoice #4092</legend>

    <div class="form-row">
      <label for="client-name">Client Name:</label>
      <input type="text" id="client-name" name="client_name" required />
    </div>

    <div class="form-row">
      <label for="amount-due">Amount Due ($):</label>
      <input type="number" id="amount-due" name="amount" min="1" step="0.01" required />
    </div>

    <div class="button-bar">
      <!-- Standard submit: Validates and posts to /invoices/send -->
      <button type="submit" name="action" value="send_invoice">
        Send to Client
      </button>

      <!-- Override: Bypasses validation, posts to /invoices/draft -->
      <button 
        type="submit" 
        formaction="/invoices/draft" 
        formnovalidate
        name="action" 
        value="save_draft"
      >
        Save as Draft
      </button>

      <!-- Override: Opens printable PDF preview in a new browser tab -->
      <button 
        type="submit" 
        formaction="/invoices/preview" 
        formmethod="get"
        formtarget="_blank"
        name="action" 
        value="preview"
      >
        Preview PDF
      </button>
    </div>
  </fieldset>
</form>
```

---

### Example 2: Intercepted AJAX Submission via `FormData` and `fetch()`
The gold standard pattern for modern single-page interactions: retains native browser constraint validation while preventing full-page reloads.

```html
<form id="profile-form" action="/api/profile" method="post">
  <div class="field">
    <label for="display-name">Display Name</label>
    <input type="text" id="display-name" name="display_name" required minlength="3" />
  </div>

  <div class="field">
    <label for="bio">Bio</label>
    <textarea id="bio" name="bio" maxlength="200"></textarea>
  </div>

  <button type="submit" id="save-btn">Update Profile</button>
  <div id="submit-status" role="status" aria-live="polite"></div>
</form>

<script>
  const form = document.getElementById('profile-form');
  const statusDiv = document.getElementById('submit-status');
  const saveBtn = document.getElementById('save-btn');

  form.addEventListener('submit', async (event) => {
    // 1. Prevent default full-page reload
    event.preventDefault();

    // 2. Disable submit button to prevent double submissions
    saveBtn.disabled = true;
    statusDiv.textContent = 'Saving changes...';

    // 3. Extract submittable form data directly from the form element
    const formData = new FormData(form);

    try {
      const response = await fetch(form.action, {
        method: form.method,
        body: formData, // Automatically sets correct multipart header with boundary
        headers: {
          'Accept': 'application/json'
        }
      });

      if (!response.ok) {
        throw new Error(`Server returned status ${response.status}`);
      }

      const result = await response.json();
      statusDiv.textContent = 'Profile successfully updated!';
    } catch (err) {
      statusDiv.textContent = `Update failed: ${err.message}`;
    } finally {
      saveBtn.disabled = false;
    }
  });
</script>
```

---

### Example 3: Safe Programmatic Submission with `requestSubmit()`
Illustrates how to trigger a form submit from an external UI element (such as a modal header button) while respecting form validation.

```html
<!-- External header toolbar -->
<header class="app-header">
  <h2>Settings Editor</h2>
  <button type="button" id="header-save-btn">Save (External)</button>
</header>

<!-- Main Form -->
<main>
  <form id="settings-form" action="/api/settings" method="post">
    <label for="org-name">Organization Name (Required):</label>
    <input type="text" id="org-name" name="org_name" required />

    <label for="contact-email">Contact Email (Required):</label>
    <input type="email" id="contact-email" name="contact_email" required />

    <!-- Hidden native submitter button for native triggers -->
    <button type="submit" id="native-submit" hidden>Submit</button>
  </form>
</main>

<script>
  const externalBtn = document.getElementById('header-save-btn');
  const settingsForm = document.getElementById('settings-form');

  externalBtn.addEventListener('click', () => {
    // CORRECT: requestSubmit triggers native validation and the 'submit' event!
    // NEVER call settingsForm.submit() here as it bypasses validation!
    settingsForm.requestSubmit();
  });

  settingsForm.addEventListener('submit', (e) => {
    e.preventDefault();
    alert('Settings form passed constraint validation and submitted safely!');
  });
</script>
```

---

### Example 4: JSON Payload Transformation from `FormData`
How to seamlessly convert a standard HTML form into structured JSON for modern REST or GraphQL APIs.

```html
<form id="order-form">
  <fieldset>
    <legend>Product Order</legend>

    <label for="sku">Product SKU:</label>
    <input type="text" id="sku" name="item.sku" value="PROD-9981" readonly />

    <label for="quantity">Quantity:</label>
    <input type="number" id="quantity" name="item.qty" value="2" min="1" max="10" />

    <label for="newsletter">Subscribe to updates:</label>
    <input type="checkbox" id="newsletter" name="preferences.newsletter" value="true" checked />

    <button type="submit">Place Order</button>
  </fieldset>
</form>

<script>
  document.getElementById('order-form').addEventListener('submit', async (e) => {
    e.preventDefault();

    const formData = new FormData(e.target);
    
    // Convert flat FormData into standard key-value dictionary
    const plainEntries = Object.fromEntries(formData.entries());

    console.log('Serialized JSON Object:', plainEntries);

    const res = await fetch('/api/orders', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json'
      },
      body: JSON.stringify(plainEntries)
    });
  });
</script>
```

---

### Example 5: File Progress Tracking and Upload Abort
Leveraging `FormData` with `XMLHttpRequest` to provide real-time percentage progress bars and cancelation controls.

```html
<form id="upload-form" action="/api/drive" method="post" enctype="multipart/form-data">
  <label for="archive-file">Choose Backup Archive (.zip):</label>
  <input type="file" id="archive-file" name="backup" accept=".zip" required />

  <div id="progress-container" hidden>
    <progress id="upload-bar" value="0" max="100"></progress>
    <span id="upload-label">0%</span>
    <button type="button" id="abort-btn">Cancel</button>
  </div>

  <button type="submit" id="start-btn">Begin Backup</button>
  <div id="upload-log" role="status" aria-live="polite"></div>
</form>

<script>
  const form = document.getElementById('upload-form');
  const progContainer = document.getElementById('progress-container');
  const progBar = document.getElementById('upload-bar');
  const progLabel = document.getElementById('upload-label');
  const abortBtn = document.getElementById('abort-btn');
  const log = document.getElementById('upload-log');

  let activeXhr = null;

  form.addEventListener('submit', (e) => {
    e.preventDefault();

    const data = new FormData(form);
    activeXhr = new XMLHttpRequest();
    activeXhr.open('POST', form.action);

    progContainer.hidden = false;
    log.textContent = 'Uploading file...';

    activeXhr.upload.onprogress = (evt) => {
      if (evt.lengthComputable) {
        const pct = Math.round((evt.loaded / evt.total) * 100);
        progBar.value = pct;
        progLabel.textContent = `${pct}%`;
      }
    };

    activeXhr.onload = () => {
      progContainer.hidden = true;
      log.textContent = activeXhr.status === 200 ? 'Upload complete!' : `Failed: ${activeXhr.statusText}`;
    };

    activeXhr.onabort = () => {
      progContainer.hidden = true;
      log.textContent = 'Upload canceled by user.';
    };

    activeXhr.send(data);
  });

  abortBtn.addEventListener('click', () => {
    if (activeXhr) activeXhr.abort();
  });
</script>
```

---

## 6. Best Practices

- [ ] **Always use `form.requestSubmit()` instead of `form.submit()`**: `form.submit()` silently ignores validation errors, skips the `submit` event, and circumvents assistive technology hooks.
- [ ] **Never manually set `Content-Type: multipart/form-data` with `fetch(..., { body: formData })`**: Let the browser automatically set the header along with the unique MIME boundary parameter (e.g., `; boundary=----WebKitFormBoundaryXYZ`). Hardcoding this header breaks server multipart parsing.
- [ ] **Ensure every submittable input has a valid `name` attribute**: Inputs without `name` attributes are completely omitted from native form submission payloads and `FormData` extraction.
- [ ] **Disable the submit button upon submission**: Prevent accidental duplicate submissions or multiple credit card charges by disabling the trigger button or tracking pending states.
- [ ] **Always support keyboard `Enter` submission**: Do not replace `<button type="submit">` with clickable `<div>` or `<span>` tags. Retain real submit buttons so users can submit by pressing `Enter` anywhere in single-line inputs.
- [ ] **Leverage `formnovalidate` for secondary actions**: Use `formnovalidate` on "Save Draft", "Back", or "Cancel" buttons so users are not blocked by invalid inputs when trying to exit or stash work.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Hardcoding `headers: { 'Content-Type': 'multipart/form-data' }` in `fetch` | **Critical** | Server fails to parse payload because the multipart boundary delimiter is missing. | Remove the manual header; let `fetch` auto-generate the header and boundary. |
| Calling `form.submit()` programmatically | **High** | Completely skips HTML5 constraint validation and the `submit` event handler. | Call `form.requestSubmit()` instead. |
| Missing `name` attribute on input elements | **High** | Input value is silently ignored during form submission and omitted from `FormData`. | Add descriptive `name` attributes to all actionable inputs. |
| Overwriting native form submission without keyboard accessibility | **High** | Keyboard and screen reader users cannot submit the form via `Enter`. | Keep a native `<button type="submit">` inside the form. |
| Disabling inputs before capturing `new FormData(form)` | **Moderate** | Disabled inputs (`disabled`) are excluded from form data serialization by specification. | Extract `new FormData(form)` *before* setting `input.disabled = true`. |
| Omitting `method="POST"` when uploading files | **Critical** | Form defaults to GET; file data is discarded and only filename is appended to URL query. | Specify `method="POST"` and `enctype="multipart/form-data"`. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Guideline 3.2.2 On Input**: Form submission must only occur on explicit user request (e.g., clicking a submit button or pressing Enter), never abruptly on `<select>` change or radio button toggle without prior warning.
- **Guideline 3.3.1 Error Identification**: If submission is prevented by validation failures, focus must shift to the first erroneous element, and error messages must be linked via `aria-describedby`.
- **Status Messages (WCAG 4.1.3)**: Use `aria-live="polite"` or `role="status"` containers to notify screen readers of asynchronous submission progress ("Saving...", "Success", "Error").

### Security
- **Cross-Site Request Forgery (CSRF)**: Every state-changing form submission (POST, PUT, DELETE) must be protected with anti-CSRF synchronizer tokens (`<input type="hidden" name="_csrf" value="...">`) and `SameSite=Lax` or `SameSite=Strict` session cookies.
- **Double Submit Attacks**: Disable submission buttons upon submit event dispatch to prevent duplicate financial or transactional mutations.

---

## 9. Comparison / When to Use What

| Technique | Baseline Status | Constraint Validation | Triggers `submit` Event | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **Native HTTP Submit (`<button type="submit">`)** | Widely Available | Full Native Validation | Yes | Multi-page apps, progressive enhancement, zero-JS environments. |
| **`form.requestSubmit()`** | Widely Available | Full Native Validation | Yes | Programmatic triggers (external toolbars, hotkeys, auto-save timers). |
| **`form.submit()`** | Widely Available | **Ignored / Skipped** | **No** | Legacy scripts; avoid in modern development. |
| **`fetch()` + `FormData`** | Widely Available | Intercepted in `submit` | Yes | Modern single-page applications, seamless AJAX updates without page reloads. |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between <code>form.submit()</code> and <code>form.requestSubmit()</code>?</strong></summary>

`form.submit()` directly triggers the low-level submission protocol without firing the `submit` event, without running HTML5 constraint validation, and without associating the submission with any specific submit button. In contrast, `form.requestSubmit()` acts as if a user clicked a submit button: it performs full constraint validation, fires the `submit` event (allowing `event.preventDefault()`), and respects button overrides (`formaction`, `formnovalidate`).
</details>

<details>
<summary><strong>2. Why will <code>fetch(url, { method: 'POST', body: formData, headers: { 'Content-Type': 'multipart/form-data' } })</code> fail on the server?</strong></summary>

When you provide a `FormData` object as the `body` to `fetch()`, the browser automatically computes a unique multipart boundary string (e.g., `boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW`) and sets the complete header. If you manually specify `'Content-Type': 'multipart/form-data'`, you strip out the boundary parameter, making it impossible for the backend parser to delimit the incoming fields and files.
</details>

<details>
<summary><strong>3. Are disabled form controls included in a form submission or <code>FormData</code> instance?</strong></summary>

No. According to the WHATWG specification, disabled form controls (`element.disabled = true`) are not submittable elements. They are completely excluded from the form data set, omitted from URL-encoded strings, and excluded from `new FormData(form)`. If you need to send an un-editable value, use the `readonly` attribute instead of `disabled`.
</details>

<details>
<summary><strong>4. How do button-level override attributes like <code>formaction</code> and <code>formnovalidate</code> work?</strong></summary>

When a form contains multiple submit buttons, clicking a specific button allows its attributes to override the parent `<form>` attributes during the submission algorithm. For example, `<button type="submit" formaction="/save-draft" formnovalidate>` overrides the form's primary `action` URL and suppresses constraint validation for that submission instance only.
</details>

<details>
<summary><strong>5. What is the browser's default encoding type (<code>enctype</code>) for forms, and when must it be changed?</strong></summary>

The default encoding type is `application/x-www-form-urlencoded`, which encodes data as key-value pairs separated by `&` with percent-encoded special characters. It must be changed to `multipart/form-data` whenever the form includes `<input type="file">` controls, because `application/x-www-form-urlencoded` serializes only the filename string and drops binary file data.
</details>

<details>
<summary><strong>6. Can you manipulate a <code>FormData</code> object before sending it to a server?</strong></summary>

Yes. `FormData` provides mutation methods including `.append(name, value)`, `.set(name, value)`, `.delete(name)`, `.get(name)`, `.getAll(name)`, and `.has(name)`. You can also listen to the native `formdata` event on the form element:
```javascript
form.addEventListener('formdata', (e) => {
  e.formData.append('timestamp', Date.now());
});
```
</details>

<details>
<summary><strong>7. How does implicit form submission work when a user presses the <code>Enter</code> key?</strong></summary>

If a form contains a submit button, pressing `Enter` while focused on a text-based input implicitly clicks the first submit button in tree order. If a form has no submit button but only has a single text input, pressing `Enter` will submit the form. If it has multiple text inputs and no submit button, pressing `Enter` generally does not submit the form in most browsers.
</details>

<details>
<summary><strong>8. How can you convert a <code>FormData</code> object into a URL query string for a GET request?</strong></summary>

You pass the `FormData` instance directly into the `URLSearchParams` constructor:
```javascript
const formData = new FormData(form);
const queryString = new URLSearchParams(formData).toString();
const url = `${form.action}?${queryString}`;
```
</details>

---

## 11. Practice Exercises

### Exercise 1: Multi-Action Form with Overrides (Easy)
- **Task**: Build an HTML form with inputs for title and content. Provide two submit buttons: "Publish" (POST to `/api/publish`, requires valid inputs) and "Save Draft" (POST to `/api/draft`, bypasses validation using `formnovalidate`).

<details>
<summary>View Solution</summary>

```html
<form action="/api/publish" method="post">
  <div class="field">
    <label for="post-title">Article Title</label>
    <input type="text" id="post-title" name="title" required minlength="5" />
  </div>

  <div class="field">
    <label for="post-content">Article Content</label>
    <textarea id="post-content" name="content" required></textarea>
  </div>

  <div class="actions">
    <button type="submit" name="status" value="published">
      Publish Article
    </button>

    <button 
      type="submit" 
      formaction="/api/draft" 
      formnovalidate 
      name="status" 
      value="draft"
    >
      Save as Draft
    </button>
  </div>
</form>
```
</details>

---

### Exercise 2: AJAX Interceptor with Form State Management (Medium)
- **Task**: Implement a script that intercepts form submission, disables all submit buttons during the network call, posts the data as `FormData` to `/api/feedback`, displays a status message, and re-enables buttons on completion.

<details>
<summary>View Solution</summary>

```html
<form id="feedback-form" action="/api/feedback" method="post">
  <label for="feedback-msg">Your Feedback:</label>
  <textarea id="feedback-msg" name="message" required></textarea>

  <button type="submit" id="submit-btn">Send Feedback</button>
  <div id="status-box" role="status" aria-live="polite"></div>
</form>

<script>
  const form = document.getElementById('feedback-form');
  const btn = document.getElementById('submit-btn');
  const statusBox = document.getElementById('status-box');

  form.addEventListener('submit', async (e) => {
    e.preventDefault();

    btn.disabled = true;
    statusBox.textContent = 'Submitting feedback...';

    const formData = new FormData(form);

    try {
      const res = await fetch(form.action, {
        method: form.method,
        body: formData
      });

      if (!res.ok) throw new Error(`HTTP Error ${res.status}`);

      statusBox.textContent = 'Thank you! Your feedback has been received.';
      form.reset();
    } catch (err) {
      statusBox.textContent = `Error: ${err.message}`;
    } finally {
      btn.disabled = false;
    }
  });
</script>
```
</details>

---

### Exercise 3: Dynamic Multi-Step Form with `requestSubmit()` Validation (Hard)
- **Task**: Create a 2-step wizard where Step 1 inputs must pass validation before advancing to Step 2. Use `form.requestSubmit()` or `form.checkValidity()` to ensure native validation triggers and focus moves to invalid inputs if Step 1 fails.

<details>
<summary>View Solution</summary>

```html
<form id="wizard-form" action="/api/register" method="post">
  <!-- Step 1 Section -->
  <fieldset id="step-1">
    <legend>Step 1: Account Information</legend>
    
    <div>
      <label for="wiz-email">Email:</label>
      <input type="email" id="wiz-email" name="email" required />
    </div>

    <div>
      <label for="wiz-pass">Password (min 8 chars):</label>
      <input type="password" id="wiz-pass" name="password" minlength="8" required />
    </div>

    <button type="button" id="next-btn">Next Step &rarr;</button>
  </fieldset>

  <!-- Step 2 Section -->
  <fieldset id="step-2" hidden>
    <legend>Step 2: Profile Details</legend>

    <div>
      <label for="wiz-bio">Short Bio:</label>
      <textarea id="wiz-bio" name="bio" required></textarea>
    </div>

    <button type="button" id="back-btn">&larr; Back</button>
    <button type="submit">Complete Registration</button>
  </fieldset>
</form>

<script>
  const step1 = document.getElementById('step-1');
  const step2 = document.getElementById('step-2');
  const nextBtn = document.getElementById('next-btn');
  const backBtn = document.getElementById('back-btn');
  const emailInput = document.getElementById('wiz-email');
  const passInput = document.getElementById('wiz-pass');

  nextBtn.addEventListener('click', () => {
    // Validate Step 1 inputs
    const isStep1Valid = emailInput.reportValidity() && passInput.reportValidity();
    
    if (isStep1Valid) {
      step1.hidden = true;
      step2.hidden = false;
      document.getElementById('wiz-bio').focus();
    }
  });

  backBtn.addEventListener('click', () => {
    step2.hidden = true;
    step1.hidden = false;
    emailInput.focus();
  });
</script>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                               FORM SUBMISSION QUICK REFERENCE                                   |
+-------------------------------------------------------------------------------------------------+
| Feature                  | Mechanism / Behavior                                                 |
+-------------------------------------------------------------------------------------------------+
| form.requestSubmit()     | Triggers native validation, fires 'submit', respects overrides       |
| form.submit()            | Low-level submit; BYPASSES validation, NO 'submit' event             |
| formaction="URL"         | Overrides form.action for specific button                            |
| formnovalidate           | Bypasses constraint validation for specific button                   |
| formmethod="post|get"    | Overrides form.method for specific button                            |
+-------------------------------------------------------------------------------------------------+
| FormData API                                                                                    |
| - const fd = new FormData(form);                                                                |
| - fd.append('key', 'val')  -> Adds duplicate or new key                                         |
| - fd.set('key', 'val')     -> Overwrites existing key                                           |
| - fd.delete('key')         -> Removes key                                                       |
| - Object.fromEntries(fd)   -> Converts to JSON-friendly object                                  |
| - new URLSearchParams(fd)  -> Converts to GET query string                                      |
|                                                                                                 |
| CRITICAL FETCH RULE: Never set Content-Type header manually when passing FormData body!         |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [WHATWG HTML Living Standard: Form submission](https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#form-submission-2)
- [MDN Web Docs: `HTMLFormElement.requestSubmit()`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLFormElement/requestSubmit)
- [MDN Web Docs: Using FormData Objects](https://developer.mozilla.org/en-US/docs/Web/API/FormData_API/Using_FormData_Objects)
- [W3C XMLHttpRequest Level 2: The FormData Interface](https://www.w3.org/TR/XMLHttpRequest2/#the-formdata-interface)

---

[← Previous: File Inputs & Drag Upload](10-file-inputs-and-drag-upload.md) | [Section Index](README.md) | [Next: Search & Filter Forms →](12-search-and-filter-forms.md)
