# Factory Pattern

A **factory** is a function or method whose job is to **create objects**. Callers ask for what they need; the factory decides *how* to build it and *which* concrete thing to return. This hides construction details and lets you change them in one place.

See also: [Constructor Functions](../05_this-and-oop/04_constructor-functions.md), [Classes](../05_this-and-oop/05_classes.md), [Mixins and Composition](../05_this-and-oop/08_mixins-and-composition.md).

## The problem

Creating objects directly spreads construction knowledge across the codebase:

```js
const user1 = { id: crypto.randomUUID(), name: 'Ada', role: 'user', createdAt: new Date() };
const user2 = { id: crypto.randomUUID(), name: 'Bob', role: 'user', createdAt: new Date() };
// every caller repeats defaults; changing the shape means editing every call site
```

And with `new`, the caller must know the exact class:

```js
const logger = process.env.NODE_ENV === 'production'
  ? new CloudLogger(apiKey)
  : new ConsoleLogger();
// this decision is now copied wherever a logger is needed
```

## Simple factory function

```js
function createUser({ name, role = 'user' }) {
  if (!name) throw new Error('name is required');
  return {
    id: crypto.randomUUID(),
    name,
    role,
    createdAt: new Date(),
  };
}

const ada = createUser({ name: 'Ada' });
const root = createUser({ name: 'Root', role: 'admin' });
```

Benefits: defaults and validation live in one place; callers do not use `new`; you can change what is returned without touching callers.

### Factories with private state

Closures give each created object its own private data (see [Module Pattern](./01_module-pattern.md)):

```js
function createAccount(owner, initial = 0) {
  let balance = initial;                // private

  return {
    owner,
    deposit(n) { balance += n; return balance; },
    withdraw(n) {
      if (n > balance) throw new Error('Insufficient funds');
      balance -= n;
      return balance;
    },
    get balance() { return balance; },
  };
}
```

## Factory vs constructor

| | Constructor (`new Class()`) | Factory function |
|---|------------------------------|------------------|
| Call style | Requires `new` | Plain call |
| Return value | Always a new instance of that class | Any object, a cached one, or a subclass |
| Naming | Class name | `createX`, `makeX`, `xFrom...` |
| Can fail early with a different result | Hard (must throw) | Easy (return `null`, cached instance, an error object) |
| `instanceof` | Works | Works only if it returns class instances |
| `this` pitfalls | Possible (forgetting `new`) | None when using closures |

You can combine them: a class with a static factory method.

```js
class Temperature {
  #celsius;

  constructor(celsius) {
    this.#celsius = celsius;
  }

  static fromCelsius(c) { return new Temperature(c); }
  static fromFahrenheit(f) { return new Temperature((f - 32) * 5 / 9); }
  static fromKelvin(k) { return new Temperature(k - 273.15); }

  get celsius() { return this.#celsius; }
  get fahrenheit() { return this.#celsius * 9 / 5 + 32; }
}

Temperature.fromFahrenheit(212).celsius;   // 100
```

Named static factories document intent better than one constructor with confusing parameters. The built-ins do this too: `Array.from`, `Array.of`, `Promise.resolve`, `Object.create`, `Buffer.from`.

## Choosing a type at runtime

A factory can pick which implementation to build from input:

```js
class Circle {
  constructor({ radius }) { this.radius = radius; }
  area() { return Math.PI * this.radius ** 2; }
}

class Rectangle {
  constructor({ width, height }) { this.width = width; this.height = height; }
  area() { return this.width * this.height; }
}

class Triangle {
  constructor({ base, height }) { this.base = base; this.height = height; }
  area() { return (this.base * this.height) / 2; }
}

const registry = { circle: Circle, rectangle: Rectangle, triangle: Triangle };

function createShape(type, options) {
  const Shape = registry[type];
  if (!Shape) throw new Error(`Unknown shape: ${type}`);
  return new Shape(options);
}

createShape('circle', { radius: 2 }).area();
createShape('rectangle', { width: 3, height: 4 }).area();
```

A **lookup table** (object or `Map`) replaces a long `switch`. Adding a new shape means adding one entry, not editing the factory.

### Open registry

Let other code register new types without modifying the factory:

```js
const shapes = new Map();

export function registerShape(type, factory) {
  shapes.set(type, factory);
}

export function createShape(type, options) {
  const make = shapes.get(type);
  if (!make) throw new Error(`Unknown shape: ${type}`);
  return make(options);
}

registerShape('square', ({ side }) => ({ side, area: () => side ** 2 }));
```

This is the heart of plugin systems.

## Factory for environment-specific objects

```js
function createLogger({ env = process.env.NODE_ENV } = {}) {
  if (env === 'test') return { info() {}, error() {} };           // silent
  if (env === 'production') return createJsonLogger();
  return createPrettyLogger();
}

const logger = createLogger();
```

Callers use `logger.info(...)` and never know which implementation they got. This is also the foundation of [dependency injection](./09_dependency-injection.md).

## Abstract factory

An **abstract factory** creates **families** of related objects that must work together. A function returns an object of factory functions:

```js
const lightTheme = {
  createButton: (label) => ({ label, bg: '#fff', fg: '#000' }),
  createInput: (placeholder) => ({ placeholder, bg: '#f5f5f5', border: '#ccc' }),
};

const darkTheme = {
  createButton: (label) => ({ label, bg: '#222', fg: '#fff' }),
  createInput: (placeholder) => ({ placeholder, bg: '#111', border: '#555' }),
};

function renderForm(theme) {
  return [theme.createInput('Email'), theme.createButton('Submit')];
}

renderForm(darkTheme);    // a consistent set of dark components
```

Use it when you need to swap an entire set (UI themes, database drivers with matching connection, query builder, and migrator, cloud provider SDK families).

## Async factories

Constructors cannot be `async`. When creation needs I/O, use an async factory function or static method:

```js
class Database {
  #conn;

  constructor(conn) { this.#conn = conn; }      // sync, takes ready dependencies

  static async connect(url) {
    const conn = await openConnection(url);
    return new Database(conn);
  }

  query(sql) { return this.#conn.query(sql); }
}

const db = await Database.connect(process.env.DATABASE_URL);
```

This keeps instances always fully initialized: there is no half-built state where `db.query` is called before the connection exists.

## Factories that return cached or pooled instances

```js
const cache = new Map();

function getParser(format) {
  if (!cache.has(format)) cache.set(format, createParser(format));
  return cache.get(format);                      // same instance for the same format
}
```

Callers cannot tell whether they received a new or a shared object, so you can change the strategy later (the flyweight idea).

## Factories for testing

Test data factories build valid objects with sensible defaults, overriding only what each test cares about:

```js
let nextId = 1;

function buildUser(overrides = {}) {
  return {
    id: nextId++,
    name: 'Test User',
    email: `user${nextId}@example.com`,
    role: 'user',
    ...overrides,
  };
}

const admin = buildUser({ role: 'admin' });
const inactive = buildUser({ active: false });
```

Tests state only the relevant detail, and adding a required field means changing one function.

## Where factories appear in real code

| Example | What it creates |
|---------|-----------------|
| `document.createElement('div')` | Element objects of the right class by tag name |
| `Array.from`, `Object.create`, `Promise.resolve`, `Buffer.from` | Instances from various inputs |
| `React.createElement`, `createStore`, `createApp` | Elements, stores, application instances |
| `http.createServer()`, `net.createConnection()` | Configured servers and sockets |
| `express()`, `fastify()` | Application objects |
| `new Intl.NumberFormat()` vs `Intl.NumberFormat()` | Callable with or without `new` |
| ORM and test libraries' `factory.build()` | Model instances |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| A factory for a trivial object with no variation | Indirection with no benefit | Use an object literal or `new` |
| Giant `switch` or `if/else` chains choosing types | Editing the factory for every new type | Registry (lookup table) |
| Factories that hide required arguments | Confusing errors later | Validate and fail early with clear messages |
| Returning different shapes from one factory | Callers need type checks | Same interface for every product |
| Global mutable registry with no cleanup | Test pollution, ordering bugs | Scoped registries; reset in tests |
| Using `async` work inside constructors | Not possible; leads to half-initialized objects | Async factory or static `create()` |
| Name confusion (`create`, `make`, `build`, `new`) | Inconsistent API | Pick a convention and keep it |
| Exposing both a class and a factory with different behavior | Two ways to do one thing | Document the preferred path |

## Key takeaways

- A factory centralizes object creation: defaults, validation, and the choice of concrete type
- In JavaScript a plain function that returns an object is often all you need; closures give private state
- Static factory methods (`fromX`) read better than overloaded constructors
- Use a registry (`Map` or object) instead of long `switch` statements, and make it open for plugins
- Abstract factories create families of related objects
- Constructors cannot be async: use an async factory so objects are always fully initialized
- Do not add a factory until creation logic is repeated or varies

**Next:** [Singleton Pattern](./03_singleton-pattern.md)
