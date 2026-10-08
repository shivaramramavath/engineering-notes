# Relations

Prisma models relations in the schema and gives you typed ways to **load** them, **filter** by them, and **write** through them. The mental model: a relation is defined by a **scalar foreign key field** plus a **virtual relation field** that references it.

Prerequisites: [Schema](./02-schema.md), [Prisma Client](./03-prisma-client.md).

## Defining relations

### One-to-many

```prisma
model User {
  id    String @id @default(uuid())
  posts Post[]                       // virtual: no column in `User`
}

model Post {
  id       String @id @default(uuid())
  authorId String                    // the real foreign key column
  author   User   @relation(fields: [authorId], references: [id], onDelete: Cascade)

  @@index([authorId])                // index the FK yourself
}
```

- The side with `@relation(fields: [...], references: [...])` **owns the foreign key**.
- The other side (`Post[]`) is just the inverse and is required for the relation to be navigable from `User`. `prisma format` adds missing back-relations.
- Make the FK optional (`authorId String?`, `author User?`) for an optional relation.

### One-to-one

```prisma
model User {
  id      String   @id @default(uuid())
  profile Profile?
}

model Profile {
  id     String @id @default(uuid())
  userId String @unique              // @unique makes it one-to-one instead of one-to-many
  user   User   @relation(fields: [userId], references: [id])
}
```

The `@unique` on the foreign key is what enforces "at most one".

### Many-to-many

**Implicit** (Prisma manages the join table):

```prisma
model Post {
  id   String @id @default(uuid())
  tags Tag[]
}
model Tag {
  id    String @id @default(uuid())
  posts Post[]
}
```

**Explicit** (you own the join model, so it can carry data):

```prisma
model PostTag {
  postId  String
  tagId   String
  addedBy String
  post    Post   @relation(fields: [postId], references: [id], onDelete: Cascade)
  tag     Tag    @relation(fields: [tagId], references: [id], onDelete: Cascade)

  @@id([postId, tagId])
}
```

Use explicit when you need extra columns (who added it, when, an order), want to query the link table directly, or want control over its name and indexes. You can't easily convert implicit to explicit later without a data migration, so decide up front.

### Multiple relations between the same models and self-relations

Give each relation a **name** so Prisma can tell them apart:

```prisma
model User {
  id       String @id
  written  Post[] @relation("Author")
  reviewed Post[] @relation("Reviewer")
}
model Post {
  id         String  @id
  authorId   String
  reviewerId String?
  author     User    @relation("Author", fields: [authorId], references: [id])
  reviewer   User?   @relation("Reviewer", fields: [reviewerId], references: [id])
}
```

## Referential actions (`onDelete` / `onUpdate`)

These are **database-level** foreign key behaviors:

| Action | When the parent is deleted |
|--------|----------------------------|
| `Cascade` | Delete the children too |
| `Restrict` | Block the delete if children exist |
| `NoAction` | Like `Restrict` (database-dependent timing) |
| `SetNull` | Set the child FK to `NULL` (FK must be optional) |
| `SetDefault` | Set the child FK to its default |

There is **no ORM-level cascade** in Prisma. If you don't specify `onDelete`, the default depends on whether the relation is required (commonly `Restrict`) or optional (commonly `SetNull`); check the Prisma docs for the exact defaults in your version, and **state the action explicitly** so the behavior is visible in the schema. Prefer `Restrict` (loud failure) for important data and `Cascade` only where children are truly owned by the parent.

(With MongoDB, or when `relationMode = "prisma"` is set, referential actions are emulated by Prisma instead of enforced by the database. You then need to add indexes on relation fields yourself, and the guarantees are weaker.)

## Loading relations

```ts
// include: load all scalars + relation
await prisma.user.findUnique({ where: { id }, include: { posts: true } });

// nested, with filtering, ordering, and limiting
await prisma.user.findUnique({
  where: { id },
  include: {
    posts: {
      where: { published: true },
      orderBy: { createdAt: 'desc' },
      take: 5,
      include: { tags: true },
    },
  },
});

// select: pick fields at every level
await prisma.post.findMany({
  select: { id: true, title: true, author: { select: { id: true, name: true } } },
});
```

### Counting relations without loading them

```ts
await prisma.user.findMany({
  select: { id: true, _count: { select: { posts: true } } },
});
// → [{ id: '...', _count: { posts: 3 } }]
```

### Join vs separate queries

Prisma can load relations either with a single joined query or with multiple queries merged in the client. The option is `relationLoadStrategy: 'join' | 'query'` on a query, and the default and availability depend on your Prisma version, so check the docs and measure on real data. Separate queries can avoid duplicating the parent row per child for large collections; joins save round trips.

### Avoiding N+1

```ts
// ❌ N+1
const posts = await prisma.post.findMany();
for (const p of posts) p.author = await prisma.user.findUnique({ where: { id: p.authorId } });

// ✅ one operation
const posts = await prisma.post.findMany({ include: { author: true } });
```

Prisma batches certain concurrent `findUnique` calls automatically, which softens some N+1 cases, but don't rely on it; write the include. Watch the query log ([setup](./01-setup.md)).

## Filtering by relations

```ts
// users who have at least one published post
await prisma.user.findMany({ where: { posts: { some: { published: true } } } });

// users with NO posts
await prisma.user.findMany({ where: { posts: { none: {} } } });

// posts whose author is active
await prisma.post.findMany({ where: { author: { active: true } } });
```

Relation filters: lists use `some` / `every` / `none`; to-one relations use the nested filter directly (or `is` / `isNot`).

## Writing through relations (nested writes)

```ts
// create a user and posts in one atomic operation
await prisma.user.create({
  data: {
    email,
    posts: { create: [{ title: 'First' }, { title: 'Second' }] },
  },
});

// connect to an existing record
await prisma.post.create({
  data: { title: 'Hi', author: { connect: { id: userId } } },
});
// or simply set the scalar FK: { title: 'Hi', authorId: userId }  (unchecked form)

// connect or create
tags: { connectOrCreate: { where: { name: 'nest' }, create: { name: 'nest' } } }

// many-to-many management
data: { tags: { connect: [{ id: t1 }, { id: t2 }] } }
data: { tags: { disconnect: [{ id: t1 }] } }
data: { tags: { set: [{ id: t2 }, { id: t3 }] } }     // replace the entire set
```

Nested writes are **atomic**: if any part fails, the whole operation rolls back, without needing an explicit transaction. That's one of Prisma's nicest properties for creating aggregates ([transactions](./07-transactions.md)).

Prisma has two input "shapes": the **checked** form (using relation fields like `author: { connect }`) and the **unchecked** form (using scalar FKs like `authorId`). You can't mix them in one `data` object. Use whichever fits; the checked form allows nested writes, the unchecked form is simpler when you have ids.

Other nested operations: `update`, `updateMany`, `upsert`, `delete`, `deleteMany`, `createMany` (inside list relations).

## Practical guidance

- **Index foreign keys** with `@@index` (and unique where one-to-one).
- **Load only what the endpoint returns**; deep `include` trees can produce huge queries. Use `select` at each level.
- **Bound nested lists** with `take`; an unbounded `include: { posts: true }` on a prolific user is a performance trap.
- **Be explicit about `onDelete`.**
- Prefer **explicit many-to-many** if the link might ever carry data.
- Keep module boundaries in mind: don't reach across modules' tables for convenience ([architecture](../02-database-foundations/01-database-architecture.md)).

## Common mistakes

- **Forgetting the back-relation field**, causing schema validation errors (run `prisma format`).
- **No `@@index` on foreign keys.**
- **Missing `@unique` on a one-to-one FK**, silently making it one-to-many.
- **Implicit many-to-many** where you later need extra link data.
- **Relying on default `onDelete`** without knowing it.
- **Unbounded nested `include`** returning huge payloads.
- **Mixing checked and unchecked inputs** in one `data` object.
- **N+1 loops** instead of `include`.
- **`set` on a many-to-many** when you meant `connect`, replacing existing links.
- **Assuming cascades exist at the client level**; they're database behavior (or emulated in `relationMode = "prisma"`).

## Debugging

- "Error validating model ... relation field is missing an opposite": add the opposite field or run `prisma format`.
- Foreign key violation (`P2003`) on delete: children exist and the action is `Restrict`/`NoAction`; delete children first or choose a different action ([database errors](../02-database-foundations/07-database-errors.md)).
- A relation property is missing from the result: it wasn't in `include`/`select`.
- Duplicate or missing rows in nested results: review `where` placement (on the parent vs inside the nested relation).
- Slow endpoint: log queries; look for deep includes, missing FK indexes, and N+1.

## Quick Summary

- A relation = scalar FK field + `@relation(fields, references)` on the owning side + an opposite relation field; `@unique` on the FK makes it one-to-one.
- Many-to-many is implicit (managed join table) or explicit (your own model, carries data).
- `onDelete`/`onUpdate` are database referential actions; there's no client-level cascade. Be explicit.
- Load with `include`/`select` (nested, filterable, limitable); `_count` counts without loading; filter with `some`/`every`/`none`.
- Nested writes (`create`, `connect`, `connectOrCreate`, `set`) are atomic; don't mix checked and unchecked inputs.
- Index FKs, bound nested lists, and avoid N+1.

## Next

[Repositories →](./05-repositories.md)
