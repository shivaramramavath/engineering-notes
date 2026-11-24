# Node.js Technical Interview: Authentication, Crypto & Security Q&A

> Critical enterprise security questions and architectural defenses covering JWT vs Session tokens, asymmetric cryptography, refresh token replay detection, prototype pollution, ReDoS, and SSRF.

---

### Q1: Compare Stateless JWTs vs Stateful Database/Redis Sessions. When should you choose one over the other?

#### In-Depth Answer

| Dimension | Stateless JWT | Stateful Session (Redis/DB) |
| :--- | :--- | :--- |
| **Storage Location** | Client-side (Memory / HTTP-only Cookie) | Server-side (Redis cluster) |
| **Revocation** | **Extremely difficult**: Valid until expiration timestamp (`exp`). Revocation requires maintaining a centralized blocklist in Redis, which negates statelessness! | **Instantaneous**: Deleting the session key in Redis revokes the user's access immediately across all devices. |
| **Payload Size** | Large (hundreds of bytes sent on every HTTP request header). | Small (opaque session ID string, e.g. 32-byte hex). |
| **Microservices** | Independent offline verification across microservices using a shared public key (JWKS) without DB round-trips. | Requires microservices to query the central Redis session store for every request. |

#### Industry Best Practice (Hybrid Approach):
- **Short-lived Stateless Access Token (15 minutes)**: JWT signed with RS256/EdDSA, verified offline by internal microservices.
- **Long-lived Stateful Refresh Token (7–30 days)**: Opaque token stored in an `httpOnly`, `secure`, `SameSite=Strict` cookie, tracked in Redis with session rotation and replay detection.

---

### Q2: Why is symmetric signing (HS256) dangerous in microservice architectures compared to asymmetric signing (RS256 / EdDSA)?

#### In-Depth Answer
- **HS256 (HMAC with SHA-256)**: Uses a **single shared secret key** for both signing and verifying tokens.
  - *The Danger*: In a microservice mesh with 20 downstream services, **every single microservice must have access to the secret key** to verify inbound requests.
  - If a low-security microservice is compromised (e.g. via an RCE vulnerability), the attacker gains the private signing key and can **forge administrative tokens for the entire enterprise**.
- **RS256 / Ed25519 (Asymmetric)**: Uses a **private/public keypair**:
  - The Auth Service retains the **Private Key** (used exclusively to sign tokens).
  - Downstream microservices only possess the **Public Key** (distributed via standard JWKS endpoints: `/.well-known/jwks.json`).
  - Even if all 20 microservices are compromised, the attacker cannot forge tokens because they only hold the public verification key.

---

### Q3: What is Refresh Token Rotation and how does Automatic Replay Detection protect users from token theft?

#### In-Depth Answer
In a standard refresh flow, a stolen refresh token gives an attacker perpetual access.
**Token Rotation with Family Tracking**:
1. When a user logs in, the auth service creates a **Token Family** (e.g. `Family_A`) and issues `Token_1`.
2. When the legitimate client sends `Token_1` to refresh:
   - The auth server invalidates `Token_1`.
   - The auth server issues `Token_2` belonging to `Family_A`.
3. **Replay Attack Scenario**:
   - An attacker intercepted `Token_1` previously and attempts to use it.
   - The auth server inspects Redis and discovers that `Token_1` was **already consumed**!
   - This proves that either the client or the network has been breached.
   - **Automatic Retaliation**: The auth server immediately revokes the **entire Token Family (`Family_A`)**, invalidating all active sessions for that user and forcing re-authentication.

---

### Q4: What is Prototype Pollution in Node.js? Provide an exploit example and modern defenses.

#### In-Depth Answer
**Prototype Pollution** occurs when an attacker injects properties into `Object.prototype`, which are then inherited by every JavaScript object across the entire Node.js runtime process.

#### Exploit Example (Recursive Deep Merge):
```javascript
// Vulnerable recursive merge function:
function merge(target, source) {
  for (let key in source) {
    if (typeof target[key] === 'object' && typeof source[key] === 'object') {
      merge(target[key], source[key]);
    } else {
      target[key] = source[key];
    }
  }
}

// Attacker-supplied JSON payload:
const maliciousPayload = JSON.parse('{"__proto__": {"isAdmin": true}}');
merge({}, maliciousPayload);

// Now EVERY object in the process inherits isAdmin:
const regularUser = {};
console.log(regularUser.isAdmin); // Prints: true! (System compromised)
```

#### Defenses:
1. **Use `Object.create(null)`**: Creates dictionary objects that do not inherit from `Object.prototype`.
2. **Use ES6 `Map`**: Maps do not have prototype inheritance vulnerabilities.
3. **Freeze the Prototype**: `Object.freeze(Object.prototype)` prevents runtime modification.
4. **Key Filtering**: Strip or reject keys matching `__proto__`, `constructor`, or `prototype` in merge utilities.
5. **Runtime Flag**: Run Node.js with `--disable-proto=delete` or `--disable-proto=throw`.

---

### Q5: What is ReDoS (Regular Expression Denial of Service) and how do you protect a Node.js event loop from it?

#### In-Depth Answer
Because Node.js executes JavaScript on a single thread, an inefficient regular expression with **catastrophic backtracking** (often $O(2^n)$ exponential complexity) can lock up the V8 main thread for minutes or hours on a 30-character string, freezing the entire server.

#### Catastrophic Backtracking Example:
```javascript
// Vulnerable regex matching grouped alternatives with repetition:
const evilRegex = /^(a+)+$/;
evilRegex.test('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa!');
// Locks the Node.js event loop 100% CPU indefinitely!
```

#### Defenses:
1. **Linting**: Use `eslint-plugin-regexp` to detect vulnerable regexes at compile time.
2. **Safe Regex Engines**: Use linear-time regular expression engines like `re2` (Google's non-backtracking automaton engine).
3. **Input Length Limits**: Validate string length with Zod (`z.string().max(100)`) before executing regexes.
4. **Node.js Timeout Flag**: Starting in modern Node.js, `RegExp` supports timeout abort options.
