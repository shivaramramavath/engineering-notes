# Common Vulnerabilities

The attacks that hit real Node.js applications most often, how each one works, and the specific code change that prevents it. The list follows the spirit of the OWASP Top 10.

## How to read this file

Every section has the same shape: **the attack → vulnerable code (❌) → fixed code (✅) → the rule to remember.** Most vulnerabilities come down to one mistake: *treating untrusted input as trusted.*

---

## 1. SQL injection

User input is concatenated into a query, so the input becomes part of the SQL itself.

```js
// ❌ email = "' OR '1'='1" turns the WHERE clause into something always true
const result = await pool.query(
  `SELECT * FROM users WHERE email = '${req.body.email}'`
);
```

```js
// ✅ parameterized query — the value is sent separately from the SQL text
const result = await pool.query(
  "SELECT * FROM users WHERE email = $1",
  [req.body.email]
);
```

**Rule:** never build SQL with template strings or `+`. Always use placeholders (`$1` in `pg`, `?` in `mysql2`). ORMs and query builders do this for you, but raw-query escape hatches (`sequelize.query`, `prisma.$queryRawUnsafe`) do not. Covered in `07-databases/postgresql/01-connection-and-queries.md`.

---

## 2. NoSQL injection

MongoDB queries are objects, so an attacker can send an *object* where you expected a *string*.

```js
// ❌ POST body: { "email": "a@b.com", "password": { "$ne": null } }
// password: { $ne: null } matches any user whose password is not null
const user = await User.findOne({
  email: req.body.email,
  password: req.body.password,
});
```

```js
// ✅ validate types first, and never compare passwords in the query
import { z } from "zod";

const schema = z.object({
  email: z.string().email(),
  password: z.string().min(1),
});

const { email, password } = schema.parse(req.body);   // rejects objects
const user = await User.findOne({ email });
const ok = user && await bcrypt.compare(password, user.passwordHash);
```

**Rule:** validate that strings are strings. Schema validation (`zod`, `joi`) kills this entire class of attack. See `09-api-development/04-validation.md`.

---

## 3. Command injection

User input reaches a shell.

```js
import { exec, execFile } from "node:child_process";

// ❌ filename = "a.txt; rm -rf /" — the shell runs both commands
exec(`convert ${req.query.filename} out.png`);
```

```js
// ✅ execFile passes arguments as an array; no shell is involved
execFile("convert", [req.query.filename, "out.png"], (err) => { /* ... */ });
```

**Rule:** avoid `exec` and `shell: true` with any user-influenced value. Prefer `execFile` / `spawn` with an argument array, and validate the input against an allow-list as well. See `02-core-modules/09-child-process.md`.

---

## 4. Cross-site scripting (XSS)

Attacker-supplied text is rendered as HTML/JavaScript in another user's browser, letting the attacker run code as that user (steal tokens, act on their behalf).

```js
// ❌ a comment containing <script>...</script> runs in every visitor's browser
app.get("/comments", async (req, res) => {
  const comments = await getComments();
  res.send(comments.map((c) => `<p>${c.text}</p>`).join(""));
});
```

```js
// ✅ 1. use a template engine that escapes by default (EJS <%= %>, Handlebars {{ }}, React)
// ✅ 2. or escape manually
function escapeHtml(str) {
  return str
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}
```

Layers of defense against XSS:

- **Escape output** for the context it lands in (HTML body, attribute, URL, JavaScript).
- **Content Security Policy** restricts which scripts may run — see `07-helmet.md`.
- **`HttpOnly` cookies** mean injected script cannot read your session or refresh token.
- If you *must* accept rich HTML, sanitize with a library like `sanitize-html` or `DOMPurify`, never a hand-written regex.

**Rule:** a pure JSON API is mostly safe from XSS on its own, but the front-end consuming it must still escape. Never return user content with `Content-Type: text/html`.

---

## 5. Cross-site request forgery (CSRF)

A malicious site makes the victim's browser send a request to *your* site, and the browser helpfully attaches the victim's cookies.

```html
<!-- on evil.com: submitting this form sends the victim's bank cookies along -->
<form action="https://bank.example/transfer" method="POST">
  <input type="hidden" name="to" value="attacker" />
  <input type="hidden" name="amount" value="1000" />
</form>
<script>document.forms[0].submit()</script>
```

Defenses:

```js
// ✅ 1. SameSite cookies (the most effective single step)
res.cookie("sid", value, { httpOnly: true, secure: true, sameSite: "lax" });

// ✅ 2. never change state on GET requests
app.post("/transfer", handler);       // not app.get

// ✅ 3. for cookie-authenticated apps, also use a CSRF token (double-submit or
//       synchronizer token) or require a custom header that cross-site forms can't set
```

**Rule:** CSRF only matters when authentication is sent *automatically* (cookies). APIs that read a token from the `Authorization` header are not vulnerable to it, because the browser doesn't attach that header on its own. See `03-sessions.md` and `05-http-web/04-cors.md` (CORS does **not** protect against CSRF by itself).

---

## 6. Broken access control and IDOR

**Insecure Direct Object Reference:** the app checks that you're logged in, but not that the resource is *yours*. This is the most common serious vulnerability in APIs.

```js
// ❌ any logged-in user can read ANY invoice by changing the id in the URL
app.get("/invoices/:id", requireAuth, async (req, res) => {
  const invoice = await Invoice.findById(req.params.id);
  res.json(invoice);
});
```

```js
// ✅ scope the query to the authenticated user
app.get("/invoices/:id", requireAuth, async (req, res) => {
  const invoice = await Invoice.findOne({
    _id: req.params.id,
    ownerId: req.user.id,
  });
  if (!invoice) return res.sendStatus(404);   // 404, not 403: don't reveal it exists
  res.json(invoice);
});
```

**Rule:** authentication answers "who are you?"; every route that touches a resource must also answer "is this yours / are you allowed?". Do the check on the server, on every request, in the same query that fetches the data. Hard-to-guess IDs (UUIDs) help but are **not** a substitute. Details in `06-express/06-auth-and-authorization.md`.

---

## 7. Mass assignment

The app copies the whole request body into the database, so the attacker sets fields they shouldn't.

```js
// ❌ POST { "name": "Sam", "role": "admin" } → instant admin
const user = await User.create(req.body);
```

```js
// ✅ pick only the fields you allow (an allow-list)
const { name, email, password } = schema.parse(req.body);
const user = await User.create({ name, email, passwordHash: await hash(password) });
```

**Rule:** never pass `req.body` straight to `create`/`update`. Whitelist fields explicitly, ideally via a validation schema that strips unknown keys (`zod`'s `.strict()` or default stripping).

---

## 8. Server-side request forgery (SSRF)

Your server fetches a URL the user provided, so the attacker points it at internal resources (cloud metadata at `169.254.169.254`, `localhost` admin panels, private networks).

```js
// ❌ GET /preview?url=http://169.254.169.254/latest/meta-data/ → leaks cloud credentials
const response = await fetch(req.query.url);
```

```js
// ✅ allow-list of hosts, https only, and resolve/validate before fetching
const ALLOWED_HOSTS = new Set(["images.example.com", "cdn.example.com"]);

const url = new URL(req.query.url);
if (url.protocol !== "https:" || !ALLOWED_HOSTS.has(url.hostname)) {
  return res.status(400).json({ error: "URL not allowed" });
}
const response = await fetch(url, { redirect: "error" });   // don't follow redirects blindly
```

**Rule:** an allow-list beats a block-list. If users truly may supply arbitrary URLs (webhooks, link previews), run the fetch from an isolated network segment, block private IP ranges *after* DNS resolution, and cap size and time. See `09-api-development/07-webhooks.md`.

---

## 9. Path traversal

User input controls a file path, and `../` escapes the intended folder.

```js
import path from "node:path";
import fs from "node:fs/promises";

// ❌ GET /files?name=../../etc/passwd
const data = await fs.readFile(`./uploads/${req.query.name}`);
```

```js
// ✅ resolve the path, then verify it is still inside the allowed directory
const UPLOAD_DIR = path.resolve("uploads");
const target = path.resolve(UPLOAD_DIR, req.query.name);

if (!target.startsWith(UPLOAD_DIR + path.sep)) {
  return res.sendStatus(400);
}
const data = await fs.readFile(target);
```

**Rule:** never join raw user input into a file path. Better still, store files under generated names (UUIDs) and keep the original filename only in the database. See `02-core-modules/02-path.md` and `06-express/07-file-upload.md`.

---

## 10. Prototype pollution

Merging untrusted objects lets an attacker modify `Object.prototype`, changing behavior across the entire process.

```js
// ❌ naive recursive merge
function merge(target, source) {
  for (const key in source) {
    if (typeof source[key] === "object") {
      target[key] = merge(target[key] ?? {}, source[key]);
    } else {
      target[key] = source[key];
    }
  }
  return target;
}

// body: {"__proto__": {"isAdmin": true}}
merge({}, JSON.parse(req.body));
({}).isAdmin;   // true — every object in the app is now "admin"
```

```js
// ✅ skip dangerous keys, or use Object.create(null) / Map, or validate with a schema
const FORBIDDEN = new Set(["__proto__", "constructor", "prototype"]);

for (const key of Object.keys(source)) {
  if (FORBIDDEN.has(key)) continue;
  // ...
}
```

**Rule:** don't hand-write deep-merge utilities for user input; use schema validation, and keep dependencies updated (`lodash.merge` and others have had this bug). See `03-javascript-for-node/04-prototypes.md`.

---

## 11. Regular expression denial of service (ReDoS)

A badly written regex takes exponential time on crafted input, and since Node is single-threaded, one request can freeze the whole server.

```js
// ❌ nested quantifiers: "aaaaaaaaaaaaaaaaaaaaaaaa!" can take minutes
const bad = /^(a+)+$/;
bad.test(userInput);
```

**Prevention:**

- Avoid nested quantifiers like `(a+)+` or `(.*)*`.
- Limit input length *before* running any regex.
- Use well-tested validators (`zod`, `validator`) instead of custom email/URL regexes.
- Consider `re2` (linear-time engine) for regexes that must process untrusted text.

See `15-performance/01-event-loop-performance.md` for why blocking the event loop is so damaging.

---

## 12. Sensitive data exposure

Secrets and personal data leak through responses, logs, and Git history.

```js
// ❌ returns the password hash, reset token, and everything else
res.json(user);

// ✅ return an explicit shape
res.json({ id: user.id, name: user.name, email: user.email });
```

```js
// ❌ stack traces and internals sent to the client
app.use((err, req, res, next) => res.status(500).json({ error: err.stack }));

// ✅ log details server-side, send a generic message
app.use((err, req, res, next) => {
  req.log?.error({ err }, "Unhandled error");
  res.status(500).json({ error: "Internal server error" });
});
```

Checklist:

- Never log passwords, tokens, `Authorization` headers, or full card numbers (redact them in your logger — `14-logging-observability/01-pino-and-structured-logging.md`).
- Keep `.env` in `.gitignore`; if a secret was ever committed, **rotate it** — deleting the commit is not enough.
- Disable `X-Powered-By` (`07-helmet.md`).
- Always use HTTPS; set HSTS.

---

## 13. User enumeration and timing attacks

Different responses for "user not found" and "wrong password" tell an attacker which emails are registered. Comparing secrets with `===` leaks information through timing differences.

```js
// ❌ tells the attacker the account exists
if (!user) return res.status(404).json({ error: "No such user" });
if (!match) return res.status(401).json({ error: "Wrong password" });

// ✅ same message, same status, same work either way (see 01-password-hashing.md)
return res.status(401).json({ error: "Invalid email or password" });
```

```js
import crypto from "node:crypto";

// ❌ early exit on the first differing character
if (providedToken === expectedToken) { /* ... */ }

// ✅ constant-time comparison (buffers must be equal length)
const a = Buffer.from(providedToken);
const b = Buffer.from(expectedToken);
const equal = a.length === b.length && crypto.timingSafeEqual(a, b);
```

The same applies to password reset ("if that email exists, we've sent a link") and registration flows.

---

## 14. Open redirect

A login page redirects to a `?next=` URL taken straight from the query string, so attackers craft links that start on your trusted domain and end on a phishing page.

```js
// ❌ /login?next=https://evil.com
res.redirect(req.query.next);

// ✅ only allow relative, same-site paths
const next = String(req.query.next ?? "/");
const safe = next.startsWith("/") && !next.startsWith("//");
res.redirect(safe ? next : "/");
```

---

## 15. Vulnerable dependencies and supply-chain attacks

Most of the code in your app is not yours. A single compromised or outdated package can undo all your careful work.

```bash
npm audit                    # list known vulnerabilities in your dependency tree
npm audit fix                # apply safe fixes
npm outdated                 # see what's behind
npm ci                       # install exactly what's in the lockfile (use in CI/production)
```

Good habits:

- **Commit `package-lock.json`** and install with `npm ci` in CI and Docker builds.
- Turn on automated updates (Dependabot or Renovate) and review the PRs.
- Prefer fewer, well-maintained dependencies; check download counts, maintainers, and last publish date before adding one.
- Watch out for **typosquatting** (`expresss`, `lodahs`) when typing `npm install`.
- Consider `npm install --ignore-scripts` for untrusted packages, since install scripts run arbitrary code.
- Enable 2FA on your own npm account (see `04-npm-ecosystem/03-publishing-a-package.md`).

---

## Quick reference table

| Vulnerability | Root cause | Primary fix |
|---|---|---|
| SQL injection | String-built queries | Parameterized queries |
| NoSQL injection | Objects accepted where strings expected | Schema validation |
| Command injection | Input reaches a shell | `execFile` + argument array, allow-list |
| XSS | Unescaped output | Escape by default, CSP, `HttpOnly` |
| CSRF | Cookies sent automatically | `SameSite`, CSRF tokens, no state change on GET |
| IDOR | Missing ownership check | Scope every query to the user |
| Mass assignment | `req.body` passed to the DB | Allow-list fields |
| SSRF | Server fetches user-supplied URL | Host allow-list, block private ranges |
| Path traversal | User input in file paths | `path.resolve` + prefix check |
| Prototype pollution | Unsafe deep merge | Block `__proto__`, validate schema |
| ReDoS | Catastrophic regex | Simple regexes, length limits |
| Data exposure | Returning/logging too much | Explicit response shapes, redaction |
| Enumeration / timing | Different responses, `===` | Generic messages, `timingSafeEqual` |
| Open redirect | Unvalidated redirect target | Allow only relative paths |
| Vulnerable deps | Outdated/untrusted packages | `npm audit`, lockfile, automated updates |

## Next

**`06-rate-limiting.md`** covers the layer that slows attackers down even when the code behind it has a weakness — especially on login and password-reset endpoints.
