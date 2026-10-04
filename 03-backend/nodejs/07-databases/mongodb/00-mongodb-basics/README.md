# 00 — MongoDB Basics

This is the only section in this guide about raw MongoDB, independent of Mongoose. It's kept deliberately short — just enough grounding to understand what Mongoose is actually built on top of, before the rest of the guide goes deep on Mongoose itself.

## In this section

| File                                    | Covers                                                                                     |
| --------------------------------------- | ------------------------------------------------------------------------------------------ |
| `01-installation-and-atlas.md`          | Getting a MongoDB instance running — locally or on Atlas                                   |
| `02-shell-and-compass.md`               | `mongosh` and Compass — interacting with a database without any code                       |
| `03-architecture-and-bson.md`           | What `mongod` actually is, and BSON/`ObjectId` — the format documents are really stored as |
| `04-databases-collections-documents.md` | The core hierarchy every MongoDB (and every Mongoose model) is built on                    |

## Why this section is short

Everything past this point in the guide is Mongoose-first — schemas, models, and Mongoose's own query API. This section exists only to make sure "a document," "a collection," "an `ObjectId`," and "BSON" aren't unfamiliar terms once Mongoose starts using them constantly without re-explaining them.

## What you should be able to do after this section

- Have a running MongoDB instance (local or Atlas) to connect to
- Run a basic query in `mongosh` or browse data in Compass
- Explain what BSON is, and what's actually encoded inside an `ObjectId`
- Describe the database → collection → document hierarchy

## Next

**`01-mongodb-native-driver`** covers the actual Node.js library — the one Mongoose itself is built on top of.
