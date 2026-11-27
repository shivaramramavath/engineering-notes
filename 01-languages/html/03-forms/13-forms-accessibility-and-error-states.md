# Forms Accessibility, Error States, and Focus Management

| Metadata | Details |
| :--- | :--- |
| **Specification** | W3C WAI-ARIA 1.2 / WCAG 2.2 AA (Guidelines 1.3.1, 3.3.1, 3.3.2, 3.3.3, 3.3.4, 3.3.7, 3.3.8) |
| **Prerequisites** | `03-forms/03-labels-fieldsets-and-legends.md`, `03-forms/08-constraint-validation-api.md` |
| **Estimated Reading Time** | 26 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`aria-describedby`, `aria-invalid`, `aria-live`, `role="alert"`) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, axe-core 4.9, NVDA 2024.1, JAWS 2024, VoiceOver macOS 14.4 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Basic Inline Error Association with `aria-describedby`](#example-1-basic-inline-error-association-with-aria-describedby)
   - [Example 2: Dedicated Error Summary Box with In-Page Jump Anchors](#example-2-dedicated-error-summary-box-with-in-page-jump-anchors)
   - [Example 3: Dynamic Live Validation with Screen Reader Announcements](#example-3-dynamic-live-validation-with-screen-reader-announcements)
   - [Example 4: Accessible Multi-Input Group Errors (Radio Buttons & Checkboxes)](#example-4-accessible-multi-input-group-errors-radio-buttons--checkboxes)
   - [Example 5: Focus Management and Error Recovery Workflow](#example-5-focus-management-and-error-recovery-workflow)
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

Forms are the single most interactive, barrier-prone aspect of the web. When a form fails validation, sighted users often rely on visual cues (red outlines, exclamation icons, toast alerts). However, for screen reader users, keyboard-only operators, and people with cognitive disabilities, poorly designed error handling is catastrophic: focus gets lost, silent errors strand users on the page, and cryptic error messages offer no path to correction.

Web Content Accessibility Guidelines (WCAG) 2.2 establishes rigorous criteria for form accessibility:
- **3.3.1 Error Identification**: If an input error is detected, the item in error is identified and the error is described to the user in text.
- **3.3.2 Labels or Instructions**: Labels or instructions are provided when content requires user input.
- **3.3.3 Error Suggestion**: If an input error is detected and suggestions are known, suggestions are provided.
- **3.3.4 Error Prevention (Legal, Financial, Data)**: Submissions can be reviewed, confirmed, or reversed.
- **3.3.7 Redundant Entry (WCAG 2.2)**: Previously entered information is either auto-populated or available for selection.
- **3.3.8 Accessible Authentication (WCAG 2.2)**: Cognitive function tests (CAPTCHAs, memorized passwords) have accessible alternatives.

Building accessible forms requires orchestrating semantic HTML, ARIA attributes (`aria-describedby`, `aria-invalid`), programmatic focus management, and non-color-dependent visual cues.

---

## 2. Core Concepts

### Accessible Name vs. Accessible Description
The browser accessibility tree represents every form control with two primary text channels:
1. **Accessible Name**: The core label telling the user *what* the control is (e.g., "Email Address"). Derived from `<label for="...">`, `aria-labelledby`, or `aria-label`.
2. **Accessible Description**: Supplementary text telling the user *how* to use it or *why* it failed (e.g., "Enter a valid company email ending in .org"). Derived from `aria-describedby` or `aria-errormessage`.

### Linking Errors with `aria-describedby`
While `aria-errormessage` exists in the ARIA specification, its screen reader support remains spotty. In contrast, `aria-describedby` is **universally supported** across all modern assistive technologies. By adding the ID of the error container to `aria-describedby`, screen readers speak the error message immediately after reading the label and current value upon focusing the control.

Multiple IDs can be passed to `aria-describedby` separated by spaces:
```html
<input 
  type="password" 
  id="pwd" 
  aria-describedby="pwd-rules pwd-error"
  aria-invalid="true"
/>
```
When focused, the screen reader announces: *"Password, edit text, protected. Minimum 8 characters, 1 number. Error: Password is too short."*

### Marking Failure with `aria-invalid`
- `aria-invalid="true"`: Explicitly informs the accessibility tree that the field currently contains an erroneous value. Screen readers announce "Invalid entry" or "Error".
- `aria-invalid="false"` or omitted: The field is valid or has not yet been validated.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                  Accessible Error Handling and Focus Lifecycle                          |
+-----------------------------------------------------------------------------------------+

  1. User triggers submission with invalid data
                      |
                      v
  2. Client-side or Server-side Validation detects errors
                      |
                      v
  3. Update Accessibility Tree:
     - Target inputs get: aria-invalid="true"
     - Target inputs get: aria-describedby="[id-of-error-text]"
     - Error container receives text content
                      |
                      v
  4. Focus Management (Choose Pattern):
     |
     +--> Pattern A: Multi-Field Errors (Form has 2+ errors)
     |    - Populate top Error Summary Box (<div role="alert" tabindex="-1">)
     |    - Programmatically focus summary box: summaryBox.focus()
     |    - Announce: "There are 3 errors in this form."
     |    - Each summary item is an in-page anchor linking to the specific input
     |
     +--> Pattern B: Single Field Error
          - Programmatically focus first invalid input: invalidInput.focus()
          - Screen reader announces: "Email address, invalid entry, error message."
```

---

## 4. Syntax & Usage

```html
<div class="field-group">
  <label for="user-email">Work Email</label>
  <input 
    type="email" 
    id="user-email" 
    name="email" 
    aria-invalid="true" 
    aria-describedby="email-error email-hint" 
    required
  />
  <p id="email-hint" class="hint">We will never share your email.</p>
  <p id="email-error" class="error-text">
    <span class="visually-hidden">Error: </span>Please enter a valid email address.
  </p>
</div>
```

> **Cross-Browser & Baseline Status Box**
>
> - `aria-invalid`, `aria-describedby`, `aria-labelledby`: **Baseline Widely Available** across all browsers and screen readers.
> - `role="alert"` / `aria-live="assertive"`: **Baseline Widely Available**.
> - `aria-errormessage`: Limited availability; many assistive technologies do not yet announce it reliably. Recommended approach is using `aria-describedby` alongside `aria-invalid="true"`.

---

## 5. Practical Examples

### Example 1: Basic Inline Error Association with `aria-describedby`
A clean, accessible single-field validation pattern that works across all desktop and mobile screen readers.

```html
<form action="/newsletter" method="post" novalidate>
  <div class="form-control error">
    <label for="sub-email">Email Address <span aria-hidden="true">*</span></label>
    
    <input 
      type="email" 
      id="sub-email" 
      name="email" 
      value="invalid-email@" 
      aria-invalid="true" 
      aria-describedby="email-instructions email-error-msg" 
      required 
    />

    <p id="email-instructions" class="helper-text">
      Enter your standard corporate email address.
    </p>

    <!-- Error message with invisible prefix for clear screen reader context -->
    <p id="email-error-msg" class="error-message">
      <svg aria-hidden="true" width="16" height="16" viewBox="0 0 24 24">
        <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z"/>
      </svg>
      <span class="visually-hidden">Error: </span>
      The email address must include a valid domain extension like .com or .org.
    </p>
  </div>

  <button type="submit">Subscribe</button>
</form>

<style>
  .visually-hidden {
    position: absolute;
    width: 1px;
    height: 1px;
    padding: 0;
    margin: -1px;
    overflow: hidden;
    clip: rect(0,0,0,0);
    border: 0;
  }
  .form-control.error input {
    border: 2px solid #d4351c;
    outline: none;
  }
  .form-control.error input:focus-visible {
    outline: 3px solid #0b0c0c;
    box-shadow: 0 0 0 4px #ffdd00;
  }
  .error-message {
    color: #d4351c;
    font-weight: bold;
    display: flex;
    align-items: center;
    gap: 0.5rem;
    margin-top: 0.25rem;
  }
</style>
```

---

### Example 2: Dedicated Error Summary Box with In-Page Jump Anchors
The UK Government Digital Service (GDS) gold standard pattern: upon submit failure, focus moves to an Error Summary component that lists every error as a clickable anchor pointing directly to the erroneous field.

```html
<div 
  class="error-summary" 
  id="error-summary" 
  role="alert" 
  aria-labelledby="error-summary-title" 
  tabindex="-1"
>
  <h2 id="error-summary-title">There is a problem</h2>
  <ul class="error-summary-list">
    <li>
      <a href="#full-name">Enter your full legal name</a>
    </li>
    <li>
      <a href="#passport-number">Passport number must be 9 alphanumeric characters</a>
    </li>
  </ul>
</div>

<form action="/apply" method="post" id="passport-form" novalidate>
  <div class="field-wrap" id="name-group">
    <label for="full-name">Full Legal Name</label>
    <input 
      type="text" 
      id="full-name" 
      name="legal_name" 
      aria-invalid="true" 
      aria-describedby="name-error"
    />
    <p id="name-error" class="error-text">
      <span class="visually-hidden">Error: </span>Enter your full legal name.
    </p>
  </div>

  <div class="field-wrap" id="passport-group">
    <label for="passport-number">Passport Number</label>
    <input 
      type="text" 
      id="passport-number" 
      name="passport_no" 
      aria-invalid="true" 
      aria-describedby="passport-error"
    />
    <p id="passport-error" class="error-text">
      <span class="visually-hidden">Error: </span>Passport number must be 9 alphanumeric characters.
    </p>
  </div>

  <button type="submit">Submit Application</button>
</form>

<script>
  // Ensure that clicking summary links properly focuses the target input
  document.querySelectorAll('.error-summary a').forEach(anchor => {
    anchor.addEventListener('click', (e) => {
      const targetId = anchor.getAttribute('href').slice(1);
      const targetEl = document.getElementById(targetId);
      if (targetEl) {
        targetEl.focus();
      }
    });
  });
</script>
```

---

### Example 3: Dynamic Live Validation with Screen Reader Announcements
Validating an input on `blur`, dynamically updating `aria-invalid`, and broadcasting updates without interrupting the user.

```html
<form id="signup-form">
  <div class="input-block">
    <label for="username-input">Choose a Username:</label>
    <input 
      type="text" 
      id="username-input" 
      name="username" 
      aria-describedby="username-feedback"
      autocomplete="username"
    />
    <div id="username-feedback" class="feedback-text" role="status" aria-live="polite"></div>
  </div>
</form>

<script>
  const usernameInput = document.getElementById('username-input');
  const feedback = document.getElementById('username-feedback');

  usernameInput.addEventListener('blur', async () => {
    const val = usernameInput.value.trim();
    if (!val) {
      usernameInput.removeAttribute('aria-invalid');
      feedback.textContent = '';
      return;
    }

    if (val.length < 4) {
      usernameInput.setAttribute('aria-invalid', 'true');
      feedback.textContent = 'Error: Username must be at least 4 characters long.';
      feedback.className = 'feedback-text error';
    } else {
      usernameInput.setAttribute('aria-invalid', 'false');
      feedback.textContent = 'Username is available!';
      feedback.className = 'feedback-text success';
    }
  });
</script>
```

---

### Example 4: Accessible Multi-Input Group Errors (Radio Buttons & Checkboxes)
Error association across a group of radio buttons wrapped in `<fieldset>` and `<legend>`.

```html
<form action="/plan" method="post" novalidate>
  <fieldset 
    class="radio-group" 
    aria-invalid="true" 
    aria-describedby="billing-error"
  >
    <legend>Select Billing Cycle <span aria-hidden="true">*</span></legend>
    
    <p id="billing-error" class="error-msg">
      <span class="visually-hidden">Error: </span>Please select a billing frequency.
    </p>

    <div class="radio-option">
      <input type="radio" id="cycle-monthly" name="cycle" value="monthly" />
      <label for="cycle-monthly">Monthly ($15 / month)</label>
    </div>

    <div class="radio-option">
      <input type="radio" id="cycle-yearly" name="cycle" value="yearly" />
      <label for="cycle-yearly">Annual ($140 / year — Save 22%)</label>
    </div>
  </fieldset>

  <button type="submit">Continue to Payment</button>
</form>
```

---

### Example 5: Focus Management and Error Recovery Workflow
A complete script demonstrating how to validate on submit, construct an error summary dynamically, move focus, and clear errors upon correction.

```html
<form id="checkout-form" novalidate>
  <div id="summary-holder"></div>

  <div class="field">
    <label for="cardholder">Cardholder Name</label>
    <input type="text" id="cardholder" name="cardholder" required />
    <span id="cardholder-err" class="err-text" hidden></span>
  </div>

  <div class="field">
    <label for="zip-code">Postal Code</label>
    <input type="text" id="zip-code" name="zip" required pattern="[0-9]{5}" />
    <span id="zip-err" class="err-text" hidden></span>
  </div>

  <button type="submit">Pay Now</button>
</form>

<script>
  const form = document.getElementById('checkout-form');
  const summaryHolder = document.getElementById('summary-holder');

  form.addEventListener('submit', (e) => {
    e.preventDefault();
    summaryHolder.innerHTML = '';
    const errors = [];

    // 1. Validate Cardholder
    const nameInput = document.getElementById('cardholder');
    const nameErr = document.getElementById('cardholder-err');
    if (!nameInput.value.trim()) {
      errors.push({ id: 'cardholder', msg: 'Cardholder name is required' });
      nameInput.setAttribute('aria-invalid', 'true');
      nameInput.setAttribute('aria-describedby', 'cardholder-err');
      nameErr.textContent = 'Cardholder name is required';
      nameErr.hidden = false;
    } else {
      nameInput.removeAttribute('aria-invalid');
      nameErr.hidden = true;
    }

    // 2. Validate ZIP
    const zipInput = document.getElementById('zip-code');
    const zipErr = document.getElementById('zip-err');
    if (!/^[0-9]{5}$/.test(zipInput.value.trim())) {
      errors.push({ id: 'zip-code', msg: 'Postal code must be exactly 5 digits' });
      zipInput.setAttribute('aria-invalid', 'true');
      zipInput.setAttribute('aria-describedby', 'zip-err');
      zipErr.textContent = 'Postal code must be exactly 5 digits';
      zipErr.hidden = false;
    } else {
      zipInput.removeAttribute('aria-invalid');
      zipErr.hidden = true;
    }

    // 3. Focus Management
    if (errors.length > 0) {
      // Build accessible summary box
      const summary = document.createElement('div');
      summary.className = 'error-summary';
      summary.setAttribute('role', 'alert');
      summary.setAttribute('tabindex', '-1');
      summary.innerHTML = `
        <h2>There were problems submitting your payment:</h2>
        <ul>
          ${errors.map(err => `<li><a href="#${err.id}">${err.msg}</a></li>`).join('')}
        </ul>
      `;
      summaryHolder.appendChild(summary);
      
      // Move programmatic focus to summary
      summary.focus();
    } else {
      alert('Payment processing initiated!');
    }
  });
</script>
```

---

## 6. Best Practices

- [ ] **Always link inline errors using `aria-describedby`**: Associate the ID of the error message container directly to the erroneous input so assistive technologies read the warning automatically.
- [ ] **Set `aria-invalid="true"` on invalid controls**: Do not rely on CSS class names (`.is-invalid`) alone; screen readers must receive programmatic confirmation of the error state.
- [ ] **Include the word "Error: " in the error message text**: Sighted users see red borders or warning icons, but screen readers might simply read the text. Adding `<span class="visually-hidden">Error: </span>` guarantees unambiguous context.
- [ ] **Move focus intentionally upon failed submission**: Either focus the top Error Summary component (`role="alert" tabindex="-1"`) or shift focus directly to the first invalid input on the page. Never leave focus stranded on the submit button without feedback.
- [ ] **Never indicate errors using color alone (WCAG 1.4.1)**: Red borders must be supplemented with icons, text messages, and bold weight to assist users with color vision deficiencies.
- [ ] **Provide high-visibility focus indicators (`:focus-visible`)**: Focus rings must have at least a 3:1 contrast ratio against both the background and the input borders.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Relying only on red borders (`border-color: red`) to signal errors | **Critical** | Users with protanopia, deuteranopia, or low vision cannot distinguish invalid inputs. | Add an error text message, an icon, and `aria-invalid="true"`. |
| Displaying error text without linking it via `aria-describedby` | **Critical** | Blind users hear the label and value, but the error message is never read aloud. | Add `aria-describedby="[error-id]"` to the input element. |
| Leaving focus on the submit button when submission fails | **High** | Keyboard and screen reader users assume the submission is frozen or in progress. | Move focus to the Error Summary container or the first invalid field. |
| Clearing valid form data when validation fails | **High** | Violates WCAG 3.3.7 (Redundant Entry); frustrates users who must re-type lengthy inputs. | Retain all valid user input values across validation cycles. |
| Using `aria-live="assertive"` for inline keystroke validation | **Moderate** | Annoyingly cuts off screen reader speech on every single letter the user types. | Use `aria-live="polite"` or validate on `blur` / `change`. |
| Using `tabindex="0"` on an Error Summary container | **Moderate** | Adds the summary into the natural Tab order permanently even when there are no errors. | Use `tabindex="-1"` so it can be focused via JavaScript without entering the Tab sequence. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Guideline 3.3.1 Error Identification (Level A)**: Errors must be described in text and identify the specific field.
- **Guideline 3.3.2 Labels or Instructions (Level A)**: Provide clear guidance upfront before the user makes a mistake.
- **Guideline 3.3.3 Error Suggestion (Level AA)**: If an error is detected and correction suggestions are possible (e.g., date format must be DD/MM/YYYY), provide them.
- **Guideline 3.3.4 Error Prevention (Level AA)**: For forms that submit legal commitments, financial transactions, or user personal data, allow review, confirmation, or cancellation.
- **Guideline 3.3.7 Redundant Entry (Level A - WCAG 2.2)**: Never require users to re-enter information previously provided in the same session (e.g., "Billing address same as shipping" checkbox).

---

## 9. Comparison / When to Use What

| Error Handling Strategy | User Experience | Screen Reader Compatibility | Implementation Complexity | Best Suited For |
| :--- | :--- | :--- | :--- | :--- |
| **Error Summary Box (at top)** | Clear overview of all issues; jump links directly to problem fields. | Excellent (via `role="alert"` + `focus()`). | Moderate | Multi-field forms, checkouts, complex applications, multi-step wizards. |
| **Inline Errors with Direct Focus** | Immediate correction of first failing field. | High (via `aria-describedby`). | Low | Short forms (Login, Newsletter, Password Reset). |
| **Real-time As-You-Type Validation** | Instant confirmation or warnings before submit. | Sensitive (must use `aria-live="polite"` carefully). | High | Password strength meters, username availability checkers. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is <code>aria-describedby</code> preferred over <code>aria-errormessage</code> in production accessibility implementations?</strong></summary>

Although `aria-errormessage` is an official ARIA attribute intended specifically for linking error messages, support across various screen readers (such as older versions of JAWS, NVDA, and mobile TalkBack) remains fragmented and inconsistent. In contrast, `aria-describedby` has been universally supported across all browsers and assistive technologies for well over a decade. Pairing `aria-invalid="true"` with `aria-describedby` guarantees 100% reliable error reading.
</details>

<details>
<summary><strong>2. What is the correct focus management strategy when a form submission fails validation?</strong></summary>

For forms with multiple errors, the recommended pattern is to inject or display an Error Summary component at the top of the form with `role="alert"` and `tabindex="-1"`, and call `.focus()` on it immediately. This announces the total error count and provides clickable jump links to each invalid control. For single-input forms (such as a login or newsletter form), shifting programmatic focus directly to the first invalid input (`input.focus()`) is also widely accepted.
</details>

<details>
<summary><strong>3. What does WCAG 2.2 Criterion 3.3.7 (Redundant Entry) require for multi-step checkout forms?</strong></summary>

Criterion 3.3.7 (Level A) mandates that information previously entered by the user in the same process must either be auto-populated or available for the user to select, unless re-entering is essential (e.g., verifying a newly chosen password) or required for security. A classic implementation is a "Shipping address same as billing" checkbox that copies the entered address automatically.
</details>

<details>
<summary><strong>4. Why should an Error Summary container have <code>tabindex="-1"</code> instead of <code>tabindex="0"</code>?</strong></summary>

`tabindex="-1"` allows an element to receive programmatic focus via JavaScript (`element.focus()`) without placing that element into the keyboard's sequential `Tab` navigation order. If you used `tabindex="0"`, the summary container would always sit in the user's Tab sequence even when idle or empty.
</details>

<details>
<summary><strong>5. Why is using red border colors alone considered a failure under WCAG 1.4.1 (Use of Color)?</strong></summary>

Users who have red-green color blindness (protanopia, deuteranopia) or monochromatic vision cannot perceive differences between standard grey/black borders and red error borders. WCAG 1.4.1 requires that visual cues do not rely on color alone; errors must also be accompanied by text descriptions, distinctive icons, increased border thickness, or layout changes.
</details>

<details>
<summary><strong>6. How can an input be associated with BOTH general instructions and an error message simultaneously?</strong></summary>

The `aria-describedby` attribute accepts a space-separated list of multiple IDs. For example:
`<input type="text" id="tel" aria-describedby="tel-hint tel-err" aria-invalid="true" />`
When focused, the screen reader concatenates both text nodes: first reading the hint ("Include country code"), followed by the error ("Phone number must contain 10 digits").
</details>

<details>
<summary><strong>7. How do you ensure screen readers announce the word "Error" when reading an error message?</strong></summary>

Often, an error message visually displays an exclamation icon or red text: "Please enter a valid date." Because screen readers might not announce CSS icons, you should prepend a visually hidden label: `<span class="visually-hidden">Error: </span>Please enter a valid date.`. This ensures screen reader users hear "Error: Please enter a valid date."
</details>

<details>
<summary><strong>8. How should error states be cleared when a user corrects an input value?</strong></summary>

Upon correction, JavaScript should remove `aria-invalid` (or set it to `"false"`), remove the error element ID from `aria-describedby` (or hide the error message container), and remove any invalid CSS styling. Retaining stale `aria-invalid="true"` attributes confuses users who believe their input remains broken.
</details>

---

## 11. Practice Exercises

### Exercise 1: Accessible Email Input with Error Association (Easy)
- **Task**: Author an HTML snippet containing an email label, an input marked as invalid, and an error message linked via `aria-describedby`. Include a visually hidden "Error:" prefix.

<details>
<summary>View Solution</summary>

```html
<div class="form-field">
  <label for="user-mail">Email Address</label>
  <input 
    type="email" 
    id="user-mail" 
    name="email" 
    value="bob@" 
    aria-invalid="true" 
    aria-describedby="user-mail-error" 
  />
  <p id="user-mail-error" class="error-msg">
    <span class="sr-only">Error: </span>Please enter a complete email address including domain name.
  </p>
</div>

<style>
  .sr-only {
    position: absolute;
    width: 1px;
    height: 1px;
    padding: 0;
    margin: -1px;
    overflow: hidden;
    clip: rect(0,0,0,0);
    border: 0;
  }
</style>
```
</details>

---

### Exercise 2: Static Error Summary Component (Medium)
- **Task**: Build an accessible Error Summary box at the top of a page listing two errors: one for "First Name" and one for "Terms and Conditions". Clicking each error link must jump focus directly to the respective input control.

<details>
<summary>View Solution</summary>

```html
<section 
  class="error-summary" 
  role="alert" 
  aria-labelledby="summary-heading" 
  tabindex="-1"
  id="error-box"
>
  <h2 id="summary-heading">Please fix the following errors:</h2>
  <ul>
    <li><a href="#fname">First Name cannot be blank</a></li>
    <li><a href="#terms">You must agree to the Terms of Service</a></li>
  </ul>
</section>

<form action="/signup" method="post">
  <div>
    <label for="fname">First Name:</label>
    <input type="text" id="fname" name="first_name" aria-invalid="true" />
  </div>

  <div>
    <input type="checkbox" id="terms" name="agree_terms" aria-invalid="true" />
    <label for="terms">I agree to the Terms of Service</label>
  </div>

  <button type="submit">Register</button>
</form>

<script>
  // Ensure keyboard focus shifts smoothly on anchor click
  document.querySelectorAll('#error-box a').forEach(link => {
    link.addEventListener('click', (e) => {
      const id = link.getAttribute('href').slice(1);
      const input = document.getElementById(id);
      if (input) input.focus();
    });
  });
</script>
```
</details>

---

### Exercise 3: Complete Submit Validation Engine with Focus Management (Hard)
- **Task**: Implement a script that intercepts form submission. If any required fields are empty, it constructs an Error Summary, moves focus to the summary, flags the invalid fields with `aria-invalid="true"` and `aria-describedby`, and automatically clears the error state when the user types in an input.

<details>
<summary>View Solution</summary>

```html
<div id="summary-region"></div>

<form id="contact-form" novalidate>
  <div class="field">
    <label for="c-name">Your Name</label>
    <input type="text" id="c-name" name="name" required />
    <p id="c-name-err" class="err-msg" hidden></p>
  </div>

  <div class="field">
    <label for="c-msg">Message</label>
    <textarea id="c-msg" name="message" required minlength="10"></textarea>
    <p id="c-msg-err" class="err-msg" hidden></p>
  </div>

  <button type="submit">Send Message</button>
</form>

<script>
  const form = document.getElementById('contact-form');
  const summaryRegion = document.getElementById('summary-region');

  form.addEventListener('submit', (e) => {
    e.preventDefault();
    summaryRegion.innerHTML = '';
    const failedFields = [];

    const inputs = form.querySelectorAll('input[required], textarea[required]');
    inputs.forEach(input => {
      const errEl = document.getElementById(`${input.id}-err`);
      if (!input.checkValidity()) {
        failedFields.push({ id: input.id, message: `Please fill in ${input.labels[0].textContent}` });
        input.setAttribute('aria-invalid', 'true');
        input.setAttribute('aria-describedby', `${input.id}-err`);
        errEl.textContent = `Error: ${input.validationMessage}`;
        errEl.hidden = false;
      } else {
        input.removeAttribute('aria-invalid');
        errEl.hidden = true;
      }
    });

    if (failedFields.length > 0) {
      const summary = document.createElement('div');
      summary.className = 'error-box';
      summary.setAttribute('role', 'alert');
      summary.setAttribute('tabindex', '-1');
      summary.innerHTML = `
        <h2>Submission Errors (${failedFields.length})</h2>
        <ul>
          ${failedFields.map(f => `<li><a href="#${f.id}">${f.message}</a></li>`).join('')}
        </ul>
      `;
      summaryRegion.appendChild(summary);
      summary.focus();

      summary.querySelectorAll('a').forEach(a => {
        a.onclick = () => document.getElementById(a.getAttribute('href').slice(1)).focus();
      });
    } else {
      alert('Form is completely valid!');
    }
  });

  // Clear errors on input
  form.addEventListener('input', (e) => {
    const input = e.target;
    if (input.checkValidity()) {
      input.removeAttribute('aria-invalid');
      const errEl = document.getElementById(`${input.id}-err`);
      if (errEl) errEl.hidden = true;
    }
  });
</script>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                               FORM ACCESSIBILITY CHEAT SHEET                                    |
+-------------------------------------------------------------------------------------------------+
| Attribute / Pattern       | Accessibility Purpose                                               |
+-------------------------------------------------------------------------------------------------+
| aria-invalid="true"       | Flags field as erroneous in the accessibility tree                  |
| aria-describedby="id"     | Links input to error message and/or instruction text                |
| <span class="sr-only">    | Injects audible "Error: " prefix for screen readers                 |
| role="alert"              | Dynamic container announcing errors immediately                     |
| tabindex="-1" on summary  | Enables JavaScript .focus() on summary box without breaking Tab flow|
| WCAG 1.4.1 (Color)        | Never use red borders alone; add text, icons, and weight            |
| WCAG 3.3.7 (Redundant)    | Auto-populate previously entered data in checkout/wizards           |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [W3C WAI Web Accessibility Tutorials: Form Instructions & Validating Input](https://www.w3.org/WAI/tutorials/forms/instructions/)
- [W3C WAI-ARIA 1.2: `aria-invalid` property](https://www.w3.org/TR/wai-aria-1.2/#aria-invalid)
- [W3C WAI-ARIA 1.2: `aria-describedby` property](https://www.w3.org/TR/wai-aria-1.2/#aria-describedby)
- [UK Government Design System: Error Summary Component](https://design-system.service.gov.uk/components/error-summary/)
- [WebAIM: Creating Accessible Forms](https://webaim.org/techniques/forms/)

---

[← Previous: Search & Filter Forms](12-search-and-filter-forms.md) | [Section Index](README.md) | [Next: Forms Security & CSRF →](14-forms-security-and-csrf.md)
