# Project Setup

The baseline Node.js backend every later chapter builds on: Express for HTTP, MongoDB for accounts, Redis for sessions and temporary tokens. Get this skeleton right once and each auth feature becomes "add a service, a route and a middleware".

**Prerequisites:** Node.js 20+, npm, basic Express and async/await.

---

## Install

```bash
mkdir server && cd server
npm init -y
npm pkg set type=module

npm i express mongoose redis argon2 jsonwebtoken cookie-parser cors helmet zod
```

`"type": "module"` enables `import` syntax, which all examples in this repo use.

Run with Node's built-in env loading (no `dotenv` needed, see [env-and-secrets.md](./env-and-secrets.md)):

```json
{
  "scripts": {
    "dev": "node --watch --env-file=.env src/server.js",
    "start": "node src/server.js"
  }
}
```

---

## Folder layout

```text
src/
├── app.js            # builds the Express app (no listen)
├── server.js         # connects DBs, then listens
├── config/           # env, db, redis
├── models/           # Mongoose schemas
├── routes/           # URL → controller wiring only
├── controllers/      # parse request, call services, send response
├── services/         # business logic (password, token, session, mfa...)
├── middleware/       # authenticate, rate-limit, validate, error-handler
└── utils/            # crypto helpers, cookies, logger
```

| Layer | Knows about | Should not |
|---|---|---|
| route | paths, middleware order | contain logic |
| controller | `req` / `res` | talk to Redis or Mongo directly |
| service | Mongo, Redis, crypto | touch `req` / `res` |

Keeping `req`/`res` out of services is what makes token and session logic testable and reusable (for example, password reset calls the same session-revocation service as logout-all).

---

## app.js vs server.js

Split them. Tests import `app` without opening a port or a database connection.

```js
// src/app.js
import express from "express";
import cookieParser from "cookie-parser";
import cors from "cors";
import helmet from "helmet";
import authRoutes from "./routes/auth.routes.js";

const app = express();

app.use(helmet());
app.use(cors({ origin: process.env.FRONTEND_URL, credentials: true }));
app.use(express.json());
app.use(cookieParser());

app.use("/api/auth", authRoutes);

export default app;
```

The mount path `/api/auth` matters: the refresh cookie is scoped to `/api/auth/refresh` (see the cookie chapter), so the route must really live there.

```js
// src/server.js
import app from "./app.js";
import { connectMongo } from "./config/db.js";
import { connectRedis } from "./config/redis.js";

await connectMongo();
await connectRedis();

app.listen(process.env.PORT || 4000);
```

```js
// src/config/db.js
import mongoose from "mongoose";

export const connectMongo = () => mongoose.connect(process.env.MONGODB_URI);
```

```js
// src/config/redis.js
import { createClient } from "redis";

export const redis = createClient({ url: process.env.REDIS_URL });

redis.on("error", (err) => console.error("Redis error:", err.message));

export const connectRedis = () => redis.connect();
```

Fail fast: if either database is unreachable at boot, the process should exit rather than serve requests that can't authenticate anyone.

---

## Middleware order

Order is behavior in Express:

```text
helmet → cors → body parser → cookie parser → (rate limit) → routes → error handler
```

- Without `cookieParser()`, `req.cookies` is `undefined` and refresh silently fails.
- Without `express.json()`, `req.body` is `undefined`.
- The error handler must be registered **last**.

Behind a reverse proxy or load balancer, set `app.set("trust proxy", ...)` correctly, otherwise `req.ip` is the proxy's address and IP-based rate limiting treats all users as one.

---

## Common mistakes

| Symptom | Cause |
|---|---|
| `Cannot use import statement outside a module` | Missing `"type": "module"` |
| Redis commands throw "client is closed" | Forgot `await redis.connect()` |
| Browser blocks requests with credentials | `cors` origin is `*` or missing `credentials: true` |
| `req.cookies` is undefined | `cookie-parser` not registered, or registered after routes |
| Tests hang after finishing | `app.listen` called inside `app.js` |

---

## Quick summary

- Split `app.js` (build) from `server.js` (connect + listen).
- Routes wire, controllers translate HTTP, services hold logic.
- Connect MongoDB and Redis before listening.
- Middleware order matters; the error handler goes last.

**Next:** [env-and-secrets.md](./env-and-secrets.md)