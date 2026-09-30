# envalid — Reference Guide

## What is envalid?

`envalid` validates and cleans environment variables — it ensures required config exists and has the correct type, converting `process.env` strings into properly typed values.

```
.env → dotenv → process.env → envalid → validated config → Application
```

## Why use it?

```js
// ❌ Without validation
const port = process.env.PORT; // might not exist, is always a string, fails silently later
```

```js
// ✅ With envalid
const env = cleanEnv(process.env, { PORT: port() });
env.PORT; // validated and converted to a number
```

## Install

```bash
npm install dotenv envalid
```

## Basic usage

```js
import "dotenv/config";
import { cleanEnv, str, port } from "envalid";

const env = cleanEnv(process.env, {
  NODE_ENV: str(),
  PORT: port(),
});

console.log(env.NODE_ENV); // "development"
console.log(env.PORT); // 5000 (number, not string)
```

## `cleanEnv()`

The core function. It takes `process.env` and a schema object, validates each variable against its rule, converts types, and returns a clean config object.

```js
const env = cleanEnv(process.env, {
  PORT: port(),
  DATABASE_URL: url(),
  JWT_SECRET: str(),
});
```

If a required variable is missing or invalid, `envalid` throws and stops the app at startup — **fail-fast validation** — instead of letting a bad config cause errors later during execution.

## Validators

| Validator | Checks for                     | Example                |
| --------- | ------------------------------ | ---------------------- |
| `str()`   | A string                       | `APP_NAME: str()`      |
| `num()`   | A number                       | `MAX_USERS: num()`     |
| `port()`  | A valid TCP port               | `PORT: port()`         |
| `bool()`  | A boolean (`"true"`/`"false"`) | `ENABLE_CACHE: bool()` |
| `url()`   | A valid URL                    | `DATABASE_URL: url()`  |
| `email()` | A valid email address          | `ADMIN_EMAIL: email()` |

By default, every validator requires the variable to be present — validation fails if it's missing or empty.

## Options

Each validator accepts an options object:

| Option       | Purpose                                                      | Example                                                             |
| ------------ | ------------------------------------------------------------ | ------------------------------------------------------------------- |
| `default`    | Value used when the variable is missing (makes it optional)  | `PORT: port({ default: 5000 })`                                     |
| `devDefault` | Value used only in development; still required in production | `PORT: port({ devDefault: 5000 })`                                  |
| `choices`    | Restricts the value to a fixed set (enum-like)               | `NODE_ENV: str({ choices: ["development", "test", "production"] })` |
| `desc`       | Documents what the variable is for                           | `PORT: port({ desc: "HTTP server port" })`                          |
| `example`    | Shows a sample value in validation error messages            | `DATABASE_URL: url({ example: "mongodb://localhost:27017/app" })`   |

Combined:

```js
NODE_ENV: str({
  choices: ["development", "test", "production"],
  default: "development",
}),
PORT: port({
  desc: "HTTP server port",
  default: 5000,
}),
DATABASE_URL: url({
  desc: "MongoDB connection URL",
  example: "mongodb://localhost:27017/app",
}),
```

## Real-world example

`src/config/env.js`:

```js
import "dotenv/config";
import { cleanEnv, port, str, url, bool } from "envalid";

export const env = cleanEnv(process.env, {
  NODE_ENV: str({
    choices: ["development", "test", "production"],
    default: "development",
  }),
  PORT: port({ default: 5000 }),
  DATABASE_URL: url(),
  REDIS_URL: url(),
  JWT_ACCESS_SECRET: str(),
  JWT_REFRESH_SECRET: str(),
  JWT_ACCESS_EXPIRES_IN: str({ default: "15m" }),
  ENABLE_SWAGGER: bool({ default: false }),
  CORS_ORIGIN: url(),
});
```

Usage elsewhere in the app:

```js
import { env } from "./config/env.js";

app.listen(env.PORT);
console.log(env.DATABASE_URL);
```

Prefer this centralized `env` object over reading `process.env.X` throughout the codebase — it's typed, validated once at startup, and self-documenting.

## `process.env` vs validated `env`

|               | `process.env.PORT`       | `env.PORT` (envalid)                |
| ------------- | ------------------------ | ----------------------------------- |
| Type          | `"5000"` (string)        | `5000` (number)                     |
| Missing value | `undefined`, fails later | Fails at startup, or uses `default` |
| Invalid value | Not detected             | Rejected immediately                |

## dotenv vs envalid

| Library   | Responsibility                            |
| --------- | ----------------------------------------- |
| `dotenv`  | Loads `.env` into `process.env`           |
| `envalid` | Validates and types environment variables |

> **dotenv loads. envalid validates.**

## Security

`envalid` validates configuration — it does **not** encrypt or secure secrets.

- Don't log validated secrets (`console.log(env.JWT_SECRET)`).
- Don't commit `.env` to version control.
- Use a real secrets manager in production where appropriate.

## One-line definition

> `envalid` validates, converts, and cleans Node.js environment variables so your application starts with a reliable, typed configuration — or fails immediately if something's wrong.
