# Typed Linked Lists and Binary Search Trees in TypeScript

> Implementation and formal typing of fundamental pointer-based data structures in TypeScript: generic Singly and Doubly Linked Lists, Binary Search Trees, recursive tree traversals, and iterable protocols.

| Metadata | Details |
| :--- | :--- |
| **Category** | Data Structures & Algorithms |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md), [12-asynchronous-and-iteration/04-iterators-and-generators.md](../12-asynchronous-and-iteration/04-iterators-and-generators.md) |
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

While JavaScript provides native high-level dynamic arrays (`Array`) and hash tables (`Map`, `Set`), standard arrays suffer from non-deterministic $O(N)$ overhead during unshift and splice operations due to contiguous memory reallocation.

Implementing classic pointer-based data structures in TypeScript delivers:
- **Predictable Performance**: $O(1)$ node insertion/removal in linked lists; $O(\log N)$ search, min, and max operations in balanced binary search trees.
- **Type-Safe Invariants**: Ensuring node values conform to generic constraints (`T extends Comparable<T>`), preventing runtime comparison errors.
- **Custom Memory & Iteration Protocols**: Implementing `Symbol.iterator` to integrate seamlessly with native `for...of` loops, spread operators, and generator pipelines.

---

## 2. Core Concepts

### Generic Node Hierarchy
A node is an encapsulated container holding data `value: T` and pointer references (`next?: ListNode<T>`, `prev?: ListNode<T>`, `left?: TreeNode<T>`, `right?: TreeNode<T>`). In TypeScript, nullability must be handled cleanly via optional properties or `null` union types under `"strictNullChecks": true`.

```
+-------------------------------------------------------------------------+
|                  Generic Pointer Data Structure Memory Layout           |
+-------------------------------------------------------------------------+
|                                                                         |
|  [ Doubly Linked List Node: ListNode<T> ]                               |
|  +-------------------+--------------------+--------------------+        |
|  | prev: Node<T>|null| value: T           | next: Node<T>|null |        |
|  +-------------------+--------------------+--------------------+        |
|                                                                         |
|  [ Binary Search Tree Node: TreeNode<T> ]                               |
|                       +------------------+                              |
|                       |  value: T        |                              |
|                       +--------+---------+                              |
|                                |                                        |
|                +---------------+---------------+                        |
|                |                               |                        |
|                v                               v                        |
|      [ left: TreeNode<T>|null ]      [ right: TreeNode<T>|null ]        |
|      (Values < T)                    (Values > T)                       |
|                                                                         |
+-------------------------------------------------------------------------+
```

### The Comparator Protocol
Binary search trees require comparing arbitrary generic types `T`. We define a type-safe comparator contract:
`type Comparator<T> = (a: T, b: T) => number;` returning `< 0` if `a < b`, `0` if equal, and `> 0` if `a > b`.

---

## 3. How It Works Internally

1. **V8 Object References**: In the V8 engine, references to `next` and `prev` nodes are stored as 64-bit pointers (or 32-bit compressed pointers under `PointerCompression`).
2. **Recursive Traversal Call Stack**: In-order, pre-order, and post-order tree traversals recurse down the call stack. Deeply unbalanced trees can trigger `RangeError: Maximum call stack size exceeded`. Iterative traversals with an explicit stack array avoid this limitation.

---

## 4. Syntax & API Reference

### Singly Linked List Node Contract
```typescript
export interface SinglyNode<T> {
  value: T;
  next: SinglyNode<T> | null;
}

export type Comparator<T> = (a: T, b: T) => number;

export const defaultComparator = <T>(a: T, b: T): number => {
  if (a < b) return -1;
  if (a > b) return 1;
  return 0;
};
```

---

## 5. Practical Examples

### Example 1: Generic Doubly Linked List with Iterable Protocol
```typescript
class DoublyNode<T> {
  public prev: DoublyNode<T> | null = null;
  public next: DoublyNode<T> | null = null;
  constructor(public value: T) {}
}

export class DoublyLinkedList<T> implements Iterable<T> {
  private head: DoublyNode<T> | null = null;
  private tail: DoublyNode<T> | null = null;
  private _size = 0;

  public get size(): number {
    return this._size;
  }

  public append(value: T): void {
    const node = new DoublyNode(value);
    if (!this.tail) {
      this.head = this.tail = node;
    } else {
      node.prev = this.tail;
      this.tail.next = node;
      this.tail = node;
    }
    this._size++;
  }

  public prepend(value: T): void {
    const node = new DoublyNode(value);
    if (!this.head) {
      this.head = this.tail = node;
    } else {
      node.next = this.head;
      this.head.prev = node;
      this.head = node;
    }
    this._size++;
  }

  public *[Symbol.iterator](): Iterator<T> {
    let curr = this.head;
    while (curr) {
      yield curr.value;
      curr = curr.next;
    }
  }
}
```

### Example 2: Generic Binary Search Tree (BST)
```typescript
import { Comparator, defaultComparator } from "./contracts.js";

class TreeNode<T> {
  public left: TreeNode<T> | null = null;
  public right: TreeNode<T> | null = null;
  constructor(public value: T) {}
}

export class BinarySearchTree<T> {
  private root: TreeNode<T> | null = null;

  constructor(private compare: Comparator<T> = defaultComparator) {}

  public insert(value: T): void {
    const newNode = new TreeNode(value);
    if (!this.root) {
      this.root = newNode;
      return;
    }

    let curr = this.root;
    while (true) {
      const cmp = this.compare(value, curr.value);
      if (cmp < 0) {
        if (!curr.left) {
          curr.left = newNode;
          break;
        }
        curr = curr.left;
      } else {
        if (!curr.right) {
          curr.right = newNode;
          break;
        }
        curr = curr.right;
      }
    }
  }

  public contains(value: T): boolean {
    let curr = this.root;
    while (curr) {
      const cmp = this.compare(value, curr.value);
      if (cmp === 0) return true;
      curr = cmp < 0 ? curr.left : curr.right;
    }
    return false;
  }

  public inOrderTraversal(): T[] {
    const result: T[] = [];
    const traverse = (node: TreeNode<T> | null) => {
      if (!node) return;
      traverse(node.left);
      result.push(node.value);
      traverse(node.right);
    };
    traverse(this.root);
    return result;
  }
}
```

### Example 3: Edge Case - Nullable Node Deletion
```typescript
// Finding the minimum node in a subtree for BST node deletion
function findMin<T>(node: TreeNode<T>): TreeNode<T> {
  let curr = node;
  while (curr.left) {
    curr = curr.left;
  }
  return curr;
}
```

### Example 4: Compiler Error & Incompatible Comparator
```typescript
interface UserAccount {
  id: number;
  name: string;
}

// Tree expecting numeric comparator
const tree = new BinarySearchTree<UserAccount>((a, b) => a.id - b.id);

// @ts-expect-error - TS2345: Argument of type 'string' is not assignable to parameter of type 'UserAccount'.
tree.insert("not_a_user");
```

### Example 5: High-Performance Iterative In-Order Traversal
```typescript
export function iterativeInOrder<T>(root: TreeNode<T> | null): T[] {
  const result: T[] = [];
  const stack: TreeNode<T>[] = [];
  let curr: TreeNode<T> | null = root;

  while (curr || stack.length > 0) {
    while (curr) {
      stack.push(curr);
      curr = curr.left;
    }
    curr = stack.pop()!;
    result.push(curr.value);
    curr = curr.right;
  }
  return result;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Circular Pointer Memory Leaks**:
   In doubly linked lists, circular references between `prev` and `next` prevent simple mark-and-sweep garbage collection if closures hold references to intermediate nodes. Explicitly set pointers to `null` on node removal.
2. **Call Stack Overflow on Deep BSTs**:
   Inserting sorted data (`1, 2, 3, 4, ...`) into an unbalanced BST degenerates into a singly linked list ($O(N)$ depth). Recursive traversals throw call stack overflows for $N > 10,000$.
3. **Floating Point Comparison Equality**:
   Comparing floating-point values using `===` in a BST can cause infinite loops or missed lookups due to precision issues. Always use epsilon tolerances.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Loose Any Nodes** | `class Node { value: any }` | Generic `Node<T>` | Disables type safety |
| **Omitting Null Checks** | Accessing `node.next.value` directly | Check `node.next !== null` | `TS2531: Object is possibly 'null'` |
| **Hardcoded Comparison** | Using `<` directly on generic `T` | Accept a typed `Comparator<T>` | Compares objects as `[object Object]` |
| **Missing Memory Cleanup** | Orphaned nodes retaining `prev`/`next` | Disconnect pointers on delete | Memory leak |
| **Unbounded Tree Recursion** | Recursive DFS on unbounded tree | Iterative traversal with stack array | `RangeError: Maximum call stack size exceeded` |
| **Modifying Iterated List** | Mutating linked list during `for..of` | Iterate over shallow clone | Infinite loop or missed nodes |

---

## 8. Performance & Type-Checker Complexity

| Structure | Access Time | Search Time | Insertion Time | Deletion Time | Space |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Doubly Linked List** | $O(N)$ | $O(N)$ | $O(1)$ (head/tail) | $O(1)$ | $O(N)$ |
| **Binary Search Tree (Balanced)** | $O(\log N)$ | $O(\log N)$ | $O(\log N)$ | $O(\log N)$ | $O(N)$ |
| **Binary Search Tree (Degenerate)**| $O(N)$ | $O(N)$ | $O(N)$ | $O(N)$ | $O(N)$ |

---

## 9. Compiler Internals

When instantiating `BinarySearchTree<T>`:
1. `ts.TypeChecker` creates a `TypeReference` node mapping generic parameter `T`.
2. The comparator signature `(a: T, b: T) => number` establishes bidirectional contextual typing when arrow functions are supplied in the constructor.
3. The iterable implementation `*[Symbol.iterator](): Iterator<T>` binds to the global TypeScript `Iterable<T>` ambient interface.

---

## 10. Best Practices & Design Guidelines

1. **Always Supply a Comparator for Custom Objects**.
2. **Implement `Iterable<T>`**: Enables natural JavaScript ergonomics (`[...list]`, `for (const x of list)`).
3. **Nullify Discarded Node Pointers**: Assist the V8 garbage collector.
4. **Use Iterative Traversals for Deep Hierarchies**: Guard against call stack overflows.

---

## 11. Real-World Case Study

An in-memory event-streaming pipeline processed 50,000 incoming telemetry packets per second. The original implementation used a JavaScript `Array` as a FIFO queue (`arr.push()` and `arr.shift()`).

**Performance Diagnosis**:
`arr.shift()` runs in $O(N)$ time because the engine must re-index every remaining element. Under 50,000 items, `shift()` consumed 78% of the CPU budget.

**Solution**:
Replaced the array with a generic **Doubly Linked List** FIFO queue.
- `enqueue` ($O(1)$) and `dequeue` ($O(1)$).
- CPU time dropped from **78%** to **3%**, eliminating event loop lag.

---

## 12. Interview Questions

<details>
<summary>1. Why does `Array.prototype.shift()` perform worse than a Linked List dequeue in JavaScript?</summary>

In JavaScript, arrays are stored contiguously in memory. Calling `shift()` removes the first element and shifts all remaining $N-1$ elements in memory down by one index ($O(N)$ operation). In a Linked List, removing the head updates only the head pointer reference ($O(1)$ operation).
</details>

<details>
<summary>2. How do you implement the JavaScript iterator protocol on a custom TypeScript class?</summary>

By implementing the `Iterable<T>` interface and defining a generator or iterator method keyed by `[Symbol.iterator]()`:
```typescript
public *[Symbol.iterator](): Iterator<T> {
  let curr = this.head;
  while (curr) { yield curr.value; curr = curr.next; }
}
```
</details>

<details>
<summary>3. What happens if you insert already sorted data into a standard Binary Search Tree?</summary>

The tree degenerates into a linear linked list where every node has only a right child. Search, insertion, and deletion complexity degrade from $O(\log N)$ to $O(N)$. Self-balancing trees (AVL, Red-Black) prevent this via tree rotations.
</details>

<details>
<summary>4. Why is generic constraint `T extends Comparable<T>` often replaced with an explicit `Comparator<T>` function in TypeScript?</summary>

TypeScript lacks native operator overloading (`<` and `>` cannot be overridden for custom classes). Passing an external `Comparator<T>` function is more flexible and works with plain interfaces and records without requiring prototype mutation.
</details>

<details>
<summary>5. How does `strictNullChecks: true` improve pointer-based data structures?</summary>

It forces developers to explicitly handle `null` or `undefined` on pointer fields (`node.next`, `node.left`), preventing `TypeError: Cannot read properties of null` runtime exceptions at compile time.
</details>

<details>
<summary>6. What is the difference between In-Order, Pre-Order, and Post-Order tree traversal?</summary>

- **In-Order**: Left subtree $\rightarrow$ Current node $\rightarrow$ Right subtree (yields sorted order in a BST).
- **Pre-Order**: Current node $\rightarrow$ Left subtree $\rightarrow$ Right subtree (used for copying or serializing trees).
- **Post-Order**: Left subtree $\rightarrow$ Right subtree $\rightarrow$ Current node (used for bottom-up deletions or directory size calculation).
</details>

<details>
<summary>7. How do you prevent call stack overflow during recursive tree traversals in V8?</summary>

By converting recursive algorithms into iterative implementations using an explicit array as an in-memory stack or queue.
</details>

<details>
<summary>8. Why should node pointers be set to `null` upon removal in a doubly linked list?</summary>

To prevent detached nodes from keeping references to the active list, allowing the garbage collector to reclaim unused nodes immediately.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Reverse a Singly Linked List in place</summary>

**Task**: Implement a function that reverses a singly linked list in $O(N)$ time and $O(1)$ space.

**Solution**:
```typescript
interface Node<T> {
  value: T;
  next: Node<T> | null;
}

export function reverseList<T>(head: Node<T> | null): Node<T> | null {
  let prev: Node<T> | null = null;
  let curr: Node<T> | null = head;

  while (curr) {
    const nextTemp = curr.next;
    curr.next = prev;
    prev = curr;
    curr = nextTemp;
  }
  return prev;
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Find the maximum depth of a Binary Tree</summary>

**Task**: Write a recursive function calculating the maximum depth of a binary tree.

**Solution**:
```typescript
interface TreeNode<T> {
  left: TreeNode<T> | null;
  right: TreeNode<T> | null;
}

export function maxDepth<T>(root: TreeNode<T> | null): number {
  if (!root) return 0;
  return 1 + Math.max(maxDepth(root.left), maxDepth(root.right));
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Detect cycle in a Linked List (Floyd's Tortoise and Hare)</summary>

**Task**: Implement cycle detection in a linked list using two pointers without extra space.

**Solution**:
```typescript
interface Node<T> {
  next: Node<T> | null;
}

export function hasCycle<T>(head: Node<T> | null): boolean {
  let slow = head;
  let fast = head;

  while (fast && fast.next) {
    slow = slow!.next;
    fast = fast.next.next;
    if (slow === fast) return true;
  }
  return false;
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Validate Binary Search Tree (IsValidBST)</summary>

**Task**: Implement a function validating whether a binary tree satisfies the BST property.

**Solution**:
```typescript
interface TreeNode {
  val: number;
  left: TreeNode | null;
  right: TreeNode | null;
}

export function isValidBST(root: TreeNode | null, min = -Infinity, max = Infinity): boolean {
  if (!root) return true;
  if (root.val <= min || root.val >= max) return false;
  return isValidBST(root.left, min, root.val) && isValidBST(root.right, root.val, max);
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Implement an AVL Tree Rotation</summary>

**Task**: Write the `rotateRight` and `rotateLeft` balancing operations for an AVL Tree node.

**Solution**:
```typescript
export class AvlNode<T> {
  public height = 1;
  public left: AvlNode<T> | null = null;
  public right: AvlNode<T> | null = null;
  constructor(public value: T) {}
}

export function rotateRight<T>(y: AvlNode<T>): AvlNode<T> {
  const x = y.left!;
  const T2 = x.right;

  x.right = y;
  y.left = T2;

  y.height = 1 + Math.max(y.left?.height ?? 0, y.right?.height ?? 0);
  x.height = 1 + Math.max(x.left?.height ?? 0, x.right?.height ?? 0);

  return x;
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Level-Order BFS Traversal with Generator</summary>

**Task**: Implement a generator yielding tree values level by level (Breadth-First Search) using an iterative queue.

**Solution**:
```typescript
export function* levelOrderTraversal<T>(root: TreeNode<T> | null): Generator<T[]> {
  if (!root) return;
  const queue: TreeNode<T>[] = [root];

  while (queue.length > 0) {
    const levelSize = queue.length;
    const currentLevel: T[] = [];

    for (let i = 0; i < levelSize; i++) {
      const node = queue.shift()!;
      currentLevel.push(node.value);
      if (node.left) queue.push(node.left);
      if (node.right) queue.push(node.right);
    }
    yield currentLevel;
  }
}
```
</details>
