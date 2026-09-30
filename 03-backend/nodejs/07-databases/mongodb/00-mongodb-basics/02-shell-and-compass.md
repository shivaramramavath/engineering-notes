# mongosh & Compass

Two ways to interact with a MongoDB database without writing any application code — useful for poking around before, and while, writing Mongoose code against the same data.

## `mongosh` — the command-line shell

```bash
mongosh                                              # connects to localhost:27017
mongosh "mongodb+srv://cluster0.abcde.mongodb.net/" --username myuser   # Atlas
```

```js
show dbs
use myapp
show collections

db.users.find()
db.users.find({ age: { $gt: 18 } })
db.users.insertOne({ name: "Alice", age: 30 })
```

`mongosh` is a real JavaScript environment — `db.users.find()` is a method call. This matters directly for Mongoose later: the filter objects you'll pass to `Model.find({...})` use exactly this same syntax, since Mongoose's query API is built directly on top of it.

```js
db.users.find().pretty();
db.users.find().limit(5).toArray();
```

---

## Compass — the GUI

Open Compass, paste in the same connection string, and you get:

- **Visual browsing** — scroll through documents without writing a query
- **A filter bar** — build a query with a form, see results update live
- **The Aggregation Pipeline Builder** — construct a pipeline stage by stage, seeing intermediate output (directly relevant later to `12-aggregation-with-mongoose/`)
- **Schema analysis** — sample a collection and see the actual shape of the data present, useful for reverse-engineering what a Mongoose schema for existing data should look like
- **"Export to language"** — build a query visually, then get the equivalent Node.js code, close to (though not identical to) the Mongoose syntax you'll actually write

---

## When to use which, while learning Mongoose

| Task                                                                             | Better tool                                           |
| -------------------------------------------------------------------------------- | ----------------------------------------------------- |
| Quickly checking what a Mongoose query actually inserted/returned                | `mongosh`                                             |
| Understanding the shape of an existing collection before writing a schema for it | Compass                                               |
| Testing a filter/aggregation idea before writing it in Mongoose                  | Either — Compass if it's complex, shell if it's quick |

## Quick summary

- `mongosh` is a JavaScript shell — its query syntax is exactly what Mongoose's own filter objects are built from
- Compass is best for visual exploration and building aggregation pipelines incrementally
- Use either to sanity-check what your Mongoose code is actually doing to the underlying data

## Next

**`03-architecture-and-bson.md`** covers what's actually happening underneath these tools — `mongod`, and the BSON format documents are really stored as.
