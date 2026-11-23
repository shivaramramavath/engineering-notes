# Linked Lists in JavaScript

> A production-grade algorithmic guide to Singly and Doubly Linked Lists in JavaScript, featuring the Sentinel node pattern, pointer manipulation, cycle detection, and cache-locality trade-offs.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [05_this-and-oop/01_classes.md](file:///e:/Learning/engineering-notes/01-languages/javascript/05_this-and-oop/01_classes.md) |
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

In contiguous arrays, inserting or deleting an element at the beginning or middle requires shifting all subsequent elements in memory, which is an $O(N)$ operation. Furthermore, dynamic arrays require periodic resizing, which reallocates memory and copies all existing elements.

A **Linked List** is a linear collection of data elements called **Nodes**, where each node points to the next node via a memory reference:
- **Instant Prepend/Append**: $O(1)$ constant-time insertion and deletion at head and tail.
- **Dynamic Allocation**: Nodes are allocated individually on the heap on demand without resizing buffers.
- **Architectural Foundation**: Serves as the internal backbone for queues, LRU caches, adjacency lists in graphs, and undo-redo history buffers.

---

## 2. Core Concepts

### Variations
- **Singly Linked List**: Each node contains a `val` and a `next` pointer. Traversal is unidirectional.
- **Doubly Linked List**: Each node contains `val`, `next`, and `prev` pointers. Traversal is bidirectional; nodes can delete themselves in $O(1)$ without predecessor lookups.
- **Circular Linked List**: The tail node's `next` pointer points back to the head node.

```
Singly Linked List:
[ Head: 10 ] ──► [ Node: 20 ] ──► [ Node: 30 ] ──► null

Doubly Linked List:
null ◄── [ Head: 10 ] ◄──► [ Node: 20 ] ◄──► [ Tail: 30 ] ──► null
```

### The Sentinel (Dummy Head) Pattern
A dummy node initialized before the actual head:
```javascript
const dummy = new ListNode(0);
dummy.next = head;
```
Using a sentinel node eliminates special-case branching when inserting or deleting the head node, ensuring uniform pointer manipulation logic across all nodes.

---

## 3. How It Works Internally

In JavaScript, each `ListNode` is a discrete heap object created via `new ListNode(val)`. Unlike arrays where elements are stored in contiguous memory addresses, linked list nodes are scattered across the V8 heap:

```
V8 Heap Memory:
Address 0x10A: { val: 10, next: 0x48F }
Address 0x48F: { val: 20, next: 0x82C }
Address 0x82C: { val: 30, next: null }
```

Following pointers from node to node requires CPU pointer chasing, which frequently causes **CPU L1/L2 cache misses** compared to contiguous array iteration.

---

## 4. Syntax & API Reference

### Full Singly Linked List Implementation
```javascript
export class ListNode {
  constructor(val = 0, next = null) {
    this.val = val;
    this.next = next;
  }
}

export class SinglyLinkedList {
  constructor() {
    this.head = null;
    this.tail = null;
    this.length = 0;
  }

  prepend(val) {
    const newNode = new ListNode(val, this.head);
    this.head = newNode;
    if (!this.tail) {
      this.tail = newNode;
    }
    this.length++;
    return this;
  }

  append(val) {
    const newNode = new ListNode(val);
    if (!this.head) {
      this.head = newNode;
      this.tail = newNode;
    } else {
      this.tail.next = newNode;
      this.tail = newNode;
    }
    this.length++;
    return this;
  }

  deleteHead() {
    if (!this.head) return null;
    const deletedVal = this.head.val;
    this.head = this.head.next;
    this.length--;
    if (this.length === 0) {
      this.tail = null;
    }
    return deletedVal;
  }

  toArray() {
    const result = [];
    let current = this.head;
    while (current) {
      result.push(current.val);
      current = current.next;
    }
    return result;
  }
}
```

---

## 5. Practical Examples

### Example 1: In-Place Singly Linked List Reversal ($O(N)$ Time, $O(1)$ Space)
```javascript
export function reverseList(head) {
  let prev = null;
  let current = head;

  while (current !== null) {
    const nextTemp = current.next; // 1. Save next reference
    current.next = prev;          // 2. Reverse pointer
    prev = current;               // 3. Advance prev pointer
    current = nextTemp;           // 4. Advance current pointer
  }

  return prev; // New head of reversed list
}
```

### Example 2: Floyd's Cycle Detection & Cycle Start Node
```javascript
export function detectCycle(head) {
  if (!head || !head.next) return null;

  let slow = head;
  let fast = head;

  // Phase 1: Determine if a cycle exists
  while (fast !== null && fast.next !== null) {
    slow = slow.next;
    fast = fast.next.next;

    if (slow === fast) {
      // Phase 2: Find cycle entry point
      let entry = head;
      while (entry !== slow) {
        entry = entry.next;
        slow = slow.next;
      }
      return entry; // Cycle start node
    }
  }

  return null; // No cycle detected
}
```

### Example 3: Merge Two Sorted Linked Lists
```javascript
import { ListNode } from './SinglyLinkedList.js';

export function mergeTwoLists(l1, l2) {
  const dummy = new ListNode(-1);
  let tail = dummy;

  let p1 = l1;
  let p2 = l2;

  while (p1 !== null && p2 !== null) {
    if (p1.val <= p2.val) {
      tail.next = p1;
      p1 = p1.next;
    } else {
      tail.next = p2;
      p2 = p2.next;
    }
    tail = tail.next;
  }

  // Attach remaining non-empty list
  tail.next = p1 !== null ? p1 : p2;

  return dummy.next;
}
```

### Example 4: Remove Nth Node from End of List (One Pass)
```javascript
import { ListNode } from './SinglyLinkedList.js';

export function removeNthFromEnd(head, n) {
  const dummy = new ListNode(0, head);
  let fast = dummy;
  let slow = dummy;

  // Advance fast pointer so gap between fast and slow is n nodes
  for (let i = 0; i <= n; i++) {
    fast = fast.next;
  }

  // Move both pointers until fast reaches past tail
  while (fast !== null) {
    fast = fast.next;
    slow = slow.next;
  }

  // slow.next is the Nth node from end; bypass it
  slow.next = slow.next.next;

  return dummy.next;
}
```

### Example 5: Palindrome Linked List Verification ($O(N)$ Time, $O(1)$ Space)
```javascript
export function isPalindromeList(head) {
  if (!head || !head.next) return true;

  // 1. Find middle node using fast and slow pointers
  let slow = head;
  let fast = head;
  while (fast.next !== null && fast.next.next !== null) {
    slow = slow.next;
    fast = fast.next.next;
  }

  // 2. Reverse second half of list
  let prev = null;
  let curr = slow.next;
  while (curr !== null) {
    const next = curr.next;
    curr.next = prev;
    prev = curr;
    curr = next;
  }

  // 3. Compare first half with reversed second half
  let p1 = head;
  let p2 = prev;
  let isPalin = true;
  while (p2 !== null) {
    if (p1.val !== p2.val) {
      isPalin = false;
      break;
    }
    p1 = p1.next;
    p2 = p2.next;
  }

  return isPalin;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Losing Pointer References**:
   Overwriting `node.next` before caching the reference (`node.next = prev`) permanently orphans the remainder of the linked list, causing memory leaks or infinite loops. Always cache `const nextTemp = node.next;` first.
2. **Null Pointer Exceptions on Traversal**:
   Calling `fast.next.next` when `fast.next === null` throws `TypeError: Cannot read properties of null (reading 'next')`. Always check `while (fast !== null && fast.next !== null)`.
3. **Circular Memory Leaks in Doubly Linked Lists**:
   Because doubly linked lists contain reciprocal pointers (`nodeA.next = nodeB; nodeB.prev = nodeA;`), naive reference-counting memory managers fail to collect them. Modern V8 mark-and-sweep garbage collection correctly handles cyclic references, provided external references to the list are removed.
4. **Boundary Mutations (Deleting Head or Tail)**:
   Deleting the head requires updating the `head` pointer. If deleting the only element in the list, both `head` and `tail` must be reset to `null`. Using sentinel nodes avoids special-case branching.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Risk | Production Fix |
| :--- | :--- | :--- |
| Overwriting `node.next` without caching | Loses the rest of the list chain permanently. | Store `const nextNode = current.next` before mutating pointers. |
| Incomplete loop condition `fast.next` | Throws runtime TypeError when traversing odd-length lists. | Check `while (fast !== null && fast.next !== null)`. |
| Branching spaghetti for head mutations | Introduces subtle bugs when mutating the first element. | Use a Dummy / Sentinel node (`const dummy = new ListNode(0, head)`). |
| Using Linked Lists where random access is needed | Traversal to index $i$ takes $O(N)$ time instead of $O(1)$. | Use standard contiguous arrays if random access by index is frequent. |
| Failing to update `tail` on append | Appends become $O(N)$ traversals instead of $O(1)$. | Maintain a persistent `tail` pointer reference. |
| Recursive reverse on large lists | Causes Call Stack Overflow on lists $>10,000$ nodes. | Implement iterative reversal with three pointers (`prev`, `curr`, `next`). |

---

## 8. Performance Characteristics

| Operation | Array (Contiguous) | Singly Linked List | Doubly Linked List |
| :--- | :--- | :--- | :--- |
| **Prepend (`unshift`)** | $O(N)$ (requires shifting) | $O(1)$ | $O(1)$ |
| **Append (`push`)** | $O(1)$ amortized | $O(1)$ (with tail pointer) | $O(1)$ |
| **Delete Head (`shift`)**| $O(N)$ (requires shifting) | $O(1)$ | $O(1)$ |
| **Delete Tail (`pop`)** | $O(1)$ | $O(N)$ (requires predecessor) | $O(1)$ |
| **Lookup by Index** | $O(1)$ | $O(N)$ | $O(N)$ |
| **Memory Locality** | High (Cache Friendly) | Low (Heap Pointer Chasing) | Low (Heap Pointer Chasing) |

---

## 9. Engine Internals

### V8 Hidden Classes & Pointer Chasing
- **Object Shape Stability**: All `ListNode` instances should be instantiated with identical property orders (`this.val = val; this.next = next;`). This guarantees that all nodes share the same V8 Hidden Class (Shape), allowing TurboFan to inline property accesses.
- **Hardware Cache Line Misses**: A contiguous JavaScript typed array or packed array occupies sequential memory bytes. When CPU reads `arr[0]`, the hardware pre-fetches the next 64 bytes into L1 data cache. With linked lists, nodes are scattered across arbitrary heap addresses, requiring separate RAM access cycles for every pointer hop.

---

## 10. Best Practices & Design Guidelines

1. **Always Use Sentinel Nodes**: Prepend a `dummy` node to simplify list deletion, insertion, and boundary conditions.
2. **Prefer Two Pointers for Position Queries**: Use fast and slow pointers to find the middle node or $N$-th from end in a single pass.
3. **Use Iteration Over Recursion for List Traversals**: Prevent V8 call stack overflow crashes on lists with thousands of nodes.
4. **Choose Doubly Linked Lists for Bidirectional Mutations**: Essential for $O(1)$ node removal in LRU Caches and task queues.

---

## 11. Real-World Case Study

### Application Undo-Redo Action History
**Scenario**: A collaborative vector graphics editor needed an undo-redo history buffer allowing users to step backward and forward through thousands of canvas edits.
**Naive Implementation**: Stored history states in an array. Calling `undo()` popped the action, but performing a new action after an undo required `history.splice(currentIndex + 1)` which shifted thousands of array elements ($O(N)$).

**Doubly Linked List Redesign**:
- Modeled history states as nodes in a Doubly Linked List.
- `undo()`: Moves pointer to `currentNode.prev` ($O(1)$).
- `redo()`: Moves pointer to `currentNode.next` ($O(1)$).
- `addAction(action)`: Discards forward history simply by setting `currentNode.next = newNode; newNode.prev = currentNode; currentNode = newNode;` ($O(1)$).

**Result**:
- State operations executed in deterministic $0.0001\text{ms}$ ($O(1)$).
- Zero array re-allocation or memory copying during complex drawing sessions.

---

## 12. Interview Questions

<details>
<summary>1. How does Floyd's Cycle-Finding Algorithm (Tortoise and Hare) work?</summary>

Initialize two pointers: `slow` advancing one node per step, and `fast` advancing two nodes per step.
1. If no cycle exists, `fast` reaches `null` ($O(N)$ time).
2. If a cycle exists, `fast` enters the cycle first. With each step, the distance between `fast` and `slow` decreases by 1 node. They must collide inside the cycle.
3. Once they collide, place a pointer at the list `head` and advance both `head` and `slow` one step at a time. The node where they meet is the exact start of the cycle.
</details>

<details>
<summary>2. Why are Sentinel (Dummy) nodes used in linked list algorithms?</summary>

Sentinel nodes eliminate special cases for the head of the list:
1. Deleting the head node does not require an `if (node === head)` branch.
2. Inserting before the head is treated identically to inserting after any internal node.
3. Merging or building lists does not require checking whether the list is currently empty.
At the end of the operation, `dummy.next` points directly to the true head.
</details>

<details>
<summary>3. Why can an array iterate faster than a linked list even if both have $O(N)$ time complexity?</summary>

Arrays are stored in contiguous blocks of memory. Modern CPU architectures use cache lines (typically 64 bytes) to prefetch adjacent data into L1/L2 caches when an element is read. In a linked list, nodes are individually allocated on the heap at scattered addresses. Traversing a linked list requires "pointer chasing", causing frequent CPU cache misses and forcing the CPU to fetch data from slower main RAM.
</details>

<details>
<summary>4. How do you reverse a Singly Linked List in-place in $O(1)$ auxiliary space?</summary>

Maintain three pointers: `prev = null`, `current = head`, and `next = null`.
In a `while (current !== null)` loop:
1. Store the next node: `next = current.next;`
2. Reverse the current node's pointer: `current.next = prev;`
3. Advance `prev`: `prev = current;`
4. Advance `current`: `current = next;`
When `current` becomes `null`, `prev` points to the new head.
</details>

<details>
<summary>5. How can you find the middle of a linked list in a single pass?</summary>

Use two pointers: initialize `slow = head` and `fast = head`.
Loop `while (fast !== null && fast.next !== null)`:
- `slow = slow.next;`
- `fast = fast.next.next;`
When the loop finishes, `slow` is located precisely at the middle node (or the second middle node for even-length lists).
</details>

<details>
<summary>6. What is the advantage of a Doubly Linked List over a Singly Linked List for node deletion?</summary>

In a Singly Linked List, deleting a given node requires finding its *predecessor* node to update `prev.next = node.next`, which requires an $O(N)$ traversal from the head. In a Doubly Linked List, the node already holds a pointer to its predecessor (`node.prev`). Deletion can be executed in $O(1)$ constant time directly: `node.prev.next = node.next; node.next.prev = node.prev;`.
</details>

<details>
<summary>7. How do you delete the $N$-th node from the end of a linked list in one pass?</summary>

Use a dummy node pointing to `head`. Advance a `fast` pointer $N + 1$ steps ahead from the dummy node. Then advance both `fast` and `slow` one step at a time until `fast` reaches `null`. At that point, `slow` is pointing directly to the node *preceding* the target node. Mutate `slow.next = slow.next.next` and return `dummy.next`.
</details>

<details>
<summary>8. How does JavaScript handle cyclic references in linked lists during garbage collection?</summary>

Modern JavaScript engines (like V8) use **Mark-and-Sweep Garbage Collection**, not naive reference counting. GC starts from designated "roots" (global window/globalThis, local call stack variables). It traces all reachable object references. If a group of circular nodes references each other but is unreachable from any active root, V8 identifies the entire circular group as garbage and reclaims their memory.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Middle of the Linked List
**Specification**: Given the `head` of a singly linked list, return the middle node. If there are two middle nodes, return the second middle node. Must run in $O(N)$ time and $O(1)$ space.

<details>
<summary>Solution</summary>

```javascript
export function middleNode(head) {
  let slow = head;
  let fast = head;

  while (fast !== null && fast.next !== null) {
    slow = slow.next;
    fast = fast.next.next;
  }

  return slow;
}
```
</details>

### Exercise 2: Delete Node in a Linked List (Without Head Reference)
**Specification**: You are given access only to the node to be deleted in a singly linked list (guaranteed not to be the tail). Delete it in $O(1)$ time without access to `head`.

<details>
<summary>Solution</summary>

```javascript
export function deleteNode(node) {
  // Overwrite current node's value with next node's value, then bypass next node
  node.val = node.next.val;
  node.next = node.next.next;
}
```
</details>
