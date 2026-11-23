# Hash Tables & Hashing Algorithms in JavaScript

> A production-grade implementation and algorithmic guide to hash tables in JavaScript, covering collision resolution strategies, dynamic rehashing, cryptographic vs non-cryptographic hash functions, and V8 OrderedHashTable mechanics.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [09_built-in-objects/04_map-set-weakmap-weakset.md](file:///e:/Learning/engineering-notes/01-languages/javascript/09_built-in-objects/04_map-set-weakmap-weakset.md) |
| **Standard Baseline** | ECMAScript 2024 / Node.js 20+ LTS / V8 Engine |
| **Status** | Production Ready |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In computational algorithms and systems design, storing and retrieving data in **$O(1)$ constant time** is essential. While arrays provide $O(1)$ lookup by numerical index, searching for an element by an arbitrary key (e.g. username, email, SKU) takes $O(N)$ linear time without indexing.

A **Hash Table** (or Hash Map) is an associative data structure that maps arbitrary keys to values using a **Hash Function**. The hash function converts a key into a numerical index within an underlying bucket array.
- **Fast Lookups**: Average-case $O(1)$ insertion, search, and deletion.
- **Deduplication**: Immediate $O(1)$ membership verification (`Set.has()`).
- **Counting & Aggregation**: Frequency counting, anagram grouping, and caching indexes.

---

## 2. Core Concepts

### 1. Hash Function Requirements
A hash function $h(k)$ maps keys of arbitrary size to a fixed-size integer:
- **Deterministic**: The same key must always produce the identical hash code.
- **Uniform Distribution**: Keys must be distributed evenly across all buckets to minimize collisions.
- **Computational Efficiency**: Hashing must execute in $O(1)$ or $O(L)$ time (where $L$ is key length).

### 2. Collision Resolution Strategies
Because the key universe is infinite while bucket capacity is finite (Pigeonhole Principle), two distinct keys can hash to the same bucket index ($h(k_1) \equiv h(k_2) \pmod M$):
- **Separate Chaining**: Each bucket holds a linked list or dynamic array of key-value pairs. On collision, new entries are appended to the bucket's chain.
- **Open Addressing (Linear/Quadratic Probing)**: All elements reside directly in the bucket array. On collision, the algorithm probes forward ($i + 1, i + 2 \dots$) to find the next unoccupied slot.

```
Separate Chaining:
Bucket Index:
[ 0 ] ──► [ "alpha": 10 ] ──► [ "omega": 42 ] (Collision chain)
[ 1 ] ──► null
[ 2 ] ──► [ "beta": 20 ]
[ 3 ] ──► null
```

### 3. Load Factor ($\alpha$) & Dynamic Rehashing
The **Load Factor** measures table occupancy:
$$\alpha = \frac{\text{Number of Stored Items } (N)}{\text{Bucket Capacity } (K)}$$
When $\alpha$ exceeds a threshold (typically $0.70$ to $0.75$), collision frequency increases and lookup time degrades toward $O(N)$. The table automatically **rehashes**: it doubles its bucket capacity ($K \to 2K$) and re-indexes all existing keys into the new bucket array.

---

## 3. How It Works Internally

```
Key: "user_42"
       │
       ▼
[ Hash Function: djb2 / MurmurHash3 ]
       │
       ▼
Integer Hash Code: 2087349182
       │
       ▼  Modulo Bucket Count: index = hash % 8
Bucket Index: 6
       │
       ▼
Bucket Array [6] ──► Inspect chain for key "user_42" ──► Return Value
```

---

## 4. Syntax & API Reference

### Full Hash Table Implementation from Scratch (Separate Chaining + Rehashing)
```javascript
/**
 * Production-ready Hash Table implementation from scratch
 * using Separate Chaining and dynamic load factor rehashing.
 */
export class HashTable {
  constructor(initialCapacity = 8, maxLoadFactor = 0.75) {
    this.capacity = initialCapacity;
    this.maxLoadFactor = maxLoadFactor;
    this.size = 0;
    this.buckets = new Array(this.capacity).fill(null).map(() => []);
  }

  /**
   * djb2 string hashing algorithm - fast, uniform non-cryptographic hash.
   */
  _hash(key) {
    const str = String(key);
    let hash = 5381;
    for (let i = 0; i < str.length; i++) {
      // hash * 33 + charCode
      hash = ((hash << 5) + hash) + str.charCodeAt(i);
      hash = hash & hash; // Convert to 32-bit integer
    }
    return Math.abs(hash) % this.capacity;
  }

  set(key, value) {
    const index = this._hash(key);
    const bucket = this.buckets[index];

    // Check if key already exists in the chain
    for (let i = 0; i < bucket.length; i++) {
      if (bucket[i][0] === key) {
        bucket[i][1] = value; // Update value
        return;
      }
    }

    bucket.push([key, value]);
    this.size++;

    // Check load factor and resize if necessary
    if (this.size / this.capacity > this.maxLoadFactor) {
      this._rehash(this.capacity * 2);
    }
  }

  get(key) {
    const index = this._hash(key);
    const bucket = this.buckets[index];

    for (let i = 0; i < bucket.length; i++) {
      if (bucket[i][0] === key) {
        return bucket[i][1];
      }
    }
    return undefined;
  }

  has(key) {
    return this.get(key) !== undefined;
  }

  delete(key) {
    const index = this._hash(key);
    const bucket = this.buckets[index];

    for (let i = 0; i < bucket.length; i++) {
      if (bucket[i][0] === key) {
        bucket.splice(i, 1);
        this.size--;
        return true;
      }
    }
    return false;
  }

  _rehash(newCapacity) {
    const oldBuckets = this.buckets;
    this.capacity = newCapacity;
    this.buckets = new Array(this.capacity).fill(null).map(() => []);
    this.size = 0;

    for (const bucket of oldBuckets) {
      for (const [key, value] of bucket) {
        this.set(key, value);
      }
    }
  }
}
```

---

## 5. Practical Examples

### Example 1: Group Anagrams (Sorted Key Hashing)
```javascript
export function groupAnagrams(words) {
  const anagramGroups = new Map();

  for (const word of words) {
    // Canonical hash key: characters sorted alphabetically
    const sortedKey = word.split('').sort().join('');

    if (!anagramGroups.has(sortedKey)) {
      anagramGroups.set(sortedKey, []);
    }
    anagramGroups.get(sortedKey).push(word);
  }

  return Array.from(anagramGroups.values());
}

// console.log(groupAnagrams(["eat", "tea", "tan", "ate", "nat", "bat"]));
// Output: [["eat","tea","ate"], ["tan","nat"], ["bat"]]
```

### Example 2: Subarray Sum Equals $K$ (Prefix Sum + Hash Map)
```javascript
/**
 * Finds total count of continuous subarrays whose sum equals k.
 * Time Complexity: O(N) | Space Complexity: O(N)
 */
export function subarraySum(nums, k) {
  const prefixCounts = new Map();
  prefixCounts.set(0, 1); // Base case: prefix sum of 0 has occurred once

  let currentSum = 0;
  let totalCount = 0;

  for (let i = 0; i < nums.length; i++) {
    currentSum += nums[i];

    // If (currentSum - k) exists in hash map, a valid subarray ends here
    const targetPrefix = currentSum - k;
    if (prefixCounts.has(targetPrefix)) {
      totalCount += prefixCounts.get(targetPrefix);
    }

    prefixCounts.set(currentSum, (prefixCounts.get(currentSum) || 0) + 1);
  }

  return totalCount;
}
```

### Example 3: Frequency Counter & Majority Element
```javascript
export function findMajorityElement(nums) {
  const counts = new Map();
  const threshold = Math.floor(nums.length / 2);

  for (const num of nums) {
    const newCount = (counts.get(num) || 0) + 1;
    if (newCount > threshold) {
      return num;
    }
    counts.set(num, newCount);
  }

  return null;
}
```

### Example 4: Longest Consecutive Sequence ($O(N)$ using Set)
```javascript
export function longestConsecutive(nums) {
  const numSet = new Set(nums); // O(N) insertion
  let longestStreak = 0;

  for (const num of numSet) {
    // Only start counting streak from the beginning of a sequence
    if (!numSet.has(num - 1)) {
      let currentNum = num;
      let currentStreak = 1;

      while (numSet.has(currentNum + 1)) {
        currentNum++;
        currentStreak++;
      }

      if (currentStreak > longestStreak) {
        longestStreak = currentStreak;
      }
    }
  }

  return longestStreak;
}
```

### Example 5: In-Memory LRU Cache Powered by JavaScript `Map`
```javascript
export class LRUCache {
  constructor(capacity) {
    this.capacity = capacity;
    this.map = new Map(); // Preserves insertion order
  }

  get(key) {
    if (!this.map.has(key)) return -1;

    // Refresh position to most recently used
    const val = this.map.get(key);
    this.map.delete(key);
    this.map.set(key, val);
    return val;
  }

  put(key, value) {
    if (this.map.has(key)) {
      this.map.delete(key);
    } else if (this.map.size >= this.capacity) {
      // Evict least recently used (first key in Map iterator)
      const oldestKey = this.map.keys().next().value;
      this.map.delete(oldestKey);
    }

    this.map.set(key, value);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Object Coercion of Object Keys in `{}`**:
   When using a plain JavaScript object `{}` as a hash table, keys are coerced to strings. If an object is passed as a key, it converts to `"[object Object]"`, overwriting any prior object key!
   ```javascript
   const objMap = {};
   const a = { id: 1 };
   const b = { id: 2 };
   objMap[a] = 'First';
   objMap[b] = 'Second';
   console.log(objMap[a]); // 'Second' (both keys were "[object Object]")!
   ```
   **Fix**: Always use standard ECMAScript `Map`, which preserves reference identity for object keys.
2. **Prototype Pollution via Plain Objects**:
   Using `const map = {}` exposes prototype keys (`toString`, `valueOf`, `constructor`, `__proto__`). Calling `map['toString']` returns a function, not `undefined`.
   **Fix**: Use `new Map()` or `Object.create(null)`.
3. **Worst-Case Hash Collision Degradation ($O(N)$)**:
   If an adversarial user sends crafted strings that produce identical hash codes (Hash-Flooding DoS attack), all elements land in a single bucket chain, degrading operations from $O(1)$ to $O(N)$ and stalling the event loop.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Risk | Production Fix |
| :--- | :--- | :--- |
| Using `{}` as a map with arbitrary user keys | Prototype pollution and prototype method collision (`toString`). | Use `new Map()` or `Object.create(null)`. |
| Passing objects as keys to `{}` | Key stringifies to `"[object Object]"`, corrupting mappings. | Use `new Map()`, which supports object references as keys. |
| Neglecting to rehash when capacity fills up | Collision chains grow long, degrading lookups to $O(N)$. | Rehash and double capacity when load factor exceeds $0.75$. |
| Inadequate hash functions (e.g. `key.length`) | Severe bucket clustering; thousands of keys collide in one bucket. | Use polynomial rolling hash (e.g. `djb2` or `Murmur3`). |
| Inefficient string keys in loops | Long dynamic string keys incur high hashing CPU overhead. | Use integer IDs or short interned strings as map keys. |
| Iterating over `for...in` on plain object map | Iterates inherited prototype properties. | Use `Map.prototype.forEach` or `Object.keys()`. |

---

## 8. Performance Characteristics

- **Time Complexity**:
  - Insert / Put: $O(1)$ average, $O(N)$ worst-case (during rehashing or hash-flooding collisions).
  - Search / Get: $O(1)$ average, $O(N)$ worst-case.
  - Delete: $O(1)$ average, $O(N)$ worst-case.
- **Space Complexity**:
  - $O(N)$ linear space proportional to stored elements and bucket buffer capacity.

---

## 9. Engine Internals

### V8 `OrderedHashTable` Architecture
In V8, `Map` and `Set` are implemented using **OrderedHashTable**:
- **Prefix Header**: Contains table size, bucket count, and deleted element counters.
- **Buckets Array**: Stores index offsets pointing into a contiguous data storage segment.
- **Data Segment**: Flat contiguous array storing `[Key, Value, ChainPointer]` tuples.
- **Tombstones & Deletions**: Deleting a key writes a tombstone (`TheHole`) without shifting memory ($O(1)$). When tombstone count exceeds $50\%$ of capacity, V8 triggers a compacting rehash.

```
V8 OrderedHashTable Memory Layout:
[ Header | Buckets: 0, 1, 2, 3 | Elements: K0, V0, Next0 | K1, V1, Next1 | ... ]
```

---

## 10. Best Practices & Design Guidelines

1. **Always Default to `Map` and `Set`**: Never use `{}` as a hash table in production unless writing simple static configs.
2. **Pre-Size Collections When Known**: Avoid repetitive rehashing by pre-sizing collections or batching inserts.
3. **Use Composite Hash Keys Deterministically**: When indexing by multiple values (e.g. coordinates `x, y`), use a delimiter: `${x}:${y}`.
4. **Use `WeakMap` for Ephemeral Data**: Prevent memory leaks by associating metadata with object keys via `WeakMap`.

---

## 11. Real-World Case Study

### High-Throughput URL Shortener Slug Resolver
**Scenario**: A URL redirection service handled 50,000 redirects per second. Each redirect required resolving a 6-character alphanumeric slug (e.g. `https://sho.rt/x8K2q1`) to its full destination URL.
**Problem**: Querying PostgreSQL for every redirect produced average response times of $45\text{ms}$, saturating database connections.

**Solution**:
- Populated an in-memory `Map` containing 500,000 active slugs.
- Lookups into `Map.get(slug)` completed in $0.00004\text{ms}$ ($40\text{ns}$).
- Slugs not present in memory fell back to database query and populated the cache with an LRU eviction policy.

**Outcome**:
- Database load dropped by 99.4%.
- P99 redirection latency dropped from $45\text{ms}$ to $0.8\text{ms}$.
- Handled 10x traffic spike on the same cloud compute infrastructure.

---

## 12. Interview Questions

<details>
<summary>1. How does a Hash Table achieve $O(1)$ average-case lookup time?</summary>

A Hash Table uses a hash function to compute an integer hash code from the key, which is mapped via modulo arithmetic to a specific array index (bucket). Because arrays support direct memory offset addressing ($O(1)$), jumping to the computed bucket takes constant time. As long as the load factor is low and the hash function distributes keys uniformly, each bucket contains very few elements (averaging 1–2), allowing instantaneous access.
</details>

<details>
<summary>2. What is the difference between Separate Chaining and Open Addressing?</summary>

- **Separate Chaining**: Buckets store linked lists or dynamic arrays of collided entries. Collisions are accommodated outside the primary array. Table can exceed 100% capacity ($\alpha > 1$).
- **Open Addressing**: All key-value pairs are stored directly within the single bucket array. Collisions are resolved by probing subsequent slots (linear, quadratic, or double hashing). Table capacity cannot exceed 100% ($\alpha < 1$).
</details>

<details>
<summary>3. Why is JavaScript's <code>Map</code> preferred over plain objects <code>{}</code> as a hash table?</summary>

1. **Any Key Type**: `Map` supports keys of any type (functions, objects, numbers, booleans), whereas `{}` converts all keys to strings or symbols.
2. **Zero Prototype Pollution**: `Map` has no default keys (`toString`, `valueOf`), eliminating prototype collision vulnerabilities.
3. **Insertion Order**: `Map` guarantees deterministic insertion order iteration.
4. **Direct Size Property**: `map.size` is $O(1)$, whereas counting object properties requires $O(N)$ `Object.keys(obj).length`.
5. **Memory & Performance**: `Map` is optimized specifically for frequent additions and deletions.
</details>

<details>
<summary>4. What happens when a Hash Table's Load Factor exceeds its threshold?</summary>

When $\alpha > \text{threshold}$ (typically 0.75), the table executes a **Rehash**:
1. It allocates a new bucket array of double capacity ($2 \times \text{capacity}$).
2. It iterates through every key in the old bucket array, recomputes each key's index using the new capacity ($hash \pmod{2K}$), and inserts them into the new buckets.
3. While individual rehashing takes $O(N)$ time, its amortized cost per insert is $O(1)$.
</details>

<details>
<summary>5. What is a Hash-Flooding DoS attack, and how is it mitigated?</summary>

An attacker crafts hundreds of thousands of HTTP query parameters or JSON keys that produce the exact same hash code in the server's hash table implementation. This forces all keys into a single collision bucket chain, degrading lookups from $O(1)$ to $O(N)$ and consuming 100% server CPU. Mitigations include using randomized hash seed salts (SipHash) and limiting maximum parameter counts in HTTP parsers.
</details>

<details>
<summary>6. How does V8 preserve insertion order in JavaScript's <code>Map</code>?</summary>

V8 uses an internal `OrderedHashTable` data structure. It maintains a contiguous array of elements in chronological insertion order alongside the bucket index array. When iterating over the map (`for...of` or `.forEach`), V8 traverses the sequential element entries directly, guaranteeing $O(1)$ ordered traversal without needing to sort keys.
</details>

<details>
<summary>7. What is the difference between a <code>Map</code> and a <code>WeakMap</code> in JavaScript?</summary>

- `Map` holds strong references to both its keys and values; keys can be any type; it is iterable and exposes `.size`.
- `WeakMap` holds **weak references** to its keys; keys **must be objects**; it is not iterable and does not expose `.size`. When an object key has no other references in memory, it is automatically garbage-collected and its entry is removed from the WeakMap without memory leaks.
</details>

<details>
<summary>8. How can you find if two strings are anagrams in $O(N)$ time using a hash map?</summary>

Count character frequencies: build a frequency map of the first string (incrementing counts) and traverse the second string (decrementing counts). If any character count drops below zero or the lengths differ, they are not anagrams. If all counts reach zero, they are anagrams. Time complexity is $O(N)$ and auxiliary space is $O(1)$ (bounded by alphabet size, e.g. 26 characters).
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Two Sum with $O(N)$ Time Complexity
**Specification**: Given an array of integers `nums` and an integer `target`, return indices of the two numbers such that they add up to `target` in $O(N)$ time using a `Map`.

<details>
<summary>Solution</summary>

```javascript
export function twoSum(nums, target) {
  const seen = new Map(); // value -> index

  for (let i = 0; i < nums.length; i++) {
    const complement = target - nums[i];
    if (seen.has(complement)) {
      return [seen.get(complement), i];
    }
    seen.set(nums[i], i);
  }

  return [];
}
```
</details>

### Exercise 2: First Unique Character in a String
**Specification**: Given a string `s`, find the first non-repeating character and return its index. If it does not exist, return `-1`. Must run in $O(N)$ time.

<details>
<summary>Solution</summary>

```javascript
export function firstUniqChar(s) {
  const charCounts = new Map();

  // Pass 1: Count character frequencies
  for (let i = 0; i < s.length; i++) {
    const char = s[i];
    charCounts.set(char, (charCounts.get(char) || 0) + 1);
  }

  // Pass 2: Find first character with frequency of 1
  for (let i = 0; i < s.length; i++) {
    if (charCounts.get(s[i]) === 1) {
      return i;
    }
  }

  return -1;
}
```
</details>
