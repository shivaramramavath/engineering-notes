# Observer Pattern

The **observer pattern** lets one object (the **subject**, or *publisher*) notify any number of other objects (**observers**, or *subscribers*) when something happens, **without knowing who they are**. It decouples the code that produces events from the code that reacts to them.

It is everywhere in JavaScript: DOM events, Node's `EventEmitter`, reactive state libraries, `MutationObserver`, RxJS, and framework change detection.

See also: [Events (Node)](../16_nodejs/05_events.md), [Event Emitter](../23_real-world-patterns/07_event-emitter.md), [Events (DOM)](../14_dom-and-browser/04_events.md), [Observers (DOM)](../14_dom-and-browser/07_observers.md).

## The problem: tight coupling

```js
class Cart {
  add(item) {
    this.items.push(item);
    badge.update(this.items.length);       // Cart knows about the badge
    analytics.track('add', item);          // ...and analytics
    inventory.reserve(item);               // ...and inventory
  }
}
```

Every new reaction requires editing `Cart`, and `Cart` cannot be used or tested without all of them.

## A minimal subject

```js
class Subject {
  #observers = new Set();

  subscribe(fn) {
    this.#observers.add(fn);
    return () => this.#observers.delete(fn);     // returns an unsubscribe function
  }

  notify(data) {
    for (const fn of [...this.#observers]) {     // copy: safe if observers unsubscribe during notify
      fn(data);
    }
  }
}

const temperature = new Subject();

const unsubscribe = temperature.subscribe((t) => console.log(`Display: ${t}°C`));
temperature.subscribe((t) => { if (t > 30) console.log('Alert: hot!'); });

temperature.notify(25);     // Display: 25°C
temperature.notify(35);     // Display: 35°C, Alert: hot!

unsubscribe();
temperature.notify(20);     // only the alert observer runs (silent, since 20 <= 30)
```

Design choices worth noticing:

- A `Set` prevents duplicate subscriptions and gives O(1) removal
- `subscribe` **returns an unsubscribe function**, which is hard to forget and easy to call in cleanup code
- Iterating over a **copy** avoids bugs when observers add or remove observers while being notified

## Observable state

Combine a value with notification, so changes publish themselves:

```js
function createObservable(initial) {
  let value = initial;
  const observers = new Set();

  return {
    get() { return value; },

    set(next) {
      if (Object.is(next, value)) return;        // skip when nothing changed
      const prev = value;
      value = next;
      for (const fn of [...observers]) fn(next, prev);
    },

    subscribe(fn) {
      observers.add(fn);
      return () => observers.delete(fn);
    },
  };
}

const count = createObservable(0);
count.subscribe((n, prev) => console.log(`${prev} → ${n}`));
count.set(1);      // 0 → 1
count.set(1);      // no output
```

This is the core of reactive state libraries (signals, stores, observables).

## Events by name: pub/sub

Add **topics** so observers subscribe only to what they care about:

```js
class EventBus {
  #handlers = new Map();      // event name → Set of handlers

  on(event, handler) {
    if (!this.#handlers.has(event)) this.#handlers.set(event, new Set());
    this.#handlers.get(event).add(handler);
    return () => this.off(event, handler);
  }

  once(event, handler) {
    const off = this.on(event, (...args) => { off(); handler(...args); });
    return off;
  }

  off(event, handler) {
    this.#handlers.get(event)?.delete(handler);
  }

  emit(event, ...args) {
    for (const handler of [...(this.#handlers.get(event) ?? [])]) {
      handler(...args);
    }
  }
}

const bus = new EventBus();
bus.on('order:placed', (order) => sendEmail(order));
bus.on('order:placed', (order) => updateStock(order));
bus.emit('order:placed', { id: 42 });
```

Now `Cart` only calls `emit('item:added', item)`, and badges, analytics, and inventory subscribe independently. Observer (the subject keeps its own list) and **publish/subscribe** (a separate broker or bus sits in between) are close cousins: with pub/sub, publishers and subscribers do not reference each other at all.

## Built-in observers

### Node.js: `EventEmitter`

```js
import { EventEmitter } from 'node:events';

class Downloader extends EventEmitter {
  async run(url) {
    this.emit('start', url);
    const res = await fetch(url);
    this.emit('done', await res.text());
  }
}

const d = new Downloader();
d.on('start', (u) => console.log('starting', u));
d.once('done', (body) => console.log(body.length));
await d.run('https://example.com');
```

Remember: listeners run synchronously, and an unhandled `'error'` event throws. See [Events (Node)](../16_nodejs/05_events.md).

### Browser: `EventTarget`, DOM events, custom events

```js
const target = new EventTarget();

const controller = new AbortController();
target.addEventListener('ping', (e) => console.log(e.detail), { signal: controller.signal });

target.dispatchEvent(new CustomEvent('ping', { detail: { at: Date.now() } }));
controller.abort();                          // removes the listener
```

`EventTarget` is available in Node too. Extending it gives you the web-standard observer API anywhere.

### Browser observer APIs

| API | Observes |
|-----|----------|
| `MutationObserver` | DOM changes (nodes added or removed, attributes) |
| `IntersectionObserver` | Elements entering or leaving the viewport |
| `ResizeObserver` | Element size changes |
| `PerformanceObserver` | Performance entries (LCP, long tasks) |
| `BroadcastChannel` | Messages between tabs |
| `Proxy` (with traps) | Property reads and writes on an object (see below) |

### Observing object changes with `Proxy`

```js
function observe(target, onChange) {
  return new Proxy(target, {
    set(obj, key, value, receiver) {
      const prev = obj[key];
      const ok = Reflect.set(obj, key, value, receiver);
      if (ok && !Object.is(prev, value)) onChange(key, value, prev);
      return ok;
    },
    deleteProperty(obj, key) {
      const ok = Reflect.deleteProperty(obj, key);
      if (ok) onChange(key, undefined, obj[key]);
      return ok;
    },
  });
}

const state = observe({ count: 0 }, (key, next, prev) => console.log(key, prev, '→', next));
state.count++;        // count 0 → 1
```

This is how several reactive frameworks track state (a `Proxy` can intercept writes without requiring a setter call). See [Proxy and Reflect](../09_built-in-objects/11_proxy-and-reflect.md). Note that this shallow proxy only sees top-level properties; nested objects need to be proxied recursively.

## Async observers

Observers can be asynchronous, but the subject does not wait unless you design it to:

```js
// Fire-and-forget: the subject does not wait; errors vanish unless handled
emitter.on('save', async (doc) => { await upload(doc); });

// Wait for all observers (and surface failures)
async function notifyAll(observers, data) {
  const results = await Promise.allSettled([...observers].map((fn) => fn(data)));
  for (const r of results) if (r.status === 'rejected') console.error(r.reason);
}
```

Event streams can also be consumed with `for await` ([Async Iterators and Generators](../11_asynchronous-javascript/07_async-iterators-and-generators.md)), and Node's `events.on(emitter, 'name')` turns an emitter into an async iterator.

## Error isolation

One failing observer should not stop the others:

```js
notify(data) {
  for (const fn of [...this.#observers]) {
    try {
      fn(data);
    } catch (err) {
      console.error('Observer failed:', err);     // log and continue
    }
  }
}
```

Choose deliberately: swallowing errors keeps the system running; letting them propagate surfaces bugs loudly. `EventTarget` reports listener errors without stopping other listeners; `EventEmitter` lets a throwing listener abort `emit`.

## Observer vs related patterns

| Pattern | Relationship |
|---------|--------------|
| **Pub/sub** | Observer with a broker in between; publishers and subscribers are fully decoupled |
| **Mediator** | One central object coordinates peers, instead of broadcast |
| **Callbacks** | One observer, supplied per call |
| **Promises** | One notification, one time |
| **Async iterators / streams** | Many values over time, pulled by the consumer (with backpressure) |
| **Reactive programming (RxJS, signals)** | Observer pattern plus composition operators (map, filter, debounce) |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Never unsubscribing | Memory leaks; handlers keep objects alive; stale updates | Return unsubscribe functions; tie to lifecycle (`AbortSignal`, component unmount) |
| Mutating the observer list while notifying | Skipped or repeated observers | Iterate over a copy |
| Observer throws and stops the rest | One bug breaks everything | Isolate with `try/catch` (log) |
| Notification cascades (observer changes state, which notifies again) | Infinite loops, hard-to-trace flows | Guard re-entrancy; batch updates |
| Order dependence between observers | Fragile behavior | Do not rely on order, or make dependencies explicit |
| Notifying when nothing changed | Needless work and re-renders | Compare with `Object.is` first |
| Sending huge payloads to every observer | Wasted memory and CPU | Send identifiers or minimal data |
| Async observers whose errors vanish | Unhandled rejections | `try/catch` inside, or `Promise.allSettled` |
| Hidden control flow ("who reacts to this?") | Hard to debug | Namespace event names, log events in dev, keep the catalog documented |
| Using a global bus for everything | Spaghetti coupling | Scoped emitters per module or feature |
| Too many listeners warning in Node | Usually a leak | Remove listeners; do not just raise the limit |

## Key takeaways

- The observer pattern lets a subject notify many observers without knowing them, decoupling producers from consumers
- A minimal version is a `Set` of functions with `subscribe` (returning an unsubscribe function) and `notify`
- Add event names for pub/sub, or wrap values to build observable state
- JavaScript has it built in: `EventTarget`, DOM events, Node's `EventEmitter`, and the browser `*Observer` APIs
- Always provide a way to unsubscribe; leaks are the most common problem
- Notify over a copy of the list, isolate observer errors, and skip notifications when nothing changed
- Prefer scoped emitters over one global bus to keep control flow understandable

**Next:** [Adapter Pattern](./07_adapter-pattern.md)
