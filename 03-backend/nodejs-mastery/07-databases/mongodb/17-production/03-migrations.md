# Migrations

Changing a schema safely once real, live data already exists — the practical reality that development-time schema changes (just edit the file) don't extend to production without a plan.

## Why this is different from development

```js
// development — just change the schema, restart, done
const userSchema = new mongoose.Schema({
  name: String,
  fullName: String, // renamed from "name" — but existing documents still have "name", not "fullName"
});
```

MongoDB's flexible schema (`00-mongodb-basics/`) means changing a Mongoose schema doesn't touch existing documents at all — a renamed or newly-required field simply doesn't exist on documents written before the change. In development with throwaway data, this doesn't matter; in production with real, valuable data, every existing document needs to be accounted for.

---

## Migration 1: Adding a new field with a default

```js
const userSchema = new mongoose.Schema({
  name: String,
  role: { type: String, default: "user" },
});
```

The easiest case: a schema `default` (`04-schemas/03-schema-type-options.md`) only applies **when a document is created or saved** — it doesn't retroactively appear on existing documents just by adding it to the schema.

```js
const user = await User.findOne({
  /* an old document, created before "role" existed */
});
user.role; // "user" — Mongoose applies the schema default in memory when the document is loaded,
// even though the field doesn't actually exist in the stored document yet
```

For **most read purposes**, this is actually sufficient — Mongoose applies schema defaults to documents in memory on read, even if they're not physically present in the stored document. If you specifically need the field to actually exist in the database (for querying/indexing on it, since `$exists`/index behavior only sees what's actually stored), a backfill is needed:

```js
await User.updateMany({ role: { $exists: false } }, { $set: { role: "user" } });
```

---

## Migration 2: Renaming a field

```js
await User.updateMany(
  { oldFieldName: { $exists: true } },
  { $rename: { oldFieldName: "newFieldName" } },
);
```

`$rename` (from the MongoDB fundamentals section) renames the field directly at the database level across every matching document — necessary since a Mongoose schema change alone (renaming in the schema definition) has zero effect on already-stored data.

### The safe order of operations for a rename

```
1. Add the new field name to the schema, KEEPING the old one too (both present)
2. Deploy application code that writes to the NEW field, but still reads the OLD one as a fallback
3. Run the $rename migration across existing documents
4. Deploy application code that only uses the new field
5. Remove the old field name from the schema entirely
```

This staged approach avoids downtime — at every step, the application works correctly against whatever mix of old-shaped and new-shaped documents currently exists, rather than requiring an instant, all-at-once cutover.

---

## Migration 3: Changing a field's type

```js
// changing "age" from a String to a Number
```

```js
const users = await User.find({ age: { $type: "string" } }); // from 07-filter-conditions/04-element-and-type-operators.md
for (const user of users) {
  await User.updateOne({ _id: user._id }, { $set: { age: Number(user.age) } });
}
```

A type change needs an explicit backfill converting the actual stored values, since Mongoose's casting only applies to _new_ writes going through the schema — it doesn't reach back and convert already-stored data.

### Batching a large backfill

```js
const BATCH_SIZE = 1000;
let processed = 0;

while (true) {
  const batch = await User.find({ age: { $type: "string" } }).limit(BATCH_SIZE);
  if (batch.length === 0) break;

  await User.bulkWrite(
    batch.map((user) => ({
      updateOne: {
        filter: { _id: user._id },
        update: { $set: { age: Number(user.age) } },
      },
    })),
  );

  processed += batch.length;
  console.log(`Processed ${processed} documents`);
}
```

For a large collection, processing in batches (rather than one enormous `updateMany`-style operation, or a naive one-at-a-time loop) balances progress visibility, memory usage, and not holding locks/resources for an excessively long single operation.

---

## Adding a new required field to an existing schema

```js
const userSchema = new mongoose.Schema({
  name: String,
  email: { type: String, required: true }, // newly required
});
```

```js
await User.findById(existingUserId); // ✅ still readable — required only affects WRITES
await User.updateOne({ _id: existingUserId }, { $set: { name: "New Name" } }); // ✅ still works —
// per 09-validation/04-, this only
// validates fields being CHANGED
```

`required` doesn't retroactively break reads or unrelated updates to old documents missing the field — but attempting to `.save()` an old document (which loads its full state, including the missing field) as-is would now fail validation, since `.save()` validates the whole document. This is worth testing deliberately before deploying a newly-required field on a schema with existing data.

---

## Migration tooling: writing them as scripts

```js
// migrations/001-backfill-user-role.js
import mongoose from "mongoose";
import User from "../models/User.js";

export async function up() {
  await User.updateMany(
    { role: { $exists: false } },
    { $set: { role: "user" } },
  );
}

export async function down() {
  await User.updateMany({}, { $unset: { role: "" } });
}
```

A simple `up`/`down` convention — `up` applies the migration, `down` reverses it if something goes wrong. For anything beyond the simplest projects, a dedicated migration tool (e.g. `migrate-mongo`) manages tracking which migrations have already run, in what order, and prevents accidentally re-running one.

---

## Testing a migration before running it in production

```js
// 1. run it against a recent production data snapshot restored to a staging environment
// 2. verify the expected number of documents were affected
// 3. spot-check a sample of migrated documents manually
```

Given that a migration touches real, valuable data, testing it against a realistic copy of production data (not just a handful of test fixtures) before running it for real is worth the extra step — a migration that works perfectly against a small, clean test dataset can still behave unexpectedly against the genuine variety and scale of real production data.

## Common mistakes

- **Assuming a schema change alone updates existing documents** — it doesn't; MongoDB's flexible schema means old documents keep their old shape until an explicit backfill runs.
- **Making a field required without considering existing documents that lack it** — reads/unrelated updates still work, but a full `.save()` on such a document will now fail.
- **Running a large migration as one giant, unbatched operation** — risks memory pressure and long lock/resource hold times; batch it instead.
- **Not testing a migration against realistic data before production** — a script that works on clean test fixtures can still fail in surprising ways against real, messy production data.
- **No rollback plan** — writing only the forward migration, with no way to reverse it if something goes wrong partway through.

## Quick summary

- MongoDB's flexible schema means a Mongoose schema change has zero automatic effect on already-stored documents — an explicit migration script is needed for anything beyond adding an optional field with a default
- The safe pattern for a rename: add the new field alongside the old, deploy dual-reading code, migrate the data, deploy new-only code, then remove the old field — avoiding a hard, instant cutover
- Batch large backfills rather than running one enormous operation or a naive one-at-a-time loop
- Test migrations against realistic (ideally production-like) data before running them for real, and always have a rollback plan

## Next

**`04-production-checklist.md`** pulls together the important details from across this entire guide into one final, consolidated checklist.
