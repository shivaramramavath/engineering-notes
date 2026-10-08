# Schemas and Models

A **schema** defines the shape, types, defaults, and validation of documents in a collection. A **model** is the compiled, queryable version of a schema (`Model<User>`). `@nestjs/mongoose` lets you write schemas as decorated classes, which fit Nest's style and TypeScript types.

Prerequisites: [Setup](./01-setup.md), [DTO vs persistence model](../../03-core-concepts/02-validation-and-serialization/01-dto.md).

## A first schema

```ts
// users/schemas/user.schema.ts
import { Prop, Schema, SchemaFactory } from '@nestjs/mongoose';
import { HydratedDocument } from 'mongoose';

export type UserDocument = HydratedDocument<User>;

export enum Role { User = 'user', Admin = 'admin' }

@Schema({ timestamps: true })               // adds createdAt / updatedAt
export class User {
  @Prop({ required: true, unique: true, lowercase: true, trim: true })
  email: string;

  @Prop({ required: true, select: false })  // excluded from queries by default
  passwordHash: string;

  @Prop({ type: String, enum: Role, default: Role.User })
  role: Role;

  @Prop({ default: true })
  active: boolean;

  @Prop({ type: [String], default: [] })
  tags: string[];

  @Prop({ type: String, default: null })
  nickname: string | null;
}

export const UserSchema = SchemaFactory.createForClass(User);
```

- `@Schema(options)` takes Mongoose schema options (`timestamps`, `collection`, `versionKey`, `toJSON`, `strict`, ...).
- `@Prop(options)` declares a field. Options include `required`, `unique`, `default`, `enum`, `trim`, `lowercase`, `min`/`max`, `minlength`/`maxlength`, `match`, `select`, `index`, `ref`.
- `SchemaFactory.createForClass(User)` builds the runtime `Schema`.
- `HydratedDocument<User>` is the type of a **document instance** (with `_id`, `save()`, etc.). `Model<User>` is the model type. A `.lean()` result is a plain object typed as `User`.

## Type inference: where `@Prop()` needs help

`@Prop()` reads the TypeScript type via reflection, which works for simple types (`string`, `number`, `boolean`, `Date`). It **can't** infer arrays, unions, nullable types, nested objects, or `ObjectId`s, and fails with an error like:

```text
Cannot determine a type for the "User.nickname" field (union/intersection/ambiguous type was used). Make sure your property is decorated with a "@Prop()" decorator.
```

Be explicit:

```ts
@Prop({ type: [String] })                tags: string[];
@Prop({ type: String, default: null })   nickname: string | null;
@Prop({ type: Types.ObjectId, ref: 'User' }) owner: Types.ObjectId;
@Prop({ type: Object })                  metadata: Record<string, unknown>;
@Prop({ type: mongoose.Schema.Types.Mixed }) anything: unknown;     // no validation, no change tracking
```

## Nested and embedded documents

```ts
@Schema({ _id: false })                  // no separate _id for the subdocument
export class Address {
  @Prop({ required: true }) street: string;
  @Prop({ required: true }) city: string;
}
export const AddressSchema = SchemaFactory.createForClass(Address);

@Schema()
export class Company {
  @Prop({ type: AddressSchema })         // single embedded document
  address: Address;

  @Prop({ type: [AddressSchema], default: [] })   // array of embedded documents
  branches: Address[];
}
```

Subdocument arrays get their own `_id`s by default (unless `_id: false`). Whether to **embed or reference** related data is covered in [populate](./04-populate.md).

## Validation: what Mongoose does and doesn't do

Mongoose validates against the schema (`required`, `enum`, `min`, `match`, custom `validate`) when you call **`save()`** or **`create()`**. Two surprises:

1. **Update queries skip validators by default.** `updateOne`, `findOneAndUpdate`, and friends don't run schema validators unless you pass `runValidators: true`, and even then validators work differently (they only see the update, and some validators like `required` only trigger on `$unset`).
2. **`unique` is not a validator.** It creates a unique **index**. Duplicates are rejected by MongoDB with error `11000`, not by Mongoose validation ([indexes](./06-indexes.md), [errors](./03-repositories.md)).

```ts
await this.userModel.updateOne({ _id }, { role: 'bogus' });                              // may write garbage
await this.userModel.updateOne({ _id }, { role: 'bogus' }, { runValidators: true });     // throws ValidationError
```

Validate **at the boundary** with DTOs and `ValidationPipe` ([DTOs](../../03-core-concepts/02-validation-and-serialization/01-dto.md), [ValidationPipe](../../03-core-concepts/02-validation-and-serialization/02-validation-pipe.md)) so bad data never reaches the model, and treat Mongoose validation as a second line of defense, not the first.

## `strict` mode drops unknown fields

By default Mongoose silently **discards fields not in the schema** on writes:

```ts
await this.userModel.create({ email, isSuperAdmin: true });   // isSuperAdmin is ignored (not in schema)
```

This is a mild mass-assignment protection but also a source of "my field disappeared" bugs. Don't rely on it for security; use whitelisted DTOs.

## IDs

- Every document gets `_id: ObjectId` by default.
- Mongoose adds an **`id` virtual** (string) on documents, but it's **not** present on `lean()` results.
- In queries, string ids are cast to `ObjectId` automatically. An **invalid** string throws `CastError`, so validate ids at the boundary (`mongoose.isValidObjectId`, a custom pipe) ([repositories](./03-repositories.md)).
- `ObjectId`s serialize to strings in JSON, but comparing them requires `.equals()` or `String(a) === String(b)`, not `===`.

## Shaping JSON output

Documents have `toJSON()`; configure it once on the schema to drop internals and rename `_id`:

```ts
UserSchema.set('toJSON', {
  versionKey: false,
  transform: (_doc, ret: Record<string, any>) => {
    ret.id = ret._id.toString();
    delete ret._id;
    delete ret.passwordHash;
    return ret;
  },
});
```

This only applies to **document instances** serialized via `toJSON`, not to `lean()` results. A safer, more explicit approach is to map to response DTOs ([serialization](../../03-core-concepts/02-validation-and-serialization/07-serialization.md)). `select: false` on sensitive props (as above) is another layer, but `create()`/`save()` results still contain the value, so don't return them directly.

## Virtuals, methods, statics

```ts
UserSchema.virtual('displayName').get(function (this: UserDocument) {
  return this.email.split('@')[0];
});

UserSchema.methods.isAdmin = function (this: UserDocument) {
  return this.role === Role.Admin;
};

UserSchema.statics.findByEmail = function (email: string) {
  return this.findOne({ email: email.toLowerCase() });
};
```

Virtuals aren't stored or queryable. Instance methods only exist on hydrated documents (not `lean()` results). Typing methods/statics needs extra interface work; many teams skip them and put logic in services or repositories instead, which is easier to test and to type.

## Timestamps and versioning

- `timestamps: true` maintains `createdAt`/`updatedAt` (including on update queries).
- `__v` (the version key) is incremented when arrays are modified through certain document operations, to catch concurrent array updates. Set `versionKey: false` to remove it, or enable `optimisticConcurrency: true` to make `save()` fail on version mismatches ([transactions](./07-transactions.md)).

## Choosing field types and shapes

| Need | Use |
|------|-----|
| Limited set of values | `type: String, enum: [...]` (validated on `save`; not enforced by the database) |
| Money | Integer minor units (cents) or `Decimal128` (returned as a `Decimal128` object, needs conversion) |
| UUIDs | `String`, or Mongoose's UUID type (behavior changed in Mongoose 9 to return BSON UUIDs; check the migration guide) |
| Flexible/unknown shape | `Mixed` or `Object`, knowing there's no validation or change detection (use `markModified`) |
| Dates | `Date`; store UTC |

Think in terms of **documents shaped for your reads**, not normalized tables; see [populate](./04-populate.md) for embed-vs-reference guidance.

## Common mistakes

- **Missing explicit `type`** on arrays, unions, nullable fields, and `ObjectId`s, causing "Cannot determine a type" errors.
- **Assuming update queries validate** (they don't without `runValidators`).
- **Treating `unique: true` as validation** rather than an index.
- **Returning documents directly**, leaking `passwordHash`/`__v` (and `_id` naming).
- **Using `===` to compare `ObjectId`s.**
- **Unbounded arrays** inside a document (growth toward the 16 MB document limit).
- **Using `Mixed` everywhere**, losing validation and change tracking.
- **Relying on `strict` mode** as a security control.
- **Putting business logic in schema methods** that don't exist on `lean()` results.
- **Money in floating-point numbers.**

## Debugging

- "Cannot determine a type for the X field": add `@Prop({ type: ... })`.
- Field missing after save: it isn't in the schema (`strict`), or `select: false` hides it on read.
- Nested change not persisted (Mixed/Object): call `doc.markModified('path')` before `save()`.
- `CastError: Cast to ObjectId failed`: an invalid id string reached a query; validate earlier.
- `doc.id` undefined: you're looking at a `lean()` result; use `_id`.
- Validation passes on update but data is wrong: add `runValidators: true` (or validate in the DTO).

## Quick Summary

- `@Schema` + `@Prop` + `SchemaFactory.createForClass` define the schema; `HydratedDocument<T>` types documents, `Model<T>` types models.
- Give `@Prop` an explicit `type` for arrays, unions, nullable fields, and `ObjectId`s.
- Schema validation runs on `save`/`create`, not (by default) on updates; `unique` is an index, not a validator; unknown fields are dropped by `strict`.
- Shape JSON via `toJSON` or response DTOs; `lean()` results are plain objects without virtuals, methods, or `id`.
- Model data for your read patterns and keep arrays bounded.

## Next

[Repositories →](./03-repositories.md)
