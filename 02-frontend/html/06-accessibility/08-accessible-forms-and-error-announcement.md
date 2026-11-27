# Accessible Forms & Error Announcement

| Metadata | Details |
|:---|:---|
| **Level** | Intermediate to Advanced |
| **Prerequisites** | `03-forms/13-forms-accessibility-and-error-states.md`, `06-accessibility/04-accessible-names-and-the-a11y-tree.md` |
| **Estimated Reading Time** | 31 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (HTML5 Forms & WAI-ARIA 1.2 Specs) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA 2024.1, JAWS 2024, VoiceOver, axe-core 4.8 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
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

### What are Accessible Forms?
An accessible form is an interactive data collection interface designed so that anyone—regardless of visual acuity, physical motor control, cognitive capability, or assistive device—can successfully understand what data is requested, navigate through inputs without traps, enter information efficiently, identify validation errors, and submit data with confidence.

### The High Stakes of Form Accessibility
Forms are the operational engine of the web: checkout funnels, job applications, banking transfers, medical records, tax filings, and authentication portals all depend on forms. When a form is inaccessible:
- Visually impaired users cannot determine what inputs require.
- Motor-impaired users experience broken tab orders and missed click targets.
- Users with cognitive disabilities get trapped by cryptic error messages and lost form state.
- Businesses suffer high cart abandonment and severe legal exposure under ADA Title III and the European Accessibility Act (EAA).

Under **WCAG 2.2**, forms are heavily regulated across multiple criteria:
- **3.3.1 Error Identification (Level A)**: Errors must be identified and described in text.
- **3.3.2 Labels or Instructions (Level A)**: Clear labels or instructions must be provided.
- **3.3.3 Error Suggestion (Level AA)**: Corrective guidance must be provided for errors.
- **3.3.4 Error Prevention (Legal/Financial) (Level AA)**: Submissions must be reversible, checked, or confirmed.
- **3.3.7 Redundant Entry (Level A)**: Previously entered data must be auto-populated.
- **3.3.8 Accessible Authentication (Level AA)**: No cognitive memorization puzzles without copy-paste or password managers.

---

## 2. Core Concepts

### Programmatic Label Association
Form controls (`<input>`, `<select>`, `<textarea>`) must have an unambiguously linked `<label>`. 

```html
<!-- Explicit Association (Industry Best Practice) -->
<label for="user-email">Corporate Email Address</label>
<input type="email" id="user-email" name="email" required>
```
When `<label for="id">` is used:
1. Clicking the visible label automatically shifts keyboard focus to the input and toggles checkboxes.
2. Sighted mobile users gain a larger touch target.
3. Screen readers announce the label as the control's Accessible Name upon focus.

### The `<fieldset>` and `<legend>` Grouping Contract
When individual form controls are conceptually related—such as radio buttons, multi-select checkboxes, or date fields (Day / Month / Year)—they **must be grouped inside a `<fieldset>` with a `<legend>`**:

```html
<fieldset>
  <legend>Preferred Contact Modality</legend>
  
  <input type="radio" id="contact-email" name="modality" value="email">
  <label for="contact-email">Email</label>

  <input type="radio" id="contact-sms" name="modality" value="sms">
  <label for="contact-sms">SMS Text</label>
</fieldset>
```
Screen readers automatically announce the `<legend>` whenever focus enters any radio button inside the group (*"Preferred Contact Modality, group. Email, radio button, 1 of 2"*), providing vital context that would otherwise be missing.

### Error Association Architecture
To communicate validation errors to screen readers, three components must work together:
1. **`aria-invalid="true"`**: Flags the input as failing validation.
2. **`aria-describedby`**: Associates the input with the unique ID of the error message container (and any helper text).
3. **The GDS (GOV.UK) Error Summary**: A focused alert container at the top of the form listing all failures with direct in-page anchor links to the offending inputs.

---

## 3. How It Works Internally

### Screen Reader Focus Event on an Invalid Input
When a screen reader user tabs into an input with validation errors, the browser's Accessibility Object Model (AOM) assembles a rich, multi-part auditory notification:

```
User tabs to: <input id="cc" aria-invalid="true" aria-describedby="cc-hint cc-err">
                              │
                              ▼
            Accessibility Engine (AOM) resolves node:
                              │
  ├─► 1. Role:         "Edit text" (or "Textbox")
  ├─► 2. Name:         Resolves from <label for="cc">: "Credit Card Number"
  ├─► 3. State:        Evaluates aria-invalid="true": "Invalid entry"
  ├─► 4. Required:     Evaluates required attribute: "Required"
  └─► 5. Description:  Resolves aria-describedby ("cc-hint cc-err"):
                       "Must be 16 digits. Error: Card number cannot be blank."
                              │
                              ▼
               Speech Synthesis Output to User:
  "Credit Card Number, edit text, invalid entry, required.
   Must be 16 digits. Error: Card number cannot be blank."
```

### The Error Summary Focus Shift Pattern
When a form submission fails validation:
1. Prevent default form submission (`event.preventDefault()`).
2. Generate an **Error Summary** container at the top of the form.
3. Set `tabindex="-1"` on the Error Summary container.
4. Call `errorSummary.focus()` via JavaScript.
5. The screen reader immediately announces the summary heading (*"There is a problem, heading 2"*), and the keyboard user can press `Tab` to navigate through links that jump focus straight to each broken field.

---

## 4. Syntax & Usage

### Core Form A11y Attributes Reference

```
+--------------------------------------------------------------------------+
| ATTRIBUTE          | ALLOWED VALUES                 | PURPOSE            |
+--------------------+--------------------------------+--------------------+
| aria-invalid       | true, false, grammar, spelling | Indicates error    |
| aria-describedby   | Space-separated list of IDs    | Links hint & errors|
| aria-errormessage  | Single ID referencing error    | Specific error link|
| autocomplete       | email, tel, address-line1, ... | Prevents re-typing |
| inputmode          | numeric, decimal, email, tel   | Mobile keyboard    |
| required           | Boolean attribute              | Mandatory field    |
+--------------------------------------------------------------------------+
```

---

## 5. Practical Examples

### Example 1: Basic — Proper Label and Helper Text Association
Demonstrating how `aria-describedby` links instructions without breaking the primary accessible name:

```html
<div class="form-field">
  <label for="user-password">Create Password</label>
  <input 
    type="password" 
    id="user-password" 
    name="password"
    autocomplete="new-password"
    required
    aria-describedby="password-rules"
  >
  <p id="password-rules" class="hint">
    Must be at least 12 characters long and include a number.
  </p>
</div>
```

### Example 2: Real-World — Complete Production Form with GDS Error Summary
The globally recognized gold standard for accessible form validation (pioneered by the UK Government Digital Service):

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Account Registration</title>
  <style>
    .error-summary {
      border: 4px solid #d4351c;
      padding: 16px;
      margin-bottom: 24px;
    }
    .error-message {
      color: #d4351c;
      font-weight: bold;
      display: block;
      margin-top: 4px;
    }
    input[aria-invalid="true"] {
      border: 3px solid #d4351c;
    }
  </style>
</head>
<body>
  <main>
    <!-- Error Summary Container (Hidden until validation fails) -->
    <div 
      id="error-summary" 
      class="error-summary" 
      tabindex="-1" 
      role="alert" 
      aria-labelledby="summary-heading"
      hidden
    >
      <h2 id="summary-heading">There is a problem</h2>
      <ul id="error-list">
        <!-- In-page jump links populated dynamically via JavaScript -->
      </ul>
    </div>

    <h1>Register for Developer Cloud</h1>

    <form id="reg-form" action="/register" method="POST" novalidate>
      <div class="form-group">
        <label for="full-name">Full name</label>
        <span id="name-error" class="error-message" hidden>Error: Enter your full name</span>
        <input 
          type="text" 
          id="full-name" 
          name="name" 
          autocomplete="name" 
          required
        >
      </div>

      <div class="form-group" style="margin-top: 16px;">
        <label for="work-email">Work email address</label>
        <span id="email-error" class="error-message" hidden>Error: Enter a valid corporate email address</span>
        <input 
          type="email" 
          id="work-email" 
          name="email" 
          autocomplete="email" 
          required
        >
      </div>

      <button type="submit" style="margin-top: 24px;">Create Account</button>
    </form>
  </main>

  <script>
    const form = document.getElementById('reg-form');
    const summary = document.getElementById('error-summary');
    const errorList = document.getElementById('error-list');

    form.addEventListener('submit', (e) => {
      e.preventDefault();
      errorList.innerHTML = '';
      let errors = [];

      const nameInput = document.getElementById('full-name');
      const nameError = document.getElementById('name-error');
      if (!nameInput.value.trim()) {
        nameInput.setAttribute('aria-invalid', 'true');
        nameInput.setAttribute('aria-describedby', 'name-error');
        nameError.hidden = false;
        errors.push({ id: 'full-name', msg: 'Enter your full name' });
      } else {
        nameInput.removeAttribute('aria-invalid');
        nameInput.removeAttribute('aria-describedby');
        nameError.hidden = true;
      }

      const emailInput = document.getElementById('work-email');
      const emailError = document.getElementById('email-error');
      if (!emailInput.value.includes('@')) {
        emailInput.setAttribute('aria-invalid', 'true');
        emailInput.setAttribute('aria-describedby', 'email-error');
        emailError.hidden = false;
        errors.push({ id: 'work-email', msg: 'Enter a valid corporate email address' });
      } else {
        emailInput.removeAttribute('aria-invalid');
        emailInput.removeAttribute('aria-describedby');
        emailError.hidden = true;
      }

      if (errors.length > 0) {
        errors.forEach(err => {
          const li = document.createElement('li');
          const a = document.createElement('a');
          a.href = `#${err.id}`;
          a.textContent = err.msg;
          li.appendChild(a);
          errorList.appendChild(li);
        });
        summary.hidden = false;
        summary.focus(); // Shift focus to error banner!
      } else {
        summary.hidden = true;
        alert('Form submitted successfully!');
      }
    });
  </script>
</body>
</html>
```

### Example 3: Edge Case — Multi-Input Date Group Pattern
Splitting birthdate into three inputs requires a `<fieldset>` and `<legend>` so users understand what each box represents:

```html
<fieldset role="group" aria-describedby="dob-hint">
  <legend>Date of Birth</legend>
  <p id="dob-hint" class="hint">For example, 27 03 1994</p>

  <div class="date-inputs" style="display: flex; gap: 8px;">
    <div>
      <label for="dob-day">Day</label>
      <input type="text" id="dob-day" name="dob_day" inputmode="numeric" maxlength="2" style="width: 40px;">
    </div>
    <div>
      <label for="dob-month">Month</label>
      <input type="text" id="dob-month" name="dob_month" inputmode="numeric" maxlength="2" style="width: 40px;">
    </div>
    <div>
      <label for="dob-year">Year</label>
      <input type="text" id="dob-year" name="dob_year" inputmode="numeric" maxlength="4" style="width: 60px;">
    </div>
  </div>
</fieldset>
```

### Example 4: Failure Case — Broken Form Accessibility Anti-Patterns
Common errors that completely derail assistive technology users:

```html
<!-- BROKEN: Placeholder used as sole label (Disappears on typing, poor contrast) -->
<input type="text" placeholder="First Name">

<!-- BROKEN: Unassociated label (Missing for="id" attribute) -->
<label>Phone Number</label>
<input type="tel">

<!-- BROKEN: Radio buttons without <fieldset> or <legend> -->
<!-- Screen reader speaks: "Small, radio button", "Medium, radio button",
     with NO indication that these refer to Shirt Size! -->
<input type="radio" id="s" name="sz"><label for="s">Small</label>
<input type="radio" id="m" name="sz"><label for="m">Medium</label>

<!-- BROKEN: Color-only error state (Fails WCAG 1.4.1 Use of Color) -->
<!-- Input turns red, but no text or aria-invalid is provided -->
<input type="text" style="border: 2px solid red;">
```

### Example 5: Accessible Production-Grade — WCAG 2.2 Redundant Entry (3.3.7) Form
Eliminating redundant typing by re-using shipping information for billing:

```html
<fieldset>
  <legend>Billing Details</legend>
  
  <div class="checkbox-option">
    <input type="checkbox" id="copy-shipping" onchange="syncAddresses(this)">
    <label for="copy-shipping">Same as shipping address</label>
  </div>

  <div id="billing-inputs">
    <div class="field">
      <label for="bill-street">Street address</label>
      <input type="text" id="bill-street" name="billing_street" autocomplete="billing street-address">
    </div>
    <div class="field">
      <label for="bill-city">City</label>
      <input type="text" id="bill-city" name="billing_city" autocomplete="billing address-level2">
    </div>
  </div>
</fieldset>

<script>
  function syncAddresses(checkbox) {
    const billingGroup = document.getElementById('billing-inputs');
    if (checkbox.checked) {
      document.getElementById('bill-street').value = document.getElementById('ship-street').value;
      document.getElementById('bill-city').value = document.getElementById('ship-city').value;
      billingGroup.hidden = true;
    } else {
      billingGroup.hidden = false;
    }
  }
</script>
```

---

## 6. Best Practices

1. **Explicit Labels for Every Field**: Always pair `<label for="id">` with `<input id="id">`.
2. **Always Group Radios in `<fieldset>` + `<legend>`**: Guarantees context is announced when focusing each radio option.
3. **Use the GDS Error Summary Pattern**: Place an error summary at the top of the form, move focus to it upon failed submit, and link to each broken input.
4. **Never Rely on Placeholders as Labels**: Placeholders disappear when text is typed and suffer from poor visual contrast.
5. **Always Set `aria-invalid="true"` on Broken Inputs**: Communicates the invalid state directly into the Accessibility Tree.
6. **Include `autocomplete` Attributes**: Supports password managers and complies with WCAG 1.3.5 and 3.3.8.
7. **Never Disable Pasting on Password Fields**: Forbidding paste violates WCAG 3.3.8 and breaks password manager integration.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | WCAG Success Criterion | Correction |
|:---|:---|:---|:---|
| Using placeholder instead of `<label>` | Critical | 3.3.2 Labels or Instructions (A) | Provide visible, permanent `<label for="id">` |
| Failing to shift focus to errors on submit failure | High | 3.3.1 Error Identification (A) | Move focus to error summary or first invalid field |
| Indicating errors solely with red border colors | High | 1.4.1 Use of Color (A) | Add visible text error message + `aria-invalid="true"` |
| Ungrouped radio buttons or checkboxes | High | 1.3.1 Info and Relationships (A) | Wrap in `<fieldset>` with descriptive `<legend>` |
| Disabling paste on passwords or credit cards | High | 3.3.8 Accessible Authentication (AA) | Remove `onpaste="return false"` event handlers |
| Error text present but not linked via `aria-describedby` | Medium | 1.3.1 Info and Relationships (A) | Add `aria-describedby="error-id"` to the input |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Cognitive Load & Form Conversions
Complex, inaccessible forms directly degrade conversion rates. Adding standard HTML `autocomplete` tokens allows browsers to autofill user credentials, addresses, and credit cards in a single click, reducing average form completion time by over 60% and eliminating user typographical errors.

### SEO Implications
Search engine crawlers evaluate form accessibility as a proxy for site quality. Valid semantic forms with clear labels, proper landmark nesting, and logical reading orders reduce bounce rates and improve overall search index quality.

### Security: Client-Side Validation is Not Security
Native HTML constraint validation (`required`, `pattern`) and ARIA error states (`aria-invalid`) are **user-experience enhancements**, not security controls. Attackers can bypass client-side validation using curl or browser DevTools in seconds. Always re-validate all submitted form fields on the backend server.

---

## 9. Comparison / When to Use What

### Labeling Approaches

| Method | Visible? | Extends Click Target? | Screen Reader Support | Best Used For |
|:---|:---:|:---:|:---:|:---|
| **`<label for="id">`** | Yes | Yes | 100% (Universal) | All standard form fields |
| **`<fieldset>` + `<legend>`** | Yes | Yes (Inside boundary) | 100% (Universal) | Radio groups, date sets, address groups |
| **`aria-label`** | No | No | 100% | Search bar with search icon submit button |
| **`placeholder`** | Temporary | No | Poor / Flaky | Supplementary example format (Never label!) |
| **`title` attribute** | Hover only | No | Weak / Ignored | Optional desktop advisory hints |

---

## 10. Interview Questions

### Q1: What are the three essential components required to make form validation errors accessible to screen reader users?
<details>
<summary>View Answer</summary>

1. **`aria-invalid="true"`**: Set on the invalid `<input>` so the screen reader immediately speaks *"Invalid entry"* upon focus.
2. **`aria-describedby`**: Set on the input referencing the `id` of the text error message, ensuring the exact error text is spoken aloud following the input's name.
3. **Focus Management (Error Summary)**: On failed form submission, focus must be shifted programmatically to an error summary container at the top of the form (or directly to the first invalid field), alerting the user that the submission did not succeed.
</details>

### Q2: Why is wrapping radio buttons inside a `<fieldset>` with a `<legend>` mandatory for accessibility?
<details>
<summary>View Answer</summary>

Without a `<fieldset>` and `<legend>`, individual radio buttons only have their individual labels (e.g., "Small", "Medium", "Large"). 

A screen reader user tabbing through the form hears *"Small, radio button"*, but has no idea what "Small" refers to (Is it shirt size? Pizza size? Storage capacity?). The `<legend>` provides the shared group context (*"T-Shirt Size, group. Small, radio button"*), satisfying **WCAG 1.3.1 (Info and Relationships)**.
</details>

### Q3: Why is relying on the `placeholder` attribute as a replacement for `<label>` considered an accessibility failure?
<details>
<summary>View Answer</summary>

1. **Vanishing Text**: As soon as a user starts typing, the placeholder text disappears. Users with memory or cognitive impairments lose track of what information was requested.
2. **Color Contrast Failure**: Default browser placeholder text is rendered in light grey, consistently failing the 4.5:1 contrast requirement of **WCAG 1.4.3**.
3. **No Click Target Expansion**: Sighted users cannot click the placeholder to focus the field (unlike a `<label>`).
4. **Screen Reader Flakiness**: Some assistive technologies ignore placeholders when calculating the Accessible Name.
</details>

### Q4: Explain the GDS (Government Digital Service) Error Summary pattern and how it works.
<details>
<summary>View Answer</summary>

The GDS Error Summary is a high-visibility container placed at the top of a page when form submission fails:
1. It contains `role="alert"` and `tabindex="-1"`.
2. It has an `<h2>` heading (e.g., *"There is a problem"*).
3. It contains an ordered or unordered list of links, where each link text describes an error and its `href` points to the corresponding broken input (`<a href="#email">Enter a valid email</a>`).
4. On submit failure, JavaScript calls `.focus()` on the summary container.
5. Clicking any error link jumps keyboard focus directly to the offending input.
</details>

### Q5: What does WCAG 2.2 Success Criterion 3.3.7 (Redundant Entry) require of multi-step forms?
<details>
<summary>View Answer</summary>

WCAG 3.3.7 (Level A) mandates that information previously entered by or provided to the user within the same process must be either:
1. Auto-populated (e.g., automatically populating billing address with shipping address).
2. Available for the user to select (e.g., a dropdown or checkbox saying *"Same as shipping address"*).

This eliminates unnecessary cognitive and motor effort, particularly for users with mobility impairments or memory challenges.
</details>

### Q6: Why does disabling paste (`onpaste="return false"`) on password fields violate WCAG 2.2?
<details>
<summary>View Answer</summary>

Disabling paste directly violates **WCAG 3.3.8 (Accessible Authentication - Level AA)**. 
Users who cannot memorize complex, high-entropy passwords rely on password managers (such as 1Password, Bitwarden, or browser keychains) to generate and paste credentials. Forbidding paste forces users to manually transcribe 30-character strings or fall back to weak, easily guessed passwords.
</details>

### Q7: What is the difference between `aria-describedby` and `aria-errormessage`?
<details>
<summary>View Answer</summary>

- `aria-describedby`: Widely supported across all browser engines and assistive technologies. Can point to multiple element IDs separated by spaces to read helper text and error messages.
- `aria-errormessage`: An ARIA 1.1+ attribute intended specifically to reference error messages when `aria-invalid="true"`. While conceptually cleaner, assistive technology support for `aria-errormessage` remains inconsistent in older screen readers compared to `aria-describedby`. Industry best practice is to use `aria-describedby` (or both together).
</details>

### Q8: What is the purpose of `inputmode="numeric"` on an `<input type="text">`?
<details>
<summary>View Answer</summary>

`inputmode="numeric"` instructs mobile operating systems (iOS and Android) to display a numeric keypad (0-9) instead of the full QWERTY keyboard. 

Unlike `<input type="number">`, which has built-in stepper arrows, scrolls values on mousewheel, and strips leading zeros, using `type="text" inputmode="numeric"` is the recommended accessible pattern for credit card numbers, ZIP codes, and phone verification codes.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Fixing an Unlabeled Form Input
**Problem**: Fix the following input so it has an explicit accessible label:
`<input type="text" id="username" placeholder="Username">`

<details>
<summary>View Solution</summary>

```html
<label for="username">Username</label>
<input type="text" id="username" name="username" autocomplete="username">
```
</details>

### Exercise 2: Easy — Grouping Checkboxes with Semantic HTML
**Problem**: Write the semantic HTML to group two newsletter subscription checkboxes under the heading "Email Subscriptions".

<details>
<summary>View Solution</summary>

```html
<fieldset>
  <legend>Email Subscriptions</legend>
  
  <div>
    <input type="checkbox" id="sub-weekly" name="sub" value="weekly">
    <label for="sub-weekly">Weekly Engineering Digest</label>
  </div>

  <div>
    <input type="checkbox" id="sub-alerts" name="sub" value="alerts">
    <label for="sub-alerts">Security & Vulnerability Alerts</label>
  </div>
</fieldset>
```
</details>

### Exercise 3: Medium — Linking an Error Message to an Input
**Problem**: An email field failed validation. Add the necessary ARIA attributes to mark the field invalid and link it to both the hint and the error message.

```html
<label for="email">Work Email</label>
<input type="email" id="email">
<span id="hint-1">We will never share your email.</span>
<span id="err-1">Please enter a corporate email address ending in .com</span>
```

<details>
<summary>View Solution</summary>

```html
<label for="email">Work Email</label>
<input 
  type="email" 
  id="email" 
  aria-invalid="true" 
  aria-describedby="hint-1 err-1"
>
<span id="hint-1">We will never share your email.</span>
<span id="err-1" role="alert">Please enter a corporate email address ending in .com</span>
```
</details>

### Exercise 4: Medium — Implementing Accessible Credit Card Input
**Problem**: Write the HTML for a credit card number input that displays a numeric keypad on mobile devices, supports autocomplete, and links to an explanatory format hint.

<details>
<summary>View Solution</summary>

```html
<label for="cc-num">Credit Card Number</label>
<input 
  type="text" 
  id="cc-num" 
  name="cardnumber"
  inputmode="numeric" 
  pattern="[0-9]{13,19}" 
  autocomplete="cc-number" 
  maxlength="19"
  aria-describedby="cc-hint"
  required
>
<span id="cc-hint" class="hint">16 digits without spaces or dashes</span>
```
</details>

### Exercise 5: Hard — Authoring the Error Summary Container
**Problem**: Write the HTML and CSS for an accessible Error Summary banner that can be focused programmatically, styled with high contrast, and announces itself as an alert to screen readers.

<details>
<summary>View Solution</summary>

```html
<div 
  id="error-summary" 
  class="error-summary-box" 
  tabindex="-1" 
  role="alert" 
  aria-labelledby="summary-title"
>
  <h2 id="summary-title">There is a problem with your submission</h2>
  <ul>
    <li><a href="#first-name">Enter your first name</a></li>
    <li><a href="#password">Password must be at least 12 characters</a></li>
  </ul>
</div>

<style>
  .error-summary-box {
    border: 4px solid #d4351c;
    padding: 20px;
    margin-bottom: 30px;
    background-color: #fff;
  }
  .error-summary-box:focus {
    outline: 3px solid #ffbf47;
    outline-offset: 2px;
  }
  .error-summary-box h2 {
    color: #d4351c;
    margin-top: 0;
  }
  .error-summary-box a {
    color: #d4351c;
    font-weight: bold;
    text-decoration: underline;
  }
</style>
```
</details>

### Exercise 6: Hard — Automated Focus Shifting on Form Submit Failure
**Problem**: Write a vanilla JavaScript submit handler that intercepts a form submission, inspects all `required` fields, and if any are empty, shifts focus to the Error Summary banner. If no Error Summary exists, it shifts focus directly to the first invalid field.

<details>
<summary>View Solution</summary>

```javascript
document.getElementById('checkout-form').addEventListener('submit', function(e) {
  const invalidFields = this.querySelectorAll(':invalid');
  
  if (invalidFields.length > 0) {
    e.preventDefault();
    
    // Flag invalid attributes for assistive technologies
    invalidFields.forEach(field => {
      field.setAttribute('aria-invalid', 'true');
    });

    const errorSummary = document.getElementById('error-summary');
    if (errorSummary) {
      errorSummary.hidden = false;
      errorSummary.focus();
    } else {
      // Fallback: Focus the first broken input directly
      invalidFields[0].focus();
    }
  }
});
```
</details>

---

## 12. Summary / Cheat Sheet

- **Explicit Labels**: Always pair `<label for="id">` with `<input id="id">`.
- **Grouping**: Wrap related radio buttons or checkboxes in `<fieldset>` + `<legend>`.
- **Error Identification**: Set `aria-invalid="true"` on invalid inputs.
- **Error Description**: Use `aria-describedby="hint-id error-id"` to connect guidance text.
- **Focus Management**: Move focus to an Error Summary banner or first invalid field upon failed submit.
- **No Placeholder Labels**: Placeholders disappear on input and fail contrast.
- **No Paste Blocking**: Disabling paste on passwords violates WCAG 3.3.8 (Accessible Authentication).
- **Redundant Entry (WCAG 3.3.7)**: Provide auto-population or "Same as shipping" options in multi-step forms.

---

## 13. Further Reading

- [GOV.UK Design System: Error Summary Component](https://design-system.service.gov.uk/components/error-summary/)
- [W3C WAI: Form Instructions and Validation Tutorials](https://www.w3.org/WAI/tutorials/forms/)
- [WebAIM: Creating Accessible Forms](https://webaim.org/techniques/forms/)
- [MDN Web Docs: How to structure a web form](https://developer.mozilla.org/en-US/docs/Learn/Forms/How_to_structure_a_web_form)

---

[Previous: 07 - Alt Text Decision Tree (07-alt-text-decision-tree.md)](07-alt-text-decision-tree.md) | [Section Index](README.md) | [Next: 09 - Accessible Tables & Lists ->](09-accessible-tables-and-lists.md)
