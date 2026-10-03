# Interfaces and Generics

Backend code is mostly about moving **shapes of data** around: a user from the database, a request body, an API response. Interfaces and type aliases describe those shapes once; generics let you write reusable code (a repository, an API wrapper) without losing type information.

## Interfaces and type aliases

```ts
interface User {
  id: string;
  name: string;
  email: string;
  role: "admin" | "user";
  createdAt: Date;
  avatarUrl?: string;   // optional
  readonly version: number; // can't be reassigned
}

const u: User = {
  id: "1",
  name: "Asha",
  email: "asha@example.com",
  role: "user",
  createdAt: new Date(),
  version: 1,
};
```

The same thing with a type alias:

```ts
type User = {
  id: string;
  name: string;
  // ...
};
```

### `interface` vs `type`

For object shapes they're nearly interchangeable. A practical rule:

| Use | When |
|-----|------|
| `interface` | Describing object shapes, especially ones that get extended or implemented by classes |
| `type` | Unions, intersections, tuples, mapped/conditional types, aliasing primitives |

```ts
type Id = string | number;                 // only `type` can do this
type Result = Success | Failure;           // unions → `type`
```

Pick one convention per project and stay consistent — the difference rarely matters more than that.

---

## Extending types

```ts
interface Timestamps {
  createdAt: Date;
  updatedAt: Date;
}

interface Post extends Timestamps {
  id: string;
  title: string;
}

// Equivalent with intersection types:
type Post2 = Timestamps & { id: string; title: string };
```

---

## Typing functions

```ts
type Handler = (input: string) => Promise<number>;

async function findUser(id: string): Promise<User | null> {
  return User.findById(id);
}
```

`async` functions always return a `Promise<T>` — annotate the **inner** type (`Promise<User | null>`), and TypeScript checks every `return` against it.

---

## Generics: reusable without losing types

Without generics, a reusable function either duplicates code per type or falls back to `any`:

```ts
function first(items: any[]): any {
  return items[0];
}
const x = first([1, 2, 3]); // x is `any` — type information lost
```

With a generic type parameter `T`:

```ts
function first<T>(items: T[]): T | undefined {
  return items[0];
}

const n = first([1, 2, 3]);       // number | undefined
const s = first(["a", "b"]);      // string | undefined
```

`T` is a placeholder: "whatever type goes in is the type that comes out." TypeScript infers it from the arguments, so you rarely write `first<number>(...)` by hand.

### Generic interfaces — the API response wrapper

```ts
interface ApiResponse<T> {
  success: boolean;
  data: T;
  message?: string;
}

interface Paginated<T> {
  items: T[];
  page: number;
  total: number;
}

function ok<T>(data: T): ApiResponse<T> {
  return { success: true, data };
}

const res = ok<Paginated<User>>({ items: [], page: 1, total: 0 });
res.data.items[0].email; // fully typed
```

This ties directly into the consistent response shapes from `09-api-development/05-error-responses.md` and pagination in `09-api-development/02-versioning-and-pagination.md`.

### Generic constraints

Sometimes `T` can't be *anything* — it must have certain properties:

```ts
function getId<T extends { id: string }>(entity: T): string {
  return entity.id;
}

getId({ id: "1", name: "Asha" }); // ✅
getId({ name: "Asha" });          // ❌ Error: property 'id' is missing
```

### A generic repository

This is where generics earn their keep in backend code (see `10-architecture/03-repository-and-service-pattern.md`):

```ts
interface Repository<T extends { id: string }> {
  findById(id: string): Promise<T | null>;
  findAll(): Promise<T[]>;
  create(data: Omit<T, "id">): Promise<T>;
  delete(id: string): Promise<void>;
}

class UserRepository implements Repository<User> {
  async findById(id: string) { /* ... */ return null; }
  async findAll() { return []; }
  async create(data: Omit<User, "id">) { /* ... */ return {} as User; }
  async delete(id: string) { /* ... */ }
}
```

One interface, any entity — and each implementation is checked against it.

---

## Utility types

TypeScript ships helpers that derive new types from existing ones, so you don't maintain near-duplicate interfaces by hand:

```ts
// Partial<T>  — all properties optional (good for PATCH bodies)
type UpdateUserDto = Partial<User>;

// Pick<T, K>  — keep only some properties
type PublicUser = Pick<User, "id" | "name" | "avatarUrl">;

// Omit<T, K>  — drop some properties
type CreateUserDto = Omit<User, "id" | "createdAt" | "version">;

// Required<T> — all properties required
type FullUser = Required<User>;

// Readonly<T> — all properties readonly
type FrozenUser = Readonly<User>;

// Record<K, V> — object with keys K and values V
const rolePermissions: Record<User["role"], string[]> = {
  admin: ["read", "write", "delete"],
  user: ["read"],
};

// ReturnType<F> / Awaited<T> — derive from functions and promises
type Created = Awaited<ReturnType<typeof createUser>>;
```

`Omit` and `Pick` are especially useful for **never leaking sensitive fields**:

```ts
type SafeUser = Omit<UserWithPassword, "passwordHash">;
```

---

## Discriminated unions

A union where each member has a shared literal field (the "discriminant") lets TypeScript narrow precisely:

```ts
type Result<T> =
  | { ok: true; value: T }
  | { ok: false; error: string };

function handle(r: Result<User>) {
  if (r.ok) {
    console.log(r.value.name); // r is the success branch
  } else {
    console.log(r.error);      // r is the failure branch
  }
}
```

Handy for service-layer return values and for modeling job/event payloads (`11-async-processing/`, `09-api-development/07-webhooks.md`).

### Exhaustiveness checking with `never`

```ts
type Event =
  | { type: "created"; id: string }
  | { type: "deleted"; id: string };

function process(e: Event) {
  switch (e.type) {
    case "created": return /* ... */;
    case "deleted": return /* ... */;
    default: {
      const _exhaustive: never = e; // ❌ errors if a new Event type is added but not handled
      return _exhaustive;
    }
  }
}
```

Add a new member to `Event` and the compiler points at every `switch` that forgot it.

---

## `satisfies` — check a shape without widening

```ts
const config = {
  port: 3000,
  env: "production",
} satisfies { port: number; env: string };

config.env; // still the literal type "production", not widened to string
```

---

## Where type definitions live

Common conventions:

```
src/
├── types/
│   ├── user.ts        # shared interfaces
│   └── express.d.ts   # declaration files (see 03-express-types.md)
├── models/
└── services/
```

Keep types **close to where they're used** when they're local to one module; promote to `types/` only when shared.

---

## Common mistakes

- **Treating a TypeScript interface as validation** — `req.body as CreateUserDto` doesn't check anything. Validate with Zod/Joi at the boundary (`06-express/05-validation.md`); you can then *derive* the type from the schema.
- **Duplicating near-identical interfaces** — use `Pick`/`Omit`/`Partial` instead so changes stay in one place.
- **Over-using generics** — if a function only ever handles one type, a generic adds noise. Reach for them when you genuinely have reuse.
- **Returning `any` from "temporary" helpers** — it leaks into every caller.
- **Forgetting that `Partial<T>` is shallow** — nested objects are not made optional.

## Quick summary

- Describe data shapes with `interface` (objects) and `type` (unions, utilities, aliases)
- Generics (`<T>`) keep reusable code type-safe: "what goes in is what comes out"
- Constrain generics with `extends` when `T` must have certain properties
- `Partial`, `Pick`, `Omit`, `Record`, `Awaited` derive new types instead of duplicating them
- Discriminated unions + `never` give precise narrowing and exhaustiveness checks

## Next

**`03-express-types.md`** applies all of this to Express — typing `req.params`, `req.body`, responses, middleware, and adding custom properties like `req.user`.
