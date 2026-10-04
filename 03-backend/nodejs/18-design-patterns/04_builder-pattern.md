# Builder Pattern

A **builder** constructs a complex object **step by step**, then produces the final result. It shines when an object has many optional settings, needs validation across several fields, or must be assembled in stages.

See also: [Factory Pattern](./02_factory-pattern.md), [Destructuring](../03_objects-and-arrays/09_destructuring.md), [Classes](../05_this-and-oop/05_classes.md), [Private Fields](../05_this-and-oop/07_private-fields.md).

## The problem: telescoping parameters

```js
new HttpRequest('POST', 'https://api.example.com/users', { 'Content-Type': 'application/json' },
  JSON.stringify(body), 5000, 3, true, undefined, 'include');
```

What does `true` mean? Which argument is `3`? Positional parameters become unreadable once there are more than two or three, especially when most are optional.

## First, try an options object

In JavaScript, **an options object with defaults** solves most of this without any pattern:

```js
function request({ method = 'GET', url, headers = {}, body, timeout = 5000, retries = 0 }) {
  if (!url) throw new Error('url is required');
  return { method, url, headers, body, timeout, retries };
}

request({
  method: 'POST',
  url: 'https://api.example.com/users',
  body: JSON.stringify({ name: 'Ada' }),
  retries: 3,
});
```

Named properties are self-documenting, order does not matter, and omitted values take defaults. Reach for a builder only when this stops being enough (see "When to use it" below).

## A fluent builder

Each method sets one piece of the configuration and **returns the builder**, so calls chain:

```js
class RequestBuilder {
  #method = 'GET';
  #url = null;
  #headers = {};
  #body;
  #timeout = 5000;
  #retries = 0;

  method(m) { this.#method = m; return this; }
  url(u) { this.#url = u; return this; }
  header(name, value) { this.#headers[name] = value; return this; }
  json(data) {
    this.#body = JSON.stringify(data);
    return this.header('Content-Type', 'application/json');
  }
  timeout(ms) { this.#timeout = ms; return this; }
  retries(n) { this.#retries = n; return this; }

  build() {
    if (!this.#url) throw new Error('url is required');
    if (this.#body && this.#method === 'GET') {
      throw new Error('GET requests cannot have a body');
    }
    return Object.freeze({
      method: this.#method,
      url: this.#url,
      headers: { ...this.#headers },
      body: this.#body,
      timeout: this.#timeout,
      retries: this.#retries,
    });
  }
}

const req = new RequestBuilder()
  .method('POST')
  .url('https://api.example.com/users')
  .json({ name: 'Ada' })
  .retries(3)
  .build();
```

Key points:

- **Private fields** hold the work in progress
- Each setter **returns `this`** (a "fluent interface")
- `build()` **validates** the whole configuration and returns a finished, ideally immutable, object
- Cross-field rules (a body requires a non-GET method) live in one place

## Builder with a static entry point

```js
class QueryBuilder {
  #table;
  #conditions = [];
  #params = [];
  #orderBy = null;
  #limit = null;

  static from(table) { return new QueryBuilder(table); }

  constructor(table) { this.#table = table; }

  where(condition, ...params) {
    this.#conditions.push(condition);
    this.#params.push(...params);
    return this;
  }

  orderBy(column, direction = 'ASC') {
    if (!['ASC', 'DESC'].includes(direction)) throw new Error('Invalid direction');
    this.#orderBy = `${column} ${direction}`;
    return this;
  }

  limit(n) { this.#limit = n; return this; }

  build() {
    let sql = `SELECT * FROM ${this.#table}`;
    if (this.#conditions.length) sql += ` WHERE ${this.#conditions.join(' AND ')}`;
    if (this.#orderBy) sql += ` ORDER BY ${this.#orderBy}`;
    if (this.#limit != null) sql += ` LIMIT ${Number(this.#limit)}`;
    return { sql, params: this.#params };
  }
}

QueryBuilder.from('users')
  .where('age > ?', 18)
  .where('country = ?', 'IN')
  .orderBy('name')
  .limit(10)
  .build();
// { sql: 'SELECT * FROM users WHERE age > ? AND country = ? ORDER BY name ASC LIMIT 10',
//   params: [18, 'IN'] }
```

Values go in `params` (never concatenated into the SQL string), which avoids injection for the values. Table and column names still need validation or an allow-list, because they cannot be parameterized. See [Security](../22_security/00_README.md).

## Immutable builders

Mutable builders can surprise you when reused:

```js
const base = new RequestBuilder().url('https://api.example.com').retries(3);
const a = base.method('POST').build();
const b = base.build();                  // method is still POST: shared mutation
```

Return a **new** builder from each step instead:

```js
const makeBuilder = (state = {}) => ({
  method: (method) => makeBuilder({ ...state, method }),
  url: (url) => makeBuilder({ ...state, url }),
  retries: (retries) => makeBuilder({ ...state, retries }),
  build() {
    if (!state.url) throw new Error('url is required');
    return Object.freeze({ method: 'GET', retries: 0, ...state });
  },
});

const base = makeBuilder().url('https://api.example.com').retries(3);
const post = base.method('POST').build();
const get = base.build();                // unaffected: still GET
```

Immutable builders are safe to share and branch, at the cost of a small allocation per step.

## Step builders: enforcing order

Sometimes steps must happen in a certain order or some fields are mandatory. Return different objects exposing only the valid next methods:

```js
function emailBuilder() {
  return {
    to(address) {
      return {
        subject(text) {
          return {
            body(content) {
              return { send: () => ({ to: address, subject: text, body: content }) };
            },
          };
        },
      };
    },
  };
}

emailBuilder().to('ada@example.com').subject('Hi').body('Hello!').send();
// emailBuilder().subject('Hi')      → TypeError: subject is not a function
```

This is most valuable in TypeScript, where the types make invalid sequences a compile error. In plain JavaScript a runtime check in `build()` is usually enough.

## Builder with a director

A **director** packages common recipes on top of a builder:

```js
class ReportDirector {
  static weekly(builder) {
    return builder.title('Weekly Report').period('7d').format('pdf').build();
  }
  static dashboard(builder) {
    return builder.title('Live Dashboard').period('1h').format('html').refresh(5).build();
  }
}
```

Useful when the same sequences repeat. In JavaScript, a plain function that takes a builder or returns preset options often serves the same purpose.

## Async or staged construction

Builders can gather configuration synchronously and do async work in `build()`:

```js
class ServerBuilder {
  #port = 3000;
  #routes = [];
  #middleware = [];

  port(p) { this.#port = p; return this; }
  route(path, handler) { this.#routes.push({ path, handler }); return this; }
  use(fn) { this.#middleware.push(fn); return this; }

  async start() {
    const server = createServer(this.#middleware, this.#routes);
    await new Promise((resolve) => server.listen(this.#port, resolve));
    return server;
  }
}

const server = await new ServerBuilder()
  .port(8080)
  .use(logger)
  .route('/health', () => 'ok')
  .start();
```

## Where builders appear in real code

| Example | What is built |
|---------|---------------|
| Query builders (Knex, Kysely, Drizzle) | SQL queries |
| `new URL()` with `searchParams.set(...)` | URLs step by step |
| `FormData`, `URLSearchParams` (`append`) | Request bodies |
| `fetch` with `Request` / `Headers` | Requests |
| Test data builders | Valid fixtures with overrides |
| `Array.prototype` chains (`.filter().map()`) | Not a builder, but the same fluent style |
| Express/Fastify app setup (`app.use().get()`) | Server configuration |
| Schema libraries (Zod: `z.string().min(3).email()`) | Immutable fluent builders for schemas |
| UI/CLI libraries (commander: `.option().command()`) | CLI definitions |

## When to use it

| Situation | Choice |
|-----------|--------|
| Up to about 5 parameters, mostly optional | Options object |
| Many optional settings with cross-field validation | Builder |
| Construction in stages, possibly over time or async | Builder |
| Want a readable, chainable DSL (queries, schemas, tests) | Fluent builder |
| Many representations from the same steps | Builder + director |
| One required field and a few defaults | Plain function or constructor |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| A builder where an options object would do | Extra class, extra code | Options object with defaults |
| No validation in `build()` | Invalid objects escape | Validate required fields and cross-field rules |
| Reusing a mutable builder | Leaked state between builds | Immutable builder, or one builder per object |
| Forgetting to return `this` | Chain breaks with `undefined` errors | Return `this` (or a new builder) from every step |
| Returning the builder's internal arrays or objects | Later builder changes mutate the product | Copy when building; freeze the result |
| Forgetting to call `build()` | A builder is passed where the product is expected | Name it clearly; validate types at the consumer |
| Builders for trivial objects | Boilerplate | Object literal |
| Building SQL or HTML by string concatenation of user input | Injection | Parameters and escaping |

## Key takeaways

- A builder assembles a complex object in steps and validates it once in `build()`
- In JavaScript, start with an **options object with defaults**: it removes most of the need
- Use a fluent builder for many options, cross-field rules, or a chainable DSL
- Keep work-in-progress private; return a frozen, finished object from `build()`
- Make builders immutable if they will be shared or branched
- Treat async setup as part of `build()` or `start()`, not the constructor
- Do not build what an object literal can express

**Next:** [Strategy Pattern](./05_strategy-pattern.md)
