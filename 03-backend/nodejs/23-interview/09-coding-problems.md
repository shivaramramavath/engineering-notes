# Node.js Technical Interview: Live Coding Challenges

> 5 essential live-coding implementation challenges asked in Senior and Staff Node.js engineering interviews, complete with production-grade TypeScript implementations, algorithmic complexity analyses, and edge case discussions.

---

### Challenge 1: Implement `EventEmitter` from Scratch

#### Requirements:
Build a custom EventEmitter class supporting:
- `on(event, listener)`: Registers a listener.
- `emit(event, ...args)`: Synchronously invokes all listeners.
- `off(event, listener)`: Removes a listener.
- `once(event, listener)`: Invokes the listener exactly once, then removes it.
- Prevent memory leaks with `setMaxListeners()`.

#### Complete Implementation:
```typescript
type Listener = (...args: any[]) => void;

export class CustomEventEmitter {
  private events = new Map<string, Listener[]>();
  private maxListeners = 10;

  setMaxListeners(n: number): this {
    this.maxListeners = n;
    return this;
  }

  on(event: string, listener: Listener): this {
    const listeners = this.events.get(event) || [];
    if (listeners.length >= this.maxListeners) {
      console.warn(
        `MaxListenersExceededWarning: Possible EventEmitter memory leak detected. ${listeners.length + 1} listeners added.`
      );
    }
    listeners.push(listener);
    this.events.set(event, listeners);
    return this;
  }

  off(event: string, listener: Listener): this {
    const listeners = this.events.get(event);
    if (!listeners) return this;

    const filtered = listeners.filter((fn) => fn !== listener && (fn as any)._original !== listener);
    if (filtered.length === 0) {
      this.events.delete(event);
    } else {
      this.events.set(event, filtered);
    }
    return this;
  }

  once(event: string, listener: Listener): this {
    const wrapper: Listener = (...args: any[]) => {
      this.off(event, wrapper);
      listener(...args);
    };
    (wrapper as any)._original = listener; // Store reference for clean off() removal
    return this.on(event, wrapper);
  }

  emit(event: string, ...args: any[]): boolean {
    const listeners = this.events.get(event);
    if (!listeners || listeners.length === 0) return false;

    // Slice array to avoid issues if listeners remove themselves during execution
    const snapshot = [...listeners];
    for (const listener of snapshot) {
      listener(...args);
    }
    return true;
  }
}
```

---

### Challenge 2: Implement an Asynchronous Concurrency Limiter (`p-limit`)

#### Requirements:
Given an array of async tasks (functions returning Promises) and a maximum concurrency $K$, execute tasks concurrently such that at most $K$ tasks run simultaneously.

#### Complete Implementation:
```typescript
export function pLimit(concurrency: number) {
  if (concurrency < 1) throw new Error('Concurrency must be at least 1');

  let activeCount = 0;
  const queue: Array<() => void> = [];

  const next = () => {
    activeCount--;
    if (queue.length > 0) {
      const task = queue.shift()!;
      task();
    }
  };

  return function run<T>(fn: () => Promise<T>): Promise<T> {
    return new Promise<T>((resolve, reject) => {
      const execute = async () => {
        activeCount++;
        try {
          const result = await fn();
          resolve(result);
        } catch (err) {
          reject(err);
        } finally {
          next();
        }
      };

      if (activeCount < concurrency) {
        execute();
      } else {
        queue.push(execute);
      }
    });
  };
}

// Usage Example:
const limit = pLimit(2); // Max 2 concurrent tasks
const tasks = [1, 2, 3, 4, 5].map((id) =>
  limit(() => new Promise((res) => setTimeout(() => { console.log(`Done: ${id}`); res(id); }, 100)))
);
await Promise.all(tasks);
```

---

### Challenge 3: Implement an LRU Cache with $O(1)$ Operations and TTL

#### Requirements:
Implement an in-memory Least Recently Used (LRU) Cache supporting `get(key)` and `set(key, value, ttlMs)` in $O(1)$ time complexity using a Hash Map and a Doubly Linked List.

#### Complete Implementation:
```typescript
class Node<K, V> {
  key: K;
  value: V;
  expiresAt: number;
  prev: Node<K, V> | null = null;
  next: Node<K, V> | null = null;

  constructor(key: K, value: V, ttlMs?: number) {
    this.key = key;
    this.value = value;
    this.expiresAt = ttlMs ? Date.now() + ttlMs : Infinity;
  }
}

export class LRUCache<K, V> {
  private capacity: number;
  private map = new Map<K, Node<K, V>>();
  private head: Node<K, V>;
  private tail: Node<K, V>;

  constructor(capacity: number) {
    this.capacity = capacity;
    // Dummy sentinel nodes
    this.head = new Node<any, any>(null, null);
    this.tail = new Node<any, any>(null, null);
    this.head.next = this.tail;
    this.tail.prev = this.head;
  }

  get(key: K): V | undefined {
    const node = this.map.get(key);
    if (!node) return undefined;

    // Check TTL expiration
    if (Date.now() > node.expiresAt) {
      this.remove(node);
      this.map.delete(key);
      return undefined;
    }

    // Move to front (most recently used)
    this.moveToHead(node);
    return node.value;
  }

  set(key: K, value: V, ttlMs?: number): void {
    if (this.map.has(key)) {
      const node = this.map.get(key)!;
      node.value = value;
      node.expiresAt = ttlMs ? Date.now() + ttlMs : Infinity;
      this.moveToHead(node);
      return;
    }

    if (this.map.size >= this.capacity) {
      // Evict least recently used node (before tail)
      const lru = this.tail.prev!;
      this.remove(lru);
      this.map.delete(lru.key);
    }

    const newNode = new Node(key, value, ttlMs);
    this.addNode(newNode);
    this.map.set(key, newNode);
  }

  private addNode(node: Node<K, V>) {
    node.prev = this.head;
    node.next = this.head.next;
    this.head.next!.prev = node;
    this.head.next = node;
  }

  private remove(node: Node<K, V>) {
    node.prev!.next = node.next;
    node.next!.prev = node.prev;
  }

  private moveToHead(node: Node<K, V>) {
    this.remove(node);
    this.addNode(node);
  }
}
```

---

### Challenge 4: Implement Async Retry with Exponential Backoff and Full Jitter

#### Requirements:
Write a resilient retry utility for network operations that implements exponential backoff with full randomization jitter (preventing thundering herd problems on downstream servers).

#### Complete Implementation:
```typescript
export interface RetryOptions {
  maxRetries: number;
  baseDelayMs: number;
  maxDelayMs: number;
  retryableErrors?: (err: any) => boolean;
}

export async function retryWithBackoff<T>(
  fn: () => Promise<T>,
  options: RetryOptions
): Promise<T> {
  const { maxRetries, baseDelayMs, maxDelayMs, retryableErrors } = options;

  let attempt = 0;
  while (true) {
    try {
      return await fn();
    } catch (err) {
      attempt++;
      if (attempt > maxRetries || (retryableErrors && !retryableErrors(err))) {
        throw err;
      }

      // Exponential backoff: base * 2^(attempt - 1)
      const exponentialDelay = Math.min(maxDelayMs, baseDelayMs * Math.pow(2, attempt - 1));

      // Full Jitter: Uniformly distributed between 0 and exponentialDelay
      const jitterDelay = Math.floor(Math.random() * exponentialDelay);

      console.warn(`[Retry Attempt ${attempt}/${maxRetries}] Waiting ${jitterDelay}ms before retry...`);
      await new Promise((resolve) => setTimeout(resolve, jitterDelay));
    }
  }
}
```

---

### Challenge 5: Implement a Custom Transform Stream for NDJSON Parsing

#### Requirements:
Write a custom `stream.Transform` class in Node.js that splits incoming binary data chunks by newline characters (`\n`) and emits parsed JavaScript objects. Must handle chunk boundary splitting (where a single JSON line spans multiple binary chunks).

#### Complete Implementation:
```typescript
import { Transform, TransformCallback } from 'node:stream';

export class NdjsonParserTransform extends Transform {
  private buffer: string = '';

  constructor() {
    super({ objectMode: true }); // Emits JS objects, accepts buffers
  }

  _transform(chunk: Buffer, _encoding: BufferEncoding, callback: TransformCallback): void {
    this.buffer += chunk.toString('utf8');
    const lines = this.buffer.split('\n');

    // The last element is either empty (if ended with \n) or incomplete
    this.buffer = lines.pop() || '';

    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed) continue;
      try {
        const parsed = JSON.parse(trimmed);
        this.push(parsed); // Emit parsed object downstream
      } catch (err: any) {
        return callback(new Error(`Malformed JSON in line: ${trimmed}`));
      }
    }

    callback();
  }

  _flush(callback: TransformCallback): void {
    if (this.buffer.trim()) {
      try {
        this.push(JSON.parse(this.buffer.trim()));
      } catch (err: any) {
        return callback(new Error(`Malformed JSON in trailing buffer: ${this.buffer}`));
      }
    }
    callback();
  }
}
```
