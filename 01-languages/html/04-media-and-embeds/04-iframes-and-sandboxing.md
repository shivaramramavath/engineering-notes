# iFrames, Sandboxing, and Embedded Browsing Contexts

| Metadata | Details |
| :--- | :--- |
| **Specification** | WHATWG HTML Living Standard (§4.8.5 The `iframe` element) / W3C Permissions Policy |
| **Prerequisites** | `01-fundamentals/01-document-structure.md`, `03-forms/14-forms-security-and-csrf.md` |
| **Estimated Reading Time** | 26 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`<iframe>`, `sandbox`, `srcdoc`, `loading="lazy"`, `allow` Permissions Policy) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, OWASP ZAP, axe-core 4.9, NVDA 2024.1 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Hardened Third-Party Widget Embed with Sandboxing](#example-1-hardened-third-party-widget-embed-with-sandboxing)
   - [Example 2: In-Memory HTML Preview with `srcdoc`](#example-2-in-memory-html-preview-with-srcdoc)
   - [Example 3: Secure Cross-Origin Communication via `postMessage`](#example-3-secure-cross-origin-communication-via-postmessage)
   - [Example 4: Restricting Hardware Permissions via the `allow` Attribute](#example-4-restricting-hardware-permissions-via-the-allow-attribute)
   - [Example 5: Lazy-Loaded Responsive Video Embed with Zero Layout Shift](#example-5-lazy-loaded-responsive-video-embed-with-zero-layout-shift)
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

The `<iframe>` (inline frame) element allows developers to nest a complete, isolated **embedded browsing context** (a separate HTML document with its own `window`, `document`, and event loop) directly within a parent webpage.

iFrames are ubiquitous across the modern web: they power YouTube and Vimeo video players, Google Maps integrations, payment gateways (Stripe, PayPal), OAuth login flows, and customer support chat widgets.

However, loading third-party content inside an iframe historically introduced catastrophic security vulnerabilities: malicious embeds could steal parent session cookies, manipulate parent DOM trees, hijack top-level window navigation, access cameras and microphones silently, or trigger drive-by malware downloads.

Modern HTML provides defense-in-depth isolation mechanisms:
- The **`sandbox`** attribute: Strips untrusted embeds of scripts, forms, popups, and same-origin privileges by default.
- The **`allow`** Permissions Policy: Explicitly controls hardware capabilities (camera, microphone, payment request, fullscreen).
- Native **`loading="lazy"`**: Defers network requests and rendering until the iframe approaches the user's viewport.
- The **`srcdoc`** attribute: Safely renders arbitrary user-generated HTML in memory without network roundtrips.

---

## 2. Core Concepts

### The `sandbox` Token Matrix
When the `sandbox` attribute is declared bare (`<iframe sandbox>`), the browser places the embedded context into **maximum lockdown**:
1. JavaScript execution is blocked.
2. Form submission is blocked.
3. The content is forced into a unique opaque origin (blocking access to `localStorage`, cookies, and parent DOM).
4. Popups and new windows are blocked.
5. Top-level window navigation is blocked.

Developers re-enable specific capabilities with granular permission tokens:
- **`allow-scripts`**: Allows the iframe to run JavaScript.
- **`allow-forms`**: Allows the iframe to submit forms.
- **`allow-same-origin`**: Retains the document's true origin (enabling cookie/storage access) rather than treating it as a unique null origin.
- **`allow-popups`**: Allows `window.open()` and `target="_blank"`.
- **`allow-top-navigation`**: Allows the iframe to navigate the top-level parent window (`window.top.location = ...`).
- **`allow-downloads`**: Allows user-initiated file downloads.
- **`allow-modals`**: Allows `alert()`, `confirm()`, and `print()`.

> [!CAUTION]
> **The Fatal Sandbox Vulnerability**: NEVER combine `allow-scripts` and `allow-same-origin` on an iframe hosted on the same origin as the parent document. This combination allows the iframe's script to reach up, access `window.parent.document`, and remove its own `sandbox` attribute completely!

### Hardware Control with the `allow` Attribute
The `allow` attribute defines the **Permissions Policy** for the frame, regulating access to sensitive browser APIs:
- `camera`, `microphone`, `geolocation`, `payment`, `fullscreen`, `display-capture`, `clipboard-read`.
- Syntax: `allow="camera 'none'; microphone 'none'; fullscreen 'self' https://trusted.com"`

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                  Browsing Context & Process Boundary Architecture                       |
+-----------------------------------------------------------------------------------------+

  Parent Window (https://bank.com)
  [ Process A / Main Thread ]
       |
       +--> Renders <iframe src="https://payment-widget.com" sandbox="allow-scripts">
               |
               v
  +-----------------------------------------------------------------------+
  | Embedded Child Browsing Context                                       |
  | [ Process B (Site Isolation) / Independent JS Event Loop ]            |
  |                                                                       |
  | Security Sandbox Enforcements:                                        |
  | - window.parent is restricted by Same-Origin Policy (SOP)             |
  | - document.domain access is blocked                                   |
  | - Access to parent DOM / cookies throws DOMException                 |
  | - Hardware APIs disabled unless whitelisted in `allow` attribute      |
  |                                                                       |
  | Communication Mechanism:                                              |
  | Secure MessageChannel: window.postMessage(data, targetOrigin)        |
  +-----------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

```html
<iframe 
  src="https://maps.example.com/embed?q=San+Francisco" 
  title="Interactive map of our downtown office location"
  width="800" 
  height="450" 
  loading="lazy" 
  sandbox="allow-scripts allow-popups" 
  allow="geolocation 'self' https://maps.example.com; fullscreen 'self'"
  referrerpolicy="no-referrer-when-downgrade"
></iframe>
```

> **Cross-Browser & Baseline Status Box**
>
> - `<iframe>`, `sandbox`: **Baseline Widely Available** across all major browsers.
> - `srcdoc`: **Baseline Widely Available** (Chrome 20+, Firefox 25+, Safari 6+, Edge 79+).
> - `loading="lazy"` on iframes: **Baseline Widely Available (2020)**.
> - `allow` (Permissions Policy): **Baseline Widely Available**.

---

## 5. Practical Examples

### Example 1: Hardened Third-Party Widget Embed with Sandboxing
Embedding a third-party weather widget with scripts permitted, but forms, storage, and top navigation strictly disabled.

```html
<div class="widget-card">
  <h3>Current Regional Weather</h3>
  
  <!-- 
    Hardened sandbox:
    - allow-scripts: Enables widget animations/rendering
    - NO allow-same-origin: Treats iframe as opaque null origin (zero cookie theft)
    - NO allow-top-navigation: Widget cannot hijack browser address bar
  -->
  <iframe 
    src="https://weather-widgets-api.com/v2/forecast?city=seattle" 
    title="Real-time Weather Forecast for Seattle, WA"
    width="320" 
    height="240" 
    sandbox="allow-scripts" 
    loading="lazy" 
    class="weather-frame"
  ></iframe>
</div>

<style>
  .weather-frame {
    border: 1px solid #e0e0e0;
    border-radius: 8px;
    display: block;
  }
</style>
```

---

### Example 2: In-Memory HTML Preview with `srcdoc`
Safely previewing user-authored Markdown/HTML output inside a code playground without writing temporary files to a server.

```html
<div class="code-playground">
  <h2>Live Document Preview</h2>

  <!-- 
    srcdoc renders HTML directly from an inline string attribute.
    sandbox="allow-scripts" ensures scripts run inside an isolated sandbox
    without same-origin access to the host site's session or cookies.
  -->
  <iframe 
    id="preview-window" 
    title="User Markdown Output Sandbox"
    sandbox="allow-scripts" 
    srcdoc="<!DOCTYPE html><html><head><style>body{font-family:sans-serif;color:#333;}</style></head><body><h1>Hello World</h1><p>Rendered securely inside srcdoc.</p></body></html>"
    width="100%" 
    height="400"
  ></iframe>
</div>

<script>
  // Dynamic update of preview window from an editor
  function updatePreview(userHtml) {
    const frame = document.getElementById('preview-window');
    // Sanitized update to srcdoc
    frame.srcdoc = `<!DOCTYPE html><html><body>${userHtml}</body></html>`;
  }
</script>
```

---

### Example 3: Secure Cross-Origin Communication via `postMessage`
Establishing a secure bidirectional messaging channel between parent application and an embedded checkout iframe.

```html
<!-- Parent Window (https://mystore.com) -->
<iframe 
  id="checkout-frame" 
  src="https://pay.secure-checkout.com/gateway" 
  title="Credit Card Payment Processor"
  width="450" 
  height="300" 
  sandbox="allow-scripts allow-forms allow-same-origin"
  allow="payment"
></iframe>

<script>
  const iframe = document.getElementById('checkout-frame');
  const TRUSTED_ORIGIN = 'https://pay.secure-checkout.com';

  // 1. Send transaction payload to iframe
  iframe.addEventListener('load', () => {
    iframe.contentWindow.postMessage({
      action: 'INIT_TRANSACTION',
      amountCents: 4999,
      orderId: 'ORD-99124'
    }, TRUSTED_ORIGIN); // STRICT targetOrigin verification!
  });

  // 2. Receive confirmation from iframe
  window.addEventListener('message', (event) => {
    // CRITICAL: Always verify event.origin before reading event.data!
    if (event.origin !== TRUSTED_ORIGIN) {
      console.warn('Blocked message from untrusted origin:', event.origin);
      return;
    }

    if (event.data.status === 'SUCCESS') {
      alert(`Payment complete! Receipt: ${event.data.transactionId}`);
    }
  });
</script>
```

---

### Example 4: Restricting Hardware Permissions via the `allow` Attribute
Embedding an interactive video conference frame that is granted access to the microphone and camera, while strictly blocking geolocation and payments.

```html
<div class="call-container">
  <!-- 
    Permissions Policy (allow):
    - camera and microphone explicitly delegated to the child origin
    - display-capture allowed for screen sharing
    - geolocation explicitly forbidden ('none')
  -->
  <iframe 
    src="https://meet.secure-video.com/room/room-8821" 
    title="Client Consultation Video Meeting"
    width="960" 
    height="540" 
    allow="
      camera https://meet.secure-video.com; 
      microphone https://meet.secure-video.com; 
      display-capture https://meet.secure-video.com; 
      fullscreen; 
      geolocation 'none'
    "
    sandbox="allow-scripts allow-same-origin allow-forms allow-popups"
    loading="lazy" 
  ></iframe>
</div>
```

---

### Example 5: Lazy-Loaded Responsive Video Embed with Zero Layout Shift
Embedding a responsive YouTube video with a modern CSS aspect-ratio container, lazy loading, and an accessible title.

```html
<div class="video-embed-wrapper">
  <iframe 
    src="https://www.youtube-nocookie.com/embed/dQw4w9WgXcQ" 
    title="Engineering Keynote: Scalable Distributed Systems"
    width="1280" 
    height="720" 
    loading="lazy" 
    allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" 
    allowfullscreen
    class="responsive-video-frame"
  ></iframe>
</div>

<style>
  .video-embed-wrapper {
    width: 100%;
    max-width: 900px;
    margin: 2rem auto;
    /* Aspect ratio allocates 16:9 box before iframe loads, preventing CLS */
    aspect-ratio: 16 / 9;
  }
  .responsive-video-frame {
    width: 100%;
    height: 100%;
    border: none;
    border-radius: 8px;
    display: block;
  }
</style>
```

---

## 6. Best Practices

- [ ] **Always provide a descriptive `title` attribute on `<iframe>`**: Screen readers announce the iframe's title to help blind users decide whether to navigate into the embedded frame or bypass it.
- [ ] **Apply the `sandbox` attribute to all third-party embeds**: Use the principle of least privilege. Grant only the exact tokens required (e.g., `sandbox="allow-scripts"`).
- [ ] **Never combine `allow-scripts` and `allow-same-origin` on your own domain**: This grants the iframe identical origin rights, allowing it to manipulate the parent DOM and break out of its sandbox.
- [ ] **Always add `loading="lazy"` to below-the-fold iframes**: Prevents embedded widgets and video players from competing with critical resources for bandwidth during page load.
- [ ] **Always verify `event.origin` in `postMessage` listeners**: Never trust incoming message payloads without verifying that `event.origin` matches your expected trusted server.
- [ ] **Always set explicit `targetOrigin` when sending `postMessage`**: Never use `postMessage(data, '*')` when transmitting sensitive data (tokens, PII); specify the exact target domain (`postMessage(data, 'https://trusted.com')`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Omitting the `title` attribute on `<iframe>` | **Critical** | Screen reader users hear "frame" without any indication of its content or purpose (WCAG 4.1.2 violation). | Add a descriptive `title="..."` attribute. |
| Combining `allow-scripts` + `allow-same-origin` on same origin | **Critical** | Iframe can reach parent DOM and remove its own sandbox restrictions, nullifying security. | Host untrusted sandboxed iframes on a dedicated, separate domain (e.g., `example-user-content.com`). |
| Omitting `event.origin` validation in `window.addEventListener('message')` | **Critical** | Any malicious tab or iframe can send forged messages, leading to DOM-XSS or unauthorized actions. | Check `if (event.origin !== EXPECTED_ORIGIN) return;`. |
| Sending `postMessage(data, '*')` with sensitive data | **High** | Any window or iframe navigating to the target can intercept the payload. | Always pass the exact destination origin as the second argument. |
| Forgetting `loading="lazy"` on embedded videos | **High** | Browsers download megabytes of third-party JS scripts immediately on page load, ruining LCP. | Add `loading="lazy"` to all non-critical iframes. |
| Styling iframes without `aspect-ratio` | **Moderate** | Causes severe Cumulative Layout Shift (CLS) when iframe finally finishes rendering. | Set CSS `aspect-ratio: 16 / 9` on container. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Guideline 4.1.2 Name, Role, Value**: Every `<iframe>` must possess an accessible name via a non-empty `title` attribute (e.g., `title="Google Maps location of Headquarters"`). Generic titles like `title="frame"` or `title="widget"` are major accessibility failures.

### Performance & Core Web Vitals
- **Native Lazy Loading**: `loading="lazy"` on iframes defers the iframe's network requests until the user scrolls within a calculated threshold distance from the viewport.
- **Site Isolation & Process Overhead**: Each unique-origin iframe often runs in an isolated browser process. Creating dozens of concurrent iframes consumes immense RAM.

### Security Defenses
- **`Content-Security-Policy: frame-ancestors`**: Protects your site from being iframed by malicious actors (Clickjacking).
- **`Content-Security-Policy: frame-src`**: Restricts the origins your page is allowed to load inside an `<iframe>`.

---

## 9. Comparison / When to Use What

| Mechanism | Isolation Boundary | Scripting Allowed | Storage Access | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **Bare `<iframe sandbox>`** | Absolute (Opaque origin) | No | Blocked | Rendering completely untrusted static user HTML. |
| **`<iframe sandbox="allow-scripts">`** | Isolated (Opaque origin) | Yes | Blocked | Interactive widgets, ads, untrusted client scripts. |
| **`<iframe sandbox="allow-scripts allow-forms allow-same-origin">`** | Partial (Retains origin) | Yes | Allowed | Authenticated third-party services (Payment, OAuth). |
| **`<iframe srcdoc="...">`** | In-memory DOM | Configurable | Configurable | Instant client-side code editors, Markdown previews. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is combining <code>allow-scripts</code> and <code>allow-same-origin</code> considered dangerous in an <code>&lt;iframe sandbox&gt;</code>?</strong></summary>

When both flags are present and the iframe is hosted on the same origin as the parent document, the iframe gains the ability to execute JavaScript *and* has access to the parent document's DOM and cookie storage. A malicious script inside the iframe can simply execute:
```javascript
window.parent.document.querySelector('iframe').removeAttribute('sandbox');
```
This enables the iframe to break out of its sandbox entirely and assume full control of the parent application.
</details>

<details>
<summary><strong>2. What is the accessibility purpose of the <code>title</code> attribute on an <code>&lt;iframe&gt;</code>?</strong></summary>

The `title` attribute provides the accessible name for the embedded browsing context in the accessibility tree. Screen reader users navigating by landmarks or frames encounter the title (e.g., "PayPal Checkout Gateway") before deciding whether to enter the frame. Without a descriptive title, the screen reader merely announces "frame", leaving blind users unaware of the frame's contents.
</details>

<details>
<summary><strong>3. How does the <code>srcdoc</code> attribute differ from the standard <code>src</code> attribute?</strong></summary>

The `src` attribute takes a URL and triggers a network request to fetch the external document. The `srcdoc` attribute takes a raw HTML string and renders that markup directly in memory without initiating any HTTP network request. If both `src` and `srcdoc` are present, `srcdoc` takes precedence in all supporting browsers.
</details>

<details>
<summary><strong>4. How should you securely handle incoming messages from an iframe using <code>window.addEventListener('message')</code>?</strong></summary>

You must always validate `event.origin` against a strict whitelist before reading `event.data`. For example:
```javascript
window.addEventListener('message', (event) => {
  if (event.origin !== 'https://trusted-domain.com') return;
  // Process event.data safely
});
```
Failing to verify `event.origin` allows any arbitrary malicious website to send messages that trigger privileged application logic.
</details>

<details>
<summary><strong>5. What is the purpose of the <code>allow</code> attribute on an <code>&lt;iframe&gt;</code>?</strong></summary>

The `allow` attribute implements the W3C Permissions Policy for the embedded browsing context. It controls which hardware features and sensitive browser APIs (such as `camera`, `microphone`, `geolocation`, `payment`, and `fullscreen`) the iframe is permitted to access, and can delegate permissions to specific origins.
</details>

<details>
<summary><strong>6. What happens when an iframe is declared with a bare <code>sandbox</code> attribute without any tokens?</strong></summary>

A bare `<iframe sandbox>` applies maximum security restrictions: JavaScript execution is disabled, forms cannot be submitted, the iframe is treated as a unique opaque origin (no cookies, no `localStorage`), popups are blocked, plugins are disabled, and top-level navigation is prohibited.
</details>

<details>
<summary><strong>7. How does <code>loading="lazy"</code> improve performance when embedding third-party widgets like YouTube or Google Maps?</strong></summary>

Embedding external widgets normally triggers dozens of third-party network requests (scripts, fonts, stylesheets, analytics) on page load. By setting `loading="lazy"`, the browser holds off downloading the iframe and its dependencies until the user scrolls within a specific threshold distance of the iframe, preserving bandwidth and boosting First Contentful Paint (FCP) and LCP.
</details>

<details>
<summary><strong>8. What is the difference between CSP <code>frame-src</code> and CSP <code>frame-ancestors</code>?</strong></summary>

- `frame-src`: Governs what URLs the *current page* is allowed to embed inside `<iframe>` elements.
- `frame-ancestors`: Governs what parent pages are allowed to embed the *current page* inside an `<iframe>` (protecting the page against Clickjacking).
</details>

---

## 11. Practice Exercises

### Exercise 1: Accessible Google Maps Embed with Lazy Loading (Easy)
- **Task**: Author an accessible, lazy-loaded `<iframe>` embedding an interactive map with dimensions of `800x450`, an accessible title, and fullscreen permission.

<details>
<summary>View Solution</summary>

```html
<iframe 
  src="https://maps.example.com/embed?location=Central+Park" 
  title="Interactive map showing pedestrian entrances to Central Park, New York"
  width="800" 
  height="450" 
  loading="lazy" 
  allow="fullscreen"
  style="border: 0; width: 100%; max-width: 800px; height: 450px;"
></iframe>
```
</details>

---

### Exercise 2: Sandboxed User HTML Preview (Medium)
- **Task**: Create an iframe using `srcdoc` to render user-submitted HTML safely. The sandbox must permit JavaScript execution inside the preview, but must strictly forbid form submissions, same-origin access, and top navigation.

<details>
<summary>View Solution</summary>

```html
<iframe 
  title="Interactive Code Demonstration Preview"
  srcdoc="<!DOCTYPE html><html><body><h2>Demo Output</h2><button onclick='alert(1)'>Click Me</button></body></html>"
  sandbox="allow-scripts allow-modals"
  width="600" 
  height="300"
></iframe>
```
</details>

---

### Exercise 3: Secure Cross-Origin Parent/Child Messenger (Hard)
- **Task**: Author the parent HTML script and child iframe HTML script demonstrating secure `postMessage` communication:
  - Parent (`https://parent.com`) embeds child (`https://payment.com/modal`).
  - Parent sends `{ action: 'PAY', amount: 100 }` to child with exact targetOrigin verification.
  - Child verifies parent origin, processes payment, and replies `{ status: 'PAID' }`.
  - Parent verifies child origin before displaying receipt.

<details>
<summary>View Solution</summary>

**Parent Window (`https://parent.com`):**
```html
<iframe id="pay-frame" src="https://payment.com/modal" title="Payment Frame"></iframe>

<script>
  const frame = document.getElementById('pay-frame');
  const CHILD_ORIGIN = 'https://payment.com';

  frame.onload = () => {
    frame.contentWindow.postMessage({ action: 'PAY', amount: 100 }, CHILD_ORIGIN);
  };

  window.addEventListener('message', (event) => {
    if (event.origin !== CHILD_ORIGIN) return;
    if (event.data.status === 'PAID') {
      alert('Transaction successful!');
    }
  });
</script>
```

**Child Frame (`https://payment.com/modal`):**
```html
<script>
  const PARENT_ORIGIN = 'https://parent.com';

  window.addEventListener('message', (event) => {
    if (event.origin !== PARENT_ORIGIN) return;

    if (event.data.action === 'PAY') {
      console.log(`Processing payment of $${event.data.amount}`);
      // Send receipt back to parent
      event.source.postMessage({ status: 'PAID' }, event.origin);
    }
  });
</script>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                 <iframe> SECURITY CHEAT SHEET                                   |
+-------------------------------------------------------------------------------------------------+
| Attribute / Token        | Purpose / Security Behavior                                          |
+-------------------------------------------------------------------------------------------------+
| title="Description"      | MANDATORY for accessibility; read by screen readers                  |
| sandbox                  | Maximum lockdown: No JS, no forms, opaque origin, no popups          |
| sandbox="allow-scripts"  | Enables JavaScript execution inside isolated opaque origin           |
| sandbox="allow-same-origin" Retains original origin storage/cookies (NEVER combine with scripts on self) |
| sandbox="allow-forms"    | Enables form submission inside the frame                             |
| allow="camera; mic"      | Grants explicit Permissions Policy access to device hardware         |
| loading="lazy"           | Defers network loading until scrolled near the viewport              |
| srcdoc="<html>..."       | Renders inline HTML string without making HTTP requests              |
| postMessage(data, origin)| Secure cross-origin communication mechanism                          |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [WHATWG HTML Living Standard: The `iframe` element](https://html.spec.whatwg.org/multipage/iframe-embed-object.html#the-iframe-element)
- [MDN Web Docs: `<iframe>`: The Inline Frame element](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/iframe)
- [W3C Permissions Policy Specification](https://w3c.github.io/webappsec-permissions-policy/)
- [OWASP: Clickjacking Defense Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Clickjacking_Defense_Cheat_Sheet.html)

---

[← Previous: Picture & Art Direction](03-picture-and-art-direction.md) | [Section Index](README.md) | [Next: Embedded SVG →](05-embedded-svg.md)
