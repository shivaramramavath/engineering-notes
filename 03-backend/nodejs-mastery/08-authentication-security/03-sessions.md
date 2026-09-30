# Sessions

Stateful authentication: the server remembers who you are, and the browser carries only an opaque ID.

## How sessions work

```
1. POST /login  (email + password)
2. Server verifies credentials, creates a session record:
      sessionId "k3j9..." → { userId: 42, role: "user", createdAt: ... }
3. Server responds with:  Set-Cookie: sid=k3j9...; HttpOnly; Secure; SameSite=Lax
4. Browser automatically sends  Cookie: sid=k3j9...  with every request
5. Server looks up "k3j9..." in the session store → knows it's user 42
6. POST /logout → server deletes the record → the cookie is now worthless
```

The cookie contains **no user data** — only a random, unguessable identifier. All real data stays on the server.

---

## Session store: memory is not an option in production

`express-session` defaults to an in-memory store. It:

- leaks memory (it was never meant for production — the library says so)
- loses every session on restart
- doesn't work with more than one server instance

Use **Redis** (see `07-databases/redis/02-caching-and-sessions.md`): fast, shared between instances, and supports automatic expiry.

---

## Setup with Express and Redis

```bash
npm install express-session connect-redis redis
```

```js
// config/session.js
import session from "express-session";
import { RedisStore } from "connect-redis";
import { createClient } from "redis";

const redisClient = createClient({ url: process.env.REDIS_URL });
await redisClient.connect();

export const sessionMiddleware = session({
  store: new RedisStore({ client: redisClient, prefix: "sess:" }),
  name: "sid", // don't keep the default "connect.sid"
  secret: process.env.SESSION_SECRET, // used to sign the cookie
  resave: false, // don't re-save unchanged sessions
  saveUninitialized: false, // don't create sessions for anonymous visitors
  rolling: false,
  cookie: {
    httpOnly: true, // JavaScript cannot read it
    secure: process.env.NODE_ENV === "production", // HTTPS only
    sameSite: "lax", // blocks most CSRF (see below)
    maxAge: 1000 * 60 * 60 * 24, // 1 day
  },
});
```

```js
// app.js
import { sessionMiddleware } from "./config/session.js";

app.set("trust proxy", 1); // required behind nginx / a load balancer for `secure` cookies
app.use(sessionMiddleware);
```

(In older `connect-redis` versions the import was a default export wrapped around `session`; check the version you install.)

### Why each option matters

| Option                     | Purpose                                                                                      |
| -------------------------- | -------------------------------------------------------------------------------------------- |
| `name`                     | The default name reveals you use Express; a custom name is minor obscurity                   |
| `secret`                   | Signs the cookie so it can't be forged. Long, random, from env. Can be an array for rotation |
| `resave: false`            | Avoids race conditions and needless writes                                                   |
| `saveUninitialized: false` | Fewer junk sessions; complies with cookie-consent rules more easily                          |
| `httpOnly`                 | Blocks XSS from stealing the cookie                                                          |
| `secure`                   | Never sent over plain HTTP                                                                   |
| `sameSite`                 | Controls cross-site sending (CSRF defense)                                                   |
| `maxAge`                   | Absolute lifetime of the cookie                                                              |

---

## Login and logout

```js
router.post("/login", async (req, res, next) => {
  try {
    const { email, password } = req.body;
    const user = await User.findOne({ email });
    const ok = user && (await bcrypt.compare(password, user.passwordHash));

    if (!ok)
      return res.status(401).json({ error: "Invalid email or password" });

    // Prevent session fixation: issue a NEW session ID at login
    req.session.regenerate((err) => {
      if (err) return next(err);

      req.session.userId = user.id;
      req.session.role = user.role;

      req.session.save((err) => {
        // make sure it's stored before responding
        if (err) return next(err);
        res.json({ id: user.id, email: user.email });
      });
    });
  } catch (err) {
    next(err);
  }
});

router.post("/logout", (req, res, next) => {
  req.session.destroy((err) => {
    if (err) return next(err);
    res.clearCookie("sid");
    res.sendStatus(204);
  });
});
```

### Session fixation

If the session ID stays the same before and after login, an attacker who planted a known ID in the victim's browser (via a link or an insecure subdomain) becomes logged in as the victim. **Always call `req.session.regenerate()` on login** and after any privilege change.

### Auth middleware

```js
export function requireLogin(req, res, next) {
  if (!req.session.userId) {
    return res.status(401).json({ error: "Not authenticated" });
  }
  next();
}

app.get("/api/me", requireLogin, async (req, res) => {
  const user = await User.findById(req.session.userId).select("-passwordHash");
  res.json(user);
});
```

---

## Timeouts: idle vs absolute

- **Idle timeout** — expire after N minutes of inactivity. Set `rolling: true` so each response refreshes the cookie, and pair it with a short `maxAge`.
- **Absolute timeout** — expire N hours after login regardless of activity. Store `req.session.createdAt` and check it in middleware.

```js
const ABSOLUTE_LIMIT = 12 * 60 * 60 * 1000;

export function enforceAbsoluteTimeout(req, res, next) {
  const created = req.session.createdAt;
  if (created && Date.now() - created > ABSOLUTE_LIMIT) {
    return req.session.destroy(() =>
      res.status(401).json({ error: "Session expired" }),
    );
  }
  next();
}
```

Sensitive apps (banking, admin panels) use short idle timeouts; consumer apps use long, persistent sessions.

---

## CSRF: the price of cookies

Because the browser attaches cookies **automatically**, a malicious site can trigger requests to your API _as the logged-in user_:

```html
<!-- evil.com -->
<form action="https://bank.com/transfer" method="POST">
  <input name="to" value="attacker" /><input name="amount" value="5000" />
</form>
<script>
  document.forms[0].submit();
</script>
```

Defenses, ideally combined:

1. **`SameSite=Lax` or `Strict`** cookies — the browser withholds them on cross-site POSTs. This alone stops the majority of attacks.
2. **CSRF tokens** — a per-session secret that must be sent in a header or form field. Use a maintained library such as `csrf-csrf` (the old `csurf` package is deprecated).
3. **Check `Origin` / `Referer`** headers on state-changing requests.
4. **Never change state on `GET`** — see `05-http-web/01-http-methods-and-status-codes.md`.
5. Require a custom header (e.g. `X-Requested-With`) on JSON APIs — cross-site forms can't set it without a CORS preflight, which your CORS policy (`05-http-web/04-cors.md`) should reject.

More detail in `05-common-vulnerabilities.md`.

---

## "Log out everywhere" and session management

Because sessions live on the server, you get features JWTs struggle with:

```js
// Store an index of a user's sessions when they log in
await redisClient.sAdd(`user-sessions:${user.id}`, req.sessionID);

// Kill all of a user's sessions (password change, "log out everywhere", account compromise)
async function destroyAllSessions(userId) {
  const ids = await redisClient.sMembers(`user-sessions:${userId}`);
  await Promise.all(ids.map((id) => redisClient.del(`sess:${id}`)));
  await redisClient.del(`user-sessions:${userId}`);
}
```

Always invalidate sessions after a password change or reset.

---

## What to store in a session

- ✅ `userId`, `role`, `createdAt`
- ❌ The whole user object (goes stale, wastes memory)
- ❌ Passwords, tokens, or large blobs

Look up fresh user data by ID when you need it.

---

## Sessions vs JWT: how to choose

| Situation                                       | Choose                       |
| ----------------------------------------------- | ---------------------------- |
| Server-rendered site or SPA on the same domain  | **Sessions**                 |
| Need instant logout / ban / permission change   | **Sessions**                 |
| Mobile apps, third-party API consumers          | **JWT** (access + refresh)   |
| Many independent services verifying identity    | **JWT** (asymmetric signing) |
| Simplest secure default for a browser-first app | **Sessions**                 |

---

## Common mistakes

| Mistake                                             | Fix                                                             |
| --------------------------------------------------- | --------------------------------------------------------------- |
| Default `MemoryStore` in production                 | Redis store                                                     |
| Not regenerating the session on login               | `req.session.regenerate()`                                      |
| `secure: true` behind a proxy without `trust proxy` | `app.set("trust proxy", 1)` — otherwise the cookie is never set |
| Weak or hard-coded `secret`                         | Long random value from env; support rotation via an array       |
| `sameSite: "none"` without need                     | Use `lax`/`strict` unless you truly need cross-site cookies     |
| Sessions never expire                               | Set `maxAge` and a Redis TTL                                    |

## Next

**`04-oauth-and-oidc.md`** covers letting users sign in through Google, GitHub, and other identity providers.
