# 03 — Setup

The MongoDB-focused portion of this guide is done. From here on, everything is Mongoose. This section covers installing it and establishing a real connection — the foundation every later section builds on.

## In this section

| File                                    | Covers                                                                                             |
| --------------------------------------- | -------------------------------------------------------------------------------------------------- |
| `01-installing-mongoose.md`             | Installing the package, and a first look at what a minimal Mongoose setup looks like               |
| `02-connecting-to-mongodb.md`           | `mongoose.connect()` in depth — options, connecting to Atlas, and connecting once at app startup   |
| `03-connection-events-and-lifecycle.md` | The connection state machine, listening for connect/error/disconnect events, and graceful shutdown |

## Why connection setup gets its own section

Every single thing later in this guide — schemas, models, queries, transactions — assumes an established connection. Getting this right once, at startup, with proper error and lifecycle handling, avoids a whole category of confusing bugs later (queries hanging because the connection never actually succeeded, or a process that doesn't shut down cleanly).

## What you should be able to do after this section

- Install Mongoose and connect to a local or Atlas MongoDB instance
- Configure connection options appropriately (pool size, timeouts)
- Listen for connection lifecycle events and know what each one means
- Shut down a Mongoose connection gracefully when the process exits

## Next

**`04-schemas`** covers defining the shape of your data — the core concept everything else in Mongoose is built around.
