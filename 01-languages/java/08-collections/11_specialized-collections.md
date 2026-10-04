# Specialized Collections

Beyond the everyday `ArrayList`/`HashMap`/`HashSet`, the JDK ships purpose-built collections that are **faster, smaller or safer** for specific situations. Knowing they exist, and when each applies, avoids reinventing them badly.

| Collection | Specialty |
|------------|-----------|
| [`EnumMap`](#enummap), [`EnumSet`](#enumset) | Enum keys/elements: array and bit-vector speed |
| [`LinkedHashMap`](#linkedhashmap-as-an-lru-cache) | Insertion/access order; LRU cache hook |
| [`IdentityHashMap`](#identityhashmap) | Keys compared by reference (`==`) |
| [`WeakHashMap`](#weakhashmap) | Keys that do not prevent garbage collection |
| [`BitSet`](#bitset) | Compact set/bit-vector of non-negative ints |
| [Concurrent and copy-on-write](#concurrent-specialists) | Thread-safe variants (summary) |
| [Third-party](#beyond-the-jdk) | Multimaps, primitive collections, caches |

## `EnumMap`

A `Map` whose keys are the constants of **one enum type**, stored in a plain array indexed by the enum's ordinal.

```java
enum Day { MON, TUE, WED, THU, FRI, SAT, SUN }

Map<Day, List<String>> schedule = new EnumMap<>(Day.class);       // the key type is passed in
schedule.computeIfAbsent(Day.MON, d -> new ArrayList<>()).add("standup");
schedule.put(Day.FRI, List.of("review"));

System.out.println(schedule);        // {MON=[standup], FRI=[review]}: always in declaration (ordinal) order
```

| Property | Detail |
|----------|--------|
| Operations | `get`, `put`, `containsKey` are **O(1)** array accesses |
| Memory | Very compact (one slot per enum constant) |
| Iteration order | **Enum declaration order**, always |
| `null` | `null` keys **not** allowed; `null` values allowed |
| Thread-safety | None |
| Better than `HashMap<Day, ...>`? | Yes: faster, smaller, ordered, type-safe |

Use it whenever keys are enum constants: counts per status, handlers per command, config per environment ([04-oop/11_enums.md](../04-oop/11_enums.md)).

## `EnumSet`

A `Set` of enum constants stored as a **bit vector** (one `long` for enums with ≤ 64 constants). Operations are a handful of bit instructions.

```java
Set<Day> weekend  = EnumSet.of(Day.SAT, Day.SUN);
Set<Day> weekdays = EnumSet.complementOf(EnumSet.of(Day.SAT, Day.SUN));
Set<Day> all      = EnumSet.allOf(Day.class);
Set<Day> none     = EnumSet.noneOf(Day.class);
Set<Day> midweek  = EnumSet.range(Day.TUE, Day.THU);
Set<Day> copy     = EnumSet.copyOf(someCollectionOfDays);

weekend.contains(Day.SAT);           // true
weekdays.add(Day.SAT);               // like any Set
```

| Property | Detail |
|----------|--------|
| Factories | `of`, `noneOf`, `allOf`, `range`, `complementOf`, `copyOf` (no public constructor) |
| Speed | Faster than `HashSet` for enums (bit operations) |
| Order | Declaration order |
| `null` | Not allowed |

Typical use: **flags and permissions** (`EnumSet<Permission>`) instead of integer bitmasks.

```java
enum Permission { READ, WRITE, DELETE }
Set<Permission> editor = EnumSet.of(Permission.READ, Permission.WRITE);
boolean canDelete = editor.contains(Permission.DELETE);          // readable and type-safe
```

## `LinkedHashMap` as an LRU cache

A `LinkedHashMap` constructed in **access-order** mode moves each entry to the end whenever it is read or written, so the **eldest** entry is the **least recently used**. Overriding `removeEldestEntry` evicts it automatically.

```java
public class LruCache<K, V> extends LinkedHashMap<K, V> {
    private final int capacity;

    public LruCache(int capacity) {
        super(16, 0.75f, true);              // true = access order
        this.capacity = capacity;
    }

    @Override
    protected boolean removeEldestEntry(Map.Entry<K, V> eldest) {
        return size() > capacity;            // called after each put: evict when over capacity
    }
}

LruCache<String, String> cache = new LruCache<>(2);
cache.put("a", "1");
cache.put("b", "2");
cache.get("a");                              // "a" is now the most recent
cache.put("c", "3");                         // evicts "b", the least recently used
cache.keySet();                              // [a, c]
```

| Caveat | Detail |
|--------|--------|
| **Not thread-safe** | In access-order mode even `get` mutates the structure; wrap with `Collections.synchronizedMap(...)` or guard with a lock |
| Size-only eviction | No expiry times, statistics, weak references or async loading |
| Inheritance exposes the whole `Map` API | A wrapper class (composition) gives a smaller API ([04-oop/10](../04-oop/10_composition-and-object-relationships.md)) |

For production caching (expiry, concurrency, statistics) use a library such as **Caffeine**: [25-real-world-patterns/00_caching.md](../25-real-world-patterns/00_caching.md). The LRU class is a classic interview answer and fine for small, single-threaded needs.

Insertion-order mode (the default) is also the quickest way to keep a **deterministic iteration order**, for example for stable JSON output or reproducible tests ([08](./08_hashmap-and-hashset.md)).

## `IdentityHashMap`

A map that compares keys with **`==`** (reference identity) instead of `equals`, and hashes them with `System.identityHashCode`.

```java
Map<String, Integer> id = new IdentityHashMap<>();
String a = new String("x");
String b = new String("x");                 // equal content, different objects
id.put(a, 1);
id.put(b, 2);
id.size();                                  // 2: two distinct keys (a HashMap would have 1)
```

It deliberately **violates the general `Map` contract**. Use it when object identity is the point:

- Graph or object-graph traversal that must visit each **object** once, even if some are `equals`
- Serialization / cloning frameworks tracking already-seen instances
- Proxies and type-keyed registries where keys are distinct instances

For identity-based **sets**: `Collections.newSetFromMap(new IdentityHashMap<>())`.

## `WeakHashMap`

Keys are held by **weak references**: when nothing else strongly references a key, the garbage collector may reclaim it and the entry **disappears** from the map.

```java
Map<Object, String> meta = new WeakHashMap<>();
Object key = new Object();
meta.put(key, "metadata");
key = null;                                  // no strong reference left
System.gc();                                 // (a hint) later: meta may become empty
```

| Facts | |
|-------|--|
| Purpose | Attach **extra data to objects you do not own**, without keeping them alive |
| Entries vanish at unpredictable times (GC) | Never rely on an entry being present or absent |
| **Values** are held strongly | If a value references its own key, the entry is never collected (a classic leak) |
| Keys compared with `equals` | Literals and interned strings are never collected |
| As a general cache | Usually the **wrong** tool: entries disappear when the key becomes unreachable, not when memory is tight or after a timeout |

Prefer Caffeine (with `weakKeys()` if needed) for caches; use `WeakHashMap` for canonicalizing mappings and listener/metadata registries. See [15-jvm-internals/04_garbage-collection.md](../15-jvm-internals/04_garbage-collection.md) for reference types.

## `BitSet`

A compact set of **non-negative integers**, one bit per possible value: 1 million flags take about 125 KB instead of megabytes for `Set<Integer>`.

```java
BitSet seen = new BitSet(100);
seen.set(5);
seen.set(10, 20);                            // set bits 10..19
seen.get(5);                                 // true
seen.clear(5);
seen.cardinality();                          // number of set bits
seen.nextSetBit(0);                          // iterate: for (int i = bs.nextSetBit(0); i >= 0; i = bs.nextSetBit(i + 1))

BitSet a = ..., b = ...;
a.and(b);  a.or(b);  a.xor(b);  a.andNot(b); // set operations on bits

// Sieve of Eratosthenes
BitSet composite = new BitSet(n + 1);
for (int i = 2; (long) i * i <= n; i++)
    if (!composite.get(i)) for (int j = i * i; j <= n; j += i) composite.set(j);
```

Use for dense integer ids, visited flags, sieves, bitmaps; not a general `Collection`. It is **not** thread-safe.

## Concurrent specialists

Summarized here; see [14-concurrency/08_concurrent-collections.md](../14-concurrency/08_concurrent-collections.md).

| Collection | When |
|------------|------|
| `ConcurrentHashMap` | The default thread-safe map; atomic `compute`/`merge` |
| `CopyOnWriteArrayList` / `CopyOnWriteArraySet` | Many reads, very few writes (listener lists); every write copies the array |
| `ConcurrentSkipListMap` / `ConcurrentSkipListSet` | Thread-safe sorted map/set |
| `ConcurrentLinkedQueue` / `ConcurrentLinkedDeque` | Non-blocking queues |
| `BlockingQueue` implementations (`ArrayBlockingQueue`, `LinkedBlockingQueue`, `PriorityBlockingQueue`, `DelayQueue`, `SynchronousQueue`) | Producer-consumer hand-off |

Avoid `Collections.synchronizedXxx` wrappers for anything but simple cases: compound actions (check-then-act) still need external locking.

## Other useful JDK pieces

| Class | Use |
|-------|-----|
| `Collections.emptyList()`, `emptyMap()`, `singletonList(x)`, `nCopies(n, x)` | Cheap immutable special cases ([14](./14_collections-and-arrays-utilities.md)) |
| `Collections.newSetFromMap(map)` | A `Set` backed by any `Map` (concurrent, identity, weak sets) |
| `AbstractMap.SimpleEntry` / `Map.entry(k, v)` | Standalone key-value pairs |
| `Properties` | String-to-String config (`.properties` files); legacy `Hashtable` base |
| `java.util.StringJoiner` | Delimited string building |
| `Spliterator`, `Iterable` | Custom traversal sources ([12](./12_iterators-and-fail-fast-behavior.md)) |

## Beyond the JDK

| Need | Library |
|------|---------|
| **Multimap** (`Map<K, List<V>>` without boilerplate), `BiMap`, `Table`, immutable collections, `Multiset` | Guava (`Multimap`, `BiMap`, `ImmutableList`) |
| **Primitive collections** (`IntList`, `LongIntMap`) with no boxing | Eclipse Collections, fastutil, HPPC |
| **Caching** with expiry, size and statistics | Caffeine |
| **Persistent / immutable data structures** | Vavr, Eclipse Collections |
| Concurrent data structures beyond the JDK | JCTools |

Add dependencies only when the JDK genuinely falls short; see [18-build-and-dependencies/02_dependency-management.md](../18-build-and-dependencies/02_dependency-management.md).

## Choosing

| Situation | Choose |
|-----------|--------|
| Keys are enum constants | `EnumMap` |
| A set of enum constants / flags | `EnumSet` |
| Insertion order matters, or a simple LRU | `LinkedHashMap` / `LinkedHashSet` |
| Identity (not equality) of keys matters | `IdentityHashMap` |
| Attach data to objects without leaking them | `WeakHashMap` (carefully) |
| Dense set of small non-negative ints | `BitSet` |
| Concurrent access | `ConcurrentHashMap` and friends |
| Production cache | Caffeine |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `HashMap<MyEnum, V>` / `HashSet<MyEnum>` | Slower, unordered | `EnumMap` / `EnumSet` |
| Forgetting the `Class` argument: `new EnumMap<>()` | Does not compile | `new EnumMap<>(Day.class)` |
| Putting `null` keys into `EnumMap`/`EnumSet` | `NullPointerException` | Avoid |
| Using an LRU `LinkedHashMap` from several threads | Corruption, lost entries | Synchronize, or use Caffeine |
| `LinkedHashMap` access-order but the eldest never evicts | Forgot `removeEldestEntry` | Override it |
| Expecting `IdentityHashMap` to behave like `HashMap` | Equal keys stored twice | Use only when identity is wanted |
| Value references its own key in a `WeakHashMap` | Entries never collected | Use weak or soft values, or a different design |
| Using `WeakHashMap` as a general cache | Unpredictable disappearance | Caffeine |
| `BitSet` with huge max indices | Large memory | Use a `HashSet`/`RoaringBitmap` for sparse sets |
| Relying on `Collections.synchronizedMap` for compound operations | Race conditions | `ConcurrentHashMap.merge`/`compute` |

## Key takeaways

- Enum keys and sets: `EnumMap`/`EnumSet` are simpler, smaller, and faster than hash-based versions
- `LinkedHashMap` in access-order mode plus `removeEldestEntry` makes a quick LRU cache (single-threaded)
- `IdentityHashMap` and `WeakHashMap` solve narrow problems; know them, but use them rarely
- `BitSet` stores dense integer sets in one bit each
- Reach for concurrent collections or a library (Guava, Caffeine, primitive collections) when the JDK basics are not enough

**Next:** [Iterators and Fail-Fast Behavior](./12_iterators-and-fail-fast-behavior.md)
