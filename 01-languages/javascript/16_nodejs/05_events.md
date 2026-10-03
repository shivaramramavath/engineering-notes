# Events

Many Node APIs (servers, streams, sockets, child processes) are built on `EventEmitter` from `node:events`. An emitter lets objects announce that something happened and lets other code react. It is Node's version of the [observer pattern](../20_design-patterns/06_observer-pattern.md).

## Basics

```js
import { EventEmitter } from 'node:events';

const bus = new EventEmitter();

bus.on('greet', (name) => console.log(`Hello, ${name}`));
bus.emit('greet', 'Ada');     // Hello, Ada   (returns true: there were listeners)
bus.emit('nobody');           // returns false
```

| Method | Purpose |
|--------|---------|
| `on(event, fn)` / `addListener` | Add a listener |
| `once(event, fn)` | Add a listener that runs at most once |
| `off(event, fn)` / `removeListener` | Remove a specific listener |
| `removeAllListeners([event])` | Remove all listeners |
| `emit(event, ...args)` | Call listeners synchronously, in registration order |
| `prependListener(event, fn)` | Add to the front of the list |
| `listenerCount(event)` | How many listeners are attached |
| `listeners(event)` | Copy of the listener array |
| `eventNames()` | Events that have listeners |

## Emitting is synchronous

`emit` calls every listener **immediately and in order**, then returns. Listeners that are `async` return promises that `emit` ignores.

```js
bus.on('x', () => console.log('A'));
bus.on('x', () => console.log('B'));

console.log('before');
bus.emit('x');
console.log('after');
// before, A, B, after
```

Defer work to a later tick if listeners must not run synchronously:

```js
process.nextTick(() => bus.emit('ready'));
```

## `this` and arrow functions

In a regular function listener, `this` is the emitter. In an arrow function it is not:

```js
bus.on('x', function () { console.log(this === bus); });   // true
bus.on('x', () => console.log(this === bus));               // false (lexical this)
```

## Removing listeners

You need the same function reference:

```js
function onData(d) { /* ... */ }
bus.on('data', onData);
bus.off('data', onData);

bus.on('data', (d) => {});   // an inline arrow cannot be removed later
```

`EventEmitter#on()` does not accept a `signal` option. To cancel waiting, use `events.once(emitter, name, { signal })` (see below), or `EventTarget`, whose `addEventListener` does accept `{ signal }`. For a plain emitter, keep the handler reference and call `off`.

## The `error` event

`'error'` is special. If you emit `'error'` and **no listener** exists, Node **throws** the error and the process crashes:

```js
const e = new EventEmitter();
e.emit('error', new Error('boom'));   // throws: Unhandled 'error' event
```

Always attach an `error` listener to emitters that can fail (sockets, streams, child processes, your own classes):

```js
e.on('error', (err) => console.error('failed:', err.message));
```

Emit real `Error` objects, not strings.

## Extending EventEmitter

```js
import { EventEmitter } from 'node:events';

class Downloader extends EventEmitter {
  async download(url) {
    this.emit('start', url);
    try {
      const res = await fetch(url);
      const body = await res.text();
      this.emit('done', body.length);
      return body;
    } catch (err) {
      this.emit('error', err);
    }
  }
}

const d = new Downloader();
d.on('start', (u) => console.log('downloading', u));
d.on('done', (n) => console.log('bytes:', n));
d.on('error', (e) => console.error(e));
await d.download('https://example.com');
```

Prefer **composition** when you only need to emit, and extend only when instances really are emitters. See [Mixins and Composition](../05_this-and-oop/08_mixins-and-composition.md).

## Waiting for events with promises

```js
import { once, on } from 'node:events';

// Wait for one event (resolves with the argument array)
const [code] = await once(child, 'exit');

// With cancellation / timeout
const [msg] = await once(socket, 'message', { signal: AbortSignal.timeout(5000) });

// Async-iterate every event
const ac = new AbortController();
for await (const [chunk] of on(stream, 'data', { signal: ac.signal })) {
  console.log(chunk);
}
```

`once()` also rejects if the emitter emits `'error'` while waiting.

## Listener limits and leaks

By default, adding more than **10** listeners for one event prints a `MaxListenersExceededWarning`. It usually signals a leak (adding a listener per request and never removing it).

```js
bus.setMaxListeners(20);                     // for this emitter
EventEmitter.defaultMaxListeners = 20;       // global default (use sparingly)
bus.setMaxListeners(0);                      // unlimited: hides leaks

bus.getMaxListeners();
```

Leak example:

```js
// BAD: every request adds a permanent listener
server.on('request', (req) => {
  config.on('change', () => update(req));
});
```

Fix by removing listeners when done, using `once`, or attaching once outside the handler.

## Special events

```js
bus.on('newListener', (event, fn) => { /* before a listener is added */ });
bus.on('removeListener', (event, fn) => { /* after one is removed */ });
```

## Async listeners and error handling

```js
bus.on('job', async (job) => {
  throw new Error('oops');       // becomes an unhandled rejection
});
```

Opt in to routing rejected promises from listeners to the `'error'` event:

```js
const bus = new EventEmitter({ captureRejections: true });
bus.on('error', (err) => console.error('listener failed:', err));
bus.on('job', async () => { throw new Error('oops'); });
```

Or wrap the body in `try/catch` yourself.

## EventTarget

Node also implements the web `EventTarget` and `Event` (used by `AbortSignal`, `BroadcastChannel`, and others):

```js
const target = new EventTarget();
target.addEventListener('ping', (e) => console.log(e.type), { once: true });
target.dispatchEvent(new Event('ping'));
```

| | `EventEmitter` | `EventTarget` |
|---|----------------|---------------|
| Origin | Node | Web standard |
| Listener args | Any values | One `Event` object |
| Special `error` behavior | Yes (throws if unhandled) | No |
| Propagation, `preventDefault` | No | Yes |
| Where it appears | Node core APIs | `AbortSignal`, web-compatible APIs |

## A small typed emitter pattern

```js
class Store extends EventEmitter {
  #state;
  constructor(initial) { super(); this.#state = initial; }

  get state() { return this.#state; }

  set(patch) {
    const prev = this.#state;
    this.#state = { ...prev, ...patch };
    this.emit('change', this.#state, prev);
  }
}

const store = new Store({ count: 0 });
store.on('change', (next, prev) => console.log(prev.count, '→', next.count));
store.set({ count: 1 });
```

See also [Event Emitter](../23_real-world-patterns/07_event-emitter.md) for building your own from scratch.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Emitting `'error'` with no listener | Process crashes | Always add an `error` listener |
| Adding listeners per request without removing | Memory leak and warnings | `once`, `off`, or attach once |
| Expecting `emit` to be async | It runs listeners synchronously | `process.nextTick` / `setImmediate` to defer |
| Removing an inline arrow listener | Cannot, no reference | Keep a named reference |
| `async` listeners that throw | Unhandled rejection | `try/catch` or `captureRejections` |
| Using `setMaxListeners(0)` to silence warnings | Hides real leaks | Find the leak |
| Emitting before anyone listens | Event is lost | Emit on the next tick or after setup |
| Arrow functions relying on `this` | `this` is not the emitter | Regular function or closure |

## Key takeaways

- `EventEmitter` is the base of streams, servers, sockets, and child processes
- `emit` is synchronous; listeners run in registration order
- Unhandled `'error'` events throw: always listen for them
- `once()` and `events.on()` from `node:events` turn events into promises and async iterators
- Watch the 10-listener warning: it usually means a leak

**Next:** [Streams](./06_streams.md)
