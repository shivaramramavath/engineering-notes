# Production JavaScript Security Checklist & Hardening Guide

> An actionable, defense-in-depth security checklist and technical implementation guide for securing modern full-stack JavaScript runtimes, APIs, and client-side web applications against critical vulnerabilities.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced / Production-Grade |
| **Prerequisites** | [01_xss.md](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security/01_xss.md), [02_csrf.md](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security/02_csrf.md), [03_prototype-pollution.md](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security/03_prototype-pollution.md), [04_input-validation.md](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security/04_input-validation.md), [05_dependency-security.md](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security/05_dependency-security.md) |
| **Standard Baseline** | ECMAScript 2024 / Node.js 20+ LTS / OWASP Top 10 & ASVS v4.0 |
| **Status** | Production Ready |

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

Securing JavaScript applications requires a proactive defense-in-depth strategy rather than reactive patching. JavaScript's dynamic typing, prototype-based inheritance, single-threaded asynchronous runtime, and rich third-party module ecosystem present unique security vectors:
- **Client-Side Exposure**: Single Page Applications (SPAs) expose all client-side code, logic, and state to the end user's browser, making token storage, DOM updates, and API consumption sensitive to injection (XSS, prototype tampering, open redirects).
- **Server-Side Surface Area**: Node.js backends run with the OS permissions of the node process. Uncaught exceptions crash the server, uncontrolled regular expressions cause catastrophic ReDoS freezing the event loop, and deserialization bugs lead to Remote Code Execution (RCE).
- **Supply Chain Vulnerabilities**: A typical production enterprise app pulls thousands of transitive npm packages, creating an expansive attack vector for malicious updates, typosquatting, and credential leaks.

A unified security checklist provides software engineers, architects, and DevOps professionals with concrete, auditable verification standards aligned with OWASP Top 10 and ASVS (Application Security Verification Standard) Level 2/3 requirements.

---

## 2. Core Concepts

### Defense-in-Depth Model
Security should not rely on a single defensive layer. If one safeguard fails (e.g., client input validation), subsequent layers (WAF, server schema validation, ORM parameterized queries, output encoding, least-privilege database user) prevent compromise.

```
       [ Client Browser / Mobile Client ]
                   │
                   ▼  TLS 1.3 / Strict-Transport-Security
           [ Edge WAF / CDN ] (Rate Limiting, DDoS, IP Filtering)
                   │
                   ▼  Secure HTTP Headers (CSP, HSTS, X-Content-Type)
      [ Reverse Proxy / API Gateway ] (TLS Termination, Auth tokens)
                   │
                   ▼  Input Sanitization & Schema Validation (Zod)
         [ Node.js Application Tier ] (Prototype Freezing, Safe Regex)
                   │
                   ▼  Parameterized Prepared Statements (No Raw SQL)
            [ Database / Storage Tier ] (Least Privilege IAM, Encryption)
```

### OWASP Top 10 Mapping for JavaScript
| Threat Category | Primary Risk in JS | Countermeasure |
| :--- | :--- | :--- |
| **A01: Broken Access Control** | Client-side routing gating admin routes; missing IDOR checks on API endpoints. | Authoritative server-side policy enforcement (`can(user, 'read', resource)`). |
| **A02: Cryptographic Failures** | Using `Math.random()` for tokens; storing plaintext secrets in Git; weak hashing (`MD5`). | Web Crypto API (`crypto.getRandomValues`), Argon2id/bcrypt, KMS secrets management. |
| **A03: Injection** | SQL/NoSQL injection, Command injection via `child_process.exec()`, XSS via `innerHTML`. | Parameterized queries, `child_process.execFile` with array args, DOMPurify, Content-Security-Policy. |
| **A04: Insecure Design** | Unlimited password attempts; rate limit omission; state mutations via GET. | Token bucket rate limiting, idempotent state-changing POST/PUT routes, account lockouts. |
| **A05: Security Misconfiguration** | Default Express settings revealing `X-Powered-By`; verbose stack traces in production. | `helmet()` middleware, stripping headers, structured production error handlers. |
| **A06: Vulnerable Dependencies** | Unpinned versions; neglected zero-day CVEs in transitive dependencies. | Dependabot, `npm audit --audit-level=high`, lockfile pinning (`npm ci`), provenance checks. |
| **A07: Identification & Auth** | Weak passwords; non-httpOnly session cookies; predictable session IDs. | Argon2id password hashing, `httpOnly`, `Secure`, `SameSite=Strict` cookies. |
| **A08: Software & Data Integrity** | Deserializing untrusted JSON payloads without validation; prototype pollution. | `Object.freeze(Object.prototype)`, schema parsing with Zod, rejecting `__proto__`. |
| **A09: Security Logging & Monitoring** | Failing to log failed logins; logging PII/tokens in plaintext logs. | Structured JSON logging (Pino) with PII redaction, centralized SIEM alerts. |
| **A10: SSRF (Server-Side Request Forgery)** | Fetching arbitrary user-provided URLs on the server (`node-fetch(req.body.url)`). | Whitelist domain schemes, resolve IP and reject RFC 1918 private IP ranges (`127.0.0.1`, `10.0.0.0/8`). |

---

## 3. How It Works Internally

### Node.js Security Architecture & Sandbox Boundaries
In Node.js, `eval()`, `new Function()`, and the `node:vm` module execute code. However, `vm.runInContext()` is explicitly **not** a security sandbox. Malicious untrusted code executed in `node:vm` can climb the prototype chain via `this.constructor.constructor('return process')()` to access the global host process and execute arbitrary shell commands.

```
Untrusted Code in VM Context
    │
    │  this.constructor.constructor("return process")()
    ▼
Global Function Constructor in Host Context
    │
    ▼
Node.js Host `process` Object
    │
    ▼
process.mainModule.require('child_process').execSync('rm -rf /')
    [ HOST SYSTEM COMPROMISED ]
```

To establish a true security boundary in Node.js:
1. Run untrusted code in an isolated child process or lightweight container with seccomp/AppArmor policies and non-root execution.
2. Use Node.js Permission Model (`--experimental-permission --allow-fs-read=/app/public`).
3. Utilize V8 Isolates via native bindings (e.g., `isolated-vm`) that enforce strict memory and CPU caps with zero prototype access to the parent isolate.

---

## 4. Syntax & API Reference

### Secure HTTP Headers Configuration with `helmet`
```javascript
import helmet from 'helmet';

// Modern production security header stack
const securityHeaders = helmet({
  contentSecurityPolicy: {
    directives: {
      defaultSrc: ["'self'"],
      scriptSrc: ["'self'", "'strict-dynamic'", (req, res) => `'nonce-${res.locals.cspNonce}'`],
      styleSrc: ["'self'", "'unsafe-inline'"],
      imgSrc: ["'self'", "data:", "https://images.example.com"],
      connectSrc: ["'self'", "https://api.example.com"],
      fontSrc: ["'self'", "https://fonts.gstatic.com"],
      objectSrc: ["'none'"],
      baseUri: ["'self'"],
      formAction: ["'self'"],
      frameAncestors: ["'none'"],
      upgradeInsecureRequests: [],
    },
  },
  crossOriginEmbedderPolicy: true,
  crossOriginOpenerPolicy: { policy: "same-origin" },
  crossOriginResourcePolicy: { policy: "same-origin" },
  dnsPrefetchControl: { allow: false },
  frameguard: { action: "deny" },
  hidePoweredBy: true,
  hsts: {
    maxAge: 31536000, // 1 year
    includeSubDomains: true,
    preload: true,
  },
  ieNoOpen: true,
  noSniff: true,
  originAgentCluster: true,
  referrerPolicy: { policy: "strict-origin-when-cross-origin" },
  xssFilter: true,
});
```

### Cryptographically Secure Random Generation
```javascript
import crypto from 'node:crypto';

// Server-side Node.js secure token generation
export function generateSecureToken(byteLength = 32) {
  return crypto.randomBytes(byteLength).toString('hex');
}

// Browser / Web Crypto API secure token generation
export function generateBrowserToken(byteLength = 32) {
  const buffer = new Uint8Array(byteLength);
  globalThis.crypto.getRandomValues(buffer);
  return Array.from(buffer, b => b.toString(16).padStart(2, '0')).join('');
}

// Constant-time string comparison to prevent timing attacks
export function constantTimeEqual(known, supplied) {
  const bufferA = Buffer.from(known, 'utf8');
  const bufferB = Buffer.from(supplied, 'utf8');

  if (bufferA.length !== bufferB.length) {
    // Prevent short-circuiting on unequal length while remaining constant-time
    crypto.timingSafeEqual(bufferA, bufferA);
    return false;
  }

  return crypto.timingSafeEqual(bufferA, bufferB);
}
```

---

## 5. Practical Examples

### Example 1: Server-Side Request Forgery (SSRF) Guard
```javascript
import dns from 'node:dns/promises';
import ipaddr from 'ipaddr.js';

export async function safeFetch(targetUrl, options = {}) {
  const parsed = new URL(targetUrl);

  // 1. Enforce HTTPS only
  if (parsed.protocol !== 'https:') {
    throw new Error('Only HTTPS protocol is allowed.');
  }

  // 2. Resolve hostname to IPv4/IPv6 addresses
  const addresses = await dns.resolve(parsed.hostname);
  if (!addresses || addresses.length === 0) {
    throw new Error('Could not resolve hostname.');
  }

  for (const addr of addresses) {
    const ip = ipaddr.parse(addr);
    const range = ip.range();

    // 3. Block private, loopback, linkLocal, carrierGradeNat, and multicast
    const blockedRanges = [
      'loopback', 'private', 'linkLocal', 'carrierNat',
      'uniqueLocal', 'broadcast', 'reserved'
    ];

    if (blockedRanges.includes(range)) {
      throw new Error(`SSRF Blocked: Destination IP ${addr} is in reserved/private range: ${range}`);
    }
  }

  // 4. Safe fetch with timeout
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), options.timeoutMs ?? 5000);

  try {
    return await fetch(targetUrl, {
      ...options,
      signal: controller.signal,
      redirect: 'manual' // Prevent open redirect to internal resource
    });
  } finally {
    clearTimeout(timeoutId);
  }
}
```

### Example 2: Express Security Hardening Middleware Stack
```javascript
import express from 'express';
import helmet from 'helmet';
import cors from 'cors';
import crypto from 'node:crypto';

export function configureAppSecurity(app) {
  // Strip Express identification
  app.disable('x-powered-by');

  // Generate per-request cryptographically secure nonce for CSP
  app.use((req, res, next) => {
    res.locals.cspNonce = crypto.randomBytes(16).toString('base64');
    next();
  });

  // Apply Helmet with dynamic nonce
  app.use(helmet({
    contentSecurityPolicy: {
      directives: {
        defaultSrc: ["'self'"],
        scriptSrc: ["'self'", (req, res) => `'nonce-${res.locals.cspNonce}'`],
      }
    }
  }));

  // Restrict CORS to explicit allowed origins
  const allowedOrigins = new Set(['https://app.example.com', 'https://admin.example.com']);
  app.use(cors({
    origin: (origin, callback) => {
      if (!origin || allowedOrigins.has(origin)) {
        callback(null, true);
      } else {
        callback(new Error('CORS violation: Origin not allowed.'));
      }
    },
    credentials: true,
    methods: ['GET', 'POST', 'PUT', 'PATCH', 'DELETE'],
    allowedHeaders: ['Content-Type', 'Authorization', 'X-Requested-With']
  }));

  // Body parser size limits to prevent Denial of Service memory exhaustion
  app.use(express.json({ limit: '100kb' }));
  app.use(express.urlencoded({ extended: false, limit: '50kb' }));
}
```

### Example 3: Cryptographic Password Hashing with Scrypt
```javascript
import crypto from 'node:crypto';

const KEY_LENGTH = 64;
const SCRYPT_OPTIONS = {
  N: 16384, // CPU/memory cost
  r: 8,     // Block size
  p: 1,     // Parallelization
  maxmem: 32 * 1024 * 1024 // 32MB max memory
};

export async function hashPassword(plainPassword) {
  const salt = crypto.randomBytes(16).toString('hex');
  return new Promise((resolve, reject) => {
    crypto.scrypt(plainPassword, salt, KEY_LENGTH, SCRYPT_OPTIONS, (err, derivedKey) => {
      if (err) return reject(err);
      resolve(`${salt}:${derivedKey.toString('hex')}`);
    });
  });
}

export async function verifyPassword(plainPassword, storedHash) {
  const [salt, key] = storedHash.split(':');
  if (!salt || !key) return false;

  return new Promise((resolve, reject) => {
    crypto.scrypt(plainPassword, salt, KEY_LENGTH, SCRYPT_OPTIONS, (err, derivedKey) => {
      if (err) return reject(err);
      const keyBuffer = Buffer.from(key, 'hex');
      resolve(crypto.timingSafeEqual(keyBuffer, derivedKey));
    });
  });
}
```

### Example 4: Logging Redaction Middleware (PII & Secret Defense)
```javascript
export function createRedactedLogger() {
  const SENSITIVE_KEYS = new Set([
    'password', 'token', 'authorization', 'secret', 'apikey',
    'creditcard', 'cvv', 'ssn', 'refreshtoken'
  ]);

  function redact(val, depth = 0) {
    if (depth > 5 || val === null || typeof val !== 'object') {
      return val;
    }

    if (Array.isArray(val)) {
      return val.map(item => redact(item, depth + 1));
    }

    const cleaned = {};
    for (const [k, v] of Object.entries(val)) {
      if (SENSITIVE_KEYS.has(k.toLowerCase())) {
        cleaned[k] = '[REDACTED]';
      } else {
        cleaned[k] = redact(v, depth + 1);
      }
    }
    return cleaned;
  }

  return {
    info: (msg, metadata = {}) => console.log(JSON.stringify({ level: 'INFO', msg, meta: redact(metadata), ts: new Date().toISOString() })),
    error: (msg, err, metadata = {}) => console.error(JSON.stringify({
      level: 'ERROR',
      msg,
      error: err instanceof Error ? { message: err.message, stack: err.stack } : err,
      meta: redact(metadata),
      ts: new Date().toISOString()
    }))
  };
}
```

### Example 5: Defense-in-Depth Child Process Command Execution
```javascript
import { execFile } from 'node:child_process';
import path from 'node:path';

// SAFE: Uses execFile (not exec/shell) and requires explicit absolute executable path and array args
export function safeConvertImage(inputFileName, outputFileName) {
  // 1. Sanitize file basenames to prevent path traversal
  const safeIn = path.basename(inputFileName);
  const safeOut = path.basename(outputFileName);

  const fullIn = path.resolve('/var/app/uploads', safeIn);
  const fullOut = path.resolve('/var/app/processed', safeOut);

  return new Promise((resolve, reject) => {
    // Never run through /bin/sh. Arguments are passed as direct system argv array.
    execFile(
      '/usr/bin/convert',
      [fullIn, '-resize', '800x800', fullOut],
      {
        timeout: 10000,
        maxBuffer: 1024 * 1024 * 2, // 2MB max stdout/stderr
        shell: false // CRITICAL: disables shell invocation, preventing command injection
      },
      (error, stdout, stderr) => {
        if (error) {
          reject(new Error(`Image conversion failed: ${stderr || error.message}`));
        } else {
          resolve(fullOut);
        }
      }
    );
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`Math.random()` in Cryptographic Contexts**:
   `Math.random()` uses the xorshift128+ PRNG algorithm in V8. After observing 2–3 consecutive float outputs, an attacker can reconstruct the internal 128-bit state seeds and predict every future "random" output (e.g., password reset tokens, MFA codes, session IDs). Always use `crypto.randomBytes()` or `crypto.getRandomValues()`.

2. **Regex Denial of Service (ReDoS) Freezing the Event Loop**:
   Because Node.js runs JavaScript on a single thread, a catastrophic backtracking regular expression (e.g., `/^(a+)+$/`) executing against input `'aaaaaaaaaaaaaaaaaaaaaaX'` consumes 100% of the V8 isolate CPU for minutes, blocking every other user on the server. Always test regular expressions with tools like `safe-regex2` or use RE2 native bindings.

3. **Insecure Path Traversal via `path.resolve`**:
   `path.resolve('/base/dir', userProvidedInput)` will treat `userProvidedInput` as the root if it starts with a leading slash `/` on POSIX (e.g., `path.resolve('/data', '/etc/passwd')` returns `'/etc/passwd'`). Always verify that `resolvedPath.startsWith(allowedBasePath)`.

4. **JSON Serialization Infinite Recursion / DoS**:
   Deeply nested JSON objects (e.g., 5,000 nested arrays `[[[[...]]]]`) sent in an HTTP request body can cause `JSON.parse()` to exceed the V8 call stack size, throwing an unhandled `RangeError: Maximum call stack size exceeded` and crashing the Node.js process. Enforce strict `express.json({ limit: '100kb' })` body limits.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Security Risk | Production Remediation |
| :--- | :--- | :--- |
| Storing JWTs in `localStorage` | Any XSS immediately compromises user session tokens. | Store session tokens in `httpOnly`, `Secure`, `SameSite=Strict` cookies. |
| `child_process.exec(cmd)` with user input | Direct arbitrary command injection on host OS. | Use `child_process.execFile()` with `shell: false` and array arguments. |
| Unsafe redirect `res.redirect(req.query.url)` | Open redirect leads to phishing and credential harvesting. | Match redirect targets against an explicit whitelist of relative URLs or domains. |
| Insecure deserialization via `serialize-javascript` / `eval` | Remote Code Execution via object prototype or function injection. | Use standard `JSON.parse()` combined with strict schema validation (Zod/Valibot). |
| Default `cors({ origin: '*' })` with credentials | Sensitive cross-origin data exfiltration. | Specify exact domain origins; never combine wildcard origins with `credentials: true`. |
| Unhandled Promise Rejections | Crashing the Node.js process in modern LTS versions. | Register global `process.on('unhandledRejection')` and wrap async route handlers. |

---

## 8. Performance Characteristics

- **Cryptographic Operations Overhead**: Symmetric encryption (`AES-256-GCM`) and hashing (`SHA-256`) in `node:crypto` execute in OpenSSL C++ bindings outside the V8 heap, making them exceptionally fast (hundreds of thousands of ops/sec). Password hashing functions (`Argon2id`, `scrypt`, `bcrypt`) are intentionally compute- and memory-intensive to defeat GPU brute-forcing. Offload password hashing to worker pools or background queues to prevent event loop starvation under high concurrency.
- **CSP & Security Headers Overhead**: Adding HTTP security headers adds less than 1KB of HTTP header metadata per response and requires zero runtime CPU in V8 after initial middleware instantiation.
- **Constant-Time Comparison**: `crypto.timingSafeEqual` performs an $O(N)$ byte-by-byte comparison without short-circuiting on mismatch. The performance penalty is negligible (sub-microsecond) while completely neutralizing remote side-channel timing attacks.

---

## 9. Engine Internals

### V8 Heap Isolation & Security Permissions
Modern Node.js incorporates the native **Permission Model** (`--experimental-permission`), which restricts the capabilities of the V8 engine at the C++ embedding layer:
- **Filesystem Constraints**: `--allow-fs-read` and `--allow-fs-write` intercept native libuv filesystem calls (`uv_fs_open`, `uv_fs_read`) before any kernel syscall is made. If a malicious script attempts path traversal, libuv aborts with `ERR_ACCESS_DENIED`.
- **Worker Thread Restrictions**: Worker threads spawned via `worker_threads` run in separate V8 Isolates with isolated heaps, preventing prototype leakage across worker boundaries.

```
       JavaScript User Code
               │
               ▼
   Node.js C++ Binding Layer
               │
   [ Permission Manager Check ] ─── DENIED ──► Throw ERR_ACCESS_DENIED
               │ (ALLOWED)
               ▼
   libuv Asynchronous Thread Pool
               │
               ▼
   Operating System Kernel Syscall (open, read, write)
```

---

## 10. Best Practices & Design Guidelines

### Production Hardening Checklist
- [ ] **1. Transport Security**: TLS 1.3 enforced on all endpoints; HSTS header enabled with `includeSubDomains` and `preload`.
- [ ] **2. HTTP Headers**: `helmet` enabled with custom Content-Security-Policy, `frameguard: deny`, `noSniff: true`.
- [ ] **3. Authentication & Sessions**: Passwords hashed using Argon2id or scrypt with per-user salt; session cookies configured with `HttpOnly`, `Secure`, `SameSite=Strict`.
- [ ] **4. Prototype Defense**: Execute `Object.freeze(Object.prototype)` at application entry or run with `--disable-proto=delete`.
- [ ] **5. Input Sanitization**: All incoming query params, route params, and JSON bodies validated against strict Zod/Valibot schemas.
- [ ] **6. Output Encoding**: All HTML dynamic outputs sanitized using DOMPurify; no dynamic `innerHTML` or `dangerouslySetInnerHTML`.
- [ ] **7. Supply Chain Security**: `package-lock.json` committed; CI runs `npm audit --audit-level=high` and uses `--ignore-scripts` during installation.
- [ ] **8. Execution Safety**: No `child_process.exec()`, `eval()`, or `new Function()`; regex validated against ReDoS.
- [ ] **9. SSRF Defense**: Server-side outbound HTTP requests resolve target DNS and reject RFC 1918 private IP ranges.
- [ ] **10. Logging & Monitoring**: PII and secrets redacted in all application logs; centralized error alerting and rate-limiting monitoring active.

---

## 11. Real-World Case Study

### High-Traffic Fintech API Hardening Post-Mortem
**Incident**: An online fintech platform experienced an event loop starvation event followed by unauthorized database record exfiltration.
**Root Cause Analysis**:
1. An unauthenticated feedback API endpoint accepted user-provided text evaluated against a catastrophic regex: `/^([a-zA-Z0-9]+\s?)+$/`. An attacker sent a payload of 45 characters, causing the single-threaded Node.js event loop to freeze for 180 seconds, triggering an automated container restart loop (Denial of Service).
2. During failover, an internal debug endpoint accepting a webhook callback URL (`POST /api/webhooks/test { "url": "..." }`) fetched the provided URL server-side without validation, allowing the attacker to query the AWS Cloud metadata service (`http://169.254.169.254/latest/meta-data/iam/security-credentials/`) and steal IAM instance role credentials (SSRF).

**Resolution Architecture**:
- Replaced vulnerable regex with a linear-time tokenization parser and added `safe-regex2` validation in the CI pipeline.
- Wrapped all outbound webhook requests with an SSRF guard library verifying that DNS resolution targets only public, non-reserved IPv4/IPv6 CIDR blocks.
- Applied Node.js rate limiting (`express-rate-limit` with Redis store) capping IP calls to 60 req/min.
- Enabled AWS IMDSv2 requiring token-based session headers, neutralizing SSRF attempts against metadata endpoints.

---

## 12. Interview Questions

<details>
<summary>1. Why is <code>Math.random()</code> insecure for generating session tokens or password reset links?</summary>

`Math.random()` in V8 uses the xorshift128+ pseudo-random number generator, which is designed for speed, not cryptographic unpredictability. Its internal state consists of only 128 bits. By observing a small sequence of generated floating-point numbers, an attacker can solve the linear recurrence equations, determine the internal PRNG state, and predict all past and future outputs. Cryptographic security requires high-entropy random generation via `crypto.randomBytes()` (Node.js) or `crypto.getRandomValues()` (Web Crypto API), which rely on operating system entropy pools (`/dev/urandom` or Windows `BCryptGenRandom`).
</details>

<details>
<summary>2. How does <code>crypto.timingSafeEqual()</code> protect against timing attacks during secret validation?</summary>

Standard string comparison (`===` or `strA == strB`) short-circuits: it compares character-by-character from left to right and returns `false` as soon as the first non-matching byte is found. An attacker measuring HTTP response times down to microsecond accuracy can determine how many leading characters of a supplied secret match the true secret, reducing the brute-force search space from exponential ($O(26^N)$) to linear ($O(26 \times N)$). `crypto.timingSafeEqual()` compares all bytes across the entire buffer length using constant-time bitwise operations, taking the exact same amount of CPU cycles regardless of where or whether a mismatch occurs.
</details>

<details>
<summary>3. Why is Node.js's <code>vm</code> module not a secure sandbox for executing untrusted user code?</summary>

The `vm` module provides contexts for running scripts, but it does not isolate the V8 prototype chain from the host environment. Any object passed into the VM context, or the context's own `this` reference, can walk up its prototype chain:
```javascript
const untrusted = `this.constructor.constructor('return process')()`;
```
`this.constructor` reaches the context's `Object` constructor; `.constructor` reaches the host `Function` constructor; calling it returns the host `process` object, granting the guest code full access to the underlying OS filesystem, network, and shell. True sandboxing requires isolated processes or native V8 isolates (e.g., `isolated-vm`).
</details>

<details>
<summary>4. What is the difference between <code>child_process.exec</code> and <code>child_process.execFile</code> regarding security?</summary>

`child_process.exec` spawns a system shell (e.g., `/bin/sh` on Unix or `cmd.exe` on Windows) and passes the command string directly to the shell parser. If user input is concatenated into the string, the shell interprets metacharacters (`;`, `&`, `|`, `` ` ``, `$()`), allowing arbitrary command injection. In contrast, `child_process.execFile` directly invokes the specified executable binary via the operating system's `execve` syscall without invoking a shell. Arguments are passed as an array of strings, ensuring user input is treated strictly as literal data arguments and never interpreted as shell command delimiters.
</details>

<details>
<summary>5. How does Content Security Policy (CSP) with nonces protect against Cross-Site Scripting (XSS)?</summary>

A strict nonce-based CSP instructs the browser to execute only `<script>` elements that possess a matching `nonce="<random-base64>"` attribute that was generated dynamically by the server for that specific HTTP response. Even if an attacker successfully injects a `<script>` tag via an XSS vulnerability, the browser blocks execution because the injected tag lacks the unpredictable, per-request nonce.
</details>

<details>
<summary>6. What is Server-Side Request Forgery (SSRF) in a Node.js application, and how do you mitigate it?</summary>

SSRF occurs when a Node.js server fetches a remote resource based on a user-controlled URL (e.g., fetching an image avatar or testing a webhook). If unvalidated, an attacker can specify internal network URLs such as `http://127.0.0.1:8080/admin` or `http://169.254.169.254/latest/meta-data/` to interact with private services or steal cloud credentials. Mitigation requires parsing the URL, resolving the domain to an IP address via DNS, and rejecting connections if the resolved IP falls within loopback, private (RFC 1918), or link-local address spaces before initiating the request.
</details>

<details>
<summary>7. Why should <code>SameSite=Strict</code> or <code>SameSite=Lax</code> be used on authentication cookies?</summary>

The `SameSite` cookie attribute controls whether cookies are sent with cross-site requests. `SameSite=None` sends cookies on all cross-site requests, leaving the application vulnerable to Cross-Site Request Forgery (CSRF). `SameSite=Strict` completely prevents the cookie from being sent on any cross-site request (including clicking external links). `SameSite=Lax` permits cookies only on top-level safe GET navigations while withholding them on cross-site POST/PUT/DELETE requests, mitigating CSRF attacks while maintaining user convenience when navigating from external links.
</details>

<details>
<summary>8. What is ReDoS (Regular Expression Denial of Service), and how does it specifically threaten Node.js?</summary>

ReDoS occurs when an evaluation of a vulnerable regular expression exhibits polynomial or exponential backtracking when matched against a crafted input string. Because Node.js utilizes a single-threaded event loop to execute JavaScript, a long-running CPU-intensive regex operation blocks the entire event loop thread. While the regex executes, no other asynchronous I/O callbacks, HTTP requests, or timers can run, causing complete denial of service for all connected clients across the entire Node.js server process.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Constant-Time API Key Verifier
**Specification**: Write a middleware function `verifyApiKey(knownApiKey)` that extracts the `X-API-Key` header from an incoming HTTP request and compares it against `knownApiKey` using `crypto.timingSafeEqual` without throwing if lengths differ.

<details>
<summary>Solution</summary>

```javascript
import crypto from 'node:crypto';

export function createApiKeyMiddleware(validApiKey) {
  const validBuffer = Buffer.from(validApiKey, 'utf8');

  return function apiKeyMiddleware(req, res, next) {
    const providedKey = req.headers['x-api-key'];

    if (!providedKey || typeof providedKey !== 'string') {
      return res.status(401).json({ error: 'Missing or invalid API key.' });
    }

    const providedBuffer = Buffer.from(providedKey, 'utf8');

    // To prevent timing leaks when lengths differ, compare validBuffer against itself
    // before returning false, ensuring constant execution time.
    if (providedBuffer.length !== validBuffer.length) {
      crypto.timingSafeEqual(validBuffer, validBuffer);
      return res.status(401).json({ error: 'Unauthorized.' });
    }

    const isValid = crypto.timingSafeEqual(validBuffer, providedBuffer);
    if (!isValid) {
      return res.status(401).json({ error: 'Unauthorized.' });
    }

    next();
  };
}
```
</details>

### Exercise 2: Implement a Safe Path Resolution Validator
**Specification**: Implement `getSafeFilePath(baseDirectory, userRelativePath)` that resolves the requested file path and guarantees it resides strictly within `baseDirectory`, preventing directory traversal attacks (e.g., `../../etc/passwd`).

<details>
<summary>Solution</summary>

```javascript
import path from 'node:path';

export function getSafeFilePath(baseDirectory, userRelativePath) {
  // Normalize and resolve base directory to absolute path
  const root = path.resolve(baseDirectory);

  // Prevent leading slashes from resetting path to filesystem root
  const sanitizedInput = userRelativePath.replace(/^(\/|\\)+/, '');

  // Resolve target path
  const target = path.resolve(root, sanitizedInput);

  // Enforce root boundary with trailing separator to prevent partial folder matches
  const rootWithSep = root.endsWith(path.sep) ? root : root + path.sep;

  if (target !== root && !target.startsWith(rootWithSep)) {
    throw new Error(`Security Violation: Path traversal detected outside root directory.`);
  }

  return target;
}
```
</details>
