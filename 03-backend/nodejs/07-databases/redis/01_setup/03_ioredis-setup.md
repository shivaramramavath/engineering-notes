# ioredis Setup

Install ioredis, connect, verify, and set up a project structure you can grow through the course.

## 1. Create the project

```bash
mkdir ioredis-playground && cd ioredis-playground
npm init -y
npm install ioredis dotenv
```

Use Node.js 18+.

## 2. JavaScript (ESM)

In `package.json` add:

```json
{ "type": "module" }
```

`src/index.js`:

```js
import Redis from "ioredis";

const redis = new Redis(); // 127.0.0.1:6379

console.log(await redis.ping()); // PONG

await redis.set("hello", "world");
console.log(await redis.get("hello")); // world

await redis.quit();
```

Run it:

```bash
node src/index.js
```

### CommonJS alternative

```js
const Redis = require("ioredis");
const redis = new Redis();
```

## 3. TypeScript setup

```bash
npm install -D typescript tsx @types/node
npx tsc --init
```

ioredis ships with its **own type definitions**, so no `@types/ioredis` is needed.

Recommended `tsconfig.json`:

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "outDir": "dist",
    "rootDir": "src"
  },
  "include": ["src"]
}
```

`src/index.ts`:

```ts
import { Redis } from "ioredis";

const redis = new Redis();

async function main() {
  const pong: string = await redis.ping();
  console.log(pong);
  await redis.quit();
}

main().catch(console.error);
```

Scripts in `package.json`:

```json
{
  "scripts": {
    "dev": "tsx watch src/index.ts",
    "build": "tsc",
    "start": "node dist/index.js"
  }
}
```

## 4. Ways to connect

```js
import { Redis } from "ioredis";

new Redis(); // localhost:6379
new Redis(6380); // custom port
new Redis(6380, "redis.example.com"); // port + host
new Redis("redis://:password@host:6379/0"); // URL (db 0)
new Redis("rediss://user:pass@host:6380"); // TLS via rediss://

new Redis({
  host: "127.0.0.1",
  port: 6379,
  username: "default",
  password: process.env.REDIS_PASSWORD,
  db: 0,
});
```

## 5. Environment configuration

`.env`:

```
REDIS_HOST=127.0.0.1
REDIS_PORT=6379
REDIS_PASSWORD=
REDIS_DB=0
```

`src/config/env.ts`:

```ts
import "dotenv/config";

export const env = {
  redis: {
    host: process.env.REDIS_HOST ?? "127.0.0.1",
    port: Number(process.env.REDIS_PORT ?? 6379),
    password: process.env.REDIS_PASSWORD || undefined,
    db: Number(process.env.REDIS_DB ?? 0),
  },
};
```

Add `.env` to `.gitignore`. Commit a `.env.example` instead.

## 6. Reusable client module

`src/redis/client.ts`:

```ts
import { Redis } from "ioredis";
import { env } from "../config/env.js";

export const redis = new Redis({
  ...env.redis,
  maxRetriesPerRequest: 3,
  retryStrategy: (times) => Math.min(times * 100, 3000), // backoff up to 3s
});

redis.on("connect", () => console.log("[redis] connecting"));
redis.on("ready", () => console.log("[redis] ready"));
redis.on("error", (err) => console.error("[redis] error:", err.message));
redis.on("close", () => console.log("[redis] connection closed"));
redis.on("reconnecting", (ms: number) =>
  console.log(`[redis] reconnecting in ${ms}ms`),
);
```

Always attach an `error` listener. Without one, connection errors are printed as unhandled error events.

Import it anywhere:

```ts
import { redis } from "./redis/client.js";
```

More on connection options, events and retries is covered in `03_ioredis-basics`.

## 7. Recommended project layout

```
ioredis-playground/
├── docker-compose.yml
├── .env
├── .env.example
├── .gitignore
├── package.json
├── tsconfig.json
└── src/
    ├── index.ts
    ├── config/
    │   └── env.ts
    ├── redis/
    │   └── client.ts          # shared connection
    ├── services/              # business logic using Redis
    ├── repositories/          # data access wrappers
    └── examples/              # one script per lesson
```

## 8. Verify with a health check

`src/examples/health.ts`:

```ts
import { redis } from "../redis/client.js";

async function main() {
  const start = Date.now();
  const pong = await redis.ping();
  console.log(`PING -> ${pong} (${Date.now() - start}ms)`);

  await redis.set("setup:test", "ok", "EX", 10);
  console.log("GET ->", await redis.get("setup:test"));
  console.log("TTL ->", await redis.ttl("setup:test"));

  await redis.quit();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
```

```bash
npx tsx src/examples/health.ts
```

Expected output:

```
[redis] ready
PING -> PONG (2ms)
GET -> ok
TTL -> 10
```

Then open Redis Insight and confirm the `setup:test` key existed briefly.

## Quick reference: ioredis basics

| Task             | Code                                    |
| ---------------- | --------------------------------------- |
| Set with expiry  | `redis.set("k", "v", "EX", 60)`         |
| Get              | `await redis.get("k")`                  |
| Delete           | `await redis.del("k")`                  |
| Hash             | `redis.hset("user:1", { name: "Ada" })` |
| Close gracefully | `await redis.quit()`                    |
| Force close      | `redis.disconnect()`                    |

## Troubleshooting

| Symptom                                | Likely cause                   | Fix                                                   |
| -------------------------------------- | ------------------------------ | ----------------------------------------------------- |
| `ECONNREFUSED 127.0.0.1:6379`          | Redis not running              | Start Docker container or service                     |
| `NOAUTH Authentication required`       | Password needed                | Set `password` / `REDIS_PASSWORD`                     |
| `WRONGPASS`                            | Wrong credentials              | Check username and password                           |
| Script never exits                     | Connection still open          | Call `redis.quit()` at the end                        |
| `MaxRetriesPerRequestError`            | Redis unreachable for too long | Check server and network, tune `maxRetriesPerRequest` |
| TS: cannot find module ending in `.js` | NodeNext resolution            | Use `.js` extension in relative imports               |

## Checklist

- [ ] Redis is running (`redis-cli ping` returns `PONG`)
- [ ] `ioredis` is installed
- [ ] Shared client module with an `error` listener
- [ ] Connection settings come from environment variables
- [ ] Health-check script runs successfully

**Next module:** [02_redis-fundamentals](../02_redis-fundamentals/README.md)
