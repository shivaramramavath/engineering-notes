# Builder Pattern

A builder constructs a complex object **step by step**, then produces the final result with a `build()` call. It separates "gathering the configuration" from "creating the thing", and gives you one place to validate the combination.

In JavaScript, an options object solves most of what builders solve elsewhere, so this pattern is worth reaching for only in specific situations.

**Prerequisites:** [Classes](../05_this-and-oop/05_classes.md), [Private Fields](../05_this-and-oop/07_private-fields.md), [Object Static Methods](../03_objects-and-arrays/04_object-static-methods.md)

---

## Start With the Alternative

```js
createRequest({
  url: "/api/users",
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: { name: "Asha" },
});
```

This is readable, order-independent, and has defaults. If this works for you, stop here.

A builder starts to pay off when:

- Steps are **conditional or incremental** (parts of the config come from different places, such as middleware or UI filters).
- You want a **fluent, discoverable API** (`.where().orderBy().limit()`).
- Validation depends on **combinations** that you want checked once at `build()`.
- You want to **reuse a partially built object** as a base for variants.

---

## A Fluent, Immutable Builder

Each method returns a **new** builder instead of mutating the current one. That makes partial builders safe to reuse.

```js
class RequestBuilder {
  #cfg;

  constructor(url, cfg = { method: "GET", headers: {}, body: undefined }) {
    this.url = url;
    this.#cfg = cfg;
  }

  #next(patch) {
    return new RequestBuilder(this.url, { ...this.#cfg, ...patch });
  }

  method(method) {
    return this.#next({ method });
  }

  header(name, value) {
    return this.#next({ headers: { ...this.#cfg.headers, [name]: value } });
  }

  json(data) {
    return this.header("Content-Type", "application/json").#next({
      body: JSON.stringify(data),
    });
  }

  build() {
    const { method, headers, body } = this.#cfg;
    if (body !== undefined && (method === "GET" || method === "HEAD")) {
      throw new Error(`${method} requests cannot have a body`);
    }
    return new Request(this.url, { method, headers, body });
  }
}
```

```js
const api = new RequestBuilder("https://example.com/api/users").header("Authorization", "Bearer abc");

const getReq = api.build();
const postReq = api.method("POST").json({ name: "Asha" }).build();
```

Notes on the code:

- `api` is unchanged after `api.method("POST")`. That is what makes it reusable as a base.
- `build()` is the only place that checks the combination of `method` and `body`. Nobody can get a half-valid request by forgetting a step order.
- `Request` is available in browsers and in Node 18+.
- `this.header(...).#next(...)` works because `#next` is a private method and the returned object is another `RequestBuilder`. Private names can be used on any instance of the same class.

---

## Mutable Builder (Simpler, Riskier)

```js
class QueryBuilder {
  #table;
  #conditions = [];
  #params = [];
  #limit;

  from(table) { this.#table = table; return this; }
  where(clause, ...params) {
    this.#conditions.push(clause);
    this.#params.push(...params);
    return this;
  }
  limit(n) { this.#limit = n; return this; }

  build() {
    if (!this.#table) throw new Error("from() is required");
    const where = this.#conditions.length ? ` WHERE ${this.#conditions.join(" AND ")}` : "";
    const limit = this.#limit != null ? ` LIMIT ${Number(this.#limit)}` : "";
    return { text: `SELECT * FROM ${this.#table}${where}${limit}`, params: this.#params };
  }
}

new QueryBuilder().from("users").where("age > ?", 18).limit(10).build();
// { text: "SELECT * FROM users WHERE age > ? LIMIT 10", params: [18] }
```

Two details that matter in real code:

- Values go into `params`, never into the SQL string. See [Input Validation](../22_security/04_input-validation.md).
- The table name and the `where` clauses are still interpolated into the string. This example is a **pattern illustration**. In real projects, use a vetted query builder or ORM and don't pass untrusted input as table names or clauses.

The mutable version is shorter, but reusing one instance for two queries mixes their state.

---

## Common Mistakes

- **Forgetting `return this`** in a mutable builder. The next call fails with `Cannot read properties of undefined`.
- **Reusing a mutable builder** for a second object. Create a new one or use the immutable form.
- **Skipping validation in `build()`** so invalid objects escape.
- **Building a builder for a three-field object.** Use an options object.
- **Required fields only checked at use time.** Fail in `build()`, with a message naming the missing step.

---

## Quick Summary

- Try an options object first. In JS it covers most cases.
- Use a builder for incremental construction, fluent APIs, and one-place validation.
- Immutable builders (return a new builder) are safe to share and branch.
- `build()` is the validation gate.

**Next:** [Strategy Pattern](./05_strategy-pattern.md)