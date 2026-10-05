# Pick, Omit, and Record

`Pick` and `Omit` build a new object type by **selecting or dropping keys** from an existing one. `Record` builds an object type from **a set of keys and one value type**. Together they cover most "derive a type from another type" work: public views of an entity, create/update inputs, lookup tables.

**Prerequisites:**
- [Partial, Required, Readonly](./00-partial-required-readonly.md)
- [keyof and typeof](../06-generics/03-keyof-and-typeof.md)
- [Index signatures](../04-objects-and-interfaces/03-index-signatures.md) (for the `Record<string, T>` section)

---

## Definitions

```ts
type Pick<T, K extends keyof T> = { [P in K]: T[P] };
type Omit<T, K extends keyof any> = Pick<T, Exclude<keyof T, K>>;
type Record<K extends keyof any, T> = { [P in K]: T };
```

(`keyof any` is `string | number | symbol`.) Two things to notice right away:

- `Pick` requires `K` to be real keys of `T`. A typo is an error.
- `Omit` is built from `Pick` and `Exclude`, and its `K` is **not** constrained to `keyof T`. A typo is silently accepted. More on this below.

## Pick: keep only these keys

```ts
interface User {
  id: number;
  name: string;
  email: string;
  passwordHash: string;
  createdAt: Date;
}

type UserPreview = Pick<User, "id" | "name">;
// { id: number; name: string }
```

`Pick` keeps the modifiers (`?`, `readonly`) of the picked properties, because it maps over keys of `T`.

Typical uses: component props that need only part of an entity, function parameters that should depend on the minimum they read.

```ts
function greet(user: Pick<User, "name">) {
  return `Hello ${user.name}`;
}
```

Narrow parameter types like this make functions easier to call and test: callers pass any object that has `name`.

## Omit: drop these keys

```ts
type PublicUser = Omit<User, "passwordHash">;
type CreateUserInput = Omit<User, "id" | "createdAt">;
```

Rule of thumb: use `Pick` when you want a **few** keys, `Omit` when you want **almost all** of them. The advantage of `Omit` is that new fields added to `User` flow into the derived type automatically. That is also its risk: a new sensitive field (say `mfaSecret`) would leak into `PublicUser` without any error. For security-facing types, prefer `Pick` (allow-list) over `Omit` (deny-list).

### Gotcha: `Omit` does not check key names

```ts
type Oops = Omit<User, "pasword">; // no error, nothing is omitted
```

If you want the check, define a strict version:

```ts
type StrictOmit<T, K extends keyof T> = Omit<T, K>;
```

### Gotcha: `Omit` does not distribute over unions

`keyof (A | B)` is only the keys **common** to both, so `Omit` on a union flattens it to those common keys and destroys the discriminant structure:

```ts
type Shape =
  | { kind: "circle"; radius: number; id: string }
  | { kind: "square"; size: number; id: string };

type NoId = Omit<Shape, "id">;
// { kind: "circle" | "square" }   <- radius and size are gone
```

Fix it with a distributive version:

```ts
type DistributiveOmit<T, K extends PropertyKey> =
  T extends unknown ? Omit<T, K> : never;

type NoId2 = DistributiveOmit<Shape, "id">;
// { kind: "circle"; radius: number } | { kind: "square"; size: number }
```

This works because a conditional type on a bare type parameter distributes over unions. See [distributive conditional types](../10-advanced-types/01-distributive-conditional-types.md).

### Gotcha: `Omit` on index-signature types

If `T` has a string index signature, `keyof T` is `string | number`, and excluding a literal from it leaves `string | number`. Known properties are lost. Avoid `Omit`/`Pick` on types that mix an index signature with named keys.

## Record: a map from keys to a value type

```ts
type Role = "admin" | "editor" | "viewer";

const permissions: Record<Role, string[]> = {
  admin: ["read", "write", "delete"],
  editor: ["read", "write"],
  viewer: ["read"],
};
```

With a **union of literals** as keys, `Record` is a lookup table where **every key is required**. That gives you free exhaustiveness: if someone adds `"guest"` to `Role`, this object stops compiling until it handles it.

```ts
const labels: Record<Role, string> = {
  admin: "Administrator",
  editor: "Editor",
  // error: Property 'viewer' is missing
};
```

This is often a cleaner alternative to a `switch` for pure mappings. For a sparse map, wrap it: `Partial<Record<Role, string>>`.

### `Record<string, T>` is an index signature

With `string` as the key type, `Record` is equivalent to `{ [key: string]: T }`. Any key is allowed, and reading one gives `T`, even if it is not there at runtime:

```ts
const cache: Record<string, number> = {};
const n = cache["missing"]; // typed number, actually undefined
```

Mitigations:

- Turn on [`noUncheckedIndexedAccess`](../13-compiler-and-tsconfig/00-compiler-options.md) so index reads include `undefined`.
- Use `Map<string, number>` when keys are dynamic and you need real "has / get" semantics.
- Use `Partial<Record<string, number>>` to make the missing case explicit.

Also: `Record<string, unknown>` is the usual "any plain object" type, but interfaces are **not** assignable to it (interfaces lack implicit index signatures, type aliases of object literals have them). If you hit that error, use `object` or a generic constraint instead.

### Pairing Record with `satisfies`

When you want a typed table but also want the literal types of the values preserved:

```ts
const routes = {
  home: "/",
  profile: "/profile",
} satisfies Record<string, `/${string}`>;

routes.home; // type is "/", not just string
```

See [type assertions and satisfies](../03-unions-and-narrowing/07-type-assertions-and-satisfies.md).

## Practical patterns

Update input: required id, everything else optional.

```ts
type UpdateUserInput = Pick<User, "id"> & Partial<Omit<User, "id">>;
```

Mutually-derived DTOs from one source of truth:

```ts
type UserRow = User;
type UserResponse = Omit<UserRow, "passwordHash" | "createdAt"> & { createdAt: string };
```

Intersections like these show as `A & B` in hover tooltips. Wrap with an `Expand` helper if you want the flattened shape (see [building custom utility types](./04-building-custom-utility-types.md)).

## Common mistakes

- **Using `Omit` for security allow-lists.** New fields leak by default. Use `Pick`.
- **Trusting `Omit` key names.** It does not validate them.
- **Using `Omit` on a discriminated union.** It collapses the union. Use `DistributiveOmit`.
- **Using `Record<string, T>` and assuming lookups can be missing.** They are typed as present unless you opt in with `noUncheckedIndexedAccess`.
- **Using `Record<K, V>` with a wide `K` (like `string`) and expecting exhaustiveness.** Exhaustiveness only works with a finite literal union.

## Debugging

- Expand the type to see the real shape: `type Expand<T> = { [K in keyof T]: T[K] } & {}`.
- If a derived type is missing properties you expected, check whether `T` is a union (use the distributive version) or has an index signature.
- If `Pick<T, "x">` errors with "Type '"x"' does not satisfy the constraint `keyof T`", the key really is not on `T`, or `T` is a union where `x` exists only on some members.

## Quick summary

- `Pick<T, K>` keeps the listed keys (checked). `Omit<T, K>` drops them (not checked).
- Prefer `Pick` for allow-lists and anything security-sensitive. Prefer `Omit` for "everything except".
- `Omit` flattens unions. Use a distributive wrapper for discriminated unions.
- `Record<UnionOfLiterals, V>` is an exhaustive lookup table. `Record<string, V>` is just an index signature.

**Next:** [Exclude, Extract, and NonNullable](./02-exclude-extract-nonnullable.md)
