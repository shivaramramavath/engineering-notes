# Cross-Site Request Forgery (CSRF) Prevention in JavaScript

> Ambient credentials, SameSite cookie mechanics, Anti-CSRF Synchronizer Tokens, Double Submit Cookies, and Origin header verification.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Headers and CORS](file:///e:/Learning/engineering-notes/01-languages/javascript/15_networking/03_headers-and-cors.md), [HTTP Server](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/08_http-server.md) |
| **Time to read** | 22 minutes |
| **Status** | Complete |
| **Standard / Runtime** | RFC 6265bis (SameSite Cookies), OWASP CSRF Prevention Cheat Sheet |

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

**Cross-Site Request Forgery (CSRF)** is an attack that tricks an authenticated user's web browser into executing unwanted, state-changing actions on a trusted web application.

The vulnerability stems from the fundamental way web browsers handle authentication cookies. When a browser issues an HTTP request to `https://bank.com`, it **automatically attaches all cookies associated with `bank.com`**—even if the request originated from an external, malicious website (`https://evil-attacker.com`).

If the server relies exclusively on automatic cookie credentials to authorize requests, an attacker who tricks an authenticated victim into visiting a malicious webpage can forge fund transfers, email changes, or password resets without the victim's knowledge or consent. Understanding how to neutralize CSRF via **`SameSite` cookie policies**, **Anti-CSRF Tokens**, and **Origin Verification** is essential for backend and full-stack security.

---

## 2. Core Concepts

### Ambient Credentials
The root cause of CSRF is **ambient authority**: the browser automatically sends session cookies, HTTP Basic Auth credentials, and client-side TLS certificates along with any cross-site request. Because the server sees a valid session cookie, it assumes the action was deliberately authorized by the user.

### Why CORS Does NOT Protect Against CSRF
A widespread myth is that Cross-Origin Resource Sharing (CORS) stops CSRF attacks. It does not:
- CORS is a browser security mechanism that restricts whether client-side JavaScript can **read** the response of a cross-origin request.
- In a CSRF attack, the attacker does not need to read the response; the damage is done the moment the server **executes the state-changing action** (e.g., deducting funds or deleting an account). Simple form POST requests (`<form method="POST">`) bypass CORS preflights completely.

### The Modern Defense Arsenal
1. **`SameSite` Cookie Attribute**: Instructs the browser whether cookies should be included in cross-site requests.
2. **Synchronizer Token Pattern (CSRF Tokens)**: A cryptographically random secret associated with the user's session, required as a hidden form field or request header on state-changing requests.
3. **Double Submit Cookie Pattern**: A stateless alternative where the CSRF token is sent both in a cookie and in a custom request header; the server verifies they match.
4. **Origin and Referer Header Verification**: Validating that the HTTP `Origin` header matches the server's own origin.

---

## 3. How It Works Internally

```
THE ANATOMY OF A CSRF ATTACK:

1. Victim logs into https://bank.com -> Browser stores Session Cookie
                                      (Set-Cookie: session_id=xyz123)

2. Victim visits malicious site: https://evil.com/trap.html
   Trap page contains auto-submitting hidden HTML form:
   <form id="f" action="https://bank.com/api/transfer" method="POST">
     <input type="hidden" name="to" value="attacker_account" />
     <input type="hidden" name="amount" value="5000" />
   </form>
   <script>document.getElementById('f').submit();</script>

3. Victim's browser issues POST request to https://bank.com/api/transfer
   AUTOMATICALLY ATTACHES: Cookie: session_id=xyz123

4. Server receives valid cookie, assumes legitimate user request,
   and transfers $5,000 to the attacker!
```

---

## 4. Syntax & API Reference

### Configuring the `SameSite` Cookie Attribute
```http
# Modern secure session cookie header
Set-Cookie: session_id=xyz123; Path=/; Secure; HttpOnly; SameSite=Lax
```

| SameSite Mode | Description | Cross-Site POST | Top-Level GET Navigation |
| :--- | :--- | :---: | :---: |
| **`SameSite=Strict`** | Never sent in cross-site requests under any condition | Blocked | Blocked (e.g. clicking link from email) |
| **`SameSite=Lax`** | Sent only on top-level safe GET navigations (Default in modern browsers) | Blocked | Sent |
| **`SameSite=None`** | Always sent with cross-site requests (Requires `Secure` flag) | Sent | Sent |

---

## 5. Practical Examples

### Example 1: Synchronizer Token Pattern Middleware (Node.js)
```javascript
import crypto from 'node:crypto';

// Session store: session_id -> { userId, csrfToken }
const sessions = new Map();

export function generateCsrfToken(sessionId) {
  const token = crypto.randomBytes(32).toString('hex');
  const session = sessions.get(sessionId) || {};
  session.csrfToken = token;
  sessions.set(sessionId, session);
  return token;
}

export function csrfProtectionMiddleware(req, res, next) {
  // Safe HTTP methods do not mutate state; skip CSRF checks for GET, HEAD, OPTIONS
  if (['GET', 'HEAD', 'OPTIONS'].includes(req.method)) {
    return next();
  }

  const sessionId = req.cookies?.session_id;
  const session = sessions.get(sessionId);

  if (!session) {
    res.writeHead(401, { 'Content-Type': 'application/json' });
    return res.end(JSON.stringify({ error: 'Unauthenticated' }));
  }

  // Read token from custom header or form body
  const submittedToken = req.headers['x-csrf-token'] || req.body?.csrf_token;

  if (!submittedToken || !crypto.timingSafeEqual(Buffer.from(submittedToken), Buffer.from(session.csrfToken))) {
    res.writeHead(403, { 'Content-Type': 'application/json' });
    return res.end(JSON.stringify({ error: 'Invalid or missing CSRF token' }));
  }

  next(); // Token is valid! Proceed to handler
}
```

### Example 2: Double Submit Cookie Pattern (Stateless)
```javascript
import crypto from 'node:crypto';

// In Double Submit Cookie, the server does not store CSRF tokens in RAM.
// It generates a cryptographically random token, sends it as a readable cookie,
// and requires the client JavaScript to echo that token in a custom header.
export function doubleSubmitCookieMiddleware(req, res, next) {
  if (['GET', 'HEAD', 'OPTIONS'].includes(req.method)) {
    return next();
  }

  const cookieToken = req.cookies?.['csrf-token'];
  const headerToken = req.headers['x-csrf-token'];

  if (!cookieToken || !headerToken) {
    res.writeHead(403);
    return res.end('CSRF Token Missing');
  }

  // Timing-safe equality comparison prevents timing attacks
  const bufA = Buffer.from(cookieToken);
  const bufB = Buffer.from(headerToken);

  if (bufA.length !== bufB.length || !crypto.timingSafeEqual(bufA, bufB)) {
    res.writeHead(403);
    return res.end('CSRF Token Mismatch');
  }

  next();
}
```

### Example 3: Verifying `Origin` and `Referer` Headers
```javascript
export function verifyOriginMiddleware(req, res, next) {
  // Only verify state-changing HTTP requests
  if (['POST', 'PUT', 'DELETE', 'PATCH'].includes(req.method)) {
    const origin = req.headers['origin'];
    const host = req.headers['host'];

    if (!origin) {
      // Fall back to Referer header if Origin is absent
      const referer = req.headers['referer'];
      if (!referer) {
        res.writeHead(403);
        return res.end('Blocked: Missing Origin and Referer headers');
      }

      const refererHost = new URL(referer).host;
      if (refererHost !== host) {
        res.writeHead(403);
        return res.end('Blocked: Cross-origin Referer mismatch');
      }
    } else {
      const originHost = new URL(origin).host;
      if (originHost !== host) {
        res.writeHead(403);
        return res.end('Blocked: Cross-origin Origin mismatch');
      }
    }
  }

  next();
}
```

### Example 4: Client-Side Fetch Wrapper with Anti-CSRF Header
```javascript
// Automatically extracting the CSRF token and attaching it to all mutating fetch requests
function getCookie(name) {
  const match = document.cookie.match(new RegExp(`(^|; )${name}=([^;]*)`));
  return match ? decodeURIComponent(match[2]) : null;
}

export async function secureFetch(url, options = {}) {
  const method = (options.method || 'GET').toUpperCase();
  const headers = new Headers(options.headers || {});

  // If request modifies state, attach custom Anti-CSRF token header
  if (!['GET', 'HEAD', 'OPTIONS'].includes(method)) {
    const csrfToken = getCookie('csrf-token') || document.querySelector('meta[name="csrf-token"]')?.content;
    if (csrfToken) {
      headers.set('X-CSRF-Token', csrfToken);
    }
  }

  return fetch(url, { ...options, headers });
}
```

### Example 5: Custom Headers to Thwart Simple Cross-Origin Requests
```javascript
// Modern single-page applications (SPAs) sending JSON payloads using custom headers:
// A cross-origin HTML form cannot set custom headers like `X-Requested-With: XMLHttpRequest`!
export function enforceCustomHeaderMiddleware(req, res, next) {
  if (!['GET', 'HEAD', 'OPTIONS'].includes(req.method)) {
    if (req.headers['x-requested-with'] !== 'XMLHttpRequest') {
      res.writeHead(400);
      return res.end('Invalid request header');
    }
  }
  next();
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`SameSite=Lax` 2-Minute Window (Lax-Allowing-Unsafe)**: In older versions of Chrome, newly set cookies with no explicit `SameSite` attribute permitted cross-site POST requests for the first 120 seconds after creation ("Lax+POST" temporary window). Always specify `SameSite=Strict` or `SameSite=Lax` explicitly!
2. **Subdomain Vulnerabilities in Double Submit Cookie**: If an attacker gains control of a subdomain (`attacker.sub.example.com`), they can write cookies scoped to `.example.com`. The server using naive Double Submit Cookie will compare the attacker's forged cookie with the attacker's forged header and pass validation. Mitigation: Use HMAC-signed CSRF tokens.
3. **GET Requests Performing State Changes**: Using HTTP `GET` for state-changing operations (`<img src="https://bank.com/transfer?to=hacker&amt=1000">`) completely bypasses `SameSite=Lax` and CSRF middleware. **Never use GET for state-changing operations.**
4. **Timing Attacks on Token Comparison**: Comparing tokens with `tokenA === tokenB` is vulnerable to timing attacks (string comparison terminates early on the first mismatched byte). Always compare tokens using `crypto.timingSafeEqual()`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Relying on CORS to stop CSRF | CORS only restricts reading responses, not executing requests | Use `SameSite` cookies and CSRF tokens |
| State-changing actions via HTTP GET (`/delete?id=5`) | Triggered automatically by `<img src="...">` tags | Use HTTP `POST` or `DELETE` |
| Comparing tokens using standard `===` operator | Susceptible to microsecond timing side-channel attacks | Use `crypto.timingSafeEqual()` |
| Omitting `SameSite` on session cookies | Defaults may be inconsistent across older browsers | Set `SameSite=Strict` or `SameSite=Lax` explicitly |
| Storing CSRF tokens in `localStorage` | If an XSS vulnerability exists, tokens are instantly stolen | Use `SameSite=Strict` cookies or hidden form fields |

---

## 8. Performance Characteristics

- **Token Generation**: Generating a 32-byte cryptographic token via `crypto.randomBytes(32)` takes $< 5$ microseconds.
- **Timing-Safe Comparison**: `crypto.timingSafeEqual()` executes in constant time ($< 1$ microsecond), eliminating timing leaks without impacting throughput.

---

## 9. Engine Internals

1. **Browser Cookie Jar Policy Evaluation**:
   - Before dispatching an outgoing HTTP request, the browser checks the target domain against the current tab's top-level origin.
   - If the request is a cross-site POST and the cookie has `SameSite=Lax` or `SameSite=Strict`, the cookie is omitted from the request header in the network stack before network transmission.

---

## 10. Best Practices & Design Guidelines

- **Primary Defense: `SameSite=Lax` or `SameSite=Strict`**: Enforce `SameSite=Lax` by default for all session cookies, and `SameSite=Strict` for sensitive banking/auth cookies.
- **Secondary Defense: Anti-CSRF Tokens**: For applications supporting traditional HTML form submissions, enforce Synchronizer Tokens.
- **Verify `Origin` and `Referer`**: Reject any state-changing request whose `Origin` does not match your trusted server host.

---

## 11. Real-World Case Study

### Protecting an E-Commerce Password Change Endpoint
```javascript
// THE SCENARIO: A legacy e-commerce portal accepted POST `/api/change-password`.
// Because session cookies lacked `SameSite` flags and the endpoint had no CSRF tokens,
// an attacker embedded a hidden form on an external forum, forcing logged-in visitors
// to silently reset their passwords to a known password.

// THE MODERN REMEDY: Layered Defense-in-Depth
import http from 'node:http';
import crypto from 'node:crypto';

const server = http.createServer((req, res) => {
  // Step 1: Reject cross-origin requests
  const origin = req.headers['origin'];
  if (origin && new URL(origin).host !== req.headers['host']) {
    res.writeHead(403);
    return res.end('Cross-Origin Request Blocked');
  }

  if (req.method === 'POST' && req.url === '/api/change-password') {
    // Step 2: Verify custom CSRF token header
    const token = req.headers['x-csrf-token'];
    const sessionToken = req.session?.csrfToken;

    if (!token || !sessionToken || !crypto.timingSafeEqual(Buffer.from(token), Buffer.from(sessionToken))) {
      res.writeHead(403);
      return res.end('CSRF Verification Failed');
    }

    // Step 3: Require current password confirmation (Re-authentication)
    // Execute password change...
  }
});
```

---

## 12. Interview Questions

<details>
<summary>1. How does Cross-Site Request Forgery (CSRF) work?</summary>
CSRF takes advantage of the browser's automatic transmission of ambient credentials (session cookies). An attacker lures an authenticated user to a malicious site that automatically submits an HTTP request (via a hidden form or script) to the vulnerable application. Because the browser automatically attaches the user's valid session cookie, the server authenticates and executes the unauthorized request.
</details>

<details>
<summary>2. What are the three modes of the SameSite cookie attribute and how do they differ?</summary>
- <b>Strict</b>: The cookie is never sent in cross-site requests under any condition (even when following a regular link from an external site).
- <b>Lax</b>: The cookie is blocked on cross-site state-changing requests (like POST), but is sent when a user navigates to the origin via a safe top-level GET request (like clicking a link).
- <b>None</b>: The cookie is sent on all cross-site requests (requires the <code>Secure</code> attribute).
</details>

<details>
<summary>3. Why doesn't CORS prevent CSRF attacks?</summary>
CORS is designed to prevent malicious websites from <i>reading</i> data from another origin. In a CSRF attack, the attacker does not need to read the server's response; the damage is inflicted when the server <i>executes</i> the state-changing action. Simple form submissions (<code><form method="POST"></code>) execute without triggering CORS preflight checks.
</details>

<details>
<summary>4. What is the Synchronizer Token Pattern?</summary>
It is an anti-CSRF defense where the server generates a cryptographically random, unpredictable token associated with the user's current session. When rendering a form or responding to an API call, the token is provided to the client. The client must submit this token in a hidden form field or request header on state-changing requests. The server validates that the submitted token matches the session token before executing the request.
</details>

<details>
<summary>5. What is the Double Submit Cookie pattern and what is its main advantage?</summary>
In the Double Submit Cookie pattern, the server sends a random CSRF token in a cookie and requires the client JavaScript to read that token and echo it in a custom request header. The server verifies that the cookie token matches the header token. Its main advantage is that it is completely stateless—the server does not need to store tokens in server-side session memory.
</details>

<details>
<summary>6. Why must crypto.timingSafeEqual() be used to compare CSRF tokens?</summary>
Standard string equality (<code>a === b</code>) compares characters sequentially and returns <code>false</code> as soon as a mismatch is found. An attacker measuring execution times with microsecond precision can determine which character failed, allowing them to guess the token byte-by-byte (timing attack). <code>crypto.timingSafeEqual()</code> takes constant time regardless of where mismatches occur.
</details>

<details>
<summary>7. Why should state-changing actions never be performed via HTTP GET?</summary>
HTTP GET requests are intended to be safe and idempotent by specification. Browsers and search crawlers freely pre-fetch and follow GET links, and <code>SameSite=Lax</code> cookies are sent on top-level GET navigations. Using GET for state changes makes an application trivially vulnerable to simple image tag injections (<code><img src="/transfer?amount=1000"></code>).
</details>

<details>
<summary>8. How does the Origin header help protect against CSRF?</summary>
The <code>Origin</code> header is automatically added by browsers on cross-origin requests and all state-changing requests (POST, PUT, DELETE). It contains the scheme, hostname, and port of the page that initiated the request. Servers can reject requests whose <code>Origin</code> does not match their own trusted domain.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build an HMAC-Signed Stateless CSRF Token Generator
Implement a token generator `createSignedCsrfToken(sessionId, secret)` and validator `verifySignedCsrfToken(token, sessionId, secret)` using `crypto.createHmac` that allows verifying tokens statelessly without in-memory sessions.

<details>
<summary>View Solution</summary>

```javascript
import crypto from 'node:crypto';

export function createSignedCsrfToken(sessionId, secret) {
  const nonce = crypto.randomBytes(16).toString('hex');
  const payload = `${sessionId}:${nonce}`;
  const hmac = crypto.createHmac('sha256', secret).update(payload).digest('hex');
  return `${payload}:${hmac}`;
}

export function verifySignedCsrfToken(token, sessionId, secret) {
  if (typeof token !== 'string') return false;
  const parts = token.split(':');
  if (parts.length !== 3) return false;

  const [tokenSessionId, nonce, receivedHmac] = parts;
  if (tokenSessionId !== sessionId) return false;

  const expectedPayload = `${sessionId}:${nonce}`;
  const expectedHmac = crypto.createHmac('sha256', secret).update(expectedPayload).digest('hex');

  const bufReceived = Buffer.from(receivedHmac, 'hex');
  const bufExpected = Buffer.from(expectedHmac, 'hex');

  if (bufReceived.length !== bufExpected.length) return false;
  return crypto.timingSafeEqual(bufReceived, bufExpected);
}
```
</details>

### Exercise 2: Anti-CSRF Fetch Interceptor
Write an HTTP client interceptor that hooks into all mutating fetch calls (`POST`, `PUT`, `DELETE`), extracts a CSRF token from a `<meta name="csrf-token" content="...">` tag in the HTML `<head>`, and automatically attaches an `X-CSRF-Token` header.

<details>
<summary>View Solution</summary>

```javascript
export function setupCsrfInterceptor() {
  const originalFetch = window.fetch;

  window.fetch = function (resource, options = {}) {
    const method = (options.method || 'GET').toUpperCase();

    if (!['GET', 'HEAD', 'OPTIONS'].includes(method)) {
      const meta = document.querySelector('meta[name="csrf-token"]');
      const token = meta?.getAttribute('content');

      if (token) {
        options.headers = new Headers(options.headers || {});
        options.headers.set('X-CSRF-Token', token);
      }
    }

    return originalFetch(resource, options);
  };
}
```
</details>
