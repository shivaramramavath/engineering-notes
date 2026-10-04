# Feature Flags

A feature flag lets you change application behavior **without deploying**: turn a risky feature off in seconds, roll a new one out to 5 percent of users, or enable it only for one tenant. Redis is a good flag store because it is fast, shared across services and easy to update. The key design rule: **evaluate flags in memory, and use Redis only to distribute changes.**

```
 Admin API ──write──► Redis (flags hash + version + Pub/Sub + audit stream)
                          │  publish "changed"             ▲ poll every few seconds (backstop)
                          ▼                                │
              ┌───────────────────────────┐                │
              │ Each app instance:        │────────────────┘
              │  in-memory flag snapshot  │
              │  isEnabled(flag, user) ◄── evaluated locally, no network call
              └───────────────────────────┘
```

## The problem

| Need | Why plain config or env vars fall short |
|------|----------------------------------------|
| Kill switch in seconds | A config change means a redeploy or restart |
| Gradual rollout (1 to 5 to 25 to 100 percent) | Needs per-user stable decisions |
| Target a tenant, a plan or a tester | Needs rules and allow lists |
| A/B tests with sticky variants | The same user must always see the same variant |
| Same answer in every service | Shared state, consistent algorithm |
| Know who changed what, when | Audit trail |

## Design rules

1. **Never call Redis per evaluation.** A page may check dozens of flags. Keep a local snapshot and refresh it in the background
2. **Evaluation is deterministic.** Same user and same flag always give the same answer, on every server
3. **Fail safe.** If Redis is down, keep the last known snapshot. If there is none, use the in-code default
4. **Percentage rollouts are sticky and monotonic.** Raising 10 to 20 percent only **adds** users, never removes
5. **A kill switch beats everything**
6. **Changes are audited**

## Data model

```
ff:flags      HASH     flagKey ► JSON definition
ff:version    STRING   integer, INCR on every change (cheap "did anything change?" check)
ff:updates    CHANNEL  Pub/Sub notification: "a flag changed, refresh now"
ff:audit      STREAM   who changed what, with before and after
ff:exp:{flag}:{variant}:{day}   HyperLogLog  unique users exposed (optional analytics)
```

A flag definition:

```json
{
  "key": "new-checkout",
  "enabled": true,
  "rollout": 25,
  "allow": ["user-qa-1"],
  "deny": [],
  "rules": [{ "attr": "plan", "op": "in", "value": ["pro", "team"] }],
  "variants": [{ "name": "control", "weight": 50 }, { "name": "treatment", "weight": 50 }],
  "version": 7,
  "updatedAt": 1759570000000,
  "owner": "checkout-team",
  "description": "Single-page checkout"
}
```

All flags together are small (hundreds of entries), so one `HGETALL` loads the whole set.

In Redis Cluster, `MULTI` over several keys needs them in one slot. Use a common hash tag if you run Cluster (`{ff}:flags`, `{ff}:version`, `{ff}:audit`).

## Evaluation: pure and testable

Evaluation takes a flag definition and a context. It touches no network, so it is trivial to unit test.

```ts
import { createHash } from "node:crypto";

export interface FlagRule { attr: string; op: "eq" | "in" | "gte"; value: unknown }
export interface Variant { name: string; weight: number }       // weights sum to 100

export interface Flag {
  key: string;
  enabled: boolean;                // master switch (kill switch when false)
  rollout: number;                 // 0 to 100 (decimals allowed, e.g. 0.5)
  allow?: string[];                // always on
  deny?: string[];                 // always off
  rules?: FlagRule[];              // all must match to be eligible
  variants?: Variant[];
  version: number;
  updatedAt: number;
  owner?: string;
  description?: string;
}

export interface Context { userId: string; attrs?: Record<string, unknown> }
export interface Decision { on: boolean; variant?: string; reason: string }

const BUCKETS = 10_000;                                          // 0.01% resolution

/** Stable bucket in [0, BUCKETS) for this flag and user. */
export function bucket(flagKey: string, userId: string): number {
  const digest = createHash("sha256").update(`${flagKey}:${userId}`).digest();
  return digest.readUInt32BE(0) % BUCKETS;
}

function matches(rule: FlagRule, ctx: Context): boolean {
  const actual = ctx.attrs?.[rule.attr];
  switch (rule.op) {
    case "eq":  return actual === rule.value;
    case "in":  return Array.isArray(rule.value) && rule.value.includes(actual);
    case "gte": return typeof actual === "number" && typeof rule.value === "number" && actual >= rule.value;
  }
}

function pickVariant(flag: Flag, userId: string): string | undefined {
  if (!flag.variants?.length) return undefined;
  const point = (bucket(`${flag.key}:variant`, userId) / BUCKETS) * 100;     // independent of the rollout bucket
  let acc = 0;
  for (const v of flag.variants) {
    acc += v.weight;
    if (point < acc) return v.name;
  }
  return flag.variants[flag.variants.length - 1].name;
}

export function evaluate(flag: Flag | undefined, ctx: Context, fallback = false): Decision {
  if (!flag) return { on: fallback, reason: "missing" };
  if (!flag.enabled) return { on: false, reason: "disabled" };                 // kill switch wins
  if (flag.deny?.includes(ctx.userId)) return { on: false, reason: "deny" };
  if (flag.allow?.includes(ctx.userId)) return { on: true, variant: pickVariant(flag, ctx.userId), reason: "allow" };
  if (flag.rules?.length && !flag.rules.every((r) => matches(r, ctx))) return { on: false, reason: "rules" };
  if (bucket(flag.key, ctx.userId) >= flag.rollout * (BUCKETS / 100)) return { on: false, reason: "rollout" };
  return { on: true, variant: pickVariant(flag, ctx.userId), reason: "rollout" };
}
```

Why hashing instead of `Math.random()`: a random decision flips on every request, so a user sees the feature appear and vanish. Hashing `flagKey:userId` makes the decision a **fixed property of the pair**. Including the flag key in the hash means different flags select different 10 percent slices, so the same unlucky users are not always the guinea pigs.

Because the check is `bucket < rollout`, increasing the percentage only moves the threshold up. Everyone already in stays in.

For anonymous visitors, issue a stable random ID in a cookie and use that as `userId`.

## The flag store: local snapshot, Redis for distribution

```ts
import Redis from "ioredis";

export const FLAG_DEFAULTS = {
  "new-checkout": false,
  "search-v2": false,
} as const;
export type FlagKey = keyof typeof FLAG_DEFAULTS;                // typos become compile errors

export class FlagStore {
  private flags = new Map<string, Flag>();
  private loadedVersion = -1;
  private timer?: NodeJS.Timeout;

  constructor(private redis: Redis, private sub: Redis, private pollMs = 10_000) {}

  async start() {
    await this.refresh().catch((e) => console.error("initial flag load failed, using defaults", e));
    await this.sub.subscribe("ff:updates");
    this.sub.on("message", () => this.refresh().catch(() => {}));         // fast path
    this.timer = setInterval(() => this.refresh().catch(() => {}), this.pollMs);   // backstop: Pub/Sub can drop messages
    this.timer.unref();
  }

  stop() {
    if (this.timer) clearInterval(this.timer);
    this.sub.disconnect();
  }

  async refresh() {
    const version = Number((await this.redis.get("ff:version")) ?? 0);
    if (version === this.loadedVersion) return;                           // nothing changed

    const raw = await this.redis.hgetall("ff:flags");
    const next = new Map<string, Flag>();
    for (const [key, json] of Object.entries(raw)) {
      try { next.set(key, JSON.parse(json) as Flag); }
      catch { console.error("invalid flag definition skipped", key); }    // one bad flag must not break the rest
    }
    this.flags = next;
    this.loadedVersion = version;
  }

  isEnabled(key: FlagKey, ctx: Context): boolean {
    return evaluate(this.flags.get(key), ctx, FLAG_DEFAULTS[key]).on;
  }

  variant(key: FlagKey, ctx: Context): string | undefined {
    return evaluate(this.flags.get(key), ctx, FLAG_DEFAULTS[key]).variant;
  }
}
```

What this gives you:

- Evaluation is a map lookup plus a hash: microseconds, no I/O
- Changes reach every instance within moments through Pub/Sub, and **within the poll interval at worst** if a message is lost
- A Redis outage changes nothing: the last snapshot keeps serving. A cold start without Redis uses `FLAG_DEFAULTS`
- The subscriber needs its own connection (see [Pub/Sub Fundamentals](../09_pub-sub/01_pub-sub-fundamentals.md))

Defaults in code should be the **safe state**: usually "off" for new features, "on" for something you only ever want to disable in an emergency.

## Changing flags: the admin path

```ts
export async function saveFlag(redis: Redis, input: Omit<Flag, "version" | "updatedAt">, actor: string) {
  validate(input);
  const beforeRaw = await redis.hget("ff:flags", input.key);
  const before = beforeRaw ? (JSON.parse(beforeRaw) as Flag) : undefined;

  const next: Flag = { ...input, version: (before?.version ?? 0) + 1, updatedAt: Date.now() };

  await redis.multi()
    .hset("ff:flags", input.key, JSON.stringify(next))
    .incr("ff:version")
    .xadd("ff:audit", "MAXLEN", "~", 10_000, "*",
      "actor", actor, "flag", input.key, "before", beforeRaw ?? "", "after", JSON.stringify(next))
    .publish("ff:updates", input.key)
    .exec();

  return next;
}

function validate(f: Omit<Flag, "version" | "updatedAt">) {
  if (!/^[a-z0-9][a-z0-9-]{1,63}$/.test(f.key)) throw new Error("invalid flag key");
  if (f.rollout < 0 || f.rollout > 100) throw new Error("rollout must be between 0 and 100");
  if (f.variants) {
    const total = f.variants.reduce((s, v) => s + v.weight, 0);
    if (Math.round(total) !== 100) throw new Error("variant weights must sum to 100");
  }
}
```

The version bump and the publish happen in the same transaction as the write, so a reader never sees "version changed" with the old data. Concurrent admin edits are rare, but to avoid silently overwriting someone else's change, compare `before.version` with the version the editor started from, or use `WATCH`.

Protect the admin API like any privileged surface: authenticate, authorize by role, rate-limit and validate input. Flag definitions can change production behavior instantly.

### Audit trail

```ts
const history = await redis.xrevrange("ff:audit", "+", "-", "COUNT", 20);
// each entry: actor, flag, before, after, and the stream ID encodes the time
```

A capped stream keeps recent history cheaply. Ship it to durable storage if you need long-term compliance records.

## Using flags in the application

```ts
// one store per process
const flags = new FlagStore(redis, redis.duplicate());
await flags.start();

app.use((req, _res, next) => {
  const ctx: Context = {
    userId: req.user?.id ?? req.anonymousId,                   // stable cookie ID for visitors
    attrs: { plan: req.user?.plan, country: req.geo?.country },
  };
  req.flags = {
    isEnabled: (key: FlagKey) => flags.isEnabled(key, ctx),
    variant: (key: FlagKey) => flags.variant(key, ctx),
  };
  next();
});

app.get("/checkout", (req, res) => {
  if (req.flags.isEnabled("new-checkout")) return renderNewCheckout(req, res);
  return renderOldCheckout(req, res);
});
```

Evaluate **once per request** and pass the result down rather than re-evaluating deep in the call stack. Use the same `bucket` function in every service (share it as a library), or the same user will get different answers in different places.

## Rollout playbook

| Step | Rollout | What to watch |
|------|---------|---------------|
| 1 | Allow list only (team, QA) | Does it work at all? |
| 2 | 1 percent | Error rate, latency, logs |
| 3 | 5 to 10 percent | Business metrics versus control |
| 4 | 25 to 50 percent | Load, cost, support tickets |
| 5 | 100 percent | Soak for a while |
| 6 | **Remove the flag and the old code** | Dead flags are debt |

If anything looks wrong, set `enabled: false`. Recovery takes seconds and needs no deploy.

## A/B tests and exposure tracking

Variant assignment is sticky for the same reason rollout is: it comes from a hash. To analyze results you need to know **who was actually exposed**, counted once per user.

```ts
// call when the user actually sees the variant (not on every evaluation)
async function recordExposure(flagKey: string, variant: string, userId: string) {
  const key = `ff:exp:${flagKey}:${variant}:${new Date().toISOString().slice(0, 10)}`;
  await redis.multi().pfadd(key, userId).expire(key, 45 * 86_400, "NX").exec();   // EXPIRE NX: Redis 7.0+
}

const control = await redis.pfcount("ff:exp:new-checkout:control:2026-10-04");        // unique users, about 0.8% error
const week = await redis.pfcount(...last7DayKeys);                                    // PFCOUNT unions multiple keys
```

A HyperLogLog counts unique users in about 12 KB per key regardless of volume. Avoid calling Redis on every evaluation by deduplicating exposures in-process for a short time, or batching them with a pipeline every few seconds. Outcome metrics (conversions) belong in your analytics system, tagged with the variant.

## Per-tenant and per-user overrides

For simple multi-tenant switches, a hash per tenant is enough:

```ts
await redis.hset("tenant:7:flags", { "new-ui": "on", beta: "off" });
const on = (await redis.hget("tenant:7:flags", "new-ui")) === "on";
```

If you evaluate many per-tenant flags per request, load that hash into the same local snapshot (refresh on change) rather than reading it on each check. The `rules` mechanism above (for example `{ attr: "tenantId", op: "in", value: [...] }`) covers most targeting without a separate structure.

## Managing flag debt

- Every flag has an **owner** and a **description**, and ideally an expiry date
- Report flags that have been at 100 percent (or 0 percent) for a long time and nag the owner
- Keep flags **independent**. Nested conditions across flags are hard to reason about
- Remove the flag, then the dead branch, then the definition from Redis
- Limit the number of long-lived flags. Permanent configuration belongs in config, not flags

## Alternatives

Managed flag services and open-source servers (and the vendor-neutral OpenFeature API) add UIs, targeting editors and analytics. If you outgrow a homegrown store, you can often keep the same `isEnabled(key, ctx)` call sites and swap the implementation. Redis remains a good fit when you want a small, fast, self-hosted system with no extra service.

## Testing

Pure evaluation tests need no Redis at all:

```ts
const base: Flag = { key: "f", enabled: true, rollout: 0, version: 1, updatedAt: 0 };

it("is deterministic for the same flag and user", () => {
  const flag = { ...base, rollout: 50 };
  const first = evaluate(flag, { userId: "u1" }).on;
  for (let i = 0; i < 100; i++) expect(evaluate(flag, { userId: "u1" }).on).toBe(first);
});

it("matches the target percentage within tolerance", () => {
  const flag = { ...base, rollout: 10 };
  const on = Array.from({ length: 50_000 }, (_, i) => evaluate(flag, { userId: `u${i}` }).on).filter(Boolean).length;
  expect(on / 50_000).toBeGreaterThan(0.09);
  expect(on / 50_000).toBeLessThan(0.11);
});

it("only adds users when the rollout increases", () => {
  const at10 = new Set<string>(), at20 = new Set<string>();
  for (let i = 0; i < 20_000; i++) {
    const id = `u${i}`;
    if (evaluate({ ...base, rollout: 10 }, { userId: id }).on) at10.add(id);
    if (evaluate({ ...base, rollout: 20 }, { userId: id }).on) at20.add(id);
  }
  for (const id of at10) expect(at20.has(id)).toBe(true);
});

it("kill switch and deny beat everything", () => {
  expect(evaluate({ ...base, rollout: 100, enabled: false }, { userId: "u1" }).on).toBe(false);
  expect(evaluate({ ...base, rollout: 100, deny: ["u1"] }, { userId: "u1" }).on).toBe(false);
  expect(evaluate({ ...base, rollout: 0, allow: ["u1"] }, { userId: "u1" }).on).toBe(true);
});

it("falls back to the default when the flag is missing", () => {
  expect(evaluate(undefined, { userId: "u1" }, true).on).toBe(true);
});
```

Integration tests against real Redis:

```ts
it("picks up changes through Pub/Sub", async () => {
  const store = new FlagStore(t.redis, t.redis.duplicate(), 60_000);   // long poll: prove Pub/Sub does the work
  await store.start();
  expect(store.isEnabled("new-checkout", { userId: "u1" })).toBe(false);

  await saveFlag(t.redis, { key: "new-checkout", enabled: true, rollout: 100 }, "test");
  await waitFor(async () => store.isEnabled("new-checkout", { userId: "u1" }), (v) => v === true);
  store.stop();
});

it("keeps serving the last snapshot when Redis fails", async () => {
  // load flags, then disconnect or kill the connection, and assert isEnabled still answers
});
```

Also test that an invalid definition is skipped without breaking other flags, and that the audit stream records each change.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Calling Redis for every flag check | Local snapshot refreshed in the background |
| `Math.random()` for rollouts | Hash `flagKey:userId` for sticky decisions |
| Hashing only the user ID | Include the flag key so different flags pick different users |
| Relying on Pub/Sub alone for propagation | Add polling on the version counter as a backstop |
| Throwing when Redis is down at startup | Fall back to in-code defaults |
| Unsafe defaults | Default to the safe state, and treat "off" as normal for new features |
| Different hashing in different services | Share one evaluation library |
| Anonymous users without a stable ID | Cookie or device ID |
| No kill switch | `enabled: false` overrides every other rule |
| Changing the hash function or bucket count mid-rollout | Users reshuffle. Treat it as a breaking change |
| Evaluating deep in code, many times per request | Evaluate once, pass the decision down |
| No audit trail | Write changes to `ff:audit` |
| Open admin endpoint | Authenticate, authorize and validate |
| Flags that never get removed | Owner, expiry and a cleanup routine |
| PII or secrets inside flag values | Flags carry configuration, not sensitive data |

## Key takeaways

- Evaluate flags from an in-memory snapshot, and use Redis to store and distribute changes
- Hash `flagKey:userId` into a bucket for stable, monotonic percentage rollouts and sticky variants
- Refresh on Pub/Sub, back it with polling on a version counter, and keep serving the last snapshot if Redis fails
- Order of precedence: kill switch, deny, allow, rules, rollout
- Write changes atomically with a version bump, a notification and an audit entry
- Track exposure with HyperLogLog, test the evaluator as a pure function and remove flags when they are done

**Previous:** [Notification System](./04_notification-system.md) | **Next:** [Event-Driven System](./06_event-driven-system.md)
