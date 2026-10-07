# ORM Comparison

This repository teaches three data layers in depth: **TypeORM**, **Prisma**, and **Mongoose**. They solve overlapping problems with different philosophies. This note helps you choose, and, just as important, tells you what the choice does *not* change.

> Tools evolve quickly. Treat the specifics below as a snapshot of how these tools are generally positioned, and check each project's current documentation (versions, supported databases, features) before committing.

Prerequisites: [Database architecture](./01-database-architecture.md), [repository pattern](./03-repository-pattern.md).

## At a glance

| | **TypeORM** | **Prisma** | **Mongoose** |
|-|-------------|------------|--------------|
| Kind | ORM (decorator-based entities) | ORM/client generated from a schema file | ODM for MongoDB |
| Databases | Several SQL databases (PostgreSQL, MySQL, SQLite, SQL Server, and others); MongoDB support exists but is limited | PostgreSQL, MySQL, SQLite, SQL Server, MongoDB, CockroachDB (check current list) | MongoDB only |
| Schema defined in | TypeScript classes with decorators | `schema.prisma` | Mongoose schemas (code) |
| Types | TypeScript classes; query result types are less precise (relations, partial selects) | Generated client; query result types follow your `select`/`include` | Good with TypeScript, but schema and interface can drift without care |
| Migrations | Generated from entity diffs or hand-written | Generated from the schema (`prisma migrate`) | None built in; scripts or external tools |
| Query style | Repository/`find` options, `QueryBuilder` | Fluent client API, raw SQL escape hatch | Query builder and aggregation pipeline |
| Relations | Lazy/eager options, `relations`, joins | `include`/`select`, nested writes | `populate`, embedded documents |
| Nest integration | Official `@nestjs/typeorm` | Custom `PrismaService` pattern (documented recipe) | Official `@nestjs/mongoose` |
| Active Record / Data Mapper | Supports both | Neither (client-based) | Model-based (Active Record-like) |

Deep dives: [TypeORM](../03-typeorm/README.md), [Prisma](../04-prisma/README.md), [Mongoose](../05-mongoose/README.md).

## Philosophies

### TypeORM: entities as classes

You describe tables as decorated classes; the ORM maps rows to instances. It's flexible (Active Record or Data Mapper, a powerful `QueryBuilder`, many databases) and fits Nest's decorator-heavy style and official integration well.

```ts
@Entity()
export class User {
  @PrimaryGeneratedColumn('uuid') id: string;
  @Column({ unique: true }) email: string;
  @OneToMany(() => Order, (o) => o.user) orders: Order[];
}
```

Trade-offs to know: result types don't always reflect which relations you actually loaded (a relation may be typed as present but be `undefined`), behaviors like lazy relations and cascades have sharp edges, and some features have historically lagged behind in polish. Maintenance activity and roadmap are worth checking when you start a long-lived project.

### Prisma: schema first, generated client

You write a `schema.prisma`; Prisma generates a type-safe client and manages migrations from it.

```prisma
model User {
  id     String  @id @default(uuid())
  email  String  @unique
  orders Order[]
}
```

```ts
const user = await prisma.user.findUnique({
  where: { id },
  include: { orders: true },        // result type includes orders because you asked for them
});
```

Strengths: precise result typing, a consistent API, and good developer experience with migrations and tooling. Trade-offs: a separate schema language and a generate step, no entity classes (so `@Exclude`-style serialization needs mapping, see [serialization](../../03-core-concepts/02-validation-and-serialization/07-serialization.md)), some advanced SQL is easier via raw queries, and how it connects and runs queries (its engine and connection handling) differs between versions, which matters for serverless and pooling. Check current docs.

### Mongoose: schemas for documents

For MongoDB, Mongoose adds schemas, validation, middleware (hooks), and `populate` on top of the driver.

```ts
@Schema({ timestamps: true })
export class User {
  @Prop({ required: true, unique: true }) email: string;
}
export const UserSchema = SchemaFactory.createForClass(User);
```

It's the natural choice if you've decided on MongoDB. It doesn't help with relational concerns, since there are no joins or foreign key constraints in the relational sense, and multi-document transactions need a replica set ([transactions](./04-transactions.md)).

## Dimensions that actually matter

| Concern | What to weigh |
|---------|---------------|
| **Database choice** | This usually decides it. MongoDB → Mongoose (or Prisma's MongoDB support); relational → TypeORM or Prisma |
| **Type safety** | Prisma's generated, query-aware types catch more mistakes at compile time; TypeORM and Mongoose rely more on discipline |
| **Complex queries** | All offer raw SQL/aggregation escape hatches; TypeORM's `QueryBuilder` and SQL-first tools are strong for dynamic composition |
| **Migrations workflow** | Prisma's schema-driven flow is smooth; TypeORM's generation works but needs review; Mongoose needs separate tooling ([migrations](./05-migrations.md)) |
| **Domain modeling** | Class-based entities (TypeORM) suit rich domain objects; Prisma returns plain objects, so you map to domain classes yourself |
| **Nest ergonomics** | Official modules for TypeORM and Mongoose; Prisma needs a small service wrapper, which is easy |
| **Team familiarity** | A tool the team knows well often beats a "better" one they don't |
| **Performance** | Differences are usually dominated by **your queries and indexes** (N+1, missing indexes), not the ORM. Measure real workloads ([indexing](./06-indexing-and-query-basics.md)) |
| **Ecosystem and maintenance** | Check release cadence, open issues, and community health at decision time |

## A pragmatic decision guide

- **You chose MongoDB** → Mongoose.
- **Relational, you value end-to-end type safety and tooling, and plain-object results are fine** → Prisma.
- **Relational, you prefer decorator/class entities, need heavy dynamic query composition, multiple SQL databases, or you're extending an existing TypeORM codebase** → TypeORM.
- **Unsure and starting a new relational project** → pick one of Prisma or TypeORM, put queries behind [repositories](./03-repository-pattern.md), and keep going. A good repository boundary makes a later migration far less painful.

There are other options in the ecosystem (for example Sequelize, MikroORM, Drizzle, and query builders like Knex or Kysely). The same questions apply to them; this repository doesn't cover them in depth.

## What the choice does *not* change

- You still need **indexes**, careful queries, and N+1 awareness.
- You still need **transactions** where multi-step writes must be atomic, and each ORM expresses them differently ([transactions](./04-transactions.md)).
- You still need **migrations** discipline in production.
- You still need **DTOs and serialization** so persistence models don't leak to clients.
- You still translate **database errors** into meaningful responses ([database errors](./07-database-errors.md)).
- Business logic belongs in services, not in the ORM layer.

## Common mistakes

- **Choosing by popularity or a benchmark** instead of your data model, team, and workload.
- **Mixing multiple ORMs** in one codebase without a strong reason.
- **Blaming the ORM** for slow endpoints that are really N+1 or missing indexes.
- **Leaking ORM types** (entities, Prisma types, documents) through controllers and across modules.
- **Using `synchronize`/`db push`** because the ORM makes it easy.
- **Using MongoDB for clearly relational data**, then rebuilding joins and integrity in application code.
- **Not checking current docs** on supported databases and features before relying on a claim.

## Quick Summary

- TypeORM = decorator-based entities, many SQL databases, official Nest module; Prisma = schema-first generated client with strong typing; Mongoose = MongoDB schemas and hooks.
- The database decision usually decides the ORM; after that, weigh type safety, query needs, migrations workflow, and team familiarity.
- Performance problems are mostly query and index problems, not ORM problems.
- Wrap data access in repositories so the choice stays reversible.
- Indexes, transactions, migrations, DTOs, and error translation matter regardless of ORM.

## Next

Section complete. Continue with [TypeORM](../03-typeorm/README.md), [Prisma](../04-prisma/README.md), or [Mongoose](../05-mongoose/README.md), then return here to revisit the trade-offs with hands-on context.

← Back to [Database foundations overview](./README.md)
