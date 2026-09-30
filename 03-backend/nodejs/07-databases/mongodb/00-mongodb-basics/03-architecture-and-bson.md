# Architecture & BSON

What MongoDB actually is under the hood, and BSON — the binary format your data is really stored as, which explains a few things about `ObjectId` and dates you'll see constantly once you're writing Mongoose schemas.

## `mongod`: the database server

`mongod` is the actual MongoDB server process — it's what you start when you `brew services start mongodb-community` or run the Docker image. It listens for connections, stores data (via the WiredTiger storage engine underneath it), and executes queries.

```
Your app (via Mongoose)
        ↓
   MongoDB driver (which Mongoose wraps — see 01-mongodb-native-driver/)
        ↓ (wire protocol)
      mongod
        ↓
   Storage engine → disk
```

In a sharded deployment, apps connect to a router (`mongos`) instead — irrelevant for almost everything in this guide, since Mongoose talks to a single connection string regardless of what's actually behind it.

## The document model, briefly

MongoDB favors storing related data together in one document, rather than normalizing it across joined tables the way a relational database would:

```js
{
  name: "Alice",
  addresses: [
    { type: "home", city: "Boston" },
    { type: "work", city: "New York" },
  ],
}
```

This directly shapes how you'll design Mongoose schemas later (`11-relationships/02-embedding-vs-referencing-in-mongoose.md`) — deciding whether related data belongs nested inside a document or as a separate, referenced collection is a real design decision MongoDB leaves up to you, unlike a relational schema that forces normalization.

---

## BSON: what's actually stored

Documents look like JSON when you read/write them, but MongoDB stores them as **BSON** — a binary format extending JSON with more precise types: real dates, distinct numeric types, binary data, and more.

```js
// what you write
{ name: "Alice", createdAt: new Date() }
```

```
// what's actually stored — a real Date type, not a string
{ name: (string) "Alice", createdAt: (BSON Date) ... }
```

This is why date range queries work correctly (`{ createdAt: { $gte: ..., $lt: ... } }`) — the value is a genuine comparable date type, not text that happens to look like one. Mongoose's own `Date` schema type (`04-schemas/02-schema-types-reference.md`) maps directly onto this BSON type.

---

## `ObjectId`

The default type for a document's `_id` field — a 12-byte value, shown as a 24-character hex string.

```
64f1a2b3   c4d5e6      f7a8b9c0d1
────────   ──────      ──────────
timestamp   random      counter
```

It encodes a creation timestamp, a random value, and an incrementing counter — generated client-side by the driver (and, in turn, by Mongoose), extremely unlikely to collide even across multiple servers with no coordination.

```js
const id = new mongoose.Types.ObjectId();
console.log(id.getTimestamp()); // the Date this ObjectId was generated
```

A useful side effect: you can extract an approximate creation time directly from an `_id`, without a separate field — though an explicit `createdAt` (which Mongoose can add automatically via schema `timestamps`, `04-schemas/05-schema-options.md`) is still clearer for anything you'll actually query or sort by.

### Comparing ObjectIds

```js
id1.equals(id2); // ✅ correct
id1 === id2; // ❌ always false — different object instances
```

This exact gotcha resurfaces constantly in Mongoose code — comparing a document's `_id` to a value from `req.params` or a JWT payload needs `.equals()` or a string comparison, not `===`.

## Quick summary

- `mongod` is the actual database server; Mongoose talks to it through the native driver, never directly
- BSON is the real, binary, more-precise-than-JSON format documents are stored as — this is why `Date` fields support real range queries
- `ObjectId` encodes a timestamp + random value + counter, generated client-side; always compare with `.equals()`, never `===`
- MongoDB's document model favors embedding related data together, a decision Mongoose schema design inherits directly

## Next

**`04-databases-collections-documents.md`** covers the concrete hierarchy — databases, collections, documents — that a Mongoose model ultimately maps onto.
