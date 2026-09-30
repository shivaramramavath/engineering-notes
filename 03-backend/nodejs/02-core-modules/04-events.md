# `events` — EventEmitter

Node's built-in pub/sub pattern: objects that emit named events, and other code that listens for them. This is the pattern underneath streams, HTTP requests/responses, and a huge portion of Node's own core modules and third-party libraries.

```js
import { EventEmitter } from "node:events";
```

## Basic usage

```js
const emitter = new EventEmitter();

emitter.on("greet", (name) => {
  console.log(`Hello, ${name}!`);
});

emitter.emit("greet", "world");
// → "Hello, world!"
```

- `.on(eventName, listener)` — register a listener for an event
- `.emit(eventName, ...args)` — trigger the event, calling every registered listener synchronously, in the order they were added, passing along any extra arguments

---

## Multiple listeners for the same event

```js
emitter.on("userCreated", (user) => sendWelcomeEmail(user));
emitter.on("userCreated", (user) => logAnalyticsEvent(user));
emitter.on("userCreated", (user) => updateSearchIndex(user));

emitter.emit("userCreated", { id: 1, email: "a@example.com" });
// all three listeners run, in registration order
```

This is the core value of the pattern: decoupling "something happened" from "everything that should happen as a result" — the code that creates a user doesn't need to know or care what else reacts to it.

---

## `.once()` — listen only one time

```js
emitter.once("ready", () => console.log("Only logs the first time"));

emitter.emit("ready"); // logs
emitter.emit("ready"); // does not log again — listener already removed
```

Useful for one-time setup/initialization events.

---

## Removing listeners

```js
function onData(chunk) {
  console.log(chunk);
}

emitter.on("data", onData);
emitter.off("data", onData); // removes this specific listener
// or the older alias:
emitter.removeListener("data", onData);

emitter.removeAllListeners("data"); // removes every listener for this event
```

You need a reference to the exact function passed to `.on()` to remove it later — an inline arrow function can't be removed this way, since you never kept a reference to it.

---

## Error events are special

```js
const emitter = new EventEmitter();

emitter.emit("error", new Error("Something broke"));
```

```
Uncaught EventEmitter error: Something broke
    at ...
```

**If an `"error"` event is emitted with no listener registered for it, Node throws and crashes the process.** This is a deliberate design choice — errors are considered too important to silently ignore. Always attach an error listener on anything that might emit one:

```js
emitter.on("error", (err) => {
  console.error("Handled error:", err.message);
});
```

This is why you'll see `.on("error", ...)` attached almost reflexively on streams, sockets, and database connections throughout Node code.

---

## Extending `EventEmitter` (the common real-world pattern)

Rather than creating a standalone `EventEmitter`, most real code extends it to build a custom class with built-in event capabilities:

```js
class OrderProcessor extends EventEmitter {
  process(order) {
    // ... do the work ...
    this.emit("processed", order);
  }
}

const processor = new OrderProcessor();
processor.on("processed", (order) => {
  console.log(`Order ${order.id} processed`);
});

processor.process({ id: 42 });
```

This is exactly the pattern Node's own `http.Server`, `fs.ReadStream`, and `net.Socket` all use internally — they're all `EventEmitter` subclasses.

---

## Max listeners warning

```js
(node:12345) MaxListenersExceededWarning: Possible EventEmitter memory leak detected.
11 listeners added. Use emitter.setMaxListeners() to increase limit
```

By default, Node warns if more than 10 listeners are attached to a single event — this is a heuristic for catching a common bug (accidentally re-registering a listener in a loop, or on every request, instead of once). If you genuinely need more listeners intentionally:

```js
emitter.setMaxListeners(20);
```

But treat this warning as a prompt to double-check for an accidental leak first, rather than immediately silencing it.

---

## Async listeners: be careful

```js
emitter.on("userCreated", async (user) => {
  await sendWelcomeEmail(user); // if this throws, it becomes an unhandled rejection
});

emitter.emit("userCreated", user);
```

`emit()` doesn't wait for async listeners to finish, and doesn't catch errors they throw — an unhandled rejection inside an async listener won't surface where you might expect. Wrap async listener logic in its own `try/catch` if you need to handle failures gracefully:

```js
emitter.on("userCreated", async (user) => {
  try {
    await sendWelcomeEmail(user);
  } catch (err) {
    console.error("Failed to send welcome email:", err);
  }
});
```

---

## Where you'll actually encounter this

You'll use `EventEmitter` directly less often than you'll rely on it indirectly, through:

- `req.on("data", ...)` / `req.on("end", ...)` on HTTP requests (`03-http.md`)
- Stream events: `stream.on("data", ...)`, `stream.on("error", ...)` (`05-streams.md`)
- `process.on("exit", ...)`, `process.on("SIGTERM", ...)` (`07-process.md`)
- Database driver and queue library events (connection lost, job completed, etc.)

## Quick summary

- `EventEmitter` implements a pub/sub pattern: `.emit()` triggers an event, `.on()` registers a listener for it
- `.once()` auto-removes itself after firing; `.off()`/`removeListener()` removes a specific listener by reference
- An `"error"` event with no listener crashes the process — always handle it explicitly
- Most real usage extends `EventEmitter` in a custom class, rather than instantiating it directly
- Async listeners aren't awaited by `emit()` — wrap their logic in `try/catch` if failures need handling

## Next

**`05-streams.md`** covers streams — themselves built on `EventEmitter`, and the mechanism behind `req`/`res` bodies and large file handling.
