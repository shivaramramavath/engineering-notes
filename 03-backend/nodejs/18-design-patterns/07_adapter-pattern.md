# Adapter Pattern

An **adapter** converts one interface into another that your code expects. It wraps something with an incompatible shape (a third-party library, a legacy API, a different data format) so the rest of your application can use it through **one consistent interface**.

Think of a power plug adapter: the device and the socket stay unchanged; the adapter sits between them.

See also: [Dependency Injection](./09_dependency-injection.md), [Decorator Pattern](./08_decorator-pattern.md), [Fetch](../15_networking/02_fetch.md), [Proxy and Reflect](../09_built-in-objects/11_proxy-and-reflect.md).

## The problem: incompatible interfaces

Your app expects a logger with `info(msg)` and `error(msg)`, but the library you adopted looks different:

```js
// What your code uses
logger.info('Server started');

// What the library offers
fancyLib.write({ level: 'INFO', text: 'Server started' });
```

Rewriting every call site couples your app to the library. Switching libraries later means touching hundreds of files.

## A simple adapter

```js
// The interface your application defines
// logger = { info(msg), error(msg) }

function createFancyLoggerAdapter(fancyLib) {
  return {
    info: (msg) => fancyLib.write({ level: 'INFO', text: msg }),
    error: (msg) => fancyLib.write({ level: 'ERROR', text: msg }),
  };
}

const logger = createFancyLoggerAdapter(fancyLib);
logger.info('Server started');
```

Now the application depends only on the interface it defined. Replacing `fancyLib` means writing one new adapter, not editing the whole codebase.

## Class-based adapter

```js
class LegacyPaymentApi {
  makePayment(cents, cardNumber, expiry) { /* ... returns 'OK' or 'FAIL' */ }
}

// Interface the app wants:
//   gateway.charge({ amount, card }) → Promise<{ success: boolean, id?: string }>

class LegacyPaymentAdapter {
  #legacy;

  constructor(legacy) { this.#legacy = legacy; }

  async charge({ amount, card }) {
    const cents = Math.round(amount * 100);                      // dollars → cents
    const status = this.#legacy.makePayment(cents, card.number, `${card.month}/${card.year}`);
    return { success: status === 'OK', id: status === 'OK' ? crypto.randomUUID() : undefined };
  }
}

const gateway = new LegacyPaymentAdapter(new LegacyPaymentApi());
await gateway.charge({ amount: 19.99, card: { number: '4242...', month: 12, year: 2028 } });
```

The adapter translates **names** (`makePayment` → `charge`), **units** (dollars to cents), **parameter shapes** (positional to object), **return values** (string to object), and **sync vs async** (it returns a promise).

## What adapters typically translate

| Difference | Example |
|-----------|---------|
| Method and property names | `getData()` ↔ `fetchData()` |
| Parameter order or shape | `(a, b, c)` ↔ `({ a, b, c })` |
| Units and formats | seconds ↔ milliseconds, dollars ↔ cents, ISO strings ↔ `Date` |
| Naming conventions | `snake_case` ↔ `camelCase` |
| Sync vs async | callbacks ↔ promises |
| Error handling | error codes ↔ exceptions ↔ result objects |
| Data structures | arrays ↔ maps, nested ↔ flat |
| Protocols | REST ↔ GraphQL, event-based ↔ streams |

## Data adapters (shape conversion)

The most common adapter in front-end and API code converts external data to your internal model:

```js
// API response (external, snake_case, strings)
const apiUser = {
  user_id: '42',
  first_name: 'Ada',
  last_name: 'Lovelace',
  created_at: '2026-03-01T10:00:00Z',
  is_active: 1,
};

// Internal model (what the app uses)
function toUser(api) {
  return {
    id: Number(api.user_id),
    fullName: `${api.first_name} ${api.last_name}`,
    createdAt: new Date(api.created_at),
    active: Boolean(api.is_active),
  };
}

// And back, when sending data to the API
function toApiUser(user) {
  const [first_name, ...rest] = user.fullName.split(' ');
  return {
    user_id: String(user.id),
    first_name,
    last_name: rest.join(' '),
    is_active: user.active ? 1 : 0,
  };
}
```

Keep the adapter at the **boundary** (where data enters or leaves). The rest of the app only ever sees the internal model, so an API change affects one file.

### Generic key conversion

```js
const camel = (s) => s.replace(/_([a-z])/g, (_, c) => c.toUpperCase());

function camelizeKeys(value) {
  if (Array.isArray(value)) return value.map(camelizeKeys);
  if (value && typeof value === 'object' && value.constructor === Object) {
    return Object.fromEntries(Object.entries(value).map(([k, v]) => [camel(k), camelizeKeys(v)]));
  }
  return value;
}

camelizeKeys({ user_id: 1, home_address: { zip_code: '560001' } });
// { userId: 1, homeAddress: { zipCode: '560001' } }
```

## Multiple providers behind one interface

Adapters let you switch or combine vendors:

```js
// App-defined interface: storage.save(key, data), storage.load(key)

const localDiskStorage = {
  save: (key, data) => fs.writeFile(`./data/${key}`, data),
  load: (key) => fs.readFile(`./data/${key}`),
};

function createS3Adapter(s3, bucket) {
  return {
    save: (key, data) => s3.putObject({ Bucket: bucket, Key: key, Body: data }),
    load: async (key) => (await s3.getObject({ Bucket: bucket, Key: key })).Body,
  };
}

function createMemoryAdapter() {
  const map = new Map();
  return {
    save: async (key, data) => { map.set(key, data); },
    load: async (key) => map.get(key),
  };
}

// The app uses `storage`, whichever one it is
const storage = process.env.NODE_ENV === 'test'
  ? createMemoryAdapter()
  : createS3Adapter(s3Client, 'my-bucket');
```

This is the basis of **ports and adapters** (hexagonal architecture): the core application defines the ports (interfaces it needs), and adapters connect them to databases, queues, HTTP, or files. It also makes tests fast, because an in-memory adapter replaces the slow real service.

## Callback-to-promise adapters

A classic adapter in JavaScript:

```js
import { promisify } from 'node:util';

function legacyRead(path, callback) { /* callback(err, data) */ }

const readAsync = promisify(legacyRead);
const data = await readAsync('/tmp/file');

// Manual version
function readPromise(path) {
  return new Promise((resolve, reject) => {
    legacyRead(path, (err, data) => (err ? reject(err) : resolve(data)));
  });
}
```

Similarly, an adapter can turn an event emitter into an async iterator (`events.on`), or a `ReadableStream` into a Node stream (`Readable.fromWeb`).

## Adapting to a standard interface

Wrapping different sources so they all look like the same standard shape:

```js
// Make any array-like or paged source iterable
function pagedSourceToIterable(fetchPage) {
  return {
    async *[Symbol.asyncIterator]() {
      let cursor;
      do {
        const { items, next } = await fetchPage(cursor);
        yield* items;
        cursor = next;
      } while (cursor);
    },
  };
}

for await (const user of pagedSourceToIterable((c) => api.listUsers({ cursor: c }))) {
  console.log(user.name);
}
```

Now code written against `for await` works with any paged API.

## Adapter via `Proxy`

A `Proxy` can adapt an interface dynamically:

```js
function withAliases(target, aliases) {
  return new Proxy(target, {
    get(obj, prop, receiver) {
      const real = Object.hasOwn(aliases, prop) ? aliases[prop] : prop;
      return Reflect.get(obj, real, receiver);
    },
  });
}

const api = withAliases(legacyApi, { getUser: 'fetchUserById', saveUser: 'persistUser' });
api.getUser(1);     // calls legacyApi.fetchUserById(1)
```

Prefer explicit wrapper objects for clarity. A proxy is better for renaming many methods or for runtime-generated APIs.

## Adapter vs related patterns

| Pattern | Purpose | Interface change? |
|---------|---------|-------------------|
| **Adapter** | Make an existing interface fit the one you need | **Yes**: converts to a different interface |
| **Decorator** | Add behavior to an object | No: same interface |
| **Facade** | Provide a simpler interface over a complex subsystem | Yes, but to simplify rather than to match an existing contract |
| **Proxy** | Control access to an object (lazy, cache, auth) | No: same interface |
| **Bridge** | Separate abstraction from implementation up front | Designed in advance rather than retrofitted |

Rule of thumb: an adapter exists because **two interfaces already exist and do not match**.

## Where adapters appear in real code

| Example | Adapts |
|---------|--------|
| `util.promisify` | Callback APIs to promises |
| `Readable.fromWeb` / `Readable.toWeb` | Node streams to web streams |
| ORM drivers and database adapters (Knex, Prisma, TypeORM dialects) | One query API to many databases |
| Storage adapters (local, S3, GCS) | One file API to many backends |
| Redux/Zustand/Query data selectors, API response mappers | Server shape to UI model |
| Polyfills | New standard API to old environments |
| Passport.js, NextAuth providers | Many identity providers to one session model |
| Logging transports (Winston, Pino) | One logger interface to many sinks |
| Test doubles / in-memory fakes | Real service to fast local implementation |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Leaking the third-party shape past the adapter | Coupling returns; adapter is pointless | Convert at the boundary; the app sees only its own types |
| An adapter that does too much (business logic, caching, retries) | Hard to understand and test | Keep it a thin translator; use decorators for extra behavior |
| Lossy conversions (dropping fields, rounding) without noting it | Silent data bugs | Document and test round trips |
| Adapting by monkey-patching the third-party object | Global side effects; surprises | Wrap, do not modify |
| No tests for the mapping | Mapping bugs go unnoticed | Test with realistic sample payloads |
| Hard-coding the adapter's choice in many places | Hard to swap | Select once at startup (composition root) |
| Adapting something you control | Needless layer | Change the code to match instead |
| Different error semantics per adapter | Callers need provider-specific handling | Normalize errors into your own error types |
| Forgetting units and time zones | Subtle wrong values | Convert explicitly and name variables with units |

## Key takeaways

- An adapter makes an existing, incompatible interface fit the interface your code expects
- Define the interface your **application** needs first, then write adapters from each external thing to it
- Use data adapters at boundaries to convert API shapes into your internal models (and back)
- Adapters translate names, parameters, units, return values, errors, and sync/async style
- Multiple adapters behind one interface let you swap vendors and use fast in-memory fakes in tests
- Keep adapters thin; add behavior with decorators and simplification with facades
- Never let the external shape leak past the adapter

**Next:** [Decorator Pattern](./08_decorator-pattern.md)
