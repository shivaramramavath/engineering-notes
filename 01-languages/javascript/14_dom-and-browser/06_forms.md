# Forms

Forms collect user input. The browser provides built-in controls, validation, and submission; JavaScript enhances them.

```html
<form id="signup" action="/signup" method="post" novalidate>
  <label for="email">Email</label>
  <input id="email" name="email" type="email" required autocomplete="email">

  <label for="age">Age</label>
  <input id="age" name="age" type="number" min="13" max="120">

  <button type="submit">Create account</button>
</form>
```

## Accessing forms and controls

```js
const form = document.getElementById("signup");        // or document.forms.signup
form.elements;                                          // all controls (HTMLFormControlsCollection)
form.elements.email;                                    // by name or id
form.elements["email"].value;
form.email.value;                                       // named access shortcut (avoid: name clashes)
form.checkValidity(); form.reportValidity();
form.reset(); form.requestSubmit(); form.submit();      // requestSubmit runs validation and fires `submit`; submit() does not
```

## Input types

| Type | Notes |
|------|-------|
| `text`, `search`, `tel`, `url`, `email`, `password` | text-like; mobile keyboards adapt |
| `number`, `range` | numeric; `valueAsNumber` |
| `date`, `time`, `datetime-local`, `month`, `week` | native pickers; `valueAsDate` |
| `checkbox`, `radio` | `checked`; radios share a `name` |
| `file` | `files`, `accept`, `multiple` |
| `color`, `hidden` | |
| `<select>`, `<textarea>`, `<datalist>`, `<output>`, `<progress>`, `<meter>` | |

Use the **right type** and `autocomplete`/`inputmode` attributes for better UX and mobile keyboards.

## Reading values

```js
input.value;                       // string, always
input.valueAsNumber;               // number (NaN if empty/invalid) for number/range/date types
input.valueAsDate;                 // Date for date/time types
checkbox.checked;                  // boolean
select.value;                      // selected option value
select.selectedOptions;            // multiple select
[...select.selectedOptions].map((o) => o.value);
radioGroup = form.elements.plan;   // RadioNodeList
radioGroup.value;                  // value of the checked radio
file.files[0];                     // File object
```

## `input` vs `change`

| Event | Fires |
|-------|-------|
| `input` | on every change of value (each keystroke, slider move) |
| `change` | when the value is **committed** (blur for text, immediately for checkbox/select) |

```js
searchInput.addEventListener("input", debounce(onSearch, 300));
select.addEventListener("change", () => load(select.value));
```

## FormData

```js
const data = new FormData(form);            // collects named, enabled controls (+ submitter if passed)
data.get("email");
data.getAll("tags");                         // multiple values for the same name
data.append("extra", "1");
data.set("age", "30");
data.delete("token");
[...data.entries()];

const plain = Object.fromEntries(data);      // last value wins for duplicate names
const search = new URLSearchParams(data);    // application/x-www-form-urlencoded
```

Group duplicates:

```js
const toObject = (fd) => {
  const out = {};
  for (const key of new Set(fd.keys())) {
    const values = fd.getAll(key);
    out[key] = values.length > 1 ? values : values[0];
  }
  return out;
};
```

`FormData` includes `File` objects and is ideal for uploads. Controls **without** a `name`, or `disabled`, are not included. Unchecked checkboxes are omitted.

## Handling submit

```js
form.addEventListener("submit", async (event) => {
  event.preventDefault();                           // stop full-page navigation

  if (!form.checkValidity()) {
    form.reportValidity();
    return;
  }

  const submitter = event.submitter;                // which button was used
  submitter.disabled = true;
  try {
    const res = await fetch(form.action, {
      method: form.method,
      body: new FormData(form),                     // multipart; the browser sets the boundary
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    showSuccess(await res.json());
  } catch (err) {
    showError(err);
  } finally {
    submitter.disabled = false;
  }
});
```

JSON submission:

```js
await fetch("/api/signup", {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify(Object.fromEntries(new FormData(form))),
});
```

Do **not** set `Content-Type` manually when sending `FormData`.

Enter in a text input triggers `submit` (via the default button): always listen for `submit` on the form, not `click` on the button.

## Constraint validation API

Built-in attributes: `required`, `type`, `min`, `max`, `step`, `minlength`, `maxlength`, `pattern`, `multiple`.

```html
<input name="zip" pattern="\d{5}" title="5 digits" required>
```

```js
input.validity;                       // ValidityState
input.validity.valueMissing;          // required but empty
input.validity.typeMismatch;          // email/url invalid
input.validity.patternMismatch;
input.validity.tooShort; input.validity.tooLong;
input.validity.rangeUnderflow; input.validity.rangeOverflow; input.validity.stepMismatch;
input.validity.customError;
input.validity.valid;

input.validationMessage;              // browser's message
input.checkValidity();                // boolean, fires `invalid` event
input.reportValidity();               // boolean, shows the bubble
input.setCustomValidity("Passwords must match");   // non-empty = invalid; "" clears it
```

Custom validation:

```js
function validatePasswords() {
  confirm.setCustomValidity(confirm.value === password.value ? "" : "Passwords do not match");
}
password.addEventListener("input", validatePasswords);
confirm.addEventListener("input", validatePasswords);
```

Custom error UI:

```js
form.addEventListener("invalid", (e) => {
  e.preventDefault();                                   // suppress the default bubble
  showFieldError(e.target, e.target.validationMessage);
}, true);                                               // `invalid` does not bubble: use capture
```

CSS hooks: `:invalid`, `:valid`, `:required`, `:optional`, `:user-invalid` / `:user-valid` (only after interaction, newer browsers), `:placeholder-shown`, `:focus-within`.

```css
input:user-invalid { border-color: crimson; }
```

`novalidate` on the form disables the browser's automatic blocking so you can run your own flow (still use `checkValidity()` if you want the built-in rules).

## Client validation is not security

Always **validate again on the server**. Client checks improve UX; attackers bypass them.

## Files

```html
<input type="file" id="pic" accept="image/*" multiple>
```

```js
pic.addEventListener("change", () => {
  for (const file of pic.files) {
    file.name; file.size; file.type; file.lastModified;
    if (file.size > 5 * 1024 * 1024) return alert("Max 5 MB");
  }
});

// preview without uploading
const url = URL.createObjectURL(file);
img.src = url;
img.onload = () => URL.revokeObjectURL(url);           // release memory

// read content
const text = await file.text();
const buffer = await file.arrayBuffer();
const reader = new FileReader();                       // older callback API
reader.readAsDataURL(file);

// upload with progress (fetch has no upload progress; use XHR or streams where supported)
const xhr = new XMLHttpRequest();
xhr.upload.onprogress = (e) => setProgress(e.loaded / e.total);
```

Drag and drop:

```js
dropzone.addEventListener("dragover", (e) => e.preventDefault());           // required to allow dropping
dropzone.addEventListener("drop", (e) => {
  e.preventDefault();
  handleFiles(e.dataTransfer.files);
});
```

`accept` is only a hint: validate type and size server-side.

## Special controls

```js
select.add(new Option("Label", "value"));
select.options[select.selectedIndex];
textarea.value; textarea.setSelectionRange(0, 5); input.select();
input.focus(); input.blur();
checkbox.indeterminate = true;
details.open = true;
```

`<output>` shows results; `<datalist>` gives suggestions; `<progress>`/`<meter>` display values.

## Disabled vs readonly

| | `disabled` | `readonly` |
|---|-----------|------------|
| Focusable | no | yes |
| Submitted with the form | **no** | yes |
| Validated | skipped | skipped |
| Typical use | unavailable | display-only values the user may copy |

## Accessibility

- Every control needs a **label** (`<label for>` or wrapping `<label>`), not just a placeholder
- Group related controls with `<fieldset>` and `<legend>`
- Connect errors to inputs with `aria-describedby`, set `aria-invalid="true"`, announce with `aria-live="polite"` or move focus to the first error
- Keep keyboard flow logical, show visible focus
- Do not disable the submit button as the only feedback: explain what is wrong

```html
<input id="email" aria-describedby="email-error" aria-invalid="true">
<p id="email-error" role="alert">Enter a valid email address.</p>
```

## Autofill, autocomplete and mobile

```html
<input name="email" type="email" autocomplete="email" inputmode="email" autocapitalize="none" spellcheck="false">
<input name="otp" autocomplete="one-time-code" inputmode="numeric" pattern="\d{6}">
<input name="pw" type="password" autocomplete="new-password">
```

Correct `autocomplete` tokens help password managers and autofill.

## IME composition

For text input in languages using IME (Chinese, Japanese, Korean), avoid acting on partial text:

```js
input.addEventListener("compositionstart", () => (composing = true));
input.addEventListener("compositionend", () => { composing = false; onSearch(input.value); });
input.addEventListener("input", (e) => { if (!e.isComposing) onSearch(input.value); });
```

## Preventing double submits and handling state

- Disable the submitter while pending, re-enable in `finally`
- Use idempotency keys for payments/mutations
- Preserve user input on errors; restore focus to the first invalid field
- Warn on unsaved changes with `beforeunload` only when a form is dirty

## Security notes

- Never trust client values (including hidden inputs)
- Use CSRF protection (SameSite cookies, tokens) for cookie-authenticated posts
- Escape/encode output; do not inject form values into HTML with `innerHTML`
- Do not store passwords or secrets in `localStorage`
- Use `autocomplete="off"` only when truly necessary (it hampers password managers)

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Listening for `click` on the submit button | Misses Enter-key submissions | Listen for `submit` on the form |
| Forgetting `preventDefault()` in an AJAX submit | Page reloads | Call it first |
| Setting `Content-Type` with `FormData` | Breaks the multipart boundary | Let the browser set it |
| Reading `value` as a number | Always a string | `valueAsNumber` / `Number()` |
| Relying on placeholders as labels | Poor accessibility | Real `<label>` |
| `name` missing on inputs | Excluded from `FormData` | Add `name` |
| Only client-side validation | Bypassable | Validate on the server too |
| `invalid` events not firing delegated handlers | They do not bubble | Capture phase |
| Not revoking object URLs | Memory leak | `URL.revokeObjectURL` |
| Treating `accept` as security | Easily bypassed | Validate server-side |
| Using `form.submit()` and expecting validation | Skips it and the `submit` event | `requestSubmit()` |

## Key takeaways

- Use semantic controls, correct input types, labels and `autocomplete`
- Handle the form's `submit` event, `preventDefault`, and send data with `FormData` (or JSON)
- Use the built-in validity API (`setCustomValidity`, `reportValidity`) and CSS pseudo-classes
- Validate on the server; make errors accessible and prevent double submits

**Next:** [Observers](./07_observers.md)
