# Dev Tooling

A reproducible local environment for auth work: MongoDB, Redis and a fake mail server in Docker, plus an API client that handles cookies. Auth flows involve email, cookies and multiple stores, so being able to see all three directly saves hours of debugging.

**Prerequisites:** Docker with Compose, [env-and-secrets.md](./env-and-secrets.md)

---

## Docker Compose

```yaml
# docker-compose.yml
services:
  mongo:
    image: mongo:7
    ports: ["127.0.0.1:27017:27017"]
    volumes: [mongo-data:/data/db]

  redis:
    image: redis:7
    command: ["redis-server", "--requirepass", "devpass"]
    ports: ["127.0.0.1:6379:6379"]
    volumes: [redis-data:/data]

  mailpit:
    image: axllent/mailpit
    ports:
      - "127.0.0.1:1025:1025"   # SMTP
      - "127.0.0.1:8025:8025"   # web inbox

volumes:
  mongo-data:
  redis-data:
```

```bash
docker compose up -d        # start
docker compose logs -f redis
docker compose down         # stop, keep data
docker compose down -v      # stop and wipe all data
```

Ports are bound to `127.0.0.1` so the databases are not reachable from your network. Even locally, practice the habit from the Redis security rule: never expose it publicly. Matching `.env`:

```bash
MONGODB_URI=mongodb://localhost:27017/auth-mastery
REDIS_URL=redis://:devpass@localhost:6379
SMTP_HOST=localhost
SMTP_PORT=1025
```

`localhost` is correct when the Node app runs on your machine. If the app runs inside Compose too, use the service names (`mongo`, `redis`, `mailpit`) as hostnames.

---

## Mailpit: test email flows

Mailpit accepts any SMTP mail and shows it at `http://localhost:8025`. Nothing is delivered to real addresses, so you can test verification and password reset freely and copy the link from the message.

With Nodemailer:

```js
import nodemailer from "nodemailer";

export const transporter = nodemailer.createTransport({
  host: process.env.SMTP_HOST,
  port: Number(process.env.SMTP_PORT),
  secure: false,
});
```

---

## Inspecting Redis

Look at what your auth code actually stores:

```bash
docker compose exec redis redis-cli -a devpass
```

```text
KEYS session:*              # fine locally; use SCAN on real servers
HGETALL session:<id>        # userId, refreshTokenHash, status...
TTL session:<id>            # seconds left, -1 = no expiry, -2 = missing
SMEMBERS user:sessions:<userId>
```

A key with `TTL = -1` is a bug: temporary credentials must always expire.

## Inspecting MongoDB

```bash
docker compose exec mongo mongosh auth-mastery
```

```js
db.users.find({}, { email: 1, isEmailVerified: 1 })
```

You should never see a plaintext password or any token in the `users` collection.

---

## API client (Postman / Bruno)

Both keep a cookie jar, so the refresh flow can be tested end to end:

1. `POST /api/auth/login` → response sets the `refreshToken` cookie.
2. `POST /api/auth/refresh` → cookie is sent automatically, new cookie replaces it.

Things that trip people up:

- The refresh cookie has `Path=/api/auth/refresh`, so it is **not** sent to other endpoints. That is intended.
- Cookies marked `Secure` are only set over HTTPS in production; the cookie config in this repo enables `secure` only when `NODE_ENV=production`, so plain `http://localhost` works in development.
- Put the access token in the `Authorization: Bearer ...` header, not a cookie.
- Use an environment variable in the client for the access token and update it from the login response.

---

## Common problems

| Symptom | Cause |
|---|---|
| `ECONNREFUSED 6379` | Redis container not running or port conflict |
| `NOAUTH Authentication required` | `REDIS_URL` missing the password |
| Emails never appear | `SMTP_PORT` is 8025 (the web UI) instead of 1025 |
| Data reappears after `down` | Volumes persist; use `down -v` for a clean slate |
| Cookie not stored in API client | Origin/CORS config or cookie `Secure` mismatch |

---

## Quick summary

- `docker compose up -d` gives you Mongo, Redis and Mailpit in one command.
- Bind local services to `127.0.0.1`.
- Mailpit makes email flows testable without sending real mail.
- Use `redis-cli` to verify TTLs and that only hashes are stored.
- Use a cookie-aware API client to test login, refresh and logout.

**Next:** [Fundamentals: authn vs authz](../01-fundamentals/authn-vs-authz.md)