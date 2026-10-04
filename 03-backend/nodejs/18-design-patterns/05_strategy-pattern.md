# Strategy Pattern

The **strategy pattern** defines a family of interchangeable algorithms and lets you **choose one at runtime**. The code that uses the algorithm (the *context*) depends on a common interface, not on any specific implementation.

In JavaScript, a strategy is usually **just a function**. Functions are first-class values, so you can pass, store, and swap them without any class hierarchy.

See also: [Higher-Order Functions](../02_functions/06_higher-order-functions.md), [Callbacks](../02_functions/05_callbacks.md), [Factory Pattern](./02_factory-pattern.md), [Dependency Injection](./09_dependency-injection.md).

## The problem: a growing conditional

```js
function calculateShipping(order, method) {
  if (method === 'standard') {
    return order.weight * 1.5;
  } else if (method === 'express') {
    return order.weight * 3 + 10;
  } else if (method === 'overnight') {
    return order.weight * 5 + 25;
  } else if (method === 'pickup') {
    return 0;
  }
  throw new Error(`Unknown method: ${method}`);
}
```

Every new method means editing this function (and re-testing everything in it). The decision logic and the algorithms are tangled together.

## Strategy as functions

Pull each algorithm into its own function and look them up by name:

```js
const shippingStrategies = {
  standard: (order) => order.weight * 1.5,
  express: (order) => order.weight * 3 + 10,
  overnight: (order) => order.weight * 5 + 25,
  pickup: () => 0,
};

function calculateShipping(order, method) {
  const strategy = shippingStrategies[method];
  if (!strategy) throw new Error(`Unknown method: ${method}`);
  return strategy(order);
}

calculateShipping({ weight: 4 }, 'express');    // 22
```

Adding a method means adding one entry. Each strategy can be tested alone. The context (`calculateShipping`) never changes.

Use a `Map` or `Object.hasOwn` if method names come from untrusted input, so a name like `"constructor"` or `"__proto__"` cannot resolve to something inherited:

```js
const strategy = Object.hasOwn(shippingStrategies, method) ? shippingStrategies[method] : undefined;
```

## Passing the strategy in

The most flexible form: the caller supplies the algorithm.

```js
function sortBy(items, compare) {
  return [...items].sort(compare);             // compare IS the strategy
}

const byPrice = (a, b) => a.price - b.price;
const byName = (a, b) => a.name.localeCompare(b.name);

sortBy(products, byPrice);
sortBy(products, byName);
```

`Array.prototype.sort`, `map`, `filter`, `reduce`, `find`, and `setTimeout` are all strategy-style APIs: the algorithm's skeleton is fixed, the varying step is a function you pass in.

### Validation with interchangeable rules

```js
const rules = {
  required: (v) => (v ? null : 'Required'),
  email: (v) => (/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(v) ? null : 'Invalid email'),
  minLength: (n) => (v) => (v.length >= n ? null : `At least ${n} characters`),
};

function validate(value, ...checks) {
  return checks.map((check) => check(value)).filter(Boolean);
}

validate('ab', rules.required, rules.minLength(5));     // ['At least 5 characters']
validate('x@y', rules.email);                           // ['Invalid email']
```

Strategies built by functions that return functions (`minLength(5)`) let you configure them.

## Strategy as classes

Use classes when strategies carry state, have several methods, or you want an explicit shared interface:

```js
class FlatRate {
  constructor(rate) { this.rate = rate; }
  cost() { return this.rate; }
}

class WeightBased {
  constructor(perKg) { this.perKg = perKg; }
  cost(order) { return order.weight * this.perKg; }
}

class Free {
  cost() { return 0; }
}

class Checkout {
  #shipping;

  constructor(shipping) { this.#shipping = shipping; }

  setShipping(strategy) { this.#shipping = strategy; }      // swap at runtime

  total(order) {
    return order.subtotal + this.#shipping.cost(order);
  }
}

const checkout = new Checkout(new WeightBased(1.5));
checkout.total({ subtotal: 100, weight: 4 });                // 106
checkout.setShipping(new Free());
checkout.total({ subtotal: 100, weight: 4 });                // 100
```

JavaScript has no interfaces, so the shared contract is a convention (here: a `cost(order)` method). **Duck typing** makes any object with that method a valid strategy. Document the contract and, in TypeScript, declare an `interface`.

## Choosing the strategy

The selection logic belongs **outside** the algorithms, in one place:

```js
function pickCompression(file) {
  if (file.size < 1024) return compressors.none;
  if (file.type.startsWith('text/')) return compressors.brotli;
  if (file.type.startsWith('image/')) return compressors.none;     // already compressed
  return compressors.gzip;
}

const compressor = pickCompression(file);
const output = await compressor(file.data);
```

You can also let strategies describe **when they apply**, and select the first that matches:

```js
const discounts = [
  { applies: (cart) => cart.items.length >= 10, apply: (total) => total * 0.9 },
  { applies: (cart) => cart.coupon === 'WELCOME', apply: (total) => total - 5 },
  { applies: () => true, apply: (total) => total },                // default
];

function priceFor(cart, subtotal) {
  const discount = discounts.find((d) => d.applies(cart));
  return discount.apply(subtotal);
}
```

## Real-world uses

| Area | Strategy examples |
|------|-------------------|
| Authentication | Passport.js strategies: local, OAuth, JWT, API key |
| Sorting and comparison | Comparator functions |
| Pricing, tax, shipping, discount rules | One function per rule |
| Retry and backoff | Fixed, linear, exponential, with jitter |
| Caching | LRU, TTL, no-cache |
| Compression and serialization | gzip, brotli; JSON, MessagePack |
| Payment processing | Stripe, PayPal, bank transfer behind one `charge()` |
| Logging | Console, file, remote sink |
| Rendering | Canvas, SVG, WebGL backends |
| Validation | Interchangeable rules per field |

### Example: retry backoff strategies

```js
const backoff = {
  fixed: (ms) => () => ms,
  linear: (ms) => (attempt) => ms * attempt,
  exponential: (ms) => (attempt) => ms * 2 ** (attempt - 1),
  jittered: (ms) => (attempt) => Math.random() * ms * 2 ** (attempt - 1),
};

async function retry(fn, { retries = 3, delay = backoff.exponential(200) } = {}) {
  for (let attempt = 1; ; attempt++) {
    try {
      return await fn();
    } catch (err) {
      if (attempt > retries) throw err;
      await new Promise((r) => setTimeout(r, delay(attempt)));
    }
  }
}

await retry(fetchData, { retries: 5, delay: backoff.jittered(100) });
```

See [Retry](../23_real-world-patterns/03_retry.md).

## Strategy vs related patterns

| Pattern | Difference |
|---------|------------|
| **Strategy** | The **caller or configuration** picks an algorithm; usually one active at a time |
| **State** | The **object itself** switches behavior as its internal state changes |
| **Factory** | Creates objects; often used to *select* a strategy |
| **Decorator** | Wraps one behavior to add to it, rather than replacing it |
| **Template method** | Fixed algorithm skeleton with overridable steps (inheritance), where strategy uses composition |
| **Callback** | A strategy for a single step, passed as an argument |

Prefer composition (strategy) to inheritance: you can change behavior at runtime and combine behaviors without deep class trees.

## Testing

Strategies are small, pure functions: ideal for testing.

```js
import test from 'node:test';
import assert from 'node:assert/strict';

test('express shipping', () => {
  assert.equal(shippingStrategies.express({ weight: 2 }), 16);
});

test('checkout uses the injected strategy', () => {
  const checkout = new Checkout({ cost: () => 7 });          // a stub strategy
  assert.equal(checkout.total({ subtotal: 10 }), 17);
});
```

Injecting a fake strategy replaces a real dependency (payment gateway, clock, random source) with a predictable one.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| A strategy class hierarchy for two trivial branches | Over-engineering | A simple `if`, or a pair of functions |
| Looking up strategies with a plain object and untrusted keys | `constructor`, `__proto__`, and inherited names resolve | `Map`, `Object.hasOwn`, or `Object.create(null)` |
| Strategies with different signatures | The context needs special cases | One agreed contract (same inputs and outputs) |
| Selection logic scattered across callers | Duplicated decisions | One selector or registry |
| Strategies depending on hidden global state | Hard to test | Pass what they need as arguments or constructor parameters |
| Mutating a shared strategy object per request | Cross-request bugs | Stateless strategies, or create one per use |
| No default or unknown-key handling | `TypeError: strategy is not a function` | Fall back to a default or throw a clear error |
| Confusing strategy with state | Wrong design | Strategy is chosen from outside; state transitions come from within |

## Key takeaways

- A strategy is an interchangeable algorithm behind a common contract; the context uses it without knowing which one
- In JavaScript, strategies are usually plain functions stored in an object or `Map`, or passed as arguments
- It replaces growing `if/else` and `switch` chains: add behavior by adding an entry, not by editing logic
- Use classes only when strategies have state or several methods
- Keep selection logic in one place and give strategies a consistent signature
- Strategies are easy to test, and injecting fakes keeps tests isolated
- Guard lookups against inherited keys and unknown names

**Next:** [Observer Pattern](./06_observer-pattern.md)
