# Connecting to MongoDB

`mongoose.connect()` in depth — options, connecting to Atlas, and the pattern for connecting exactly once at application startup.

## Basic connection

```js
import mongoose from "mongoose";

await mongoose.connect("mongodb://localhost:27017/myapp");
```

Under the hood, this creates and manages a native `MongoClient` (`01-mongodb-native-driver/01-connecting-with-the-native-driver.md`) for you — you never interact with `MongoClient` directly in normal Mongoose usage.

---

## Connecting to Atlas

```js
await mongoose.connect(
  "mongodb+srv://myuser:mypassword@cluster0.abcde.mongodb.net/myapp?retryWrites=true&w=majority",
);
```

Identical call, just a different connection string — see `00-mongodb-basics/01-installation-and-atlas.md` for getting this string from the Atlas dashboard. Never hardcode credentials directly in source — load the connection string from an environment variable:

```js
await mongoose.connect(process.env.MONGODB_URI);
```

---

## Connection options

```js
await mongoose.connect(process.env.MONGODB_URI, {
  maxPoolSize: 10,
  minPoolSize: 2,
  serverSelectionTimeoutMS: 5000, // how long to try finding a server before failing
  socketTimeoutMS: 45000, // how long a socket can be idle before closing
});
```

| Option                     | Purpose                                                                 |
| -------------------------- | ----------------------------------------------------------------------- |
| `maxPoolSize`              | Maximum simultaneous connections in the pool                            |
| `minPoolSize`              | Minimum connections kept open, ready to use                             |
| `serverSelectionTimeoutMS` | How long to wait for a usable server before throwing a connection error |
| `socketTimeoutMS`          | How long an individual operation can take before timing out             |

Full depth on tuning these for real workloads in `14-performance/04-connection-pooling.md`.

---

## Connect exactly once, at application startup

```js
// db.js
import mongoose from "mongoose";

export async function connectDB() {
  await mongoose.connect(process.env.MONGODB_URI);
  console.log("MongoDB connected");
}
```

```js
// app.js
import { connectDB } from "./db.js";

await connectDB();

app.listen(3000, () => console.log("Server running"));
```

Exactly the same principle as the native driver (`01-mongodb-native-driver/01-connecting-with-the-native-driver.md`): connect once when the app starts, and every subsequent `Model.find()`/`Model.create()` call throughout the app's lifetime reuses that same underlying connection and pool. Never call `mongoose.connect()` inside a request handler.

---

## What happens if you query before connecting

```js
const User = mongoose.model("User", userSchema);

// called before mongoose.connect() has resolved
const users = await User.find(); // this doesn't throw immediately...
```

Mongoose **buffers** operations issued before a connection is established, by default — `User.find()` above doesn't fail outright; it waits, up to a timeout, for the connection to become ready. This is convenient (you can define models and issue queries before `connect()` technically resolves, and it usually just works), but can also mask a genuine connection problem by turning it into a mysterious hang instead of an immediate, clear error.

### Disabling buffering, for clearer failures

```js
mongoose.set("bufferCommands", false);
```

With buffering off, a query issued before a real connection exists fails immediately with a clear error instead of hanging — a reasonable choice in production, where you'd rather fail fast and loudly than have a request appear to hang.

---

## Multiple connections (a brief mention)

```js
const secondaryConnection = mongoose.createConnection(
  process.env.SECONDARY_DB_URI,
);
const AuditLog = secondaryConnection.model("AuditLog", auditLogSchema);
```

`mongoose.connect()` establishes Mongoose's single **default** connection, used by `mongoose.model()`. `createConnection()` creates an additional, independent connection — useful for the (relatively rare) case of a single application needing to talk to more than one MongoDB database/cluster at once.

## Common mistakes

- **Calling `mongoose.connect()` inside a route handler or on every request** — connect once, at startup, and reuse the connection for the app's entire lifetime.
- **Hardcoding a connection string (with real credentials) in source code** — always load it from an environment variable (`process.env.MONGODB_URI`), tying back to the `dotenv`/`envalid` patterns from earlier in this documentation set.
- **Assuming a query issued right after `connect()` is guaranteed to succeed** — `connect()` returns a promise; always `await` it (or otherwise ensure the connection is established) before assuming the app is ready to serve requests.
- **Not setting `serverSelectionTimeoutMS`** and being surprised by how long Mongoose waits before reporting a connection failure with an unreachable/misconfigured database.

## Quick summary

- `await mongoose.connect(uri, options)` establishes Mongoose's default connection — call it once, at startup, never per-request
- Load the connection string from an environment variable, never hardcode it
- Mongoose buffers commands issued before the connection is ready by default — convenient, but can mask real connection failures as hangs; `bufferCommands: false` trades that convenience for fail-fast clarity
- `createConnection()` is the escape hatch for talking to more than one database from a single app

## Next

**`03-connection-events-and-lifecycle.md`** covers what happens after `connect()` — the connection state machine, and handling disconnects and shutdown properly.
