# Linked Lists Algorithmic Patterns in Java

> Singly, doubly, and circular linked lists, Fast & Slow pointer (Tortoise and Hare) patterns, iterative/recursive reversals, and Merge K Sorted Lists in Java.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Advanced |
| **Prerequisites** | [`27-dsa/00_complexity-analysis.md`](00_complexity-analysis.md), [`08-collections/07_linkedlist.md`](../08-collections/07_linkedlist.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified (Java 8 to 25 LTS) |
| **Standard** | Java SE 21 / 25 LTS |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

A Linked List is a linear data structure where elements (nodes) are stored non-contiguously in heap memory. Each node encapsulates data and one or more reference pointers (`next`, `prev`) linking to adjacent nodes.

Unlike arrays, linked lists provide constant-time $O(1)$ insertions and deletions at known pointer locations without requiring element shifting or array reallocation. However, random access is not supported ($O(N)$ traversal).

---

## 2. Core Concepts

### 2.1 Linked List Topologies

```
1. Singly Linked List:
   [Head] -> [Node 1 | next] -> [Node 2 | next] -> [Node 3 | null]

2. Doubly Linked List:
   [Head] <-> [prev | Node 1 | next] <-> [prev | Node 2 | next] <-> [Tail]

3. Circular Linked List:
   [Head] -> [Node 1] -> [Node 2] -> [Node 3] ---+
     ^                                           |
     +-------------------------------------------+
```

### 2.2 The Sentinel (Dummy Head) Pattern

A Sentinel or Dummy Head node (`ListNode dummy = new ListNode(0)`) simplifies edge cases:
- Eliminates special branch conditions for modifying or deleting the true head of the list.
- Eliminates null pointer checks during list construction.
- The true result list is accessed via `dummy.next`.

### 2.3 Floyd’s Cycle Detection Algorithm (Tortoise and Hare)

To detect whether a linked list contains a cycle without consuming $O(N)$ extra memory:
- **Slow Pointer**: Advances 1 node per step ($S_{t+1} = S_t \to \text{next}$).
- **Fast Pointer**: Advances 2 nodes per step ($F_{t+1} = F_t \to \text{next} \to \text{next}$).
- If a cycle exists, the fast pointer will inevitably lap and meet the slow pointer within the loop.
- To find the **cycle entry point**: Reset one pointer to `head`; advance both pointers 1 node at a time. The node where they meet is the start of the cycle!

---

## 3. How It Works Internally

### 3.1 Pointer Chasing and Cache Miss Penalties

```
Array (Contiguous Memory):
[Index 0] [Index 1] [Index 2] [Index 3] -> Single L1 Cache Line Fetch! Fast!

Linked List (Heap Fragmented):
Heap Address: 0x1A20 -> Node A (points to 0x8F40)
Heap Address: 0x8F40 -> Node B (points to 0x3310)
Heap Address: 0x3310 -> Node C (points to 0xD290)
Result: Every pointer dereference triggers a separate DRAM memory access!
Cache Miss Stall: ~100-200 CPU cycles per node traversal!
```

---

## 4. Syntax & Usage

### 4.1 Canonical Java ListNode Representation

```java
public class ListNode {
    public int val;
    public ListNode next;

    public ListNode() {}
    public ListNode(int val) { this.val = val; }
    public ListNode(int val, ListNode next) { this.val = val; this.next = next; }
}
```

---

## 5. Practical Examples

### 5.1 Pattern 1: In-Place Reversal of Singly Linked List (LeetCode 206)
```java
public class ReverseLinkedList {
    public static ListNode reverseList(ListNode head) {
        ListNode prev = null;
        ListNode curr = head;

        while (curr != null) {
            ListNode nextTemp = curr.next; // Save next pointer
            curr.next = prev;              // Invert link
            prev = curr;                   // Advance prev
            curr = nextTemp;               // Advance curr
        }
        return prev; // New head
    }
}
```

### 5.2 Pattern 2: Floyd’s Cycle Detection & Entry Point (LeetCode 142)
```java
public class LinkedListCycleII {
    public static ListNode detectCycle(ListNode head) {
        if (head == null || head.next == null) return null;

        ListNode slow = head;
        ListNode fast = head;

        // Step 1: Detect presence of cycle
        while (fast != null && fast.next != null) {
            slow = slow.next;
            fast = fast.next.next;
            if (slow == fast) {
                // Step 2: Locate cycle start node
                ListNode entry = head;
                while (entry != slow) {
                    entry = entry.next;
                    slow = slow.next;
                }
                return entry;
            }
        }
        return null; // No cycle
    }
}
```

### 5.3 Pattern 3: Merge Two Sorted Lists (LeetCode 21)
```java
public class MergeTwoLists {
    public static ListNode mergeTwoLists(ListNode list1, ListNode list2) {
        ListNode dummy = new ListNode(-1);
        ListNode current = dummy;

        while (list1 != null && list2 != null) {
            if (list1.val <= list2.val) {
                current.next = list1;
                list1 = list1.next;
            } else {
                current.next = list2;
                list2 = list2.next;
            }
            current = current.next;
        }

        current.next = (list1 != null) ? list1 : list2;
        return dummy.next;
    }
}
```

### 5.4 Pattern 4: Remove Nth Node From End of List (LeetCode 19)
```java
public class RemoveNthFromEnd {
    public static ListNode removeNthFromEnd(ListNode head, int n) {
        ListNode dummy = new ListNode(0, head);
        ListNode fast = dummy;
        ListNode slow = dummy;

        // Advance fast by n+1 steps
        for (int i = 0; i <= n; i++) {
            fast = fast.next;
        }

        // Move fast and slow in unison
        while (fast != null) {
            fast = fast.next;
            slow = slow.next;
        }

        // Delete the nth node
        slow.next = slow.next.next;
        return dummy.next;
    }
}
```

### 5.5 Pattern 5: Merge K Sorted Lists using Priority Queue (LeetCode 23)
```java
import java.util.Comparator;
import java.util.PriorityQueue;

public class MergeKSortedLists {
    public static ListNode mergeKLists(ListNode[] lists) {
        if (lists == null || lists.length == 0) return null;

        PriorityQueue<ListNode> minHeap = new PriorityQueue<>(Comparator.comparingInt(a -> a.val));

        for (ListNode node : lists) {
            if (node != null) minHeap.offer(node);
        }

        ListNode dummy = new ListNode(0);
        ListNode current = dummy;

        while (!minHeap.isEmpty()) {
            ListNode smallest = minHeap.poll();
            current.next = smallest;
            current = current.next;

            if (smallest.next != null) {
                minHeap.offer(smallest.next);
            }
        }

        return dummy.next;
    }
}
```

### 5.6 Pattern 6: Reorder List / Palindrome Check via Midpoint & Invert
```java
public class PalindromeLinkedList {
    public static boolean isPalindrome(ListNode head) {
        if (head == null || head.next == null) return true;

        // 1. Find midpoint with Fast & Slow
        ListNode slow = head, fast = head;
        while (fast.next != null && fast.next.next != null) {
            slow = slow.next;
            fast = fast.next.next;
        }

        // 2. Reverse second half
        ListNode secondHalf = ReverseLinkedList.reverseList(slow.next);
        ListNode p1 = head;
        ListNode p2 = secondHalf;

        // 3. Compare halves
        boolean result = true;
        while (p2 != null) {
            if (p1.val != p2.val) {
                result = false;
                break;
            }
            p1 = p1.next;
            p2 = p2.next;
        }

        return result;
    }
}
```

### 5.7 Pattern 7: LRU Cache with Custom Doubly Linked List (LeetCode 146)
```java
import java.util.HashMap;
import java.util.Map;

public class LruCacheStructure {
    private static class DNode {
        int key, val;
        DNode prev, next;
        DNode(int k, int v) { this.key = k; this.val = v; }
    }

    private final int capacity;
    private final Map<Integer, DNode> map = new HashMap<>();
    private final DNode head = new DNode(0, 0);
    private final DNode tail = new DNode(0, 0);

    public LruCacheStructure(int capacity) {
        this.capacity = capacity;
        head.next = tail;
        tail.prev = head;
    }

    public int get(int key) {
        DNode node = map.get(key);
        if (node == null) return -1;
        moveToHead(node);
        return node.val;
    }

    public void put(int key, int value) {
        DNode node = map.get(key);
        if (node != null) {
            node.val = value;
            moveToHead(node);
        } else {
            if (map.size() >= capacity) {
                DNode evicted = tail.prev;
                removeNode(evicted);
                map.remove(evicted.key);
            }
            DNode newNode = new DNode(key, value);
            map.put(key, newNode);
            addNode(newNode);
        }
    }

    private void addNode(DNode node) {
        node.next = head.next;
        node.prev = head;
        head.next.prev = node;
        head.next = node;
    }

    private void removeNode(DNode node) {
        node.prev.next = node.next;
        node.next.prev = node.prev;
    }

    private void moveToHead(DNode node) {
        removeNode(node);
        addNode(node);
    }
}
```

---

## 6. Best Practices

1. **Always Use a Dummy Node for Structural Modifications**: Prevents complex conditional logic when mutating the head node.
2. **Sever Detached Nodes**: When partitioning or reordering lists, set the tail node's `next = null` explicitly to prevent accidental cycles.
3. **Prefer Iterative over Recursive Reversals**: Recursive reversals consume $O(N)$ stack space and can trigger `StackOverflowError` on lists with $10,000+$ elements.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Losing Next Reference** | Overwriting `curr.next = prev` before saving `curr.next` severs the remaining list | Store `ListNode temp = curr.next` before mutating pointer |
| **NullPointerException in Fast Pointer**| `fast.next.next` throws NPE when `fast.next == null` | Loop condition must be `while (fast != null && fast.next != null)` |
| **Unintended Infinite Cycle** | Moving nodes between lists without setting `next = null` creates infinite loop | Terminate spliced lists with `null` |
| **Memory Leaks from Retained References**| Deleted node remains referenced by static roots or listeners | Clear unused object references |

---

## 8. Performance & Security Notes

### 8.1 Stack Overflow in Recursive Traversal
Recursion on deeply nested lists ($N > 10,000$) exhausts default JVM thread stack size ($1\text{ MB} \approx 8,000\text{ frames}$). Always implement list algorithms iteratively using `while` loops.

---

## 9. Comparison / When to Use What

| Operation | `ArrayList` | `LinkedList` / `ListNode` |
| :--- | :--- | :--- |
| **Index Access (`get(i)`)** | $O(1)$ | $O(N)$ |
| **Prepend (`addFirst`)** | $O(N)$ (Element shift) | $O(1)$ |
| **Append (`addLast`)** | $O(1)$ amortized | $O(1)$ |
| **Delete at Known Pointer** | $O(N)$ | $O(1)$ |
| **Cache Locality** | Excellent (L1/L2 hits) | Poor (Pointer chasing cache misses) |
| **Memory Overhead** | Low (~4 bytes/int) | High (24 bytes per node on 64-bit JVM) |

---

## 10. Interview Questions

<details>
<summary>1. Prove why Floyd's Cycle Detection works and why Fast and Slow pointers must meet.</summary>
Let the cycle length be $C$. Once both pointers enter the cycle, the distance between them decreases by 1 in each step because the fast pointer moves at speed 2 and slow at speed 1 (relative speed $= 2 - 1 = 1$). Since the gap closes by 1 every step, they must meet within at most $C$ iterations.
</details>

<details>
<summary>2. Why does resetting one pointer to head and moving both at speed 1 find the cycle start?</summary>
Let $L$ be distance from head to cycle entrance, and $d$ be distance from entrance to meeting point. The slow pointer traveled $L + d$. The fast pointer traveled $L + d + kC$. Since fast moves at 2x speed: $2(L + d) = L + d + kC \implies L + d = kC \implies L = kC - d$. Thus, moving $L$ steps from head and $L$ steps from the meeting point lands both pointers simultaneously at the entrance!
</details>

<details>
<summary>3. What is the time and space complexity of Merging K Sorted Lists using a Priority Queue?</summary>
Time: $O(N \log K)$ where $N$ is total number of nodes across all lists, and $K$ is number of lists. Space: $O(K)$ auxiliary space to maintain the min-heap containing at most $K$ node references.
</details>

<details>
<summary>4. Why is a Dummy Head node used in linked list algorithms?</summary>
A dummy head eliminates special conditional checks when mutating or deleting the true head of the list, simplifying the code and eliminating `NullPointerException` edge cases.
</details>

<details>
<summary>5. How do you find the middle node of a linked list in a single pass?</summary>
Use Fast and Slow pointers. Fast moves 2 steps, Slow moves 1 step. When Fast reaches the end of the list, Slow is at the exact middle node.
</details>

<details>
<summary>6. What are the memory layout trade-offs of a Java `LinkedList` compared to `ArrayList`?</summary>
Each `LinkedList.Node` contains a 12-byte header, 4-byte padding, and three 8-byte references (`item`, `next`, `prev`), consuming 40 bytes per node on a 64-bit JVM without compressed OOPs. An `ArrayList` only stores references in a contiguous array (8 bytes per reference).
</details>

<details>
<summary>7. Can you reverse a singly linked list in $O(N)$ time and $O(1)$ space?</summary>
Yes, by using 3 pointers (`prev`, `curr`, `nextTemp`) to invert the direction of the `next` pointer in-place in a single iteration.
</details>

<details>
<summary>8. How do you check if a linked list is a palindrome in $O(N)$ time and $O(1)$ auxiliary space?</summary>
Find the middle node using Fast & Slow pointers, reverse the second half of the list in-place, compare the first half and reversed second half node-by-node, and optionally restore the second half back to original order.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Swap Nodes in Pairs (LeetCode 24)</summary>

```java
public class SwapPairs {
    public static ListNode swapPairs(ListNode head) {
        ListNode dummy = new ListNode(0, head);
        ListNode point = dummy;

        while (point.next != null && point.next.next != null) {
            ListNode swap1 = point.next;
            ListNode swap2 = point.next.next;

            swap1.next = swap2.next;
            swap2.next = swap1;
            point.next = swap2;
            point = swap1;
        }
        return dummy.next;
    }
}
```
</details>

<details>
<summary>Exercise 2: Copy List with Random Pointer (LeetCode 138)</summary>

```java
// O(N) Time, O(1) Auxiliary Space using interweaving nodes
class Node {
    int val;
    Node next, random;
    Node(int val) { this.val = val; }
}

public class CopyRandomList {
    public static Node copyRandomList(Node head) {
        if (head == null) return null;

        // 1. Interweave copy nodes: A -> A' -> B -> B'
        Node curr = head;
        while (curr != null) {
            Node copy = new Node(curr.val);
            copy.next = curr.next;
            curr.next = copy;
            curr = copy.next;
        }

        // 2. Assign random pointers
        curr = head;
        while (curr != null) {
            if (curr.random != null) curr.next.random = curr.random.next;
            curr = curr.next.next;
        }

        // 3. Separate lists
        curr = head;
        Node dummy = new Node(0);
        Node copyCurr = dummy;
        while (curr != null) {
            copyCurr.next = curr.next;
            curr.next = curr.next.next;
            copyCurr = copyCurr.next;
            curr = curr.next;
        }

        return dummy.next;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Dummy Head**: `new ListNode(0, head)` simplifies edge-case head deletions and list splices.
- **Fast & Slow**: Finds midpoint (speed 2:1) and detects cycles (Floyd's algorithm).
- **Cycle Entry Point**: Reset pointer to head after collision; advance both by 1.
- **Reversal**: 3 pointers (`prev`, `curr`, `nextTemp`) invert links in-place in $O(N)$ time and $O(1)$ space.
- **Heap Merge**: `PriorityQueue<ListNode>` merges $K$ sorted lists in $O(N \log K)$ time.

---

## 13. Further Reading

- [Robert Sedgewick, *Algorithms, 4th Edition* (Linked List Algorithms)](https://algs4.cs.princeton.edu/13stacks/)
- [LeetCode Linked List Card](https://leetcode.com/explore/learn/card/linked-list/)
