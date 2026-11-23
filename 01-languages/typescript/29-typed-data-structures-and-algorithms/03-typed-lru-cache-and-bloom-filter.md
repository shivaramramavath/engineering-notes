# Typed LRU Cache and Probabilistic Bloom Filter in TypeScript

> Implementation and formal typing of high-performance caching and probabilistic filtering: generic O(1) LRU Cache via Hash Map + Doubly Linked List, and Bloom Filter with bitwise hashing.

| Metadata | Details |
| :--- | :--- |
| **Category** | Data Structures & Algorithms |
| **Difficulty** | Advanced |
| **Prerequisites** | [29-typed-data-structures-and-algorithms/00-typed-linked-lists-and-trees.md](./00-typed-linked-lists-and-trees.md), [01-fundamentals/04-operators-expressions.md](../01-fundamentals/04-operators-expressions.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

High-throughput systems require fast, memory-bounded caching and pre-filtering to protect expensive backend databases:
1. **LRU Cache (Least Recently Used)**: Discards the least recently accessed items first when memory reaches capacity. Combining a `Map` with a **Doubly Linked List** guarantees $O(1)$ time complexity for both `get` and `set` operations.
2. **Bloom Filter**: A space-efficient probabilistic data structure used to test whether an element is definitely NOT in a set or MIGHT be in a set. False positives are possible, but false negatives are mathematically impossible. It prevents expensive database disk lookups for non-existent keys.

TypeScript enables both structures to be generic, type-safe, and memory-constrained using native typed arrays (`Uint8Array`) and bitwise operations.

---

## 2. Core Concepts

### LRU Cache Architecture (Hash Map + Doubly Linked List)
- **Hash Map (`Map<K, Node<K, V>>`)**: Provides $O(1)$ lookup for existing nodes.
- **Doubly Linked List**: Maintains chronological access order. Most recently accessed nodes move to the head; when capacity is exceeded, the tail node is evicted in $O(1)$ time.

```
+-------------------------------------------------------------------------+
|                  LRU Cache Dual Data Structure Layout                   |
+-------------------------------------------------------------------------+
|                                                                         |
|  Map: { "k1": NodeA, "k2": NodeB, "k3": NodeC }                         |
|                                                                         |
|  Doubly Linked List (Ordered by recency):                               |
|                                                                         |
|  [ HEAD (Most Recent) ] <-> [ NodeB ] <-> [ NodeA ] <-> [ TAIL (Old) ]  |
|                                                                         |
|  - get("k1"): Moves NodeA to HEAD in O(1)                               |
|  - set("k4"): Capacity exceeded -> Evicts TAIL (NodeC) in O(1)          |
|                                                                         |
+-------------------------------------------------------------------------+
```

### Bloom Filter Architecture
A bit array of size $M$ with $K$ independent hash functions.
- **Insert**: Hash item with all $K$ functions and set corresponding bits to `1`.
- **Query**: Check bits at all $K$ hash positions. If *any* bit is `0`, the item is definitely absent. If all are `1`, it is probably present.

---

## 3. How It Works Internally

1. **Map Storage in V8**: `Map` in modern V8 maintains insertion order internally, but moving an entry to the front requires re-insertion. A dedicated doubly linked list avoids Map mutation overhead.
2. **Bit Operations**: The Bloom Filter stores bits in a `Uint8Array`. For bit index $B$:
   - Byte index: `B >> 3` (equivalent to $\lfloor B / 8 \rfloor$)
   - Bit offset: `B & 7` (equivalent to $B \pmod 8$)

---

## 4. Syntax & API Reference

### LRU Cache Contract
```typescript
export interface ILRUCache<K, V> {
  readonly capacity: number;
  readonly size: number;
  get(key: K): V | undefined;
  set(key: K, value: V): void;
  has(key: K): boolean;
  clear(): void;
}
```

---

## 5. Practical Examples

### Example 1: Generic LRU Cache Implementation
```typescript
class LRUNode<K, V> {
  public prev: LRUNode<K, V> | null = null;
  public next: LRUNode<K, V> | null = null;
  constructor(public key: K, public value: V) {}
}

export class LRUCache<K, V> {
  private lookup = new Map<K, LRUNode<K, V>>();
  private head: LRUNode<K, V> | null = null;
  private tail: LRUNode<K, V> | null = null;

  constructor(public readonly capacity: number) {
    if (capacity <= 0) throw new Error("Capacity must be positive");
  }

  public get size(): number {
    return this.lookup.size;
  }

  public get(key: K): V | undefined {
    const node = this.lookup.get(key);
    if (!node) return undefined;
    this.moveToHead(node);
    return node.value;
  }

  public set(key: K, value: V): void {
    let node = this.lookup.get(key);
    if (node) {
      node.value = value;
      this.moveToHead(node);
      return;
    }

    node = new LRUNode(key, value);
    this.lookup.set(key, node);
    this.attachToHead(node);

    if (this.lookup.size > this.capacity) {
      this.evictTail();
    }
  }

  private moveToHead(node: LRUNode<K, V>): void {
    if (node === this.head) return;
    this.detach(node);
    this.attachToHead(node);
  }

  private attachToHead(node: LRUNode<K, V>): void {
    node.prev = null;
    node.next = this.head;
    if (this.head) {
      this.head.prev = node;
    }
    this.head = node;
    if (!this.tail) {
      this.tail = node;
    }
  }

  private detach(node: LRUNode<K, V>): void {
    if (node.prev) node.prev.next = node.next;
    if (node.next) node.next.prev = node.prev;
    if (node === this.head) this.head = node.next;
    if (node === this.tail) this.tail = node.prev;
    node.prev = null;
    node.next = null;
  }

  private evictTail(): void {
    if (!this.tail) return;
    const oldTail = this.tail;
    this.lookup.delete(oldTail.key);
    this.detach(oldTail);
  }
}
```

### Example 2: Probabilistic Bloom Filter with Typed Bitmask
```typescript
export class BloomFilter {
  private bitArray: Uint8Array;

  constructor(
    public readonly sizeInBits: number = 1024,
    public readonly hashCount: number = 3
  ) {
    this.bitArray = new Uint8Array(Math.ceil(sizeInBits / 8));
  }

  private hash(value: string, seed: number): number {
    let h = 0x811c9dc5 ^ seed;
    for (let i = 0; i < value.length; i++) {
      h ^= value.charCodeAt(i);
      h = Math.imul(h, 0x01000193);
    }
    return Math.abs(h % this.sizeInBits);
  }

  public add(value: string): void {
    for (let i = 0; i < this.hashCount; i++) {
      const bitIndex = this.hash(value, i);
      const byteIdx = bitIndex >> 3;
      const bitOffset = bitIndex & 7;
      this.bitArray[byteIdx]! |= 1 << bitOffset;
    }
  }

  public mightContain(value: string): boolean {
    for (let i = 0; i < this.hashCount; i++) {
      const bitIndex = this.hash(value, i);
      const byteIdx = bitIndex >> 3;
      const bitOffset = bitIndex & 7;
      if ((this.bitArray[byteIdx]! & (1 << bitOffset)) === 0) {
        return false; // Definitely NOT in set!
      }
    }
    return true; // Probably in set
  }
}
```

### Example 3: Edge Case - LRU with TTL (Time-To-Live)
```typescript
interface TTLValue<V> {
  value: V;
  expiresAt: number;
}

export class TTLLRUCache<K, V> {
  private cache: LRUCache<K, TTLValue<V>>;

  constructor(capacity: number, private defaultTtlMs = 60000) {
    this.cache = new LRUCache(capacity);
  }

  public set(key: K, value: V, ttlMs = this.defaultTtlMs): void {
    this.cache.set(key, { value, expiresAt: Date.now() + ttlMs });
  }

  public get(key: K): V | undefined {
    const entry = this.cache.get(key);
    if (!entry) return undefined;
    if (Date.now() > entry.expiresAt) {
      // Lazy eviction on expiration
      return undefined;
    }
    return entry.value;
  }
}
```

### Example 4: Compiler Error & Key-Value Type Safety
```typescript
const userCache = new LRUCache<string, { username: string }>(100);

userCache.set("u_101", { username: "Alice" });

// @ts-expect-error - TS2345: Argument of type 'number' is not assignable to parameter of type 'string'.
userCache.get(101);
```

### Example 5: High-Performance Bitwise MurmurHash3
```typescript
export function murmur3(key: string, seed = 0): number {
  let h1 = seed >>> 0;
  const c1 = 0xcc9e2d51;
  const c2 = 0x1b873593;

  for (let i = 0; i < key.length; i++) {
    let k1 = key.charCodeAt(i);
    k1 = Math.imul(k1, c1);
    k1 = (k1 << 15) | (k1 >>> 17);
    k1 = Math.imul(k1, c2);

    h1 ^= k1;
    h1 = (h1 << 13) | (h1 >>> 19);
    h1 = (Math.imul(h1, 5) + 0xe6546b64) >>> 0;
  }

  h1 ^= key.length;
  h1 ^= h1 >>> 16;
  h1 = Math.imul(h1, 0x85ebca6b);
  h1 ^= h1 >>> 13;
  h1 = Math.imul(h1, 0xc2b2ae35);
  h1 ^= h1 >>> 16;

  return h1 >>> 0;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Bloom Filter False Positives**:
   A Bloom filter can never return false negatives, but its false positive probability grows as more elements are inserted. Optimal bit size is $M = -\frac{N \ln P}{(\ln 2)^2}$, where $N$ is item count and $P$ is target false positive rate.
2. **Deleting from a Standard Bloom Filter**:
   Standard Bloom filters do not support deletion because multiple keys share the same bit positions. Deleting an item requires a **Counting Bloom Filter** (using 4-bit counters instead of single bits).
3. **Memory Leaks in Detached LRU Nodes**:
   When evicting tail nodes, ensure all pointers (`prev`, `next`) are set to `null` to facilitate V8 garbage collection.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Array Shift for LRU** | Using array unshift/shift | Map + Doubly Linked List | $O(N)$ vs $O(1)$ performance |
| **Unbounded In-Memory Map** | Storing items in standard Map | Bound size via LRU cache | Process out of memory (OOM) |
| **Deleting from Bloom Filter** | Flipping bits to 0 on remove | Use Counting Bloom Filter | Corrupts all other shared keys |
| **Under-sized Bloom Filter** | Small bit array with high items | Size using logarithmic formulas | 100% false positive rate |
| **Slow String Hash** | Character code summation | MurmurHash3 or FNV-1a | High hash collision clustering |
| **Missing Capacity Guard** | `capacity: 0` or negative | Throw on invalid capacity | Infinite loop during eviction |

---

## 8. Performance & Type-Checker Complexity

| Data Structure | Lookup Time | Insert Time | Space Complexity |
| :--- | :--- | :--- | :--- |
| **LRU Cache** | $O(1)$ | $O(1)$ | $O(C)$ where $C$ is capacity |
| **Bloom Filter** | $O(K)$ hashes | $O(K)$ hashes | $O(M)$ bits |

---

## 9. Compiler Internals

When using `LRUCache<K, V>`:
1. `ts.TypeChecker` verifies that `K` satisfies Map key constraints and `V` matches node values.
2. Typed array operations (`Uint8Array`) leverage JavaScript native TypedArray SIMD optimizations in V8.

---

## 10. Best Practices & Design Guidelines

1. **Always Cap In-Memory Caches**: Prevent memory exhaustion crashes.
2. **Use Bloom Filters as Pre-Flight Guards**: Avoid querying disk/database for missing keys.
3. **Use Typed Arrays (`Uint8Array`) for Bit Operations**: Minimizes V8 memory footprint.
4. **Implement Defensive Cleanup**: Clear detached pointers on node eviction.

---

## 11. Real-World Case Study

A high-traffic e-commerce portal handled 100,000 product view requests per second. Malicious web crawlers queried millions of non-existent product SKUs, bypassing the Redis cache and hitting the Postgres primary database directly with unindexed misses.

**Architecture Fix**:
1. Added a **Bloom Filter** initialized with 500,000 valid product IDs (occupying only 600KB in RAM).
2. Incoming requests were tested via `bloomFilter.mightContain(sku)`:
   - If `false`, the API returned `404 Not Found` immediately (0ms database impact).
   - If `true`, the request proceeded to the LRU cache and database.

**Outcome**: Database CPU load dropped from **94%** to **12%**, completely neutralizing crawler traffic.

---

## 12. Interview Questions

<details>
<summary>1. Why does an LRU Cache combine a Hash Map with a Doubly Linked List?</summary>

A Hash Map provides $O(1)$ key lookup but cannot efficiently maintain chronological access order. A Doubly Linked List provides $O(1)$ node insertion and deletion when node pointers are known, but takes $O(N)$ to search. Combining them allows $O(1)$ lookup (via the map) and $O(1)$ reordering/eviction (via the linked list).
</details>

<details>
<summary>2. What is the fundamental guarantee of a Bloom Filter?</summary>

A Bloom Filter guarantees zero false negatives: if it says an item is NOT in the set, the item is 100% guaranteed not to be in the set. If it says an item IS in the set, there is a small, tunable probability of a false positive.
</details>

<details>
<summary>3. Why can't you easily delete items from a standard Bloom Filter?</summary>

Multiple distinct keys can hash to the same bit positions. If you set a bit to 0 to delete one key, you may inadvertently set bits to 0 for other keys that are still present in the filter.
</details>

<details>
<summary>4. What is a Counting Bloom Filter?</summary>

A variation that replaces single bits with 4-bit or 8-bit counters. Adding an element increments the counters, and removing an element decrements the counters, enabling deletions at the cost of $4\times$ to $8\times$ more memory.
</details>

<details>
<summary>5. How does `Uint8Array` optimize bit manipulation in JavaScript?</summary>

`Uint8Array` allocates a contiguous, unmanaged chunk of binary memory outside the standard V8 pointer-tagged heap, allowing dense byte packing and high-speed bitwise operations (`|`, `&`, `>>`).
</details>

<details>
<summary>6. How do you calculate the optimal number of hash functions $K$ for a Bloom filter?</summary>

The optimal number of hash functions is $K = \frac{M}{N} \ln 2 \approx 0.693 \times \frac{M}{N}$, where $M$ is the bit array size and $N$ is the number of inserted elements.
</details>

<details>
<summary>7. What happens when an LRU Cache gets a cache hit?</summary>

The corresponding node is retrieved from the Hash Map in $O(1)$ time, detached from its current position in the Doubly Linked List, and re-attached at the head of the list to reflect recent access.
</details>

<details>
<summary>8. How do you implement an LFU (Least Frequently Used) cache compared to an LRU cache?</summary>

LFU tracks execution frequency counters for each key. It requires either a min-heap or an array of doubly linked lists indexed by frequency count, adding significant complexity over LRU.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Calculate Bloom Filter optimal bit size</summary>

**Task**: Write a utility function calculating the required bit size $M$ for $N$ items with target false positive rate $P$.

**Solution**:
```typescript
export function calculateBloomBits(n: number, p: number): number {
  return Math.ceil(-(n * Math.log(p)) / (Math.LN2 * Math.LN2));
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Clear all entries in LRUCache</summary>

**Task**: Implement `clear()` on the generic `LRUCache`.

**Solution**:
```typescript
public clear(): void {
  this.lookup.clear();
  this.head = null;
  this.tail = null;
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Most Recently Used (MRU) eviction strategy</summary>

**Task**: Implement an MRU cache that evicts the most recently used item rather than the least recently used.

**Solution**:
```typescript
// In an MRU cache, eviction removes this.head instead of this.tail
private evictHead(): void {
  if (!this.head) return;
  const oldHead = this.head;
  this.lookup.delete(oldHead.key);
  this.detach(oldHead);
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Counting Bloom Filter with Deletion</summary>

**Task**: Implement a Counting Bloom Filter supporting `.remove(value)` using a `Uint8Array` of 8-bit counters.

**Solution**:
```typescript
export class CountingBloomFilter {
  private counters: Uint8Array;

  constructor(public size = 1024, public hashCount = 3) {
    this.counters = new Uint8Array(size);
  }

  private hash(val: string, seed: number): number {
    let h = 0x811c9dc5 ^ seed;
    for (let i = 0; i < val.length; i++) {
      h ^= val.charCodeAt(i);
      h = Math.imul(h, 0x01000193);
    }
    return Math.abs(h % this.size);
  }

  public add(val: string): void {
    for (let i = 0; i < this.hashCount; i++) {
      const idx = this.hash(val, i);
      if (this.counters[idx]! < 255) this.counters[idx]!++;
    }
  }

  public remove(val: string): void {
    for (let i = 0; i < this.hashCount; i++) {
      const idx = this.hash(val, i);
      if (this.counters[idx]! > 0) this.counters[idx]!--;
    }
  }

  public mightContain(val: string): boolean {
    for (let i = 0; i < this.hashCount; i++) {
      if (this.counters[this.hash(val, i)]! === 0) return false;
    }
    return true;
  }
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Thread-Safe LRU Cache with Asynchronous Loader</summary>

**Task**: Implement `getOrSet(key, asyncLoader)` in LRUCache with in-flight promise deduplication (thundering herd protection).

**Solution**:
```typescript
import { LRUCache } from "./lru.js";

export class AsyncLoadingLRU<K, V> {
  private cache: LRUCache<K, V>;
  private inFlight = new Map<K, Promise<V>>();

  constructor(capacity: number) {
    this.cache = new LRUCache(capacity);
  }

  public async getOrSet(key: K, loader: () => Promise<V>): Promise<V> {
    const cached = this.cache.get(key);
    if (cached !== undefined) return cached;

    if (this.inFlight.has(key)) {
      return this.inFlight.get(key)!;
    }

    const promise = loader()
      .then((res) => {
        this.cache.set(key, res);
        this.inFlight.delete(key);
        return res;
      })
      .catch((err) => {
        this.inFlight.delete(key);
        throw err;
      });

    this.inFlight.set(key, promise);
    return promise;
  }
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Scalable Dynamic Bloom Filter</summary>

**Task**: Design a Scalable Bloom Filter that dynamically instantiates a new Bloom filter layer when the current layer's capacity threshold is reached.

**Solution**:
```typescript
import { BloomFilter } from "./bloom.js";

export class ScalableBloomFilter {
  private filters: BloomFilter[] = [];
  private currentCount = 0;

  constructor(private capacityPerLayer = 10000, private errorRate = 0.01) {
    this.filters.push(new BloomFilter(capacityPerLayer * 10, 4));
  }

  public add(val: string): void {
    if (this.currentCount >= this.capacityPerLayer) {
      this.filters.push(new BloomFilter(this.capacityPerLayer * 10, 4));
      this.currentCount = 0;
    }
    this.filters[this.filters.length - 1]!.add(val);
    this.currentCount++;
  }

  public mightContain(val: string): boolean {
    return this.filters.some((filter) => filter.mightContain(val));
  }
}
```
</details>
