# Middleware and Hooks

Mongoose middleware (also called **hooks**) are functions that run before or after certain operations: hash a password before `save`, add a soft-delete filter before every `find`, log after `updateOne`. They're powerful and implicit, which is exactly why you need to know **which operations trigger which hooks**. Most hook bugs are "my hook didn't run".

Prerequisites: [Schemas and models](./02-schemas-and-models.md), [repositories](./03-repositories.md).

> **Mongoose 9 note.** Callback-style middleware is gone: `pre()` hooks must be **`async` functions or return a Promise**; `next()` is no longer supported there (and the legacy `isAsync`/`function(next, done)` form is removed). The examples below use async hooks, which work in Mongoose 8 and 9. Code written as `schema.pre('save', function (next) { ...; next(); })` must be converted when upgrading. Check the migration guide for details such as error-handling middleware signatures in your version.

## The four kinds of middleware

| Kind | `this` is | Hooks on | Example |
|------|-----------|----------|---------|
| **Document** | The document | `save`, `validate`, `init`, `updateOne`/`deleteOne` (document form) | Hash password on `save` |
| **Query** | The `Query` | `find`, `findOne`, `findOneAndUpdate`, `updateOne`, `deleteOne`, `countDocuments`, ... | Soft-delete filter |
| **Aggregate** | The `Aggregate` | `aggregate` | Prepend a `$match` stage |
| **Model** | The model | `insertMany`, `bulkWrite` | Validate bulk input |

## Basic examples

```ts
// users/schemas/user.schema.ts
UserSchema.pre('save', async function () {
  if (!this.isModified('passwordHash')) return;
  // transform the field before it's persisted
  this.email = this.email.toLowerCase();
});

UserSchema.post('save', function (doc) {
  // runs after a successful save; `doc` is the saved document
});
```

Rules:

- Use a **regular `function`**, not an arrow function, so `this` is bound by Mongoose.
- Throw (or reject) in a `pre` hook to **abort** the operation and surface the error to the caller.
- `post` hooks run after the operation; they can't change the result in most cases.
- The hook is attached to the **schema before the model is compiled**. In Nest, that means before `forFeature` registers it ([DI section below](#using-nest-providers-in-hooks)).

## Query middleware: soft delete and defaults

```ts
UserSchema.pre(/^find/, function () {
  // `this` is the Query; applies to find, findOne, findOneAndUpdate...
  this.where({ deletedAt: null });
});
```

Useful query helpers inside the hook: `this.getFilter()`, `this.setQuery()`, `this.getUpdate()`, `this.getOptions()`, `this.where()`, `this.projection()`.

Caveats:

- A regex hook name like `/^find/` also matches `findOneAndUpdate`, `findOneAndDelete`, etc. That's often wanted, but check whether it's right for each.
- It doesn't cover `countDocuments`, `updateOne`, `deleteOne`, or `aggregate` unless you hook them too, so "soft-deleted rows are hidden" is only true for the operations you covered. Test each query path ([soft delete](../../08-architecture-and-patterns/04-real-world-patterns/02-soft-delete.md)).
- Populated relations run their own queries, which also trigger the **populated model's** query hooks.
- You need an escape hatch for admin/restore flows (for example a query option such as `{ withDeleted: true }` checked via `this.getOptions()`).

## What hooks do NOT cover (the big one)

| You call | Document `save` hooks run? | Query hooks run? | Validators run? |
|----------|---------------------------|------------------|-----------------|
| `doc.save()` / `Model.create()` | **Yes** | No | Yes |
| `Model.updateOne()` / `findOneAndUpdate()` / `updateMany()` | **No** | Yes (`updateOne`, `findOneAndUpdate`, ...) | Only with `runValidators: true` |
| `Model.insertMany()` | No (has its own `insertMany` model hook) | No | Yes |
| `Model.bulkWrite()` | No | No | **No** |
| Raw collection / driver calls (`Model.collection.*`) | No | No | No |
| `lean()` queries | n/a (no documents) | Yes (query hooks run) | n/a |

The classic bug: a password-hashing hook on `save`, then an update path that does `updateOne({ _id }, { passwordHash: plain })`, which stores the **plaintext** because the `save` hook never ran. Defend by:

- Doing critical transformations (hashing, normalization) in **one explicit place** (a service/repository method) rather than relying on hooks everywhere.
- Adding matching **query** hooks for update operations where a rule must hold, and handling `bulkWrite` explicitly.
- Enforcing invariants that must **always** hold at the database level (unique indexes, schema validation) rather than in hooks.

## Using Nest providers in hooks

Hooks are attached when the schema is built, outside DI. To use a Nest provider, build the schema with `forFeatureAsync`:

```ts
MongooseModule.forFeatureAsync([
  {
    name: User.name,
    imports: [ConfigModule],
    inject: [ConfigService],
    useFactory: (config: ConfigService) => {
      const schema = UserSchema;
      schema.pre('save', async function () {
        // use `config` (captured by closure) here
      });
      return schema;
    },
  },
]);
```

The factory runs once at module initialization; the closure captures the injected providers. Watch for these pitfalls:

- `UserSchema` is a **module-level singleton**. If the factory (or an import) adds the same hook twice, it runs twice. Build/attach hooks in exactly one place, or create a fresh schema with `SchemaFactory.createForClass(User)` inside the factory.
- Injected providers captured in a hook **must be singletons**. A request-scoped provider can't be used this way ([scopes](../../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md)).
- Heavy logic in hooks (network calls, emails) makes saves slow and failure-prone. Prefer explicit service code or events.

## Plugins: reusable hooks

A plugin is a function that adds fields/hooks to any schema:

```ts
function softDeletePlugin(schema: Schema) {
  schema.add({ deletedAt: { type: Date, default: null } });
  schema.pre(/^find/, function () {
    if (!this.getOptions().withDeleted) this.where({ deletedAt: null });
  });
}

UserSchema.plugin(softDeletePlugin);
```

Register globally (all schemas on a connection) via `connectionFactory`:

```ts
MongooseModule.forRootAsync({
  useFactory: (config: ConfigService) => ({
    uri: config.getOrThrow('MONGODB_URI'),
    connectionFactory: (connection) => {
      connection.plugin(softDeletePlugin);
      return connection;
    },
  }),
  inject: [ConfigService],
});
```

Global plugins are convenient and invisible, so document them, and remember they apply to **every** model.

## Hooks vs service logic: how much magic?

| Use hooks for | Prefer explicit code for |
|---------------|--------------------------|
| Cross-cutting, uniform behavior (timestamps, soft-delete filter, auditing fields) | Business rules and workflows |
| Data normalization that must apply on every `save` | Side effects: emails, events, webhooks |
| Defense-in-depth defaults | Anything that must also work for bulk/update operations |

Side effects in `post('save')` run even for saves inside a transaction that later aborts, and they fire before you know the whole operation succeeded. Publish events **after** your service has committed ([transactions](./07-transactions.md), [outbox](../../08-architecture-and-patterns/04-real-world-patterns/03-transactional-outbox.md)).

## Testing hooks

Hooks are best verified against a real MongoDB (or in-memory server): create/update through the model and assert the stored result, covering every write path you support ([integration testing](../01-testing/05-integration-testing.md)). Unit-testing a hook in isolation mostly tests Mongoose.

## Common mistakes

- **Using `next()` in `pre` hooks on Mongoose 9** (removed); use async functions.
- **Arrow functions**, losing `this`.
- **Assuming `save` hooks run on `updateOne`/`findOneAndUpdate`/`bulkWrite`.**
- **Soft-delete hook covering `find` but not `countDocuments`, `updateOne`, or `aggregate`.**
- **Registering the same hook twice** on a shared schema object.
- **Hooks with side effects** (email, HTTP) that slow or break saves.
- **Using request-scoped providers** in hooks.
- **Critical invariants enforced only in hooks**, bypassed by bulk or driver-level writes.
- **Forgetting an escape hatch** (`withDeleted`) for admin/restore flows.

## Debugging

- Hook not running: check which operation you called (table above), whether the hook was attached before the model was compiled, and that it isn't an arrow function.
- Hook runs twice: attached twice (module-level schema reused, or both a global plugin and a local call).
- `TypeError: next is not a function` or hooks silently misbehaving after upgrading: Mongoose 9 removed callback-style `pre` middleware; convert to async.
- Wrong filter in query hooks: log `this.getFilter()` inside the hook.
- Set `mongoose.set('debug', true)` in development to see the final operations sent to MongoDB.

## Quick Summary

- Hooks come in four kinds (document, query, aggregate, model); `this` differs per kind; use regular functions.
- Mongoose 9: `pre` hooks are async/promise-based only; no `next()`.
- `save` hooks don't run for updates, `bulkWrite`, or driver calls: cover each write path or enforce invariants in the database.
- Use `forFeatureAsync` to access Nest providers; avoid attaching hooks twice; avoid side effects in hooks.
- Use plugins for reusable behavior and test hooks against a real database.

## Next

[Indexes →](./06-indexes.md)
