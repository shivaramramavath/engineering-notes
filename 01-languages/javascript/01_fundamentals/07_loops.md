# Loops

Loops repeat code. JavaScript offers classic loops, iterable-aware loops, and array methods.

## `for`

```js
for (let i = 0; i < 5; i++) {
  console.log(i);
}
```

Three parts: **init**; **condition** (checked before each pass); **update** (after each pass). Every part is optional: `for (;;) {}` is an infinite loop.

## `while` and `do...while`

```js
let n = 3;
while (n > 0) {
  n--;
}

do {
  attempt(); // runs at least once
} while (!success());
```

## `for...of`: values of an iterable

Works with arrays, strings, `Map`, `Set`, generators, `NodeList`.

```js
for (const item of ["a", "b"]) console.log(item);
for (const ch of "hey") console.log(ch);

for (const [i, item] of ["a", "b"].entries()) console.log(i, item);
for (const [key, value] of new Map([["x", 1]])) console.log(key, value);
```

## `for...in`: enumerable property **keys**

```js
const user = { name: "Ada", age: 36 };
for (const key in user) console.log(key, user[key]);
```

It also walks **inherited** enumerable properties and returns array indexes as **strings**. Prefer these:

```js
Object.keys(user); // ["name", "age"]
Object.values(user); // ["Ada", 36]
Object.entries(user); // [["name", "Ada"], ["age", 36]]

for (const [k, v] of Object.entries(user)) console.log(k, v);
```

## `break`, `continue`, labels

```js
for (const n of [1, 2, 3, 4]) {
  if (n === 2) continue; // skip this pass
  if (n === 4) break; // leave the loop
  console.log(n); // 1, 3
}

outer: for (const row of grid) {
  for (const cell of row) {
    if (cell === target) break outer; // exit both loops
  }
}
```

## Array iteration methods

| Method               | Purpose                  | Can `break`? |
| -------------------- | ------------------------ | ------------ |
| `forEach`            | run code per item        | No           |
| `map`                | transform to a new array | No           |
| `filter`             | keep matching items      | No           |
| `reduce`             | fold into one value      | No           |
| `some` / `every`     | test with early exit     | Stops early  |
| `find` / `findIndex` | first match              | Stops early  |

Details in `03_objects-and-arrays/07_array-methods.md`.

## Loops and `async`

```js
// Sequential: waits for each
for (const id of ids) {
  await save(id);
}

// forEach ignores returned promises, so this does NOT wait
ids.forEach(async (id) => {
  await save(id);
});

// Parallel
await Promise.all(ids.map((id) => save(id)));
```

## Closures in loops

```js
for (let i = 0; i < 3; i++) setTimeout(() => console.log(i)); // 0 1 2
for (var j = 0; j < 3; j++) setTimeout(() => console.log(j)); // 3 3 3
```

## Choosing a loop

| Need                                      | Use                           |
| ----------------------------------------- | ----------------------------- |
| Count, or need the index                  | `for`                         |
| Every value of an array, string, Set, Map | `for...of`                    |
| Keys and values of a plain object         | `Object.entries` + `for...of` |
| Unknown number of passes                  | `while`                       |
| Run at least once                         | `do...while`                  |
| Transform data                            | `map` / `filter` / `reduce`   |
| Await each item in order                  | `for...of` with `await`       |

## Loop pitfalls

| Pitfall                            | Why it hurts                   | Better                                |
| ---------------------------------- | ------------------------------ | ------------------------------------- |
| Infinite loop (missing update)     | Freezes the tab or process     | Check the exit condition              |
| `for...in` on arrays               | String indexes, inherited keys | `for...of`                            |
| `async` callback in `forEach`      | Not awaited                    | `for...of` or `Promise.all`           |
| Changing an array while looping it | Skipped items                  | Loop a copy or use `filter`           |
| `var` in loops with callbacks      | Shared binding                 | `let`                                 |
| `.length` recomputed in a hot loop | Tiny cost, more noise          | Fine in modern engines, keep readable |

## Key takeaways

- `for...of` for values, `for...in` only for object keys (and rarely)
- `break` and `continue` work in loops, not in `forEach`
- Use `for...of` with `await` for sequential async work
- `let` gives each iteration its own binding

**Next:** [Functions](../02_functions/00_README.md)
