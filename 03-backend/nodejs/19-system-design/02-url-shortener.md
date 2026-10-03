# URL Shortener

Turn `https://example.com/products/very/long/path?utm_source=newsletter` into `https://sho.rt/aZ3k9Q` — and redirect quickly when someone clicks it. It looks trivial, which is why it is a favorite design problem: the interesting parts are **ID generation**, **a read-heavy workload**, and **caching**.

## Requirements

**Functional**
- Create a short link from a long URL
- Visiting the short link redirects to the original URL
- Optional: custom aliases, expiry dates, click analytics

**Non-functional**
- Redirects must be **fast** (low latency) and **highly available** — a broken link is a visible failure
- Short codes must be **unique** and hard to guess if links are meant to be private
- The system is **heavily read-biased** — links are created once and clicked many times

**Out of scope for this design:** user accounts, link editing, abuse/malware scanning (mention them, but don't design them).

## Scale estimate

Assumptions:
- 100 million new links per month
- Read:write ratio of 100:1

| Quantity | Calculation | Result |
|----------|-------------|--------|
| Writes | 100M ÷ (30 × 86,400) | ~40 writes/sec |
| Reads | 40 × 100 | ~4,000 redirects/sec average |
| Peak reads | ~3–5× average | ~12,000–20,000/sec |
| Records over 5 years | 100M × 12 × 5 | ~6 billion links |
| Storage per record | ~500 bytes (URL + metadata) | ~3 TB total |

Conclusions: writes are trivial; **reads need caching**; 3 TB fits a single database comfortably with a good index (or can be sharded later); the main design question is how to generate 6 billion unique short codes.

## API

```
POST /api/links
  body:    { "url": "https://example.com/long/path", "alias"?: "my-sale", "expiresAt"?: "2027-01-01T00:00:00Z" }
  returns: 201 { "code": "aZ3k9Q", "shortUrl": "https://sho.rt/aZ3k9Q" }

GET /:code
  returns: 301 or 302 redirect with Location: <original url>
           404 if unknown, 410 if expired
```

## Data model

```
links
  code        VARCHAR(10)  PRIMARY KEY     -- the short code
  long_url    TEXT         NOT NULL
  created_at  TIMESTAMP
  expires_at  TIMESTAMP    NULL
  owner_id    ...          NULL
```

Access is almost entirely **lookup by `code`**, so a key-value-shaped store works well. PostgreSQL with a primary key on `code`, or MongoDB with a unique index, are both fine (`07-databases/`). A distributed key-value store becomes attractive only at much larger scale.

## High-level design

```
 Create:  Client ──► API ──► generate code ──► Database
                                           └─► (optionally warm cache)

 Redirect: Client ──► CDN/LB ──► API ──► Redis cache ──hit──► 302
                                    │
                                    └──miss──► Database ──► populate cache ──► 302
```

## The core problem: generating unique short codes

### Code length

Using Base62 (`a-z`, `A-Z`, `0-9` = 62 characters):

| Length | Possible codes |
|--------|----------------|
| 6 | 62⁶ ≈ 56.8 billion |
| 7 | 62⁷ ≈ 3.5 trillion |

6 characters cover our 6 billion links with room to spare; 7 gives comfortable headroom.

### Approach 1 — Hash the URL and take a prefix

```js
import { createHash } from "node:crypto";

function codeFromUrl(url) {
  return createHash("sha256").update(url).digest("base64url").slice(0, 7);
}
```

- ✅ Same URL → same code (natural de-duplication)
- ❌ **Collisions**: truncating a hash means two different URLs can produce the same prefix. You must check the database and retry with a salt on conflict.
- ❌ Same URL for two users always gives the same link, which breaks per-user analytics or expiry.

### Approach 2 — Random codes with collision retry

```js
import { randomBytes } from "node:crypto";

const ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

function randomCode(length = 7) {
  const bytes = randomBytes(length);
  return Array.from(bytes, (b) => ALPHABET[b % 62]).join("");
}

async function createLink(longUrl) {
  for (let attempt = 0; attempt < 5; attempt++) {
    const code = randomCode();
    try {
      await db.links.insert({ code, longUrl });       // PRIMARY KEY rejects duplicates
      return code;
    } catch (err) {
      if (!isDuplicateKeyError(err)) throw err;       // collision → try another code
    }
  }
  throw new Error("Could not allocate a unique code");
}
```

- ✅ Simple, unpredictable codes
- ✅ **The database's uniqueness constraint is the source of truth** — never "check then insert" (that races between instances); insert and handle the duplicate error
- ❌ As the table fills, collisions become more likely, so retries increase. With 7 characters and billions of rows this stays rare.
- Note: `b % 62` introduces a tiny bias (256 isn't divisible by 62). For a short-code generator that's acceptable; for security tokens it isn't — use rejection sampling or a library.

### Approach 3 — Counter + Base62 encoding

Generate a unique integer ID, then encode it in Base62:

```js
function toBase62(n) {
  if (n === 0n) return "0";
  let out = "";
  while (n > 0n) {
    out = ALPHABET[Number(n % 62n)] + out;
    n /= 62n;
  }
  return out;
}

toBase62(125n);          // "21"
toBase62(1_000_000_000n); // "15ftgG"
```

- ✅ **No collisions by construction** — every integer is unique
- ✅ Shortest possible codes (they grow only as the counter grows)
- ❌ **Sequential codes are guessable** and let anyone enumerate your links (`/1000`, `/1001`, ...). Mitigate by shuffling the alphabet, or running the number through a reversible scramble before encoding.
- ❌ A single counter is a **bottleneck and a single point of failure** if shared naively.

### Distributing the counter

| Technique | Idea |
|-----------|------|
| **Range allocation** | A coordinator hands each API instance a block (e.g. 10,000 IDs). Each instance serves IDs from its block locally and fetches a new block when empty. Fast, few coordination calls. Unused IDs are lost if an instance crashes — harmless given the huge space. |
| **Redis `INCR`** | Atomic and fast; one call per link. Redis must be durable (persistence/replication) or you risk reissuing IDs after a failure. |
| **Snowflake-style IDs** | Combine timestamp + machine ID + sequence into a 64-bit integer; no central coordinator at all. |

A solid default for this design: **range allocation backed by a database sequence or Redis**, encoded as Base62.

## The redirect path (where the traffic is)

```js
app.get("/:code", async (req, res, next) => {
  try {
    const { code } = req.params;

    let longUrl = await redis.get(`link:${code}`);

    if (!longUrl) {
      const link = await db.links.findByCode(code);
      if (!link) return res.status(404).send("Not found");
      if (link.expiresAt && link.expiresAt < new Date()) return res.status(410).send("Expired");

      longUrl = link.longUrl;
      await redis.set(`link:${code}`, longUrl, { EX: 86_400 });
    }

    res.redirect(302, longUrl);
  } catch (err) {
    next(err);
  }
});
```

### Caching strategy

Click traffic follows a **power law**: a small fraction of links get most of the clicks. A cache holding the hot few percent absorbs the vast majority of reads — ~12,000/sec peak can be mostly served from Redis and the database sees only misses.

- **Cache-aside** with a TTL (as above)
- **Cache negative results briefly** (a short TTL for "not found") so a flood of requests for nonexistent codes doesn't hammer the database
- **Respect expiry** — if a link has an `expiresAt`, set the cache TTL no longer than the time remaining
- **Eviction** — LRU keeps hot links in memory

### 301 vs 302

| Status | Meaning | Effect |
|--------|---------|--------|
| **301** Moved Permanently | Browsers and caches may remember the redirect | Fewer requests to you — but you **lose click analytics** and can't change or expire the target |
| **302** Found (temporary) | Every click comes back to you | Enables analytics, expiry, and edits — at the cost of more traffic |

If analytics or expiry matter, use **302** (or 307). Use 301 only when links are truly permanent and you don't need counts.

## Click analytics (optional)

Don't write to the database on the redirect path — it makes the fastest operation in the system wait on a write. Instead, **emit an event and process it asynchronously**:

```js
res.redirect(302, longUrl);                       // respond first
clickQueue.add("click", {                         // then enqueue (fire-and-forget)
  code, at: Date.now(), ua: req.get("user-agent"), ip: req.ip,
}).catch((err) => logger.warn({ err }, "click event dropped"));
```

Workers batch these into an analytics store (`11-async-processing/01-queues-and-bullmq.md`, `06-job-processing-system.md`). Accepting that a click event might occasionally be dropped is a deliberate trade for redirect latency.

## Security and abuse

- **Validate URLs** — only allow `http`/`https`; reject `javascript:`, `data:`, and similar schemes. Normalize before storing.
- **Open-redirect and phishing abuse** — attackers use shorteners to hide malicious links. Real services scan targets against blocklists and offer reporting. At minimum, rate limit link creation (`01-scalable-api-and-rate-limiter.md`).
- **Custom aliases** — check against reserved words (`api`, `admin`, `login`) so they can't shadow real routes.
- **Don't expose enumeration** — avoid sequential codes if links are meant to be private.

## Scaling

- **App tier:** stateless; scale horizontally behind a load balancer
- **Cache:** Redis (cluster if one node's memory or throughput isn't enough)
- **Database:** primary key lookups are cheap; add read replicas first. Shard by `code` (hash of code) only if a single primary can't keep up with writes or storage
- **Global latency:** put a CDN or edge function in front for redirects, or replicate data across regions
- **Cleanup:** a scheduled job deletes expired rows (`11-async-processing/03-scheduled-jobs.md`)

## Trade-offs summary

| Decision | Trade-off |
|----------|-----------|
| Random codes vs counter | Unguessable but needs collision handling, vs collision-free but guessable |
| 301 vs 302 | Fewer requests vs analytics and control |
| Synchronous vs queued analytics | Accurate counts vs redirect latency |
| Longer codes | More capacity and safer vs less "short" |
| Cache TTL | Freshness (expiry, edits) vs hit rate |

## Common mistakes

- **Check-then-insert for uniqueness** — races between instances; rely on a unique constraint and handle the error.
- **Truncated hashes with no collision handling** — duplicates will happen eventually.
- **Sequential, guessable codes for private links** — anyone can enumerate them.
- **A single global counter with no plan for failure** — bottleneck and outage risk.
- **Writing analytics synchronously on every redirect** — slows the hottest path.
- **Using 301 and then wanting analytics or expiry** — browsers cached the redirect; you can't take it back.
- **No URL validation** — dangerous schemes and open-redirect abuse.
- **Aliases that collide with application routes** — `/api` shouldn't be a short code.
- **Never expiring or cleaning old links** — unbounded growth.

## Quick summary

- Read-heavy (~100:1) → **cache hot links in Redis**, keep the database lookup a simple primary-key read
- Short codes: Base62, 6–7 characters is plenty; choose random + unique constraint, or counter + Base62 with range allocation
- Uniqueness is enforced by the **database constraint**, not by app-level checks
- 302 keeps analytics and control; 301 trades them for fewer requests
- Record clicks **asynchronously** via a queue
- Validate URLs, rate limit creation, reserve route names

## Next

**`03-chat-system.md`** designs a real-time system where connection management and message fan-out are the hard problems.
