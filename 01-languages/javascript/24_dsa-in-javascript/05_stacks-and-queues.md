# Stacks, Queues & Monotonic Patterns in JavaScript

> A production-grade algorithmic guide to Stacks, Queues, Deques, and Monotonic structures in JavaScript, featuring $O(1)$ queue implementations, expression parsing, and sliding window maximums.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [04_linked-lists.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/04_linked-lists.md) |
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

Linear collections often require specialized insertion and extraction rules to enforce access ordering. Two fundamental abstract data types govern these access rules:
- **Stack (LIFO - Last-In, First-Out)**: The most recently added element is the first to be removed. Modeled after a stack of plates. Essential for call stacks, undo/redo buffers, syntax parsing, and backtracking algorithms.
- **Queue (FIFO - First-In, First-Out)**: Elements are removed in the exact order they were inserted. Modeled after a checkout line. Essential for event loop task queues, breadth-first search (BFS), request rate limiting, and printer spooling.

A common performance hazard in JavaScript is using `array.shift()` to implement a queue. Because `shift()` shifts every remaining element in memory, each dequeue takes **$O(N)$ linear time**, degrading algorithms into $O(N^2)$ bottlenecks. Production queues must be implemented using linked nodes or ring buffers to achieve true **$O(1)$ constant time** dequeuing.

---

## 2. Core Concepts

### Stack Operations ($O(1)$)
- `push(val)`: Adds an element to the top of the stack.
- `pop()`: Removes and returns the top element.
- `peek()`: Inspects the top element without removing it.
- `isEmpty()`: Checks if the stack has zero elements.

### Queue Operations ($O(1)$)
- `enqueue(val)`: Adds an element to the back (tail) of the queue.
- `dequeue()`: Removes and returns the element at the front (head).
- `peek()`: Inspects the front element.
- `isEmpty()`: Checks if the queue has zero elements.

### Advanced Variants
1. **Deque (Double-Ended Queue)**: Supports insertion and removal at both ends ($O(1)$ at head and tail).
2. **Monotonic Stack / Queue**: Maintains elements in strictly increasing or decreasing order. Elements that violate monotonicity are popped before insertion. Unlocks optimal $O(N)$ solutions for range queries and Next Greater Element problems.

```
Stack (LIFO):                   Queue (FIFO):
  [ Top: 30 ]  ◄── Push / Pop      [ Enqueue ] ──► [ 30 | 20 | 10 ] ──► [ Dequeue: 10 ]
  [      20 ]                                         (Back)     (Front)
  [ Base:10 ]
```

---

## 3. How It Works Internally

### The $O(1)$ Queue via Linked List
To eliminate the $O(N)$ array shift penalty:
- Maintain `head` and `tail` pointers.
- **Enqueue**: Attach new node to `tail.next`, advance `tail` ($O(1)$).
- **Dequeue**: Read `head.val`, advance `head = head.next` ($O(1)$).

```
Enqueue (Tail):
[ Head: 10 ] ──► [ Node: 20 ] ──► [ Tail: 30 ] ──► [ NewNode: 40 ] (New Tail)

Dequeue (Head):
(Removed 10) ◄── [ Head: 20 ] ──► [ Tail: 30 ]
```

---

## 4. Syntax & API Reference

### Production-Grade $O(1)$ Queue Implementation
```javascript
class QueueNode {
  constructor(val) {
    this.val = val;
    this.next = null;
  }
}

/**
 * Production FIFO Queue guaranteeing O(1) enqueue and dequeue operations.
 */
export class FastQueue {
  constructor() {
    this.head = null;
    this.tail = null;
    this.size = 0;
  }

  enqueue(val) {
    const node = new QueueNode(val);
    if (!this.head) {
      this.head = node;
      this.tail = node;
    } else {
      this.tail.next = node;
      this.tail = node;
    }
    this.size++;
  }

  dequeue() {
    if (!this.head) return undefined;
    const value = this.head.val;
    this.head = this.head.next;
    this.size--;
    if (this.size === 0) {
      this.tail = null;
    }
    return value;
  }

  peek() {
    return this.head ? this.head.val : undefined;
  }

  isEmpty() {
    return this.size === 0;
  }
}
```

---

## 5. Practical Examples

### Example 1: Valid Parentheses Matching using a Stack
```javascript
export function isValidParentheses(s) {
  const stack = [];
  const bracketMap = {
    ')': '(',
    '}': '{',
    ']': '[',
  };

  for (let i = 0; i < s.length; i++) {
    const char = s[i];

    if (char === '(' || char === '{' || char === '[') {
      stack.push(char);
    } else if (bracketMap[char]) {
      // Must match most recent opening bracket
      if (stack.length === 0 || stack.pop() !== bracketMap[char]) {
        return false;
      }
    }
  }

  return stack.length === 0;
}
```

### Example 2: Next Greater Element (Monotonic Decreasing Stack)
```javascript
/**
 * For each element in nums, finds the next greater element to its right.
 * Time Complexity: O(N) | Space Complexity: O(N)
 */
export function nextGreaterElement(nums) {
  const result = new Array(nums.length).fill(-1);
  const stack = []; // Stores indices of elements

  for (let i = 0; i < nums.length; i++) {
    const current = nums[i];

    // Maintain monotonic decreasing stack
    while (stack.length > 0 && nums[stack[stack.length - 1]] < current) {
      const prevIndex = stack.pop();
      result[prevIndex] = current;
    }

    stack.push(i);
  }

  return result;
}

// console.log(nextGreaterElement([2, 1, 2, 4, 3]));
// Output: [4, 2, 4, -1, -1]
```

### Example 3: Sliding Window Maximum using Monotonic Deque ($O(N)$ Time)
```javascript
/**
 * Returns maximum value in each sliding window of size k.
 * Time Complexity: O(N) - Each index pushed and popped at most once.
 */
export function maxSlidingWindow(nums, k) {
  const result = [];
  const deque = []; // Stores indices, elements ordered decreasingly by value

  for (let i = 0; i < nums.length; i++) {
    // 1. Remove indices that fall outside the current window
    if (deque.length > 0 && deque[0] < i - k + 1) {
      deque.shift();
    }

    // 2. Maintain decreasing order: remove smaller elements from back
    while (deque.length > 0 && nums[deque[deque.length - 1]] < nums[i]) {
      deque.pop();
    }

    deque.push(i);

    // 3. Append maximum (at deque front) once window reaches size k
    if (i >= k - 1) {
      result.push(nums[deque[0]]);
    }
  }

  return result;
}
```

### Example 4: Evaluate Reverse Polish Notation (Postfix Evaluation)
```javascript
export function evalRPN(tokens) {
  const stack = [];

  for (const token of tokens) {
    if (token === '+' || token === '-' || token === '*' || token === '/') {
      const b = stack.pop();
      const a = stack.pop();

      switch (token) {
        case '+': stack.push(a + b); break;
        case '-': stack.push(a - b); break;
        case '*': stack.push(a * b); break;
        case '/': stack.push(Math.trunc(a / b)); break; // Truncate toward zero
      }
    } else {
      stack.push(Number(token));
    }
  }

  return stack.pop();
}

// console.log(evalRPN(["2", "1", "+", "3", "*"])); // (2 + 1) * 3 = 9
```

### Example 5: Implement Queue Using Two Stacks (Amortized $O(1)$)
```javascript
export class MyQueue {
  constructor() {
    this.inStack = [];
    this.outStack = [];
  }

  push(x) {
    this.inStack.push(x);
  }

  pop() {
    this._shiftStacks();
    return this.outStack.pop();
  }

  peek() {
    this._shiftStacks();
    return this.outStack[this.outStack.length - 1];
  }

  empty() {
    return this.inStack.length === 0 && this.outStack.length === 0;
  }

  _shiftStacks() {
    if (this.outStack.length === 0) {
      while (this.inStack.length > 0) {
        this.outStack.push(this.inStack.pop());
      }
    }
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **The `array.shift()` Queue Trap**:
   In simple scripts, developers write `queue.push(item)` and `queue.shift()`. In loops processing 100,000 tasks, `shift()` takes hundreds of milliseconds because V8 must re-index the array in memory on every dequeue. Always use a linked-list queue for large collections.
2. **Stack Underflow on Empty Collections**:
   Calling `pop()` or `peek()` on an empty stack returns `undefined`. If your data can legitimately store `undefined`, checking `if (stack.pop() === undefined)` will falsely trigger empty-collection logic. Always check `isEmpty()` or `.length === 0` explicitly.
3. **Monotonic Stack Strictness (Equal Elements)**:
   Carefully verify whether a problem requires strict inequality (`<`) or non-strict (`<=`). In the Next Greater Element problem, using `<=` skips duplicate adjacent maximums.
4. **Integer Division in RPN (`Math.trunc` vs `Math.floor`)**:
   In JavaScript, `Math.floor(-6 / 132)` returns `-1`, whereas integer division in C/Java truncates toward zero (`0`). For evaluation problems (like LeetCode 150), always use `Math.trunc()` to avoid off-by-one negative division errors.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Hazard | Production Fix |
| :--- | :--- | :--- |
| `array.shift()` as an algorithmic queue | Degrades $O(N)$ operations into **$O(N^2)$** quadratic bottleneck. | Use a Linked List Queue or ring buffer with $O(1)$ dequeue. |
| Using recursion where stack depth exceeds 10,000 | Throws `RangeError: Maximum call stack size exceeded`. | Convert recursive traversal into an explicit array Stack. |
| Neglecting stack underflow check before peek | Causes silent bugs or unexpected `undefined` evaluations. | Guard with `if (stack.length > 0)` or `!queue.isEmpty()`. |
| `Math.floor` instead of `Math.trunc` on division | Corrupts negative division results during expression parsing. | Use `Math.trunc(a / b)` for integer truncation toward zero. |
| Mutating queue while iterating breadth-first | Index shifts cause missed nodes or infinite loops. | Use `queue.dequeue()` in a `while (!queue.isEmpty())` loop. |
| Retaining unbounded items in queues | Causes V8 memory bloat and eventual out-of-memory crashes. | Implement queue backpressure or eviction limits. |

---

## 8. Performance Characteristics

- **Stack Operations (`push` / `pop`)**:
  - JavaScript arrays naturally function as high-performance stacks: `arr.push()` and `arr.pop()` operate in **$O(1)$ amortized** time at native C++ speeds.
- **Queue Operations**:
  - Array-based queue (`push` + `shift`): Enqueue $O(1)$, Dequeue **$O(N)$**.
  - Linked-list queue: Enqueue **$O(1)$**, Dequeue **$O(1)$**, Space $O(N)$.
- **Monotonic Stack Time Complexity**:
  - Even though it contains nested loops, every element is pushed onto the stack at most once and popped at most once, guaranteeing **$O(N)$** aggregate time.

---

## 9. Engine Internals

### V8 Call Stack vs. Macro/Microtask Queues
- **The Execution Stack**: V8 maintains an internal C++ stack representing the execution context hierarchy. Local primitive variables and return addresses reside directly in these frames.
- **Microtask Queue (`Promise.then`, `queueMicrotask`)**: Implemented as a FIFO queue. V8 drains this queue completely before returning control to the browser rendering engine or libuv event loop.
- **Macrotask Queue (`setTimeout`, I/O events)**: Managed by libuv/Blink. One task is processed per event loop iteration, followed by a complete drain of the microtask queue.

---

## 10. Best Practices & Design Guidelines

1. **Use Array for Stacks, Linked List for Queues**: Standard JavaScript arrays are already optimal stacks; use `FastQueue` for FIFO queues.
2. **Apply Monotonic Stacks to "Next Greater/Smaller" Problems**: Whenever a problem asks for the nearest greater or smaller element, a monotonic stack provides the optimal $O(N)$ solution.
3. **Use Level-by-Level BFS for Shortest Path**: In unweighted graphs and trees, use a FIFO queue to guarantee finding the shortest path first.
4. **Guard Postfix Evaluations**: Validate that the stack contains at least two elements before applying any binary operator.

---

## 11. Real-World Case Study

### Browser History Navigation (Dual-Stack Architecture)
**Scenario**: Modern web browsers implement the "Back" and "Forward" navigation buttons.
**Architecture**:
- Maintained using two stacks: `backStack` and `forwardStack`.
- When user navigates to a new URL:
  1. Push current page to `backStack`.
  2. Clear `forwardStack`.
  3. Load new page.
- When user clicks "Back":
  1. Pop destination URL from `backStack`.
  2. Push current page to `forwardStack`.
  3. Navigate to destination.
- When user clicks "Forward":
  1. Pop destination URL from `forwardStack`.
  2. Push current page to `backStack`.
  3. Navigate to destination.

**Impact**:
- All navigation transitions execute in deterministic $O(1)$ constant time.
- Memory is strictly bounded to the maximum configured history length.

---

## 12. Interview Questions

<details>
<summary>1. Why is <code>array.shift()</code> a performance anti-pattern for implementing a Queue in JavaScript?</summary>

In JavaScript, arrays are backed by contiguous blocks of memory. While `array.push()` and `array.pop()` operate at the end of the array in $O(1)$ amortized time, `array.shift()` removes the element at index 0. To maintain contiguous indexing, the engine must iterate through every remaining element in memory and shift it left by one index. In a queue handling $N$ items, calling `shift()` on each element degrades total processing time to **$O(N^2)$**. A linked-list queue updates only pointer references, achieving true **$O(1)$** dequeue.
</details>

<details>
<summary>2. What is a Monotonic Stack, and what class of algorithmic problems does it solve?</summary>

A Monotonic Stack is a stack whose elements are maintained in strictly increasing or strictly decreasing order. When a new element violates the ordering, elements are popped until the invariant is restored. It solves problems involving **nearest greater or smaller elements** (e.g. Next Greater Element, Daily Temperatures, Largest Rectangle in Histogram, Trapping Rain Water), reducing naive $O(N^2)$ scans to **$O(N)$** linear time.
</details>

<details>
<summary>3. How does implementing a Queue using Two Stacks achieve amortized $O(1)$ time complexity?</summary>

We use `inStack` for pushes and `outStack` for pops.
- `push(x)`: Simply `inStack.push(x)` ($O(1)$).
- `pop()`: If `outStack` is empty, pop all elements from `inStack` and push them into `outStack` (reversing their order to FIFO), then pop from `outStack`.
Although shifting elements takes $O(N)$ when `outStack` is empty, each element is moved at most once from `inStack` to `outStack` over its entire lifetime. Thus, the total cost for $N$ operations is $2N$, making the **amortized cost per operation $O(1)$**.
</details>

<details>
<summary>4. What is the difference between a Deque and a standard Queue?</summary>

A standard Queue permits element insertion only at the back (`enqueue`) and removal only at the front (`dequeue`). A **Deque (Double-Ended Queue)** allows insertion and removal at both ends: `pushFront`, `popFront`, `pushBack`, and `popBack`, all executing in $O(1)$ time. Deques are essential for sliding window algorithms and work-stealing schedulers.
</details>

<details>
<summary>5. How can you simulate recursion using an iterative Stack to prevent call stack overflow?</summary>

Every recursive function can be converted to an iterative function by allocating an explicit array `stack = []`. Instead of making a recursive call, push an object representing the function's arguments and execution state onto `stack`. Run a `while (stack.length > 0)` loop, popping the top state and processing it. Because heap memory in V8 is far larger than the call stack limit ($\approx 10,000$ frames), an explicit heap stack can handle millions of items without overflowing.
</details>

<details>
<summary>6. How does a Breadth-First Search (BFS) utilize a Queue?</summary>

BFS explores nodes level by level. Starting at the root node, it enqueues the start node. In a loop `while (!queue.isEmpty())`:
1. It dequeues the front node.
2. It processes the node.
3. It enqueues all unvisited adjacent neighbors.
Because the queue processes elements in FIFO order, all nodes at distance $d$ are completely explored before any node at distance $d+1$ is examined, guaranteeing the shortest path in unweighted graphs.
</details>

<details>
<summary>7. What is the space complexity of an algorithm that uses an explicit stack of size $N$?</summary>

The auxiliary space complexity is **$O(N)$** linear space, where $N$ is the number of elements simultaneously held in the stack. In the worst-case scenario (e.g. all elements strictly decreasing in a monotonic increasing stack, or a fully unbalanced tree in iterative DFS), the stack stores all $N$ elements.
</details>

<details>
<summary>8. Why must <code>Math.trunc()</code> be used instead of <code>Math.floor()</code> when parsing Reverse Polish Notation division?</summary>

In mathematical division between integers, division is expected to truncate toward zero (e.g. `6 / -132 = 0`). In JavaScript, standard division yields `-0.0454...`. Using `Math.floor(-0.0454...)` rounds down away from zero to `-1`, which produces an incorrect calculation. `Math.trunc(-0.0454...)` strips the fractional digits, correctly evaluating to `-0` or `0`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement Min Stack
**Specification**: Design a stack that supports `push`, `pop`, `top`, and retrieving the minimum element in **$O(1)$ constant time**.

<details>
<summary>Solution</summary>

```javascript
export class MinStack {
  constructor() {
    this.stack = [];
    this.minStack = [];
  }

  push(val) {
    this.stack.push(val);
    // Push to minStack if empty or val <= current minimum
    if (this.minStack.length === 0 || val <= this.minStack[this.minStack.length - 1]) {
      this.minStack.push(val);
    }
  }

  pop() {
    const val = this.stack.pop();
    if (val === this.minStack[this.minStack.length - 1]) {
      this.minStack.pop();
    }
    return val;
  }

  top() {
    return this.stack[this.stack.length - 1];
  }

  getMin() {
    return this.minStack[this.minStack.length - 1];
  }
}
```
</details>

### Exercise 2: Daily Temperatures
**Specification**: Given an array of integers `temperatures` representing daily temperatures, return an array `answer` such that `answer[i]` is the number of days you have to wait after the $i$-th day to get a warmer temperature. If there is no future day for which this is possible, keep `answer[i] == 0`. Must run in $O(N)$ time using a Monotonic Stack.

<details>
<summary>Solution</summary>

```javascript
export function dailyTemperatures(temperatures) {
  const n = temperatures.length;
  const result = new Array(n).fill(0);
  const stack = []; // Stores indices

  for (let i = 0; i < n; i++) {
    const currentTemp = temperatures[i];

    while (stack.length > 0 && temperatures[stack[stack.length - 1]] < currentTemp) {
      const prevIndex = stack.pop();
      result[prevIndex] = i - prevIndex;
    }

    stack.push(i);
  }

  return result;
}
```
</details>
