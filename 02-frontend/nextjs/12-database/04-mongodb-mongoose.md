# MongoDB and Mongoose

MongoDB stores JSON-like **documents** in **collections**. Mongoose adds schemas, validation, models and query helpers on top of the official driver. This note covers connecting from Next.js, defining models, reading and writing safely, and the serialization problems that surface when documents cross into React.

> Verified against the Mongoose Next.js and transactions guides. Atlas and index details are general MongoDB knowledge.

## What it is, and when to use it

| MongoDB fits | PostgreSQL fits better |
|---|---|
| Documents whose shape varies or nests deeply | Highly relational data with many joins |
| Whole-document reads and writes (a profile, a CMS entry) | Strong cross-row constraints and reporting |
| Rapid iteration on shape | Strict schema enforced by the database |

Modeling rule: **embed** data that is read together and bounded (an address inside a user), **reference** data that is shared, large or unbounded (posts by an author). There are no joins by default; `populate` or `$lookup` loads references.

## Setup

```bash
npm install mongoose
```

```bash
# .env.local
MONGODB_URI="mongodb+srv://user:pass@cluster.mongodb.net/mydb?retryWrites=true&w=majority"
```

For local development run MongoDB in Docker, or use a free Atlas cluster. If you use transactions locally, start a **single-node replica set** (see [Transactions](./05-transactions.md#mongoose)).

## The connection helper

The Mongoose guide's helper:

```ts
// lib/mongodb.ts
import mongoose from "mongoose";

const MONGODB_URI = process.env.MONGODB_URI;

export default async function dbConnect() {
  if (!MONGODB_URI) {
    throw new Error("Please define the MONGODB_URI environment variable");
  }
  await mongoose.connect(MONGODB_URI);
  return mongoose;
}
```

Per the guide, Mongoose manages the connection and **calling `mongoose.connect()` when already connected does nothing**, so you can call `dbConnect()` in every Server Component, action and Route Handler.

If you want to be explicit about concurrent first calls (serverless cold starts, several components awaiting at once), cache the connection **promise** on `globalThis`. This is a common pattern, not from the guide:

```ts
import mongoose from "mongoose";

const g = globalThis as unknown as { _mongoose?: Promise<typeof mongoose> };

export default function dbConnect() {
  const uri = process.env.MONGODB_URI;
  if (!uri) throw new Error("Please define the MONGODB_URI environment variable");
  g._mongoose ??= mongoose.connect(uri, { bufferCommands: false });
  return g._mongoose;
}
```

Wrap with `import "server-only"` and only import it from server code. Pool size defaults to 100 in the driver; for serverless set `maxPoolSize` lower in the connect options.

**Runtime:** Mongoose needs the **Node.js runtime** (the driver uses Node's `net`). Set `export const runtime = "nodejs"` where the default does not already apply; the Edge Runtime is not supported.

## Models

```ts
// models/Post.ts
import mongoose, { Schema, type InferSchemaType, type Model } from "mongoose";

const PostSchema = new Schema(
  {
    authorId: { type: Schema.Types.ObjectId, ref: "User", required: true, index: true },
    title: { type: String, required: true, trim: true, maxlength: 200 },
    body: { type: String, default: "" },
    status: { type: String, enum: ["draft", "published"], default: "draft" },
    tags: [{ type: String }],
  },
  { timestamps: true },
);

PostSchema.index({ authorId: 1, createdAt: -1 });

export type PostDoc = InferSchemaType<typeof PostSchema>;

// Reuse the model during hot reload to avoid "OverwriteModelError"
export default (mongoose.models.Post as Model<PostDoc>) || mongoose.model<PostDoc>("Post", PostSchema);
```

The guide's pattern is `mongoose.models.User || mongoose.model("User", UserSchema)`, which "prevents model recompilation errors during hot reloading". `timestamps: true` adds `createdAt` and `updatedAt`.

Schema validation (`required`, `enum`, `maxlength`) runs on `save()` and `create()`; **many update queries skip it unless you opt in** (`runValidators: true`). Validate input with a schema library (Zod) in the action anyway.

## Reading and writing

```ts
import dbConnect from "@/lib/mongodb";
import Post from "@/models/Post";

await dbConnect();

// Create
const post = await Post.create({ authorId, title: "Hello" });

// Read: lean() returns plain objects (faster, no Mongoose document methods)
const posts = await Post.find({ status: "published" })
  .select("title status createdAt")
  .sort({ createdAt: -1 })
  .limit(20)
  .lean();

const one = await Post.findById(id).lean();

// Update scoped by owner
const res = await Post.updateOne({ _id: id, authorId }, { $set: { title: "New" } }, { runValidators: true });
// res.matchedCount === 0  => not found or not yours

// Delete scoped by owner
await Post.deleteOne({ _id: id, authorId });

// Populate a reference
const withAuthor = await Post.find().populate("authorId", "name").lean();
```

The guide's App Router example calls `await dbConnect()` then `User.find({}).lean()` inside an async Server Component.

## Next.js DAL

```ts
// data/posts.ts
import "server-only";
import { cache } from "react";
import dbConnect from "@/lib/mongodb";
import Post from "@/models/Post";
import { verifySession } from "@/app/lib/dal";

export type PostDTO = { id: string; title: string; status: string; createdAt: string };

export const getMyPosts = cache(async (): Promise<PostDTO[]> => {
  const { userId } = await verifySession();
  await dbConnect();
  const docs = await Post.find({ authorId: userId }).sort({ createdAt: -1 }).limit(50).lean();
  return docs.map((d) => ({
    id: d._id.toString(),                       // ObjectId -> string
    title: d.title,
    status: d.status,
    createdAt: d.createdAt.toISOString(),       // Date -> string
  }));
});
```

```ts
// app/actions/posts.ts
"use server";
import { revalidatePath } from "next/cache";
import { Types } from "mongoose";
import dbConnect from "@/lib/mongodb";
import Post from "@/models/Post";
import { verifySession } from "@/app/lib/dal";

export async function renamePost(id: string, title: string) {
  const { userId } = await verifySession();
  if (!Types.ObjectId.isValid(id)) return { message: "Not found" };    // validate before querying
  await dbConnect();
  const res = await Post.updateOne({ _id: id, authorId: userId }, { $set: { title: title.trim() } }, { runValidators: true });
  if (res.matchedCount === 0) return { message: "Not found" };
  revalidatePath("/posts");
  return { success: true };
}
```

## Serialization: documents are not plain objects

Props passed from a Server Component to a Client Component, and Server Action return values, must be serializable. Mongoose results contain values that are not:

| Value | Problem | Fix in the DTO |
|---|---|---|
| `ObjectId` (`_id`, refs) | Not a plain value | `.toString()` |
| `Date` | Not plain for Client props | `.toISOString()` |
| Mongoose document (no `.lean()`) | Class instance with methods | Use `.lean()` and map fields |
| `Decimal128`, `Buffer`, `Map` | Not plain | Convert to string, number or object |

The Pages Router guide uses `JSON.parse(JSON.stringify(users))` in `getServerSideProps`; in the App Router prefer explicit DTO mapping, which also controls which fields leave the server.

## Indexes

Define indexes in the schema (`index: true`, `schema.index({...})`). Mongoose builds them on startup in development (`autoIndex`); in production, create them deliberately (they can be slow on large collections), commonly turning `autoIndex` off and using a migration script.

| Index | When |
|---|---|
| Single field (`authorId`) | Equality filters |
| Compound (`authorId`, `createdAt`) | Filter then sort; order matters |
| Unique (`email`) | Enforce uniqueness at the database |
| TTL (`expireAfterSeconds`) | Auto-delete sessions or tokens |
| Text / Atlas Search | Search |

## Security: NoSQL injection

Operators like `$ne` and `$gt` in user input change a query's meaning. If you pass a request body directly as a filter, an attacker can send `{ "password": { "$ne": null } }`.

```ts
// BAD
await User.findOne({ email: body.email, password: body.password });

// GOOD: validate types first, query by a string you control
const { email } = z.object({ email: z.email() }).parse(body);
const user = await User.findOne({ email });
```

Validate input with a schema, cast to strings, and consider Mongoose's `sanitizeFilter` option to strip `$` operators from filters.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `OverwriteModelError: Cannot overwrite 'Post' model` | Model re-registered on hot reload | `mongoose.models.Post \|\| mongoose.model(...)` |
| `Operation buffering timed out` | Not connected, wrong URI, IP not allowed in Atlas | `await dbConnect()` first; check URI and network allowlist |
| `Only plain objects can be passed to Client Components` | ObjectId/Date/document in props | DTO mapping, `.lean()` |
| `Module not found: net` | Mongoose imported in Edge or client code | Node runtime, server-only |
| `MongoServerSelectionError` | Network, DNS (`mongodb+srv`), credentials | Check URI, allowlist, DNS |
| Cast to ObjectId failed | Invalid ID string in a query | `Types.ObjectId.isValid` first |
| Validation seems ignored on update | Update queries do not validate by default | `runValidators: true` |
| Too many connections on Atlas | Many serverless instances | Lower `maxPoolSize`; reuse the connection |
| Duplicate key error `E11000` | Unique index violation | Catch `error.code === 11000` |
| Slow queries | Missing index | `.explain()`; add an index |

## Common mistakes

| Mistake | Fix |
|---|---|
| Returning Mongoose documents to components | `.lean()` + DTO |
| Skipping `dbConnect()` | Call it before queries in each server entry point |
| Trusting request bodies as filters | Validate and cast |
| Not scoping updates by `authorId` | Include the owner in the filter |
| Unbounded arrays inside documents | Reference instead of embedding |
| Relying on schema validation for security | Validate input too |
| Assuming transactions work on any local MongoDB | They need a replica set |
| Building production indexes via `autoIndex` on huge collections | Deliberate index creation |

## Quick Summary

- Mongoose gives schemas and models over MongoDB; connect once with a helper (`mongoose.connect` is a no-op when already connected).
- Reuse models on hot reload with `mongoose.models.X || mongoose.model(...)`; run on the Node.js runtime only.
- Use `.lean()` and map to DTOs: convert `ObjectId` and `Date` before they reach components.
- Scope writes by owner, validate input types (NoSQL injection), set `runValidators` on updates.
- Index deliberately; use [transactions](./05-transactions.md) (replica set required) for multi-document writes.

## Next

- [Transactions](./05-transactions.md)
- [Database Architecture](./00-database-architecture.md)
- [Validation](../07-server-actions/02-validation.md)

Sources: [Mongoose with Next.js](https://mongoosejs.com/docs/nextjs.html), [Mongoose transactions](https://mongoosejs.com/docs/transactions.html)
