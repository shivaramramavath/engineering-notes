# Module 03: Forms, Input Modalities & Validation

Welcome to Module 03 of the HTML Learning Track. This module covers the interactive data collection, validation, and submission engine of the World Wide Web: HTML forms.

---

## Module Overview

HTML forms connect end users to backend systems. From authentication portals and billing checkouts to telemetry search queries and file uploads, forms govern user input, accessibility validation, and secure data transfer.

In this module, you will master:
1. **Form Mechanics & Submission Lifecycle**: HTTP methods (`GET`, `POST`, `dialog`), encoding types (`multipart/form-data`, `application/x-www-form-urlencoded`), and progressive enhancement.
2. **Input Types & Mobile Modalities**: All 22 native `<input>` types, mobile virtual keyboard targeting with `inputmode`, and keyboard action buttons via `enterkeyhint` (2024 Baseline).
3. **Labels, Fieldsets & Grouping**: Explicit programmatic labelling (`<label for>`), logical input grouping with `<fieldset>` and `<legend>`, and accessible radio/checkbox groups.
4. **Selections & Autocomplete Comboboxes**: Dropdown `<select>`, multi-select arrays, `<optgroup>`, client-side `<datalist>` auto-suggestions, and the future of customizable select.
5. **Text Editing & Action Controls**: Multi-line `<textarea>`, button types (`submit`, `reset`, `button`), and attribute overrides (`formaction`, `formmethod`).
6. **Live Outputs & Progress Gauges**: Real-time calculated `<output>`, deterministic/indeterminate `<progress>`, and scalar range `<meter>` gauges.
7. **Declarative Constraint Validation**: Native HTML5 validation attributes (`required`, `pattern`, `minlength`, `maxlength`, `min`, `max`, `step`), and modern CSS validation pseudo-classes (`:user-valid`, `:user-invalid`).
8. **JavaScript Constraint Validation API**: Deep dive into the `ValidityState` object, `checkValidity()`, `reportValidity()`, and custom error messaging via `setCustomValidity()`.
9. **Autofill & Password Managers**: The WHATWG Autofill specification, multi-token `autocomplete` values, and modern password/Passkey manager integration.
10. **File Ingestion & Drag-and-Drop**: `<input type="file">`, MIME filtering via `accept`, mobile media capture (`capture`), and accessible multi-file staging.
11. **Form Submission & FormData**: Lifecycle of form submission, intercepting `submit` events, the `FormData` object, and multipart HTTP boundary serialization.
12. **Search & Filter Forms**: Real-time faceted search UX, `type="search"`, debouncing, and accessible dynamic results announcements via `aria-live`.
13. **Accessible Form Design & Error Handling**: Accessible error patterns (WCAG 3.3.1, 3.3.2, 3.3.3), `aria-invalid`, `aria-describedby` error associations, and error focus management.
14. **Form Security & CSRF Defense**: Cross-Site Request Forgery (CSRF) anti-tokens, `SameSite` cookie flags, clickjacking mitigation, and preventing autofill credential sniffing.
15. **Dialog & Popover Forms**: In-dialog forms using `<form method="dialog">`, `HTMLDialogElement.returnValue`, and top-layer popover forms.

---

## Lessons in this Module

| # | Lesson | Description | Core Concepts |
| :--- | :--- | :--- | :--- |
| **01** | [`01-form-basics-and-methods.md`](01-form-basics-and-methods.md) | Form Architecture & HTTP Methods | `<form>`, `action`, `method="GET|POST|dialog"`, `enctype`, progressive enhancement |
| **02** | [`02-input-types-and-mobile-keyboards.md`](02-input-types-and-mobile-keyboards.md) | Input Types & Mobile Modalities | All 22 input types, `inputmode`, `enterkeyhint` (2024 Baseline) |
| **03** | [`03-labels-fieldsets-and-legends.md`](03-labels-fieldsets-and-legends.md) | Accessible Labelling & Input Grouping | `<label for>`, implicit wrapping, `<fieldset>`, `<legend>`, radio groups |
| **04** | [`04-select-datalist-and-optgroup.md`](04-select-datalist-and-optgroup.md) | Dropdown Selections & Comboboxes | `<select>`, `<option>`, `<optgroup>`, `<datalist>` auto-suggestions |
| **05** | [`05-textarea-and-button-types.md`](05-textarea-and-button-types.md) | Textarea & Button Types | `<textarea>`, wrap modes, `<button type="submit|button|reset">`, overrides |
| **06** | [`06-output-progress-and-meter.md`](06-output-progress-and-meter.md) | Output, Progress, and Meter Gauges | `<output>`, `<progress>`, `<meter>` ranges, accessibility semantics |
| **07** | [`07-built-in-constraint-validation.md`](07-built-in-constraint-validation.md) | Declarative Constraint Validation | `required`, `pattern`, `minlength`, `step`, `:valid`, `:user-valid` |
| **08** | [`08-constraint-validation-api.md`](08-constraint-validation-api.md) | JavaScript Constraint Validation API | `ValidityState`, `checkValidity()`, `reportValidity()`, `setCustomValidity()` |
| **09** | [`09-autocomplete-tokens-and-autofill.md`](09-autocomplete-tokens-and-autofill.md) | WHATWG Autofill & Password Managers | `autocomplete` tokens, credential managers, Passkey hints |
| **10** | [`10-file-inputs-and-drag-upload.md`](10-file-inputs-and-drag-upload.md) | File Uploads & Ingestion Modalities | `<input type="file">`, `accept`, `multiple`, `capture`, drag-and-drop |
| **11** | [`11-form-submission-and-formdata.md`](11-form-submission-and-formdata.md) | Form Submission Lifecycle & FormData | `submit` event, `FormData`, multipart boundary encoding, overrides |
| **12** | [`12-search-and-filter-forms.md`](12-search-and-filter-forms.md) | Accessible Search & Filtering Forms | `<input type="search">`, facets, live announcements (`aria-live`) |
| **13** | [`13-forms-accessibility-and-error-states.md`](13-forms-accessibility-and-error-states.md) | Accessible Form Architecture & Errors | `aria-describedby`, `aria-invalid`, error summaries, focus recovery |
| **14** | [`14-forms-security-and-csrf.md`](14-forms-security-and-csrf.md) | Form Security, CSRF & Phishing Defense | CSRF tokens, `SameSite` cookies, clickjacking, autofill security |
| **15** | [`15-dialog-and-popover-forms.md`](15-dialog-and-popover-forms.md) | Modal Dialog & Popover Forms | `<form method="dialog">`, `returnValue`, popover forms, focus trapping |

---

## Verification & Prerequisites

Before studying forms, ensure you are comfortable with semantic landmarks and element selection covered in [Module 02: Semantics & Document Structure](../02-semantics-and-structure/README.md).

All code samples comply with the WHATWG HTML Living Standard and WCAG 2.2 AA accessibility standards.
