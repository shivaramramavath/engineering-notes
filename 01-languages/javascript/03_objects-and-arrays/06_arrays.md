# Arrays

An array is an **ordered, zero-indexed list** of values. Technically it is an object with numeric keys and a special `length`.

```
index:   0     1     2
value:  "a"   "b"   "c"       length = 3
```

## Creating arrays

```js
const a = [1, 2, 3];
const empty = [];
const mixed = [1, "two", { three: 3 }, [4]];      // any types

Array.of(7);                          // [7]
Array.from("abc");                    // ["a", "b", "c"]
Array.from({ length: 3 }, (_, i) => i * 2);   // [0, 2, 4]
new Array(3);                         // [empty × 3]  (sparse, avoid)
new Array(3).fill(0);                 // [0, 0, 0]
[...Array(3).keys()];                 // [0, 1, 2]
```

Careful: `new Array(3)` makes an empty array of length 3, but `new Array(3, 4)` makes `[3, 4]`. Use `Array.of` or `Array.from`.

## Access and length

```js
const list = ["a", "b", "c"];

list[0];              // "a"
list[list.length - 1];// "c"
list.at(-1);          // "c" (ES2022, negative index)
list[10];             // undefined

list.length;          // 3
list.length = 1;      // truncates to ["a"]
list[5] = "x";        // creates holes: ["a", empty × 4, "x"]
```

## Mutating methods (change the original)

| Method | Effect |
|--------|--------|
| `push(...items)` | add to end, returns new length |
| `pop()` | remove from end, returns item |
| `unshift(...items)` | add to start |
| `shift()` | remove from start |
| `splice(start, deleteCount, ...items)` | insert/remove anywhere |
| `sort()`, `reverse()` | reorder in place |
| `fill(value, start?, end?)` | overwrite range |
| `copyWithin(...)` | copy within the same array |

```js
const nums = [1, 2, 3, 4, 5];
nums.splice(1, 2);          // removes [2, 3] → nums = [1, 4, 5]
nums.splice(1, 0, "a");     // insert → [1, "a", 4, 5]
```

## Non-mutating alternatives (ES2023)

| Mutating | Copying version |
|----------|-----------------|
| `sort()` | `toSorted()` |
| `reverse()` | `toReversed()` |
| `splice()` | `toSpliced()` |
| `arr[i] = v` | `with(i, v)` |

## Checking and converting

```js
Array.isArray([]);          // true (typeof [] is "object")
[1, 2, 3].join("-");        // "1-2-3"
"a,b,c".split(",");         // ["a", "b", "c"]
Array.from(new Set([1, 1, 2]));   // [1, 2]
[...new Map([[1, "a"]])];   // [[1, "a"]]
String([1, [2, 3]]);        // "1,2,3"
```

## Holes (sparse arrays)

```js
const sparse = [1, , 3];
sparse.length;        // 3
1 in sparse;          // false
sparse.map(x => x * 2);   // [2, empty, 6] (holes skipped)
```

Avoid holes. Use `undefined` or `null` explicitly.

## Iterating

```js
for (const item of list) { ... }               // values
for (const [i, item] of list.entries()) { ... }// index + value
list.forEach((item, i) => { ... });
for (let i = 0; i < list.length; i++) { ... }  // when you need control
```

`for...in` on arrays gives string indexes and inherited keys, so avoid it.

## Multi-dimensional arrays

```js
const grid = [
  [1, 2, 3],
  [4, 5, 6],
];
grid[1][2];        // 6

const board = Array.from({ length: 3 }, () => Array(3).fill(0));
// NOT Array(3).fill(Array(3).fill(0)) which shares one row 3 times
```

## Array-likes and typed arrays

`arguments`, `NodeList`, strings are **array-like**. Convert with `Array.from(x)` or `[...x]`.
Typed arrays (`Uint8Array`, `Float64Array`) are covered in `09_built-in-objects/10_typed-arrays-and-arraybuffer.md`.

## Performance notes

| Operation | Cost |
|-----------|------|
| index read/write, `push`, `pop` | O(1) |
| `shift`, `unshift`, `splice` in the middle | O(n) |
| `includes`, `indexOf` | O(n) |
| `Set` / `Map` lookup | O(1) average |

For frequent membership checks, use a `Set`.

## Array pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `typeof arr === "object"` | Cannot detect arrays | `Array.isArray` |
| `new Array(n)` | Sparse array, quirky | `Array.from({ length: n })` |
| `fill` with an object | Same reference in every slot | `Array.from({ length: n }, () => ({}))` |
| Modifying while iterating | Skipped or repeated items | Iterate a copy or use `filter` |
| `delete arr[i]` | Leaves a hole | `splice` |
| `arr.length = 0` on shared arrays | Clears for everyone | Create a new array |
| `arr1 == arr2` | Reference comparison | Compare elements |

## Key takeaways

- Arrays are ordered objects; use `Array.isArray` to detect them
- Know which methods mutate and their copying counterparts
- Avoid sparse arrays and `fill` with objects
- Use `Set` for fast membership tests

**Next:** [Array Methods](./07_array-methods.md)
