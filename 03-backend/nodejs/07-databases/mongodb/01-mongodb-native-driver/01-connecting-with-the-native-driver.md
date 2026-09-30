# Connecting with the Native Driver

Connecting to MongoDB using the official `mongodb` package directly — no Mongoose involved. Worth seeing once, since it's exactly what Mongoose does internally when you call `mongoose.connect()`.

## Install

```bash
npm install mongodb
```

## Basic connection

```js
import { MongoClient } from "mongodb";

const uri = "mongodb://localhost:27017";
const client = new MongoClient(uri);

async function main() {
  await client.connect();
  console.log("Connected to MongoDB");

  const db = client.db("myapp");
  const users = db.collection("users");

  const result = await users.insertOne({ name: "Alice", age: 30 });
  console.log(result.insertedId);

  await client.close();
}

main().catch(console.error);
```

Notice the shape: `client` → `db` → `collection` — exactly the hierarchy from `00-mongodb-basics/04-databases-collections-documents.md`, expressed directly in code.

---

## Connecting to Atlas

```js
const uri =
  "mongodb+srv://myuser:mypassword@cluster0.abcde.mongodb.net/myapp?retryWrites=true&w=majority";
const client = new MongoClient(uri);
```

Identical API — only the connection string changes, same as covered in `00-mongodb-basics/01-installation-and-atlas.md`.

---

## Connection options

```js
const client = new MongoClient(uri, {
  maxPoolSize: 10, // maximum number of connections in the pool
  minPoolSize: 2,
  serverSelectionTimeoutMS: 5000,
});
```

The driver maintains a **connection pool** rather than opening a new connection per query — reusing a small number of live connections across many operations. Full depth on this in `14-performance/04-connection-pooling.md`, since Mongoose exposes (and defaults) these same underlying options.

---

## Keeping one client for the app's lifetime

```js
// ❌ opens and closes a new connection for every single request — wasteful and slow
app.get("/users", async (req, res) => {
  const client = new MongoClient(uri);
  await client.connect();
  const users = await client.db("myapp").collection("users").find().toArray();
  await client.close();
  res.json(users);
});
```

```js
// ✅ connect once at startup, reuse the same client (and its pool) for every request
const client = new MongoClient(uri);
await client.connect();
const db = client.db("myapp");

app.get("/users", async (req, res) => {
  const users = await db.collection("users").find().toArray();
  res.json(users);
});
```

This exact mistake — reconnecting per request instead of once at startup — is the single most common native-driver performance bug, and is precisely why Mongoose's own connection model (`03-setup/02-connecting-to-mongodb.md`) is designed around connecting once when the app starts.

---

## Checking the connection is alive

```js
await client.db("admin").command({ ping: 1 });
console.log("MongoDB connection is healthy");
```

A common health-check pattern — useful directly, and exactly what's happening underneath Mongoose's own connection-state events (`03-setup/03-connection-events-and-lifecycle.md`).

---

## Closing the connection gracefully

```js
process.on("SIGTERM", async () => {
  await client.close();
  process.exit(0);
});
```

Ties directly into graceful shutdown handling — closing the database connection cleanly when the process is asked to stop, rather than dropping in-flight operations abruptly.

## Common mistakes

- **Creating a new `MongoClient` per request** — always connect once, reuse the client for the app's entire lifetime.
- **Forgetting `await client.connect()`** before running any operation — though modern versions of the driver auto-connect lazily on first operation, being explicit avoids relying on that behavior.
- **Not closing the client on shutdown** — can leave connections open unnecessarily when the process exits.

## Quick summary

- `MongoClient` → `.db(name)` → `.collection(name)` is the core object hierarchy, matching database → collection directly
- Connect once, at application startup, and reuse the same client for every subsequent operation — never per-request
- Connection pooling is handled by the driver automatically; Mongoose exposes the same tunable options later
- This is exactly the mechanism Mongoose wraps — `mongoose.connect()` creates and manages a `MongoClient` for you internally

## Next

**`02-native-crud-methods.md`** covers the actual read/write methods available once connected — the ones Mongoose's own `Model` methods are built directly on top of.
