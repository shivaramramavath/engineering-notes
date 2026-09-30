# Lua Scripts

Redis can run **Lua scripts on the server**. A script executes **atomically**: no other command runs until it finishes. Scripts can read values, make decisions and write, all in one round trip. That makes them the right tool for compare-and-set, safe lock release, rate limiters and inventory.

## Anatomy

```lua
-- KEYS[1]: key to read/write   ARGV[1]: an argument
local current = redis.call("GET", KEYS[1])
if current == ARGV[1] then
  return redis.call("DEL", KEYS[1])
end
return 0
```

- `KEYS` is an array of key names, `ARGV` an array of other arguments. **Both hold strings**
- Lua arrays are **1-indexed**
- `redis.call(cmd, ...)` runs a Redis command and **raises an error** if it fails
- `redis.pcall(cmd, ...)` returns the error as a value instead of raising

## Running scripts with ioredis

### `eval`

```ts
const script = `
  local current = redis.call("GET", KEYS[1])
  if current == ARGV[1] then
    return redis.call("DEL", KEYS[1])
  end
  return 0
`;

const result = await redis.eval(script, 1, "shop:lock:order:5501", token);
//                                     ^ number of KEYS, then keys, then args
```

### `evalsha`

`EVAL` sends the whole script every time. `EVALSHA` sends only its SHA1 hash after the script is cached on the server:

```ts
const sha = (await redis.script("LOAD", script)) as string;
await redis.evalsha(sha, 1, "shop:lock:order:5501", token);
```

If the cache was flushed (restart, failover, `SCRIPT FLUSH`) you get `NOSCRIPT` and must reload. That's easy to forget, which is why `defineCommand` exists.

### `defineCommand` (recommended)

ioredis registers the script as a **custom method**. It uses `EVALSHA` and falls back to `EVAL` automatically on `NOSCRIPT`.

```ts
redis.defineCommand("releaseLock", {
  numberOfKeys: 1,
  lua: `
    if redis.call("GET", KEYS[1]) == ARGV[1] then
      return redis.call("DEL", KEYS[1])
    end
    return 0
  `,
});

const released = await (redis as any).releaseLock("shop:lock:order:5501", token);
```

You pass keys first (as many as `numberOfKeys`), then arguments. Each defined command also gets a `Buffer` variant (`releaseLockBuffer`).

### TypeScript typing

Augment the interface once so your custom commands are typed:

```ts
// src/redis/commands.ts
import { Redis } from "ioredis";

declare module "ioredis" {
  interface RedisCommander<Context> {
    releaseLock(key: string, token: string): Promise<number>;
    rateLimit(key: string, limit: number, windowSec: number): Promise<[number, number]>;
    reserveStock(key: string, qty: number): Promise<number>;
  }
}

export function registerCommands(redis: Redis) {
  redis.defineCommand("releaseLock", { numberOfKeys: 1, lua: RELEASE_LOCK });
  redis.defineCommand("rateLimit",   { numberOfKeys: 1, lua: RATE_LIMIT });
  redis.defineCommand("reserveStock",{ numberOfKeys: 1, lua: RESERVE_STOCK });
}
```

(The exact interface name and generics can differ between ioredis versions. Check the typings of the version you installed.)

### Keep scripts in `.lua` files

```
src/redis/scripts/
  release-lock.lua
  rate-limit.lua
  reserve-stock.lua
```

```ts
import { readFileSync } from "node:fs";
import { join } from "node:path";

const load = (name: string) =>
  readFileSync(join(__dirname, "scripts", `${name}.lua`), "utf8");

redis.defineCommand("rateLimit", { numberOfKeys: 1, lua: load("rate-limit") });
```

Files get syntax highlighting, and you can test them with `redis-cli --eval`.

## Example scripts

### 1. Safe lock release (compare-and-delete)

```lua
-- KEYS[1] = lock key, ARGV[1] = my token
if redis.call("GET", KEYS[1]) == ARGV[1] then
  return redis.call("DEL", KEYS[1])
else
  return 0
end
```

Without Lua, `GET` then `DEL` can delete **someone else's** lock if yours expired in between.

### 2. Atomic fixed-window rate limit

```lua
-- KEYS[1] = counter key, ARGV[1] = limit, ARGV[2] = window seconds
local count = redis.call("INCR", KEYS[1])
if count == 1 then
  redis.call("EXPIRE", KEYS[1], tonumber(ARGV[2]))
end
local ttl = redis.call("TTL", KEYS[1])
if count > tonumber(ARGV[1]) then
  return {0, ttl}          -- blocked, seconds until reset
end
return {1, ttl}            -- allowed
```

```ts
const [allowed, retryAfter] = await (redis as any).rateLimit(`rate:${ip}`, 100, 60);
if (!allowed) res.set("Retry-After", String(retryAfter)).status(429).end();
```

`INCR` + `EXPIRE` can never be split, so a crash can't leave a counter without a TTL.

### 3. Reserve stock only if enough is available

```lua
-- KEYS[1] = stock key, ARGV[1] = quantity
local stock = tonumber(redis.call("GET", KEYS[1]) or "0")
local qty = tonumber(ARGV[1])
if stock < qty then
  return -1                                 -- insufficient
end
return redis.call("DECRBY", KEYS[1], qty)   -- remaining stock
```

No retry loop, no dedicated connection, no oversell.

### 4. Get-or-create with a TTL

```lua
local v = redis.call("GET", KEYS[1])
if v then return v end
redis.call("SET", KEYS[1], ARGV[1], "EX", tonumber(ARGV[2]))
return ARGV[1]
```

## Type conversion

**Redis reply → Lua** (what `redis.call` gives you):

| Redis | Lua |
|-------|-----|
| Integer | number |
| Bulk string | string |
| Array | table (1-indexed) |
| Nil | `false` |
| Status (`OK`) | table `{ok="OK"}` |
| Error | table `{err="..."}` |

**Lua → Redis reply** (what your script returns to ioredis):

| Lua | JavaScript result |
|-----|------------------|
| number | integer (**decimals are truncated**) |
| string | string |
| table (array) | array (stops at the first `nil`) |
| `true` | `1` |
| `false` / `nil` | `null` |
| `{ok="..."}` | status string |
| `{err="..."}` | error |

Practical consequences:

- Redis nil comes back as `false` in Lua, so use `if v then`, not `if v ~= nil`
- To return a float, convert it: `return tostring(x)`, then parse in JS
- `ARGV` values are strings. Wrap with `tonumber()` before doing math or comparisons
- Return arrays without `nil` holes, or the array is cut off there

## Useful built-ins

```lua
cjson.decode(ARGV[1])          -- JSON string → table
cjson.encode(tbl)              -- table → JSON string
redis.sha1hex("text")
redis.log(redis.LOG_WARNING, "something")
redis.call("TIME")             -- {seconds, microseconds} (server clock)
```

Use `redis.call("TIME")` when scripts need the current time, instead of passing the client's clock, so all clients agree.

## Rules and limits

### 1. Declare every key in `KEYS`

```lua
-- Bad: builds a key name inside the script
redis.call("GET", "user:" .. ARGV[1])

-- Good: the key is passed in
redis.call("GET", KEYS[1])
```

Cluster routes the script by its declared keys. Undeclared keys can point at the wrong node, and they also break other tooling. In Cluster, all `KEYS` must share a slot (hash tags).

### 2. Keep scripts short and fast

A script **blocks the whole server** while it runs, exactly like a slow command.

- After `busy-reply-threshold` (older name `lua-time-limit`, default 5000 ms), other clients start receiving `BUSY` errors
- `SCRIPT KILL` stops a script that hasn't written anything yet. Otherwise only `SHUTDOWN NOSAVE` helps
- Avoid unbounded loops, and never run `SCAN`-style whole-keyspace work in a script

### 3. No globals

Redis disallows accidental global variables. Always use `local`.

### 4. Determinism and replication

Modern Redis (5+) replicates a script's **effects**, not the script itself, so commands like `TIME` and random functions are allowed. Still keep scripts simple and side-effect focused.

### 5. Script cache

Loaded scripts live in server memory and are lost on restart and on failover to a replica that hasn't loaded them. `defineCommand` and `NOSCRIPT` fallback handle it. If you call `evalsha` directly, catch `NOSCRIPT` and reload.

### 6. Errors

`redis.call` raising an error aborts the script. Commands that already ran **stay applied** (no rollback), so order operations so a mid-script failure is harmless, or use `pcall`.

```lua
local ok, err = pcall(function() ... end)
```

## Testing scripts

From the shell (a comma separates keys from args):

```bash
redis-cli --eval src/redis/scripts/rate-limit.lua rate:test , 5 60
```

In Node tests, run against a real Redis (Testcontainers, see `18_testing-and-debugging`). Mock libraries often don't implement Lua faithfully.

Debugging:

```lua
redis.log(redis.LOG_NOTICE, "count=" .. tostring(count))
```

Redis also ships a Lua debugger (`redis-cli --ldb --eval ...`) for step-through debugging.

## Redis Functions (Redis 7+)

Functions are the successor to `EVAL` for **managed, named, persistent** server-side code:

```
FUNCTION LOAD "#!lua name=mylib\nredis.register_function('hello', function(keys, args) return 'hi' end)"
FCALL hello 0
```

They are stored with the dataset (replicated and persisted), so there is no `NOSCRIPT`. For most Node.js apps, `defineCommand` with `EVAL` remains simpler, and it works on managed services that restrict functions. Consider Functions when you want versioned server-side libraries.

## Scripts vs the alternatives

| | Lua | MULTI/EXEC | WATCH loop |
|---|-----|-----------|-----------|
| Atomic | Yes | Yes | Yes (with retries) |
| Read-then-decide | **Yes** | No | Yes (client side) |
| Round trips | 1 | 1 | 2+ per attempt |
| Contention behavior | Serialized, no retries | n/a | Retries |
| Server CPU | Uses the main thread | Minimal | Minimal |

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Keys built inside the script | Pass every key through `KEYS` |
| Math on `ARGV` without `tonumber` | `tonumber(ARGV[i])` |
| Returning floats | Return strings via `tostring` |
| `if v ~= nil` on `redis.call("GET")` | `if v then` (nil is `false`) |
| Long-running scripts | Keep them short, batch outside |
| Forgetting `NOSCRIPT` after restart | `defineCommand` or reload logic |
| Cross-slot keys in Cluster | Hash tags |
| Assuming rollback on error | Order writes carefully |
| Duplicating script text in code | `.lua` files, one loader |

## Key takeaways

- Lua gives **atomic read-decide-write** in one round trip
- Use `defineCommand`: automatic `EVALSHA`, `NOSCRIPT` fallback, typed methods
- Pass keys via `KEYS`, args via `ARGV`, and convert types deliberately
- Keep scripts short, because they block the server

**Next:** [Atomic and Conditional Operations](./04_atomic-and-conditional-ops.md)
