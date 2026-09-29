# Recursion

A **recursive** function calls itself to solve smaller versions of the same problem.

```
solve(n)  =  base case                (stop)
          |  combine( solve(smaller) ) (recurse)
```

## The two required parts

1. **Base case**: when to stop
2. **Recursive case**: move toward the base case

```js
function factorial(n) {
  if (n <= 1) return 1; // base case
  return n * factorial(n - 1); // recursive case
}
factorial(5); // 120
```

Trace:

```
factorial(3)
 └ 3 * factorial(2)
        └ 2 * factorial(1)
               └ 1
```

## The call stack

Each call adds a **stack frame**. Too many frames cause an error.

```js
function forever() {
  return forever();
}
forever(); // RangeError: Maximum call stack size exceeded
```

Typical limit: roughly 10,000 frames (varies by engine and frame size). More in `04_scope-and-execution/03_execution-context-and-call-stack.md`.

## Classic examples

```js
// Sum an array
const sum = ([head, ...tail]) => (head === undefined ? 0 : head + sum(tail));

// Fibonacci (slow: exponential time)
const fib = (n) => (n < 2 ? n : fib(n - 1) + fib(n - 2));

// Flatten nested arrays
const flatten = (arr) =>
  arr.reduce((acc, x) => acc.concat(Array.isArray(x) ? flatten(x) : x), []);

// Deep object walk
function walk(node, visit) {
  visit(node);
  for (const child of node.children ?? []) walk(child, visit);
}
```

## Memoization

Cache results to avoid repeated work.

```js
function memoize(fn) {
  const cache = new Map();
  return (n) => {
    if (cache.has(n)) return cache.get(n);
    const value = fn(n);
    cache.set(n, value);
    return value;
  };
}

const fib = memoize((n) => (n < 2 ? n : fib(n - 1) + fib(n - 2)));
fib(50); // fast: O(n)
```

## Converting recursion to iteration

```js
function factorialLoop(n) {
  let result = 1;
  for (let i = 2; i <= n; i++) result *= i;
  return result;
}
```

For trees or graphs use an explicit stack:

```js
function walkIterative(root, visit) {
  const stack = [root];
  while (stack.length) {
    const node = stack.pop();
    visit(node);
    stack.push(...(node.children ?? []));
  }
}
```

## Tail calls and trampolines

A **tail call** is the last action of a function. The spec includes tail call optimization, but only Safari (JavaScriptCore) implements it, so do not rely on it.

A **trampoline** avoids stack growth:

```js
const trampoline =
  (fn) =>
  (...args) => {
    let result = fn(...args);
    while (typeof result === "function") result = result();
    return result;
  };

const sumTo = trampoline(function go(n, acc = 0) {
  return n === 0 ? acc : () => go(n - 1, acc + n);
});
sumTo(100000); // no stack overflow
```

## Recursion vs iteration

|             | Recursion                                            | Iteration                              |
| ----------- | ---------------------------------------------------- | -------------------------------------- |
| Best for    | trees, nested data, divide and conquer, backtracking | flat sequences, counters, large depths |
| Readability | clear for recursive structures                       | clear for simple repetition            |
| Memory      | one frame per call                                   | constant                               |
| Risk        | stack overflow                                       | infinite loop                          |

## Common recursive algorithms

| Problem                 | Idea                                         |
| ----------------------- | -------------------------------------------- |
| Tree traversal          | visit node, recurse on children              |
| Binary search           | recurse on the half that can hold the target |
| Merge sort / quicksort  | split, solve halves, combine                 |
| Permutations / subsets  | choose, recurse, undo (backtracking)         |
| Deep clone / deep equal | recurse into nested objects                  |

## Recursion pitfalls

| Pitfall                              | Why it hurts       | Better                                |
| ------------------------------------ | ------------------ | ------------------------------------- |
| Missing or unreachable base case     | Stack overflow     | Test the smallest input first         |
| Not moving toward the base case      | Infinite recursion | Shrink the input each step            |
| Repeated subproblems (`fib`)         | Exponential time   | Memoization or iteration              |
| Very deep recursion                  | Stack overflow     | Iteration, explicit stack, trampoline |
| Copying arrays each call (`...tail`) | O(n²) work         | Pass an index                         |
| Assuming tail call optimization      | Only some engines  | Do not rely on it                     |

## Key takeaways

- Every recursive function needs a base case and progress toward it
- The call stack limits depth; deep problems need loops or trampolines
- Memoize overlapping subproblems
- Recursion shines on trees and nested data

**Next:** [Objects and Arrays](../03_objects-and-arrays/00_README.md)
