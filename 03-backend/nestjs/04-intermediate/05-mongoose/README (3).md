# Mongoose

Mongoose is the standard ODM (Object Document Mapper) for **MongoDB** in Node.js. It adds schemas, validation, middleware (hooks), and `populate` on top of the MongoDB driver. `@nestjs/mongoose` is the official Nest integration: it turns schemas into injectable `Model`s.

```text
 @Schema classes ──► SchemaFactory.createForClass ──► Schema
                                                        │
 MongooseModule.forRootAsync  ──► Connection ───────────┤
 MongooseModule.forFeature([{ name, schema }])  ────────┴──► Model<User> (injectable)
                                                                │
                                   find / create / update / aggregate
                                                                │
                                                                ▼
                                                            MongoDB
```

> **Version note.** Mongoose **9** is current: it requires async/promise-based middleware (callback-style `next()` in `pre()` hooks was removed), uses MongoDB driver v7, and tightened several typings and behaviors. Mongoose 8 is still widely deployed. Examples here use **async hooks that work on both**, and flag version-sensitive points. Check the peer-dependency range of the `@nestjs/mongoose` version you install to see which Mongoose major it supports, and read the [Mongoose migration guide](https://mongoosejs.com/docs/migrating_to_9.html) before upgrading.

## Reading order

| # | Note | Answers |
|---|------|---------|
| 01 | [Setup](./01-setup.md) | Install, `forRootAsync`, `forFeature`, injecting models, replica set for local dev |
| 02 | [Schemas and models](./02-schemas-and-models.md) | `@Schema`/`@Prop`, types, validation gotchas, `toJSON`, `strict` |
| 03 | [Repositories](./03-repositories.md) | CRUD, `lean()`, update semantics, NoSQL injection, error mapping |
| 04 | [Populate](./04-populate.md) | References, virtual populate, `$lookup`, embed vs reference |
| 05 | [Middleware and hooks](./05-middleware-hooks.md) | Document/query hooks, DI in hooks, what hooks don't cover |
| 06 | [Indexes](./06-indexes.md) | Compound/TTL/partial indexes, `autoIndex`, `explain` |
| 07 | [Transactions](./07-transactions.md) | Replica sets, `withTransaction`, sessions, when to avoid them |

## Prerequisites

- [Database foundations](../02-database-foundations/README.md) (connection, repository pattern, transactions concepts)
- [Dynamic modules](../../03-core-concepts/04-modules-and-di/03-dynamic-modules.md) and [configuration](../../03-core-concepts/03-configuration/README.md)
- Basic MongoDB concepts: collections, documents, `_id`, `ObjectId`

## Related

- [ORM comparison](../02-database-foundations/08-orm-comparison.md)
- [Database errors](../02-database-foundations/07-database-errors.md) (duplicate key `11000`)
- [Testing with a real database](../01-testing/05-integration-testing.md)
- [Mongoose quick reference](../../13-quick-reference/09-mongoose.md)
