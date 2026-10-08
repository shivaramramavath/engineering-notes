# Typed Event Emitter

An **event emitter** lets one part of a program announce that something happened, and lets other parts react without the announcer knowing who is listening (the observer or publish/subscribe pattern). Plain JavaScript emitters are stringly typed: event names are strings and payloads are `any`, so typos and wrong payloads slip through. TypeScript lets you describe each event and its payload once, and have `on`, `off`, and `emit` checked against it.

**Prerequisites:**
- [Generic types](../06-generics/01-generic-types.md) and [generic constraints](../06-generics/02-generic-constraints-and-defaults.md)
- [keyof and typeof](../06-generics/03-keyof-and-typeof.md)
- [Mapped types](../10-advanced-types/03-mapped-types.md) and [variadic tuple types](../10-advanced-types/06-variadic-tuple-types.md)

---

## The problem

```ts
import { EventEmitter } from "node:events";

const bus = new EventEmitter();

bus.on("userLoggedIn", (user) => {          // `user` is any
  console.log(user.nmae);                   // typo, no error
});

bus.emit("userLogedIn", { name: "Asha" });  // typo in the event name, no error, nothing happens
```

Two bugs, and the compiler sees neither: a misspelled event name silently never fires, and the listener's payload is untyped.

## Describe the events once

Define a map from event name to the **tuple of arguments** the listeners receive:

```ts
type AppEvents = {
  login: [user: User];
  logout: [];
  error: [error: Error, context?: string];
  progress: [done: number, total: number];
};
```

Tuples (with optional labels) describe positional arguments, matching how `emit(event, ...args)` works. An event with no payload has an empty tuple.

## Node's built-in EventEmitter

Recent versions of `@types/node` allow EventEmitter to be generic over such a map:

```ts
import { EventEmitter } from "node:events";

const bus = new EventEmitter<AppEvents>();

bus.on("login", (user) => console.log(user.name));      // user: User
bus.emit("progress", 3, 10);                            // checked: (number, number)
bus.emit("progress", "3");                              // error
bus.on("loggedIn", () => {});                           // error: not a known event
```

Support depends on your `@types/node` version, so check yours. If your version does not provide generics, or you want an emitter that also runs in browsers, write a small one.

## A minimal typed emitter

```ts
type Listener<Args extends unknown[]> = (...args: Args) => void;

class TypedEmitter<Events extends { [K in keyof Events]: unknown[] }> {
  private listeners: { [K in keyof Events]?: Set<Listener<Events[K]>> } = {};

  on<K extends keyof Events>(event: K, listener: Listener<Events[K]>): () => void {
    const set = (this.listeners[event] ??= new Set());
    set.add(listener);
    return () => this.off(event, listener);              // returns an unsubscribe function
  }

  once<K extends keyof Events>(event: K, listener: Listener<Events[K]>): () => void {
    const unsubscribe = this.on(event, (...args) => {
      unsubscribe();
      listener(...args);
    });
    return unsubscribe;
  }

  off<K extends keyof Events>(event: K, listener: Listener<Events[K]>): void {
    this.listeners[event]?.delete(listener);
  }

  emit<K extends keyof Events>(event: K, ...args: Events[K]): void {
    this.listeners[event]?.forEach((listener) => listener(...args));
  }
}
```

Using it:

```ts
const bus = new TypedEmitter<AppEvents>();

const stop = bus.on("progress", (done, total) => {
  console.log(`${done}/${total}`);          // done: number, total: number
});

bus.emit("progress", 1, 4);                 // ok
bus.emit("progress", 1);                    // error: expected 2 arguments
bus.emit("logout");                         // ok: no payload
bus.emit("login", { name: "Asha" });        // checked against User

stop();                                     // unsubscribe
```

The generic parameter `K extends keyof Events` ties the event name to its argument tuple: `on("progress", ...)` gives the listener `(done: number, total: number)`, and `emit("progress", ...)` requires exactly those arguments. This uses the same indexed-access and rest-parameter techniques from [variadic tuple types](../10-advanced-types/06-variadic-tuple-types.md).

### Why the odd constraint

`Events extends { [K in keyof Events]: unknown[] }` says "every property of `Events` is an array/tuple". A simpler `Record<string, unknown[]>` constraint would reject **interfaces**, because interfaces do not get an implicit index signature. The self-referential mapped constraint accepts both `type` aliases and `interface`s.

## Payload-object style

Some codebases prefer one payload value per event instead of positional arguments:

```ts
type Events = {
  login: { user: User };
  logout: undefined;
  progress: { done: number; total: number };
};

class PayloadEmitter<E> {
  private handlers: { [K in keyof E]?: Set<(payload: E[K]) => void> } = {};

  on<K extends keyof E>(event: K, handler: (payload: E[K]) => void): () => void {
    const set = (this.handlers[event] ??= new Set());
    set.add(handler);
    return () => set.delete(handler);
  }

  emit<K extends keyof E>(event: K, ...payload: E[K] extends undefined ? [] : [payload: E[K]]): void {
    this.handlers[event]?.forEach((h) => h((payload as unknown[])[0] as E[K]));
  }
}
```

Named payload objects are easier to extend without breaking listeners (you can add a field), while tuples match Node's convention. Pick one style and stay consistent.

## Derived helpers with mapped and template literal types

Typed events combine well with the type-level features from earlier sections:

```ts
// "on" + capitalized event name, for a convenience API
type Handlers<E extends { [K in keyof E]: unknown[] }> = {
  [K in keyof E & string as `on${Capitalize<K>}`]?: (...args: E[K]) => void;
};

const handlers: Handlers<AppEvents> = {
  onLogin: (user) => console.log(user.name),
  onProgress: (done, total) => console.log(done / total),
};
```

See [template literal types](../10-advanced-types/04-template-literal-types.md).

## Browser events

The DOM has its own typing. `addEventListener` has overloads keyed by event name, which is why `e` is a `MouseEvent` for `"click"`:

```ts
window.addEventListener("click", (e) => console.log(e.clientX));   // e: MouseEvent
```

For custom events on DOM targets, `CustomEvent<T>` carries a typed `detail`:

```ts
const event = new CustomEvent<{ id: string }>("item-added", { detail: { id: "42" } });
element.dispatchEvent(event);

element.addEventListener("item-added", (e) => {
  // e is `Event` unless you augment HTMLElementEventMap, so cast or augment
  const { id } = (e as CustomEvent<{ id: string }>).detail;
});
```

To get fully typed custom events, augment `HTMLElementEventMap` (or `WindowEventMap`) with your event name ([global and module augmentation](../09-declaration-files/02-global-and-module-augmentation.md)). In React, prefer props and callbacks over a global emitter ([event types](../19-react-and-frontend/01-event-types.md)).

## Consuming events as async iteration

Node can adapt events to an async iterator, which turns a stream of events into a loop ([iterators and generators](../12-async-and-iteration/04-iterators-and-generators.md)):

```ts
import { on, once } from "node:events";

// wait for a single event as a promise
const [result] = await once(emitter, "ready");

// consume events as they arrive
for await (const [data] of on(emitter, "message")) {
  handle(data);
}
```

Pass an `AbortSignal` to stop waiting ([concurrency patterns](../12-async-and-iteration/05-concurrency-patterns.md)). These helpers are typed for the general case, so with a custom emitter you may need to annotate the result.

## Behavior to design for

### Errors in listeners

A listener that throws propagates out of `emit` and may skip later listeners. In Node, emitting the special event `"error"` with no listener **throws**. Decide the policy:

```ts
emit<K extends keyof Events>(event: K, ...args: Events[K]): void {
  for (const listener of this.listeners[event] ?? []) {
    try {
      listener(...args);
    } catch (e) {
      console.error(`Listener for "${String(event)}" failed`, e);   // isolate one bad listener
    }
  }
}
```

Isolating listeners keeps one failure from silencing the others. Async listeners are harder: a rejected promise from an `async` listener is **not** caught by this `try/catch`, because `emit` does not await it. Handle errors inside async listeners, or provide an `emitAsync` that awaits them ([error handling strategies](../11-error-handling/03-error-handling-strategies.md)).

### Memory leaks

Every `on` without a matching `off` keeps the listener (and anything it closes over) alive. Always unsubscribe when the subscriber is done: components unmounting, requests finishing, objects being disposed. Returning an unsubscribe function from `on` makes cleanup easy to do correctly. Node warns when many listeners are attached to one event, which usually indicates a leak.

### Ordering and re-entrancy

Listeners run in registration order, synchronously, inside `emit`. A listener that emits another event runs nested. A listener that adds or removes listeners during emission can change who runs. Copy the set before iterating (`[...set]`) if that matters.

### Event emitter vs alternatives

| Use an emitter when | Prefer something else when |
|---|---|
| Many independent subscribers react to something | One caller wants one result (`await` a function or promise) |
| The emitter should not know its listeners | The relationship is a direct dependency (inject it, [DI](./05-dependency-injection.md)) |
| Events are fire-and-forget notifications | You need guaranteed ordering, delivery, or retries (use a queue) |
| It is a UI or lifecycle signal | Complex state changes (a [state machine](./06-state-machines.md) or store is clearer) |

Emitters make control flow implicit: following "what happens when X occurs" means searching for listeners. Use them for genuine decoupling, not as a replacement for function calls.

## Important rules and misconceptions

- **Events are synchronous by default.** `emit` calls listeners before returning. Nothing is asynchronous unless the listener is.
- **Types do not check at runtime.** Another module can still call `emit` through an `any` cast.
- **An interface map needs the mapped-type constraint** (not `Record<string, unknown[]>`), or `interface` event maps are rejected.
- **`off` needs the same function reference.** An inline arrow in `on` cannot be removed later unless you keep it, which is why returning an unsubscribe closure is handy.
- **A typed emitter documents the contract,** and the event map becomes the one place to learn which events exist.

## Common mistakes

- Using string event names with an untyped emitter.
- Forgetting to unsubscribe, leaking listeners and their closures.
- Passing a new inline function to `off` and expecting removal.
- Letting a throwing listener break `emit` for everyone else.
- `async` listeners with unhandled rejections.
- Emitting inside listeners and creating hard-to-follow cycles.
- Using a global emitter as a replacement for explicit dependencies, hiding data flow.
- Declaring the event map as `Record<string, unknown[]>` and losing per-event payload types.

## Debugging

- Log `emit` calls (event name, arguments) and listener counts during development to see who is subscribed.
- If an event "does nothing", check that the name matches exactly (the typed version makes this a compile error) and that the listener was registered **before** the emit.
- If listener counts grow over time, search for `on` calls without matching cleanup.
- If a listener runs twice, check whether it was registered twice (hot reload, repeated setup code).
- For "emit throws unexpectedly", check for the `"error"` event convention in Node, and for listeners that throw.

## Quick summary

- Describe events in one map of event name to argument tuple (or payload type), and type `on`, `off`, and `emit` generically with `K extends keyof Events`.
- Node's `EventEmitter` can be generic in recent `@types/node` versions. A small custom `TypedEmitter` is easy to write and works anywhere.
- Constrain with `{ [K in keyof Events]: unknown[] }` so interfaces work too. Return an unsubscribe function from `on`.
- Design for listener errors, async listeners, cleanup, and re-entrancy. Prefer direct calls or injection when there is one clear collaborator.
- Use `events.once` and `events.on` to turn events into promises and async iterators.

**Next:** [18 Testing and Debugging](../18-testing-and-debugging/README.md)
