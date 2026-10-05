# Strategy Pattern

The strategy pattern lets you **swap an algorithm at runtime** by putting each variant behind the same interface and passing the chosen one in. It replaces long `if/else` or `switch` chains that pick behavior by type.

In JavaScript, functions are values, so a strategy is usually just a function. No class hierarchy is needed.

**Prerequisites:** [Higher-Order Functions](../02_functions/06_higher-order-functions.md), [Callbacks](../02_functions/05_callbacks.md)

---

## The Problem

```js
function shippingCost(order, method) {
  if (method === "standard") return order.weightKg * 5;
  if (method === "express") return order.weightKg * 12 + 20;
  if (method === "pickup") return 0;
  throw new Error(`Unknown method: ${method}`);
}
```

Every new method means editing this function, and the function grows. Each rule also can't be tested or reused on its own.

---

## Strategies as Functions

Give every strategy the same signature, then select one by key.

```js
const shippingStrategies = {
  standard: (order) => order.weightKg * 5,
  express: (order) => order.weightKg * 12 + 20,
  pickup: () => 0,
};

function shippingCost(order, method) {
  const strategy = Object.hasOwn(shippingStrategies, method)
    ? shippingStrategies[method]
    : null;
  if (!strategy) throw new Error(`Unknown method: ${method}`);
  return strategy(order);
}

shippingCost({ weightKg: 2 }, "express"); // 44
```

Adding a method is now adding an entry. Each strategy is a small pure function you can unit test directly.

The "context" can also receive the function itself rather than a key:

```js
function checkout(order, shipping) {
  return order.subtotal + shipping(order);
}

checkout(order, shippingStrategies.express);
checkout(order, () => 0); // ad-hoc strategy, e.g. a promo
```

---

## You Already Use It

```js
users.sort((a, b) => a.age - b.age);        // comparator = sorting strategy
items.filter((x) => x.active);              // predicate = filtering strategy
JSON.stringify(data, replacerFn);           // replacer = serialization strategy
```

Any API that accepts a callback to customize behavior is using this pattern. See [Sorting and Searching](../03_objects-and-arrays/08_sorting-and-searching.md).

---

## A Practical Example: Backoff

Retry logic stays the same while the delay rule varies ([Retry](../23_real-world-patterns/03_retry.md)).

```js
const backoff = {
  fixed: (ms) => () => ms,
  exponential: (base, max = 30_000) => (attempt) => Math.min(base * 2 ** attempt, max),
};

async function retry(fn, { retries = 3, delay = backoff.fixed(500) } = {}) {
  for (let attempt = 0; ; attempt++) {
    try {
      return await fn();
    } catch (err) {
      if (attempt >= retries) throw err;
      await new Promise((r) => setTimeout(r, delay(attempt)));
    }
  }
}

await retry(() => fetch(url), { delay: backoff.exponential(200) });
```

Here the strategy is created by a function that closes over its configuration (`base`, `max`). Closures are how strategies carry their own parameters.

---

## Class-Based Strategies

Use classes when a strategy has **its own state** or multiple methods.

```js
class RateLimitedPricing {
  #calls = 0;
  price(item) {
    this.#calls++;
    return item.price * (this.#calls > 3 ? 1.1 : 1);
  }
}
```

If every strategy has exactly one method and no state, a class is extra ceremony. Use a function.

---

## When to Use It

- You have a chain of branches choosing between algorithms of the **same shape**.
- Variants change independently of the caller (pricing, validation, sorting, formatting, retry delay).
- You want to pick or replace the behavior from config or tests.

When not to: two branches that will never grow. A plain `if` is clearer.

---

## Common Mistakes

- **Inconsistent signatures.** If `express` needs `(order, distance)` while the others take `(order)`, the caller starts branching again. Pass a context object so every strategy can ignore what it doesn't need.
- **Unchecked keys** from user input (`shippingStrategies["toString"]`). Use `Object.hasOwn` or a `Map`.
- **Losing `this`.** If a strategy is an object method that uses `this`, passing `obj.method` detaches it. Bind it or use closures ([call, apply, bind](../05_this-and-oop/02_call-apply-bind.md)).
- **Strategies reaching into shared mutable state.** That makes them order-dependent and hard to test.
- **No default.** Decide whether an unknown key should throw or fall back, and make it explicit.

---

## Quick Summary

- Strategy = one interface, many interchangeable algorithms, selected by the caller or config.
- In JS the interface is usually a function signature.
- A lookup object replaces `if/else` chains, and each strategy is testable alone.
- Use classes only when a strategy needs state.

**Next:** [Observer Pattern](./06_observer-pattern.md)