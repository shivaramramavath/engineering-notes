# Lambda Handlers

AWS Lambda runs a function of yours when an event arrives, scales it by running more copies, and bills you for the time it runs. You ship code, not servers. The model is simple, but a few details about **how the runtime reuses your code between invocations** decide whether a Lambda is fast, cheap and correct, or slow and subtly broken. This note is about those details: handler shape, what runs when, cold starts, bundling, layers and config.

Prerequisites: [SDK v3 and credentials](../01-setup/01-sdk-v3-and-credentials.md). In Lambda your credentials are the function's **execution role**, injected automatically; you never configure keys.

```bash
npm install -D typescript esbuild @types/aws-lambda @types/node
```

---

## The handler

A handler is an exported function. Lambda calls it with the **event** (shape depends on who triggered you) and a **context** object.

```ts
// src/handler.ts
import type { SQSEvent, Context } from "aws-lambda";

export const handler = async (event: SQSEvent, context: Context) => {
  console.log(context.awsRequestId, event.Records.length);
  return { processed: event.Records.length };
};
```

- The function's **handler setting** names it as `<file>.<export>`. If your bundle is `index.mjs` and you export `handler`, the setting is `index.handler`.
- `@types/aws-lambda` provides event types for most triggers (`SQSEvent`, `S3Event`, `APIGatewayProxyEventV2`, …) so the event isn't `any`. Types describe the shape; they don't validate it. Validate untrusted input yourself.
- Write **`async` handlers** and return a value or throw. Don't mix `async` with the old `callback` parameter.
- The return value is JSON-serialized and given to the caller **if the invocation was synchronous**. For asynchronous invokers (S3, SNS, EventBridge) it's ignored. API Gateway expects a specific response shape ([API Gateway APIs](./02-api-gateway-apis.md)).

### Await everything

Lambda **freezes the execution environment the moment your handler returns**. Work still in flight gets paused, and may resume during a later invocation or be discarded.

```ts
// BAD: returns before the write finishes
export const handler = async () => {
  void saveAudit();        // unawaited
  return "ok";
};

// GOOD
export const handler = async () => {
  await saveAudit();
  return "ok";
};
```

Fire-and-forget logging or metrics that "usually work" are a classic source of missing data.

---

## What runs when

```text
New execution environment                 Reused ("warm") environment
┌───────────────────────────────┐
│ INIT: download code, start    │  ← cold start: your top-level code runs ONCE here
│ runtime, run module top level │
└──────────────┬────────────────┘
               ▼
          INVOKE #1 ──freeze──▶ INVOKE #2 ──freeze──▶ INVOKE #3 ... ──▶ eventually shut down
          handler()             handler()             handler()
```

- **Top-level code runs once per execution environment**; the handler runs on every invocation.
- An environment handles **one invocation at a time**. Concurrent requests get separate environments, so module variables are shared by *sequential* invocations, never by concurrent ones.
- Lambda may keep an environment for minutes to hours, or discard it any time. Never assume reuse; never assume it won't happen.

What that means for where code goes:

```ts
// ── INIT: once per environment. Put expensive, reusable things here ──
import { S3Client } from "@aws-sdk/client-s3";
import { loadConfig } from "./config.js";

const config = loadConfig();                       // validate env once
const s3 = new S3Client({ region: config.region }); // reuse connections and credential cache

let dbPool: Pool | undefined;                      // lazy: only if a code path needs it
const getPool = () => (dbPool ??= createPool(config));

// ── INVOKE: every time. Per-request state lives only in local variables ──
export const handler = async (event: MyEvent) => {
  const pool = getPool();
  // ...
};
```

Rules of thumb:

- **Create SDK clients and connections at init** and reuse them. (Creating a client per invocation throws away connection pooling and credential caching.)
- **Never keep per-request data in module variables.** The next invocation on the same environment will see it. This is the Lambda version of a global-state leak.
- Caches (config, secrets, JWKS) belong at module level with a TTL ([secrets and parameters](../02-services/08-secrets-and-parameters.md)).
- `/tmp` persists across warm invocations (512 MB by default, configurable) and is handy as a scratch cache, but treat it as best-effort.

---

## Cold starts

A **cold start** is an invocation that has to create a new environment: INIT runs before your handler. It shows up at first traffic, after idle periods, when scaling up, and after deployments.

You can see it in the log `REPORT` line, which has `Init Duration` **only on cold starts**:

```text
REPORT RequestId: ...  Duration: 41.2 ms  Billed Duration: 42 ms
       Memory Size: 512 MB  Max Memory Used: 96 MB  Init Duration: 312.5 ms
```

(`Max Memory Used` is how you right-size memory.)

For a small bundled Node function, cold starts are typically a fraction of a second; they grow with bundle size and with whatever your init code does. What you control in code:

1. **Bundle and minify** the function ([below](#bundling-with-esbuild)). Fewer files to load, and tree-shaking drops unused code.
2. **Import only what you use**: `@aws-sdk/client-s3`, never the v2 `aws-sdk` or an aggregate client you don't need ([SDK note](../01-setup/01-sdk-v3-and-credentials.md)).
3. **Keep init lean.** Don't fetch secrets, open connections or build big objects that some invocations don't need; make them lazy.
4. **Give it enough memory.** CPU scales with memory, so a function that's CPU-starved at 128 MB can run faster *and* sometimes cheaper at a higher setting. Measure rather than guess (the open-source AWS Lambda Power Tuning tool automates this).
5. **Use arm64 (Graviton)** where your dependencies support it: usually cheaper, often as fast or faster.
6. **Provisioned concurrency** keeps environments pre-initialized for latency-critical paths. It's a configuration option with a cost, not a code change.

Don't over-engineer this. Many functions (queue workers, S3 processors) don't care about a few hundred milliseconds. User-facing APIs on the critical path do.

---

## Configuration that matters

| Setting | Notes |
|---|---|
| Memory | 128 MB to 10,240 MB. Also sets CPU share |
| Timeout | Default 3 s, max 15 minutes. Set it deliberately; see event-source rules in [event-driven Lambda](./03-event-driven-lambda.md) |
| Architecture | `arm64` or `x86_64`. Native dependencies must match |
| Ephemeral storage (`/tmp`) | 512 MB default, configurable higher |
| Environment variables | 4 KB total. Not a place for secrets (read them at runtime by name) |
| Payload | About 6 MB for synchronous request/response. Asynchronous events are much smaller. Check current limits |
| Package size | 50 MB zipped (direct upload), 250 MB unzipped including layers. Container images go much higher |
| Runtime | Use a currently supported Node.js runtime version; AWS retires old ones |

### Environment variables

Read them from `process.env`, validated once at init with the typed config from the [SDK note](../01-setup/01-sdk-v3-and-credentials.md). Lambda provides some for free: `AWS_REGION`, `AWS_LAMBDA_FUNCTION_NAME`, `AWS_LAMBDA_FUNCTION_MEMORY_SIZE`, `AWS_LAMBDA_LOG_GROUP_NAME`, `AWS_EXECUTION_ENV`, plus the credential variables (`AWS_ACCESS_KEY_ID`, …) that the SDK's provider chain picks up. That's why `new S3Client({})` works here with no config.

> **Misconception:** `AWS_NODEJS_CONNECTION_REUSE_ENABLED=1` was needed for SDK **v2**. SDK v3 reuses connections by default, so you don't need it.

---

## Bundling with esbuild

Shipping a `node_modules` folder works but is slow to load. Bundling your TypeScript into one file is the standard approach.

```bash
esbuild src/handler.ts \
  --bundle --platform=node --target=node22 --format=esm \
  --outfile=dist/index.mjs \
  --minify --sourcemap \
  --external:@aws-sdk/*
```

(Set `--target` to the Node version of your function's runtime.)

Decisions in there:

- **`--external:@aws-sdk/*`**: the managed Node.js runtime already includes SDK v3, so leaving it out shrinks your bundle and speeds cold starts. The trade-off: you get *the runtime's bundled version*, which AWS updates over time. If you need a specific SDK version or behavior, bundle it instead (drop the flag).
- **ESM output (`.mjs`)** allows top-level `await` at init (handy for loading config). If a bundled CommonJS dependency complains that `require` is not defined, add a shim banner:

```bash
--banner:js="import { createRequire } from 'module'; const require = createRequire(import.meta.url);"
```

- **Source maps**: bundled and minified stack traces are unreadable. Emit `--sourcemap` and set the function's environment variable `NODE_OPTIONS=--enable-source-maps` so errors point at your TypeScript.
- **Native modules** (like `sharp`) can't be bundled by esbuild. They must be installed for Lambda's Linux and **your function's architecture**, then shipped as `node_modules` or in a layer. "Works locally, `Cannot find module` or an invalid ELF header in Lambda" almost always means the wrong platform build.

If you deploy with CDK, its `NodejsFunction` construct runs esbuild for you with similar options; the projects use it.

---

## Layers

A **layer** is a zip that Lambda extracts into `/opt` for your function. For Node, put packages under `nodejs/node_modules` in the zip and they're resolvable at runtime.

Good uses: native binaries or big shared dependencies used by many functions, shared internal code, and extensions (like the Parameters and Secrets extension).

Not what they're for:

- **Layers don't make cold starts faster.** Their contents still count toward the 250 MB limit and still load.
- For most TypeScript functions, bundling with esbuild is simpler and gives you tree-shaking, which layers can't.
- A function can use up to five layers; version them, since layers are immutable and functions pin a version.

---

## The context object

```ts
export const handler = async (event: unknown, context: Context) => {
  context.awsRequestId;                 // unique per invocation: put it in every log line
  context.functionName;
  context.getRemainingTimeInMillis();   // time left before the timeout
};
```

`getRemainingTimeInMillis()` is how long-running handlers (batch processors) stop gracefully: check it between items and hand back the unprocessed ones instead of being killed mid-item.

---

## Errors

If your handler throws (or the process crashes), the invocation fails. What *happens next* depends on who invoked you:

| Invoker | On failure |
|---|---|
| Synchronous (API Gateway, SDK `Invoke`) | Error returned to the caller. You decide on retry |
| Asynchronous (S3, SNS, EventBridge) | Lambda retries, then drops or sends to a failure destination |
| Event source mapping (SQS, streams) | Batch is retried via the source's own rules |

Specifics are in [event-driven Lambda](./03-event-driven-lambda.md). In the handler itself: log with the request ID, then **rethrow** if you want failure semantics. Swallowing the error with a `catch` and returning normally tells Lambda it *succeeded*, so the message is deleted and never retried.

Unhandled promise rejections outside the handler (in background callbacks) can crash the environment; don't leave floating promises.

---

## Concurrency and downstream limits

Every concurrent invocation is its own environment, so a burst of 500 requests is 500 environments, each with its own connections. Lambda scales faster than most databases tolerate.

- **Relational databases:** put **RDS Proxy** (or a similar pooler) between Lambda and the DB, and keep per-environment pools tiny (often 1 connection).
- **Cap concurrency** with *reserved concurrency* on the function (or `maxConcurrency` on an SQS event source) to protect downstream systems.
- Accounts have a regional concurrency quota (1,000 by default, adjustable). Exceeding it **throttles** invocations.

---

## Response streaming (Node.js)

Normally Lambda buffers the whole response. With **response streaming** the function can start sending bytes while it's still working, which suits LLM output ([Bedrock](../02-services/07-bedrock.md)) or large payloads.

```ts
import { pipeline } from "node:stream/promises";
import { Readable } from "node:stream";

export const handler = awslambda.streamifyResponse(async (event, responseStream, context) => {
  const out = awslambda.HttpResponseStream.from(responseStream, {
    statusCode: 200,
    headers: { "Content-Type": "text/plain; charset=utf-8" },
  });
  await pipeline(Readable.from(["hello ", "streaming ", "world\n"]), out);
});
```

`awslambda` is a global provided by the Node runtime (recent `@types/aws-lambda` versions declare it). Streaming needs the function exposed in a way that supports it, for example a Lambda **function URL with invoke mode `RESPONSE_STREAM`**; check the docs for which front doors support it before designing around it.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Creating SDK clients inside the handler | Create at module level |
| Unawaited promises | `await` everything before returning |
| Per-request data in module variables | Local variables only; module scope is shared across invocations |
| `catch` that swallows the error | Rethrow (or return a failure response) so retries/DLQs engage |
| Shipping the whole `node_modules`, or the v2 `aws-sdk` | Bundle with esbuild; use v3 clients |
| Native module built on your laptop | Build for Lambda's OS and architecture |
| Unreadable stack traces | Source maps + `NODE_OPTIONS=--enable-source-maps` |
| Timeout of 3 s left at the default | Set it from your real worst case |
| 500 concurrent invocations exhaust DB connections | RDS Proxy, small pools, reserved concurrency |
| Expecting layers to speed up cold starts | They don't; bundle instead |
| Assuming a warm environment | Code must work cold; treat warmth as a bonus |
| Secrets in env var values | Put the secret's name in env, read at runtime |

Debugging starts at the CloudWatch log group `/aws/lambda/<function-name>`: the `REPORT` line (duration, memory, init), your `console.log` output and the error stack. See [logging and tracing](../04-production/02-logging-and-tracing.md).

---

## Quick summary

- A handler is an exported `async (event, context)` function; types come from `@types/aws-lambda`.
- Top-level code runs once per environment (init); the handler runs per invocation. Put clients and config at init, per-request state in locals.
- Await all work before returning; the environment freezes on return.
- Cold starts: bundle with esbuild, import narrowly, keep init lean, tune memory, consider arm64 and provisioned concurrency. `Init Duration` in the REPORT line tells you if it's happening.
- SDK v3 ships in the runtime (mark it external) unless you need a pinned version; native modules and layers need the right platform.
- Errors: rethrow so the trigger's retry rules apply. Scale-out can overwhelm downstreams, so cap it.

## Next

[API Gateway APIs](./02-api-gateway-apis.md): putting HTTP in front of a handler.