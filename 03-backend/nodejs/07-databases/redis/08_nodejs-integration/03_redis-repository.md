# Redis Repository

A **repository** stores and loads domain objects. The rest of the app asks for a `User`, and the repository decides how that becomes hashes, indexes and TTLs. This keeps Redis details out of business code and gives you one place to keep data and indexes consistent.

## When a repository makes sense

| Use it for | Don't bother for |
|------------|------------------|
| Entities you **own** in Redis (sessions, carts, feature flags, presence, drafts) | Plain cache entries (use the [service](./02_redis-service.md)) |
| Objects with secondary indexes or uniqueness rules | One-off counters |
| Data with mapping rules (dates, booleans, enums) | Data whose source of truth is another database |

If Redis is only a cache of a database entity, the **database repository** is the real repository, and Redis sits behind a cache layer.

## The interface

```ts
// src/repositories/user.repository.ts
export interface User {
  id: string;
  email: string;
  name: string;
  active: boolean;
  createdAt: Date;
  version: number;
}

export interface UserRepositoryPort {
  create(input: Omit<User, "version" | "createdAt">): Promise<User>;
  findById(id: string): Promise<User | null>;
  findByEmail(email: string): Promise<User | null>;
  findMany(ids: string[]): Promise<User[]>;
  listRecent(limit: number, offset?: number): Promise<User[]>;
  update(id: string, changes: Partial<Pick<User, "name" | "active">>, expectedVersion?: number): Promise<User | null>;
  delete(id: string): Promise<boolean>;
}

export class ConflictError extends Error {}
export class VersionConflictError extends Error {}
```

## Data layout

```
shop:user:{id}              hash    the entity (all fields as strings)
shop:idx:user:email:{email} string  email → id  (uniqueness + lookup)
shop:idx:user:created       zset    id scored by createdAt (ms), for listing and paging
```

## Mapping: explicit, not magic

Redis stores strings, so define the conversion once:

```ts
const toHash = (u: User): Record<string, string> => ({
  id: u.id,
  email: u.email,
  name: u.name,
  active: u.active ? "1" : "0",
  createdAt: String(u.createdAt.getTime()),
  version: String(u.version),
});

const fromHash = (h: Record<string, string>): User | null => {
  if (!h.id) return null;                              // hgetall returns {} for a missing key
  return {
    id: h.id,
    email: h.email,
    name: h.name,
    active: h.active === "1",
    createdAt: new Date(Number(h.createdAt)),
    version: Number(h.version),
  };
};
```

Keep these functions **pure and next to the repository**. They are the only place that knows the storage format.

## The implementation

```ts
import type { Redis } from "ioredis";
import { keys } from "../redis/keys.js";

export class UserRepository implements UserRepositoryPort {
  constructor(private redis: Redis) {}

  async create(input: Omit<User, "version" | "createdAt">): Promise<User> {
    const user: User = { ...input, createdAt: new Date(), version: 1 };
    const email = user.email.toLowerCase();

    // 1. Reserve the email atomically. NX guarantees only one creator wins.
    const reserved = await this.redis.set(keys.userEmailIndex(email), user.id, "NX");
    if (reserved !== "OK") throw new ConflictError(`email already used: ${email}`);

    // 2. Write the entity and the listing index together.
    try {
      await this.redis.multi()
        .hset(keys.user(user.id), toHash({ ...user, email }))
        .zadd(keys.userCreatedIndex(), user.createdAt.getTime(), user.id)
        .exec();
    } catch (err) {
      await this.redis.unlink(keys.userEmailIndex(email));   // release the reservation
      throw err;
    }
    return { ...user, email };
  }

  async findById(id: string) {
    return fromHash(await this.redis.hgetall(keys.user(id)));
  }

  async findByEmail(email: string) {
    const id = await this.redis.get(keys.userEmailIndex(email.toLowerCase()));
    return id ? this.findById(id) : null;
  }

  async findMany(ids: string[]) {
    if (ids.length === 0) return [];
    const p = this.redis.pipeline();
    ids.forEach((id) => p.hgetall(keys.user(id)));
    const res = (await p.exec()) ?? [];
    return res.flatMap(([err, h]) => {
      if (err) throw err;
      const u = fromHash(h as Record<string, string>);
      return u ? [u] : [];
    });
  }

  async listRecent(limit: number, offset = 0) {
    const ids = await this.redis.zrange(keys.userCreatedIndex(), offset, offset + limit - 1, "REV");
    return this.findMany(ids);
  }

  async update(id: string, changes: Partial<Pick<User, "name" | "active">>, expectedVersion?: number) {
    const fields: string[] = [];
    if (changes.name !== undefined) fields.push("name", changes.name);
    if (changes.active !== undefined) fields.push("active", changes.active ? "1" : "0");
    if (fields.length === 0) return this.findById(id);

    // Atomic "update if the version matches", implemented in Lua (see below)
    const result = await (this.redis as any).userUpdate(
      keys.user(id),
      expectedVersion ?? -1,          // -1 = don't check
      ...fields
    );
    if (result === -1) return null;                                    // not found
    if (result === -2) throw new VersionConflictError(`stale version for ${id}`);
    return this.findById(id);
  }

  async delete(id: string) {
    const user = await this.findById(id);
    if (!user) return false;

    await this.redis.multi()
      .unlink(keys.user(id))
      .unlink(keys.userEmailIndex(user.email))
      .zrem(keys.userCreatedIndex(), id)
      .exec();
    return true;
  }
}
```

Register the update script once (see [Lua Scripts](../06_advanced-commands/03_lua-scripts.md)):

```lua
-- KEYS[1] = user hash
-- ARGV[1] = expected version (-1 = skip check), ARGV[2..] = field, value, field, value ...
local exists = redis.call("EXISTS", KEYS[1])
if exists == 0 then return -1 end

local expected = tonumber(ARGV[1])
if expected >= 0 then
  local current = tonumber(redis.call("HGET", KEYS[1], "version"))
  if current ~= expected then return -2 end
end

for i = 2, #ARGV, 2 do
  redis.call("HSET", KEYS[1], ARGV[i], ARGV[i + 1])
end
return redis.call("HINCRBY", KEYS[1], "version", 1)
```

```ts
redis.defineCommand("userUpdate", { numberOfKeys: 1, lua: loadScript("user-update") });
```

What the design achieves:

| Concern | Technique |
|---------|-----------|
| Unique email under concurrency | `SET ... NX` reservation (a race between two creators has one winner) |
| Entity and index consistent | `multi()` for the writes that must go together |
| Lost updates | Version field checked in Lua |
| N+1 reads | Pipeline in `findMany` |
| Paging | Sorted-set index with `ZRANGE ... REV`, plus `LIMIT`-style offsets |
| Missing key vs empty hash | `fromHash` returns `null` when `id` is absent |

### Changing the email

Changing an indexed field means updating the index, and uniqueness must hold during the swap:

```ts
async changeEmail(id: string, newEmail: string) {
  const user = await this.findById(id);
  if (!user) return null;
  const next = newEmail.toLowerCase();

  const reserved = await this.redis.set(keys.userEmailIndex(next), id, "NX");
  if (reserved !== "OK") throw new ConflictError("email already used");

  await this.redis.multi()
    .hset(keys.user(id), "email", next)
    .unlink(keys.userEmailIndex(user.email))
    .exec();
  return this.findById(id);
}
```

## TTL and entities

If the entity should expire (sessions, drafts, carts), set the TTL **with the write** and expire its indexes with it:

```ts
await this.redis.multi()
  .hset(keys.cart(id), toHash(cart))
  .expire(keys.cart(id), 7 * 24 * 3600)
  .exec();
```

Indexes that point at expiring entities will accumulate **dangling entries** (the entity expired, the index entry stayed). Options:

- Give indexes the same TTL (or slightly longer)
- Filter dangling IDs on read (`findMany` already skips missing entities) and clean them up lazily
- Run a periodic cleanup with `ZREMRANGEBYSCORE`

## Pagination notes

- **Offset paging** (`ZRANGE REV offset limit`) is simple, but items shift when new ones are added. Fine for admin lists
- **Cursor paging** is stable: use the last item's score (`ZRANGE ... BYSCORE REV LIMIT`) with `(` for exclusive bounds

```ts
async listBefore(cursorMs: number, limit: number) {
  const ids = await this.redis.zrange(
    keys.userCreatedIndex(), `(${cursorMs}`, "-inf", "BYSCORE", "REV", "LIMIT", 0, limit
  );
  return this.findMany(ids);
}
```

## An in-memory implementation for tests

Because services depend on `UserRepositoryPort`, tests can use a simple fake:

```ts
export class InMemoryUserRepository implements UserRepositoryPort {
  private users = new Map<string, User>();

  async create(input: Omit<User, "version" | "createdAt">) {
    const email = input.email.toLowerCase();
    if ([...this.users.values()].some((u) => u.email === email)) throw new ConflictError("email");
    const user = { ...input, email, createdAt: new Date(), version: 1 };
    this.users.set(user.id, user);
    return user;
  }
  async findById(id: string) { return this.users.get(id) ?? null; }
  async findByEmail(email: string) {
    return [...this.users.values()].find((u) => u.email === email.toLowerCase()) ?? null;
  }
  async findMany(ids: string[]) { return ids.flatMap((id) => (this.users.has(id) ? [this.users.get(id)!] : [])); }
  async listRecent(limit: number, offset = 0) {
    return [...this.users.values()].sort((a, b) => +b.createdAt - +a.createdAt).slice(offset, offset + limit);
  }
  async update(id: string, changes: any, expectedVersion?: number) {
    const u = this.users.get(id);
    if (!u) return null;
    if (expectedVersion !== undefined && u.version !== expectedVersion) throw new VersionConflictError(id);
    Object.assign(u, changes, { version: u.version + 1 });
    return u;
  }
  async delete(id: string) { return this.users.delete(id); }
}
```

Write **one shared contract test suite** and run it against both implementations, so the fake can't drift from the real one:

```ts
describe.each([
  ["in-memory", () => new InMemoryUserRepository()],
  ["redis", () => new UserRepository(testRedis)],
])("UserRepository (%s)", (_name, make) => {
  it("rejects duplicate emails", async () => {
    const repo = make();
    await repo.create({ id: "1", email: "a@x.com", name: "A", active: true });
    await expect(repo.create({ id: "2", email: "A@x.com", name: "B", active: true })).rejects.toBeInstanceOf(ConflictError);
  });
});
```

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Returning raw hashes to callers | Map to domain types in one place |
| Forgetting to update indexes on writes and deletes | Do it in the repository, in `multi()` |
| Check-then-set for uniqueness | `SET NX` reservation |
| Read-modify-write for updates | Lua with a version check |
| N `hgetall` calls in a loop | Pipeline |
| Dangling index entries after TTL expiry | Matching TTLs, lazy cleanup |
| `HGETALL` treated as `null` when missing | Check for the empty object |
| Fake drifts from the real repository | Shared contract tests |

## Key takeaways

- A repository hides layout, mapping and index maintenance behind a domain-shaped interface
- Use `SET NX` for uniqueness, `multi()` for grouped writes, Lua for versioned updates
- Pipeline multi-entity reads
- Test the fake and the real implementation with the same suite

**Next:** [Typed Redis Client](./04_typed-redis-client.md)
