# Content Security Policy (CSP) in Meta: Directives, Limits, and XSS Defense

| Metadata | Details |
| :--- | :--- |
| **Specification** | W3C Content Security Policy Level 3 / WHATWG HTML Living Standard |
| **Prerequisites** | `05-head-metadata-and-seo/01-head-anatomy.md`, `03-forms/14-forms-security-and-csrf.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`<meta http-equiv="Content-Security-Policy">`) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | Google CSP Evaluator, OWASP ZAP, W3C CSP Validator |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: The Canonical Static Site Strict CSP Meta Tag](#example-1-the-canonical-static-site-strict-csp-meta-tag)
   - [Example 2: Hash-Based Inline Script Authorization](#example-2-hash-based-inline-script-authorization)
   - [Example 3: Form Action and Destination Lockdown (`form-action`)](#example-3-form-action-and-destination-lockdown-form-action)
   - [Example 4: Demonstrating Directives Forbidden in Meta (`frame-ancestors`)](#example-4-demonstrating-directives-forbidden-in-meta-frame-ancestors)
   - [Example 5: DevTools CSP Violation Debugging and Enforcement](#example-5-devtools-csp-violation-debugging-and-enforcement)
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

Cross-Site Scripting (XSS) remains one of the most destructive and prevalent vulnerabilities in web development. If an attacker injects malicious JavaScript into your document (via an unescaped comment, query string, or compromised dependency), that script can steal session cookies, access `localStorage` tokens, log keystrokes, and make authenticated financial API requests on behalf of the victim.

**Content Security Policy (CSP)** is an HTTP and HTML security standard designed specifically to neutralize XSS, data exfiltration, and clickjacking attacks. 

While CSP is traditionally delivered via HTTP response headers (`Content-Security-Policy: ...`), modern web applications—particularly static single-page apps (SPAs), JAMstack sites hosted on GitHub Pages or S3/CloudFront, and local progressive web apps—often lack server-side header modification capabilities. 

The HTML `<meta http-equiv="Content-Security-Policy">` element provides **client-side CSP enforcement** directly within HTML `<head>`. Understanding which directives work in `<meta>` and which are strictly ignored by browsers is essential for deploying defense-in-depth web architectures.

---

## 2. Core Concepts

### What Works in `<meta>` vs. What Is Ignored
The W3C specification strictly differentiates directives that can be defined inside an HTML `<meta>` tag versus those that **require true HTTP headers**:

| CSP Directive | Supported in `<meta>`? | Why / Limitation |
| :--- | :--- | :--- |
| **`default-src`** | **YES** | Sets fallback policy for all resource types. |
| **`script-src`** | **YES** | Controls allowed JavaScript execution sources. |
| **`style-src`** | **YES** | Controls allowed CSS stylesheet sources. |
| **`img-src` / `font-src`** | **YES** | Whitelists image and web font endpoints. |
| **`connect-src`** | **YES** | Restricts `fetch()`, `XMLHttpRequest`, and WebSockets. |
| **`object-src`** | **YES** | Governs `<object>` and `<embed>` plugins. |
| **`form-action`** | **YES** | Restricts valid `<form action="...">` submission targets. |
| **`frame-ancestors`** | **NO (IGNORED)** | **Must be an HTTP header.** Browser must check embedding permission *before* parsing the HTML body! |
| **`report-uri` / `report-to`** | **NO (IGNORED)** | Reporting endpoint configuration is forbidden in `<meta>`. |
| **`sandbox`** | **NO (IGNORED)** | Cannot sandbox the document itself via `<meta>`. |

### The Core Directives of a Strict CSP
- **`default-src 'self'`**: Only resources from the exact same origin are allowed by default.
- **`object-src 'none'`**: Completely blocks legacy Flash/Java plugins and arbitrary object injection.
- **`base-uri 'self'`**: Prevents attackers from injecting a malicious `<base href="...">` to hijack relative URLs.
- **No `'unsafe-inline'` or `'unsafe-eval'`**: Disables inline `<script>` tags, inline `onclick` handlers, and `eval()` execution.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                        Browser Engine CSP Meta Enforcement Flow                         |
+-----------------------------------------------------------------------------------------+

  HTML Parser enters <head>
       |
       v
  Encounters <meta http-equiv="Content-Security-Policy" content="...">
       |
       +--> Security Context instantiated in Document execution environment!
       |    - Compiles policy whitelist and source hashes.
       |    - Filters out forbidden directives (e.g. ignores frame-ancestors).
       |
  Parser encounters: <script src="https://untrusted-cdn.com/malware.js">
       |
       v
  Security Engine checks `script-src`:
  - Does "https://untrusted-cdn.com" match the CSP policy?
       |
       +--> NO:
       |      1. Network request is immediately ABORTED!
       |      2. Error logged to Console: "Refused to load the script because it violates..."
       |      3. Zero malicious code executes!
```

---

## 4. Syntax & Usage

```html
<head>
  <meta charset="utf-8" />

  <!-- 
    Strict CSP via HTML Meta:
    - default-src 'self': Everything defaults to same-origin.
    - script-src 'self' https://trusted-cdn.com: Scripts restricted to self and CDN.
    - object-src 'none': Disables all plugins.
    - base-uri 'self': Prevents <base> hijacking.
  -->
  <meta 
    http-equiv="Content-Security-Policy" 
    content="default-src 'self'; script-src 'self' https://trusted-cdn.com; style-src 'self' 'unsafe-inline'; object-src 'none'; base-uri 'self';" 
  />

  <title>Secured Web Application</title>
</head>
```

> **Cross-Browser & Baseline Status Box**
>
> - `<meta http-equiv="Content-Security-Policy">`: **Baseline Widely Available** across all major browsers since 2015.
> - Directives `frame-ancestors` and `report-uri`: By W3C specification, these are **completely ignored** when delivered in a `<meta>` tag; they require true HTTP headers.

---

## 5. Practical Examples

### Example 1: The Canonical Static Site Strict CSP Meta Tag
A robust, production-ready CSP configuration for a static web application hosted on GitHub Pages, S3, or Vercel.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />

  <!-- 
    STRICT CSP POLICY BREAKDOWN:
    - default-src 'self': Disallows third-party origins unless explicitly whitelisted
    - script-src 'self': Disallows inline scripts (<script>...) and unapproved domains
    - style-src 'self' https://fonts.googleapis.com: Allows self and Google Fonts CSS
    - font-src 'self' https://fonts.gstatic.com: Allows Google Fonts WOFF2 files
    - img-src 'self' data: https://images.unsplash.com: Allows self, base64, and Unsplash
    - connect-src 'self' https://api.mybackend.com: Allows Fetch/XHR to backend API only
    - object-src 'none': Completely disables <object> and <embed>
    - base-uri 'self': Blocks malicious <base> injection
    - form-action 'self': Forms can only submit to the origin
  -->
  <meta 
    http-equiv="Content-Security-Policy" 
    content="
      default-src 'self';
      script-src 'self';
      style-src 'self' https://fonts.googleapis.com;
      font-src 'self' https://fonts.gstatic.com;
      img-src 'self' data: https://images.unsplash.com;
      connect-src 'self' https://api.mybackend.com;
      object-src 'none';
      base-uri 'self';
      form-action 'self';
    " 
  />

  <title>Enterprise Security Portal</title>
  <link rel="stylesheet" href="/css/main.css" />
  <script defer src="/js/app.js"></script>
</head>
<body>
  <h1>Secure Dashboard</h1>
</body>
</html>
```

---

### Example 2: Hash-Based Inline Script Authorization
How to securely execute a specific inline initialization script without opening the door to arbitrary XSS via `'unsafe-inline'`.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />

  <!-- 
    SHA-256 HASH AUTHORIZATION:
    Instead of 'unsafe-inline', we calculate the SHA-256 hash of the exact script body:
    echo -n "window.ENV = 'production';" | openssl dgst -sha256 -binary | openssl base64
    Output: 'sha256-4YtY9k7fF7Dk6kF9sB2C...'
    
    The browser calculates the hash of the inline script and executes it ONLY
    if the hash matches! Any modified or injected script is instantly blocked.
  -->
  <meta 
    http-equiv="Content-Security-Policy" 
    content="default-src 'self'; script-src 'self' 'sha256-qznLcsROx4GACP2dm0UCK3NzsLUKKA8ISChmg/eiAlg='; object-src 'none';" 
  />

  <title>Hash-Secured Application</title>
</head>
<body>
  <!-- This exact inline script matches the hash in CSP and runs successfully -->
  <script>window.ENV = 'production';</script>

  <!-- Any attacker injection like this will be IMMEDIATELY BLOCKED: -->
  <!-- <script>stealSessionCookies();</script> -->
</body>
</html>
```

---

### Example 3: Form Action and Destination Lockdown (`form-action`)
Restricting `<form action="...">` targets directly in HTML meta to prevent form hijacking and credential exfiltration.

```html
<head>
  <meta 
    http-equiv="Content-Security-Policy" 
    content="default-src 'self'; form-action 'self' https://checkout.stripe.com; object-src 'none';" 
  />
</head>
<body>
  <!-- ALLOWED: Matches 'self' -->
  <form action="/login" method="post">
    <button type="submit">Log In</button>
  </form>

  <!-- ALLOWED: Explicitly whitelisted origin -->
  <form action="https://checkout.stripe.com/pay" method="post">
    <button type="submit">Pay with Stripe</button>
  </form>

  <!-- BLOCKED BY BROWSER: Form submission to unapproved origin is halted! -->
  <form action="https://evil-phishing-stealer.com/post" method="post">
    <button type="submit">Compromised Action</button>
  </form>
</body>
```

---

### Example 4: Demonstrating Directives Forbidden in Meta (`frame-ancestors`)
Demonstrating why Clickjacking defense via `frame-ancestors` CANNOT be configured inside `<meta>`.

```html
<!-- DANGEROUS SECURITY MISTAKE -->
<head>
  <!-- 
    FATAL BUG:
    frame-ancestors is completely IGNORED by all browsers inside a <meta> tag!
    The W3C CSP spec requires frame-ancestors to be delivered via HTTP response headers
    because the browser must decide whether to frame the document BEFORE parsing HTML!
  -->
  <!-- <meta http-equiv="Content-Security-Policy" content="frame-ancestors 'none';"> -->
</head>

<!-- 
  CORRECT CLICKJACKING DEFENSE:
  Must be delivered as an HTTP Response Header from the web server or CDN:
  Content-Security-Policy: frame-ancestors 'none';
  X-Frame-Options: DENY
-->
```

---

### Example 5: DevTools CSP Violation Debugging and Enforcement
Understanding browser console security warnings when CSP blocks unauthorized resources.

```html
<head>
  <meta 
    http-equiv="Content-Security-Policy" 
    content="default-src 'self'; script-src 'self'; style-src 'self';" 
  />
</head>
<body>
  <!-- VIOLATION 1: Inline style violates style-src 'self' (Needs 'unsafe-inline' or hash) -->
  <h1 style="color: red;">Welcome</h1>

  <!-- VIOLATION 2: Inline onclick handler violates script-src 'self' -->
  <button onclick="alert(1)">Click Me</button>

  <!-- 
    CONSOLE OUTPUT IN DEVTOOLS:
    [Report Only / Violation] Refused to apply inline style because it violates the following Content Security Policy directive: "style-src 'self'".
    [Report Only / Violation] Refused to execute inline event handler because it violates the following Content Security Policy directive: "script-src 'self'".
  -->
</body>
```

---

## 6. Best Practices

- [ ] **Always declare `<meta http-equiv="Content-Security-Policy">` as high as possible in `<head>`**: Policies only take effect for elements parsed *after* the meta tag appears in the document.
- [ ] **Always set `object-src 'none'`**: Disables obsolete binary plugin interfaces and prevents `<object>` injection vulnerabilities.
- [ ] **Always set `base-uri 'self'`**: Prevents attackers from using DOM injection to insert a `<base href="https://evil.com">` tag that redirects all relative script and form URLs.
- [ ] **Never use `'unsafe-inline'` for `script-src`**: Allowing inline scripts completely undermines the primary security benefit of CSP against XSS. Use external scripts or cryptographic hashes.
- [ ] **Deliver `frame-ancestors 'none'` via true HTTP response headers**: Never attempt to prevent Clickjacking using a `<meta>` tag; browsers strictly ignore `frame-ancestors` in HTML markup.
- [ ] **Explicitly whitelist `connect-src`**: Restrict `fetch()` and `XMLHttpRequest` endpoints to trusted API domains to prevent malicious data exfiltration.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Attempting to use `frame-ancestors` in `<meta>` | **Critical** | Completely ignored by browsers; application remains vulnerable to Clickjacking. | Send `frame-ancestors 'none'` as a real HTTP response header. |
| Attempting to use `report-uri` in `<meta>` | **Moderate** | Browser ignores the reporting directive; zero violation reports are sent. | Configure violation reporting via HTTP headers. |
| Using `'unsafe-inline'` in `script-src` | **High** | Permits inline `<script>` tags, defeating CSP's primary defense against XSS. | Use external scripts or SHA-256 hashes (`'sha256-...'`). |
| Placing CSP `<meta>` after stylesheets or scripts in `<head>` | **High** | Resources loaded before the meta tag are not restricted by the policy. | Place CSP meta tag as the very first line after `<meta charset>`. |
| Forgetting `data:` in `img-src` when using inline SVGs/canvases | **Moderate** | Canvas `toDataURL()` or base64 data URIs fail to render. | Add `data:` to `img-src`. |
| Omitting `base-uri 'self'` | **High** | Attackers can inject `<base>` tags to redirect relative script loads to external domains. | Declare `base-uri 'self';`. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Security Architecture (XSS Defense)
- **Defense in Depth**: CSP is not a substitute for proper input sanitization and context-aware HTML output escaping. It is a secondary safety net designed to mitigate damage if an injection vulnerability occurs.

### Performance Considerations
- Pre-computing SHA-256 hashes for critical inline configuration scripts allows you to avoid external network requests for tiny 1-line scripts while maintaining strict CSP enforcement.

---

## 9. Comparison / When to Use What

| Delivery Mechanism | Supported Directives | Clickjacking (`frame-ancestors`) | Reporting Support | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **HTTP Response Header** | **All 100% of directives** | **Yes (`frame-ancestors`)** | **Yes (`report-to`)** | Dynamic web servers (Node, Go, Python, Nginx). |
| **HTML `<meta>` Tag** | Most resource directives | **NO (Ignored by spec)** | **NO (Ignored by spec)** | JAMstack, GitHub Pages, static S3 sites, PWAs. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is the <code>frame-ancestors</code> directive completely ignored when specified inside an HTML <code>&lt;meta&gt;</code> tag?</strong></summary>

`frame-ancestors` defines whether an external website is permitted to embed the current document inside an `<iframe>` (the primary defense against Clickjacking). By browser security design, the browser must evaluate whether embedding is allowed **before** it downloads and parses the HTML document body. If `frame-ancestors` were allowed in `<meta>`, a malicious framing page could potentially manipulate DOM parsing or intercept the byte stream. Therefore, the W3C specification strictly dictates that `frame-ancestors` is valid only in HTTP response headers.
</details>

<details>
<summary><strong>2. How does a strict CSP neutralize Reflected and Stored Cross-Site Scripting (XSS)?</strong></summary>

The majority of XSS attacks rely on injecting inline scripts (`<script>steal()</script>`) or inline event handlers (`<img src=x onerror=steal()>`). A strict CSP forbids `'unsafe-inline'`, causing the browser engine to refuse to execute any inline script blocks or event handlers entirely. Furthermore, even if an attacker attempts to load an external script (`<script src="https://evil.com/payload.js">`), the browser blocks the network fetch because `evil.com` is not in the `script-src` whitelist.
</details>

<details>
<summary><strong>3. How does cryptographic hash-based authorization work in a Content Security Policy?</strong></summary>

Instead of allowing all inline scripts via `'unsafe-inline'`, a developer computes the cryptographic hash (SHA-256, SHA-384, or SHA-512) of the exact characters inside a trusted inline script block. The base64-encoded hash is added to `script-src` (e.g., `'sha256-abc...'`). When parsing the HTML, the browser computes the hash of the inline script: if it matches the CSP header, it executes; if an attacker modifies even a single space or injects new code, the hash mismatches and execution is blocked.
</details>

<details>
<summary><strong>4. Why is setting <code>base-uri 'self'</code> an essential component of a strict CSP?</strong></summary>

If `base-uri` is not restricted, an attacker who achieves HTML injection could insert a `<base href="https://attacker.com/">` tag into the document. This alters the resolution root of all relative URLs on the page. Subsequent relative scripts (`<script src="app.js">`) or form targets (`<form action="/login">`) will resolve to `https://attacker.com/app.js` and `https://attacker.com/login`, effectively hijacking script execution and credentials. Setting `base-uri 'self'` restricts `<base>` to the current origin.
</details>

<details>
<summary><strong>5. What is the role of <code>object-src 'none'</code>?</strong></summary>

`object-src` regulates the loading of resources via `<object>` and `<embed>` tags. Historically, attackers leveraged these tags to load legacy Flash files (`.swf`) or Java applets to bypass Same-Origin Policies and execute cross-site scripting. Setting `object-src 'none'` permanently disables this entire legacy attack surface.
</details>

<details>
<summary><strong>6. Can you specify multiple Content Security Policies on the same webpage?</strong></summary>

Yes. An application can receive a CSP via an HTTP response header and define another CSP via an HTML `<meta>` tag. In such scenarios, the browser enforces **all** policies simultaneously. A resource is only permitted if it satisfies **every** defined policy. A `<meta>` tag can only tighten (make more restrictive) an existing HTTP policy; it can never relax or override an HTTP policy.
</details>

<details>
<summary><strong>7. How does the <code>connect-src</code> directive prevent data exfiltration?</strong></summary>

Even if an attacker finds a way to read a sensitive CSRF token or user password in JavaScript, they must transmit that data back to their server. `connect-src` restricts the valid destinations for `fetch()`, `XMLHttpRequest`, WebSockets, and `navigator.sendBeacon()`. If `connect-src` is set to `'self'`, any attempt to send data to `https://attacker.com/steal` is immediately blocked by the browser's network layer.
</details>

<details>
<summary><strong>8. Why does <code>style-src 'self' 'unsafe-inline'</code> frequently appear in production CSPs, and what is the associated risk?</strong></summary>

Many popular CSS-in-JS libraries (such as styled-components, Emotion) and UI frameworks inject dynamic styles directly into the DOM via inline `<style>` tags or the `style` attribute. To avoid breaking these libraries, teams often permit `'unsafe-inline'` for styles. While CSS injection is less dangerous than JavaScript injection, malicious CSS can be used to exfiltrate input values using attribute selectors and background images. Modern frameworks are increasingly moving to CSS nonce support to eliminate `'unsafe-inline'`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Minimal Static Site CSP Meta Tag (Easy)
- **Task**: Author an HTML `<meta>` tag for a static blog hosted on GitHub Pages. The policy must allow resources from `'self'`, allow images from `https://images.unsplash.com`, disable all plugins (`object-src 'none'`), and lock down the base URI.

<details>
<summary>View Solution</summary>

```html
<meta 
  http-equiv="Content-Security-Policy" 
  content="default-src 'self'; img-src 'self' https://images.unsplash.com; object-src 'none'; base-uri 'self';" 
/>
```
</details>

---

### Exercise 2: Form Action Lockdown Policy (Medium)
- **Task**: Create an HTML `<head>` containing a CSP meta tag that allows scripts and styles only from `'self'`, but restricts `<form action>` submissions exclusively to the current origin and `https://checkout.stripe.com`.

<details>
<summary>View Solution</summary>

```html
<head>
  <meta charset="utf-8" />
  <meta 
    http-equiv="Content-Security-Policy" 
    content="default-src 'self'; script-src 'self'; style-src 'self'; form-action 'self' https://checkout.stripe.com; object-src 'none'; base-uri 'self';" 
  />
  <title>Payment Gateway</title>
</head>
```
</details>

---

### Exercise 3: CSP Security Audit and Bug Hunt (Hard)
- **Task**: Review the following enterprise `<head>` markup. Identify 3 critical security flaws, explain why each is problematic, and write the corrected CSP meta tag:
  ```html
  <head>
    <meta http-equiv="Content-Security-Policy" content="
      default-src *; 
      script-src 'self' 'unsafe-inline' 'unsafe-eval'; 
      frame-ancestors 'none';
    ">
  </head>
  ```

<details>
<summary>View Solution</summary>

**Flaws Identified:**
1. **`default-src *`**: A wildcard allows loading resources (images, fonts, styles, connections) from any arbitrary domain on the internet, completely undermining origin restrictions.
2. **`'unsafe-inline'` and `'unsafe-eval'` in `script-src`**: Allows arbitrary inline `<script>` tags, inline event handlers, and `eval()` strings, completely negating CSP's defense against XSS.
3. **`frame-ancestors 'none'` inside a `<meta>` tag**: Browsers completely ignore `frame-ancestors` in `<meta>` per the W3C specification; the site remains 100% vulnerable to Clickjacking unless delivered via an HTTP response header.

**Corrected CSP Meta Tag (for HTML):**
```html
<meta 
  http-equiv="Content-Security-Policy" 
  content="default-src 'self'; script-src 'self'; object-src 'none'; base-uri 'self';" 
/>
```
*(Note: `frame-ancestors 'none'` must be migrated to the web server's HTTP response headers).*
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                 CSP IN META QUICK REFERENCE                                     |
+-------------------------------------------------------------------------------------------------+
| Directive                | Syntax Example                          | Status in <meta>           |
+-------------------------------------------------------------------------------------------------+
| default-src              | default-src 'self'                      | Supported                  |
| script-src               | script-src 'self' 'sha256-...'          | Supported                  |
| object-src               | object-src 'none'                       | Supported (MANDATORY)      |
| base-uri                 | base-uri 'self'                         | Supported (MANDATORY)      |
| form-action              | form-action 'self' https://pay.com      | Supported                  |
| frame-ancestors          | frame-ancestors 'none'                  | FORBIDDEN / IGNORED        |
| report-uri / report-to   | report-uri /csp-endpoint                | FORBIDDEN / IGNORED        |
| Golden Rule              | Never use 'unsafe-inline' in script-src! Use hashes or external JS.|
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [W3C Content Security Policy Level 3 Specification](https://www.w3.org/TR/CSP3/)
- [MDN Web Docs: Content Security Policy (CSP)](https://developer.mozilla.org/en-US/docs/Web/HTTP/CSP)
- [Google CSP Evaluator Tool](https://csp-evaluator.withgoogle.com/)
- [OWASP: Content Security Policy Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Content_Security_Policy_Cheat_Sheet.html)

---

[← Previous: PWA Web App Manifest](09-pwa-manifest.md) | [Section Index](README.md) | [Next: Performance Hints & Speculation Rules →](11-performance-hints-and-priorities.md)
