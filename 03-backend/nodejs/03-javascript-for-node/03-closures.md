# Closures

A closure is a function that remembers the variables from the scope it was created in, even after that outer scope has finished executing. This shows up constantly in Node — middleware factories, callbacks, and module-level state all lean on it.

## The core idea

```js
function makeCounter() {
  let count = 0;
  return function () {
    count++;
    return count;
  };
}

const counter = makeCounter();
console.log(counter()); // 1
console.log(counter()); // 2
console.log(counter()); // 3
```

`count` lives inside `makeCounter`'s scope, which has already finished running by the time `counter()` is called later — but the returned inner function still has access to it. That persistent, private access is a closure.

Each call to `makeCounter()` creates a **separate** closure with its own independent `count`:

```js
const counterA = makeCounter();
const counterB = makeCounter();
counterA(); // 1
counterA(); // 2
counterB(); // 1 — completely independent from counterA
```

---

## Where you've already used this: middleware factories

```js
function rateLimiter(maxRequests) {
  let requestCount = 0;

  return function (req, res, next) {
    requestCount++;
    if (requestCount > maxRequests) {
      return res.status(429).json({ error: "Too many requests" });
    }
    next();
  };
}

app.use(rateLimiter(100));
```

`rateLimiter(100)` runs once, capturing `maxRequests` and creating a fresh `requestCount` — the returned middleware function closes over both, remembering them across every subsequent request. This exact pattern is how most configurable Express middleware (`06-express/02-middleware.md`) is built.

---

## Where you've already used this: `setTimeout`/callbacks

```js
function scheduleGreeting(name) {
  setTimeout(() => {
    console.log(`Hello, ${name}!`); // closes over `name`
  }, 1000);
}

scheduleGreeting("Alice");
scheduleGreeting("Bob");
```

Each callback remembers its own `name`, even though `scheduleGreeting` has long since returned by the time the timeout fires.

---

## A classic gotcha: closures in loops

```js
// ❌ logs "3, 3, 3" — not "0, 1, 2" as you might expect
for (var i = 0; i < 3; i++) {
  setTimeout(() => console.log(i), 100);
}
```

`var` is function-scoped, not block-scoped — there's only **one** `i`, shared by all three callbacks, and by the time any of them run, the loop has already finished with `i` at `3`.

```js
// ✅ logs "0, 1, 2" — each iteration gets its own `i`
for (let i = 0; i < 3; i++) {
  setTimeout(() => console.log(i), 100);
}
```

`let` is block-scoped — each loop iteration creates a genuinely new binding of `i`, so each closure captures a different value. This is one of the most common real-world closure bugs, and is exactly why modern code defaults to `let`/`const` over `var`.

---

## Private state via closures (before classes had private fields)

```js
function createBankAccount(initialBalance) {
  let balance = initialBalance; // not accessible from outside

  return {
    deposit(amount) {
      balance += amount;
      return balance;
    },
    withdraw(amount) {
      if (amount > balance) throw new Error("Insufficient funds");
      balance -= amount;
      return balance;
    },
    getBalance() {
      return balance;
    },
  };
}

const account = createBankAccount(100);
account.deposit(50);
console.log(account.getBalance()); // 150
console.log(account.balance); // undefined — truly inaccessible
```

`balance` has no way to be accessed except through the returned methods — a form of true privacy that predates JavaScript's native `#privateField` class syntax (`04-prototypes.md`), and still shows up in factory-function-style code.

---

## Module-level closures

```js
// db.js
let connection = null;

export function connect() {
  connection = /* ... establish connection ... */;
}

export function getConnection() {
  if (!connection) throw new Error("Not connected yet");
  return connection;
}
```

Every function exported from this module closes over the same module-level `connection` variable — a common pattern for holding a single shared resource (a database connection, a cache client) across an app without needing a class or a global.

---

## Memory implications

Closures keep their captured variables alive in memory for as long as the closure itself is reachable — even a single large object captured unnecessarily can prevent it from being garbage collected. This is a common, subtle source of memory leaks in long-running Node processes, covered in `06-memory-management.md`.

## Common mistakes

- **Using `var` in a loop with an async callback** — the classic "all callbacks see the final value" bug; use `let` instead.
- **Capturing more than you need** — a closure holding onto a large object it barely uses can keep that object in memory far longer than necessary.
- **Not realizing each factory call creates independent state** — a common assumption error is thinking `counterA` and `counterB` above would share state, when each call to `makeCounter()` creates its own closure.

## Quick summary

- A closure is a function plus the variables from the scope it was created in, remembered even after that scope has returned
- Configurable middleware, per-callback state (`setTimeout`), and private state before class private fields all rely on this
- `let`/`const` in loops avoid the classic "all closures share one final value" bug that `var` causes
- Closures can extend an object's lifetime in memory for as long as the closure itself is reachable

## Next

**`04-prototypes.md`** covers JavaScript's other core mechanism for sharing behavior between objects — prototypal inheritance, and how `class` relates to it.
