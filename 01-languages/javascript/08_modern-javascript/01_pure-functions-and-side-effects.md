# Pure Functions and Side Effects

A **pure function** has two properties:

1. **Deterministic**: the same arguments always produce the same result
2. **No side effects**: it does not change anything outside itself

```js
const add = (a, b) => a + b;              // pure

let total = 0;
const addToTotal = (n) => (total += n);   // impure: changes outer state
```

## Impure signals

| Impurity | Example |
|----------|---------|
| Reads hidden state | `Date.now()`, `Math.random()`, global variables, `process.env` |
| Mutates arguments | `list.push(x)`, `obj.count++` |
| Mutates outer variables | `counter++` |
| I/O | `console.log`, `fetch`, `fs.readFile`, DOM changes, `localStorage` |
| Throws or depends on exceptions | `throw` is debatable; treat as an effect |
| Returns different objects for equal input | acceptable if values are equal; reference identity is what breaks purity checks |

## Side effects

A **side effect** is any observable change outside the function's return value.

```js
function save(user) {
  db.insert(user);        // effect
  console.log("saved");   // effect
  return user.id;
}
```

Programs need effects (they exist to do something). FP does not ban them; it **pushes them to the edges**.

## Referential transparency

A call can be replaced by its result without changing the program.

```js
const double = (n) => n * 2;
double(4) + double(4);    // same as 8 + 8
```

This makes code easy to reason about, cache, reorder and test.

## Benefits

| Benefit | Why |
|---------|-----|
| Testability | No mocks: input in, output out |
| Predictability | Nothing changes behind your back |
| Memoization | Safe to cache results |
| Parallelism | No shared mutable state |
| Debugging | Failures depend only on arguments |
| Refactoring | Move or reuse without surprises |

## Making functions pure

### Pass dependencies in

```js
// impure
const greet = () => `Hello at ${new Date().toISOString()}`;

// pure
const greet2 = (now) => `Hello at ${now.toISOString()}`;
greet2(new Date());
```

```js
// impure: random inside
const pickImpure = (list) => list[Math.floor(Math.random() * list.length)];

// pure: random source injected
const pick = (list, rand) => list[Math.floor(rand * list.length)];
pick(list, Math.random());
```

### Return new data instead of mutating

```js
// impure
function addItem(cart, item) { cart.items.push(item); return cart; }

// pure
const addItem2 = (cart, item) => ({ ...cart, items: [...cart.items, item] });
```

### Avoid shared variables

```js
let discount = 0.1;
const priceImpure = (p) => p * (1 - discount);   // depends on outer state

const price = (p, discount) => p * (1 - discount);
```

## Pure core, impure shell

Structure programs as pure logic surrounded by a thin layer that performs effects.

```js
// pure core
const buildInvoice = (order, taxRate) => ({
  id: order.id,
  subtotal: order.items.reduce((s, i) => s + i.price * i.qty, 0),
  tax: order.items.reduce((s, i) => s + i.price * i.qty, 0) * taxRate,
});

// impure shell
async function handleOrder(id) {
  const order = await db.getOrder(id);                 // effect in
  const invoice = buildInvoice(order, config.tax);     // pure
  await mailer.send(order.email, invoice);             // effect out
}
```

## Purity in practice

| Function | Pure? |
|----------|-------|
| `(a, b) => a + b` | Yes |
| `arr.map(x => x * 2)` | Yes (returns new array) |
| `arr.sort()` | **No**, mutates the array |
| `arr.toSorted()` | Yes |
| `Math.max(...nums)` | Yes |
| `Math.random()` | No |
| `str.toUpperCase()` | Yes (strings are immutable) |
| `JSON.parse(text)` | Yes for the same text (may throw) |
| `console.log(x)` | No |

## Idempotent vs pure

**Idempotent**: calling twice has the same effect as once (`PUT`, `Set.add`, setting a value). Idempotent functions can still be impure. Purity is stricter.

## Controlling effects

| Technique | Idea |
|-----------|------|
| Dependency injection | Pass `fetch`, `now`, `random`, `logger` as arguments |
| Return descriptions of effects | `{ type: "SEND_EMAIL", to, body }` executed elsewhere (Redux, Elm style) |
| Wrap effects in thunks | `() => fetch(url)` delays running |
| Isolate in a boundary layer | Handlers, controllers, `main()` |
| Use Result/Either for errors | Return errors as values (see patterns file) |

```js
const makeClock = (now = () => Date.now()) => ({ stamp: (x) => ({ ...x, at: now() }) });
makeClock(() => 1000).stamp({ a: 1 });   // deterministic in tests
```

## Unavoidable state

Not everything must be pure. Keep impurity **local and labeled**: a function that mutates only its own local variables is still externally pure.

```js
function sum(list) {
  let total = 0;                // local mutation, invisible outside
  for (const n of list) total += n;
  return total;
}
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Hidden reads of globals or time | Non-deterministic results | Pass values in |
| `sort`, `reverse`, `splice` on inputs | Mutates the caller's data | `toSorted`, copies |
| `console.log` inside "pure" helpers | Effect leaks in | Log at the boundary |
| Returning the same mutable object you received | Callers share state | Return a new object |
| Treating "no visible mutation" as pure while reading `Date.now()` | Still impure | Inject time |
| Making everything pure at any cost | Complexity, performance | Pure core, effects at the edges |

## Key takeaways

- Pure = same input, same output, no side effects
- Pass in what functions need instead of reading hidden state
- Keep a pure core and push I/O to the edges
- Prefer non-mutating array methods (`map`, `filter`, `toSorted`)

**Next:** [Immutability](./02_immutability.md)
