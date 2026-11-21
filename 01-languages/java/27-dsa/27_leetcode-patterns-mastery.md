# LeetCode Patterns Mastery: Blind 75 and NeetCode 150

> Comprehensive blueprint for technical coding interview dominance: Blind 75 and NeetCode 150 topic taxonomies, interview time budgets, live debugging protocols, and production vs competitive coding divergence.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | All notes in [27-dsa](README.md) (00 through 26) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8+ (Verified on Java 25 LTS) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [The Core Curriculum: Blind 75 vs NeetCode 150](#2-the-core-curriculum-blind-75-vs-neetcode-150)
3. [The 45-Minute Interview Time Budget](#3-the-45-minute-interview-time-budget)
4. [Master Category Taxonomies & Signature Problems](#4-master-category-taxonomies--signature-problems)
5. [Practical Live-Coding Problem Walkthroughs](#5-practical-live-coding-problem-walkthroughs)
6. [Best Practices for Technical Communication](#6-best-practices-for-technical-communication)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Production Engineering vs Competitive Coding](#8-production-engineering-vs-competitive-coding)
9. [Trade-Off Matrix: Time vs Space vs Simplicity](#9-trade-off-matrix-time-vs-space-vs-simplicity)
10. [Interview Meta-Questions & Behavioral Probes](#10-interview-meta-questions--behavioral-probes)
11. [Practice Exercises: The Benchmark Triad](#11-practice-exercises-the-benchmark-triad)
12. [Summary / Quick Reference Card](#12-summary--quick-reference-card)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Technical coding interviews conducted by Tier-1 technology companies (Google, Meta, Amazon, Microsoft, Apple, Netflix) are designed to evaluate three core dimensions:
1. **Algorithmic Competence**: Translating unstructured ambiguous requirements into optimal computational algorithms.
2. **Coding Fluidity & Precision**: Writing clean, bug-free, idiomatic Java code within a 25-minute live coding window.
3. **Engineering Communication**: Explaining trade-offs, thinking out loud, and responding proactively to hints and edge-case probes.

The **Blind 75** (curated by an ex-Meta tech lead) and **NeetCode 150** represent the gold standard problem distributions. Rather than grinding 2,000 arbitrary problems, mastering these 150 archetypes guarantees complete coverage of interview problem spaces.

---

## 2. The Core Curriculum: Blind 75 vs NeetCode 150

```
NeetCode 150 Category Distribution:
------------------------------------------------------------
1.  Arrays & Hashing (9 problems)      -> Two Pointers / Hash Tables
2.  Two Pointers (5 problems)          -> Boundary Convergence
3.  Sliding Window (6 problems)        -> Monotonic Substrings/Subarrays
4.  Stack (7 problems)                 -> Monotonic Stack / Matching
5.  Binary Search (7 problems)         -> Monotonic Search Spaces
6.  Linked List (11 problems)          -> Pointer Manipulation / Cycles
7.  Trees (15 problems)                -> Traversals, BST, Invariants
8.  Tries (3 problems)                 -> Prefix Matching / Max XOR
9.  Heap / Priority Queue (7 problems) -> Top-K / Median Balancing
10. Backtracking (9 problems)          -> Combinations / Permutations
11. Graphs (13 problems)               -> BFS, DFS, Dijkstra, Topo Sort
12. Advanced Graphs (6 problems)       -> Network Flow, MST, Bellman-Ford
13. 1D Dynamic Programming (12 prob)   -> Optimal Substructure / Rolling
14. 2D Dynamic Programming (11 prob)   -> Grid Paths / Alignments
15. Greedy (8 problems)                -> Exchange Arguments
16. Intervals (6 problems)             -> Overlap Coalescing / Sweeps
17. Math & Geometry (8 problems)       -> Fast Pow / Matrix Geometry
18. Bit Manipulation (7 problems)      -> Two's Complement / XOR
Total: 150 Problems (Blind 75 is a strict 50% subset)
```

---

## 3. The 45-Minute Interview Time Budget

A 45-minute technical coding interview requires disciplined pacing:

```
[00:00 - 05:00] Phase 1: Clarification & Constraint Analysis
                - Ask about input scale (N), duplicate elements, nulls, negative numbers.
                - Confirm return types and edge cases.
                - Write 2-3 sample test cases on the whiteboard/doc.

[05:00 - 12:00] Phase 2: Solution Architecture & Trade-Off Alignment
                - State the brute force approach and its time/space complexity.
                - Propose the optimal pattern (e.g. "We can optimize this to O(N) using Sliding Window").
                - Verbally walk through the algorithm steps before writing code.
                - Wait for the interviewer's explicit agreement to proceed!

[12:00 - 32:00] Phase 3: Live Implementation (20 Minutes)
                - Write clean, idiomatic Java using descriptive variable names.
                - Modularize code into clear helper methods.
                - Keep talking out loud: state invariants as you code.

[32:00 - 40:00] Phase 4: Dry-Run Verification & Edge-Case Probing
                - Manually trace the code line-by-line with a sample input.
                - Test boundary conditions: empty input, N=1, duplicates, extreme values.
                - Fix any off-by-one errors proactively before the interviewer points them out!

[40:00 - 45:00] Phase 5: Complexity Analysis & Q&A
                - Deliver authoritative Time Complexity (e.g., O(N log K)) and Space Complexity.
                - Ask thoughtful engineering questions about the company's systems.
```

---

## 4. Master Category Taxonomies & Signature Problems

### Category 1: Arrays & Hashing
- **Signature Problems**: Two Sum, Group Anagrams, Top K Frequent Elements, Product of Array Except Self, Longest Consecutive Sequence.
- **Core Strategy**: Map lookups for $O(1)$ complement lookups; frequency buckets for $O(N)$ linear sorting.

### Category 2: Two Pointers & Sliding Window
- **Signature Problems**: 3Sum, Container With Most Water, Trapping Rain Water, Longest Substring Without Repeating Characters, Minimum Window Substring.
- **Core Strategy**: Maintain monotonic window boundaries $[L, R]$; shrink $L$ upon constraint breach.

### Category 3: Trees & Graphs
- **Signature Problems**: Lowest Common Ancestor, Binary Tree Maximum Path Sum, Serialize and Deserialize Binary Tree, Course Schedule I & II, Word Ladder, Number of Islands, Alien Dictionary.
- **Core Strategy**: Kahn's in-degree BFS for DAGs; Bidirectional BFS for shortest path word ladders; post-order DFS for subtree synthesis.

### Category 4: Dynamic Programming & Greedy
- **Signature Problems**: Coin Change, Longest Increasing Subsequence, Edit Distance, Burst Balloons, Jump Game II, Partition Equal Subset Sum.
- **Core Strategy**: 4-step framework (State, Transition, Base Case, Order); reduce 2D tables to rolling 1D arrays.

---

## 5. Practical Live-Coding Problem Walkthroughs

### Walkthrough 1: Product of Array Except Self (LeetCode 238, $O(N)$ Time, $O(1)$ Extra Space)

```java
public class ProductExceptSelf {
    public int[] productExceptSelf(int[] nums) {
        int n = nums.length;
        int[] output = new int[n];

        // Pass 1: Prefix products stored directly in output array
        output[0] = 1;
        for (int i = 1; i < n; i++) {
            output[i] = output[i - 1] * nums[i - 1];
        }

        // Pass 2: Suffix products maintained in a single scalar variable
        int suffix = 1;
        for (int i = n - 1; i >= 0; i--) {
            output[i] *= suffix;
            suffix *= nums[i];
        }

        return output;
    }
}
```

### Walkthrough 2: Minimum Window Substring (LeetCode 76, Hard Sliding Window)

```java
public class MinimumWindowSubstring {
    public String minWindow(String s, String t) {
        if (s.length() < t.length()) return "";

        int[] targetFreq = new int[128];
        for (char c : t.toCharArray()) targetFreq[c]++;

        int[] windowFreq = new int[128];
        int required = 0;
        for (int count : targetFreq) {
            if (count > 0) required++;
        }

        int left = 0, formed = 0;
        int minLen = Integer.MAX_VALUE;
        int startIdx = 0;

        for (int right = 0; right < s.length(); right++) {
            char in = s.charAt(right);
            windowFreq[in]++;

            if (targetFreq[in] > 0 && windowFreq[in] == targetFreq[in]) {
                formed++;
            }

            // Shrink window from the left while all characters are satisfied
            while (left <= right && formed == required) {
                if (right - left + 1 < minLen) {
                    minLen = right - left + 1;
                    startIdx = left;
                }

                char out = s.charAt(left);
                windowFreq[out]--;
                if (targetFreq[out] > 0 && windowFreq[out] < targetFreq[out]) {
                    formed--;
                }
                left++;
            }
        }

        return minLen == Integer.MAX_VALUE ? "" : s.substring(startIdx, startIdx + minLen);
    }
}
```

### Walkthrough 3: Serialize and Deserialize Binary Tree (LeetCode 297, Hard Tree DFS)

```java
import java.util.*;

public class Codec {
    private static final String NULL_SENTINEL = "#";
    private static final String DELIMITER = ",";

    // Encodes a tree to a single string using pre-order traversal
    public String serialize(TreeNode root) {
        StringBuilder sb = new StringBuilder();
        buildString(root, sb);
        return sb.toString();
    }

    private void buildString(TreeNode node, StringBuilder sb) {
        if (node == null) {
            sb.append(NULL_SENTINEL).append(DELIMITER);
            return;
        }
        sb.append(node.val).append(DELIMITER);
        buildString(node.left, sb);
        buildString(node.right, sb);
    }

    // Decodes your encoded data to tree
    public TreeNode deserialize(String data) {
        Queue<String> tokens = new ArrayDeque<>(Arrays.asList(data.split(DELIMITER)));
        return buildTree(tokens);
    }

    private TreeNode buildTree(Queue<String> tokens) {
        String val = tokens.poll();
        if (val == null || val.equals(NULL_SENTINEL)) {
            return null;
        }

        TreeNode node = new TreeNode(Integer.parseInt(val));
        node.left = buildTree(tokens);
        node.right = buildTree(tokens);
        return node;
    }

    public static class TreeNode {
        int val;
        TreeNode left, right;
        TreeNode(int val) { this.val = val; }
    }
}
```

---

## 6. Best Practices for Technical Communication

1. **Think Out Loud Continuously**: If you are silent for more than 45 seconds, the interviewer cannot gauge your thought process. Say: "I'm currently considering how to handle the duplicate elements..."
2. **Never Jump Straight Into Code**: Writing code before agreeing on the high-level design is the #1 reason candidates fail coding interviews. Secure verbal alignment first.
3. **Use Meaningful Identifiers**: Avoid variables named `a`, `b`, `t1`, `temp2`. Use `currNode`, `windowStart`, `minDistance`, `inDegree`.
4. **Be Responsive to Hints**: Interviewers rarely give accidental hints. When an interviewer asks, "What would happen if the array had negative numbers?", treat it as an immediate course-correction signal.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Professional Protocol |
|---|---|---|
| Silent Coding | Interviewer cannot follow reasoning; low collaboration rating | Speak continuously; announce design decisions |
| Hasty Optimization | Gets stuck in complex data structure; runs out of time | State brute force first, then optimize incrementally |
| Sloppy Edge Cases | Bug discovered by interviewer during review | Proactively test empty input, single element, negative numbers |
| Hand-waving Complexity | Vague "It's roughly $O(N)$" answers degrade seniority rating | Give rigorous bounds: "Time is $O(V + E)$ because each vertex and edge is visited once; Auxiliary space is $O(V)$ for the queue." |

---

## 8. Production Engineering vs Competitive Coding

Interview coding exists at the intersection of production systems and competitive algorithms:

| Dimension | Competitive Coding (CP) | Production Engineering & Interviews |
|---|---|---|
| **Naming Conventions** | Terse: `n`, `m`, `ans`, `f[][]` | Descriptive: `studentCount`, `memoizedDistances` |
| **Error Handling** | None (assume valid input) | Input validation, null checks, boundary defensive guards |
| **Data Structures** | Primitive arrays, static globals | Modern Java Collections, records, clean encapsulation |
| **Memory Allocation** | Global static buffers `int a[1000000]` | Dynamic allocation, awareness of GC pressure and heap locality |
| **Code Structure** | Single monolithic `main()` function | Modularized helper methods with single responsibilities |

---

## 9. Trade-Off Matrix: Time vs Space vs Simplicity

Every algorithmic choice embodies an engineering trade-off:

```
[Brute Force]          Simple code, $O(1)$ space, but intolerable $O(N^2)$ or $O(2^N)$ time.
       |
       v (Trade space for time)
[Hash Table / Set]     Fast $O(N)$ time, but consumes $O(N)$ heap memory and incurs GC overhead.
       |
       v (Sort first)
[Two Pointers / BS]    Reduces space to $O(1)$, but increases time to $O(N \log N)$ due to sorting.
       |
       v (Hardware optimization)
[Bitmask / Arrays]     Optimal $O(N)$ time, $O(1)$ space, exceptional cache locality, but bounded to fixed bit widths.
```

---

## 10. Interview Meta-Questions & Behavioral Probes

<details>
<summary>1. "How would this algorithm perform if the dataset does not fit into RAM (e.g. 10 Terabytes on disk)?"</summary>

Transition to External Memory / Distributed algorithms:
1. **External Merge Sort**: Split the 10TB file into 1GB chunks that fit in RAM, sort each chunk in memory, write back to disk, then perform a K-Way Merge using a Min-Heap streaming from disk buffers.
2. **MapReduce / Distributed Framework**: Partition keys across worker nodes using hash-based sharding.
</details>

<details>
<summary>2. "How would you make this solution thread-safe for concurrent read/write traffic?"</summary>

1. Read-heavy workloads: Use `java.util.concurrent.locks.ReentrantReadWriteLock` or `StampedLock` for optimistic reading.
2. High-throughput concurrent maps: Replace `HashMap` with `ConcurrentHashMap` (striped lock buckets).
3. Concurrent queues: Use lock-free `ConcurrentLinkedQueue` or bounded `ArrayBlockingQueue`.
</details>

<details>
<summary>3. "Can this time complexity be improved from $O(N \log N)$ to $O(N)$?"</summary>

Check if:
1. Inputs are bounded integers: Use Counting Sort, Radix Sort, or Bucket Sort ($O(N)$).
2. Elements are in range $[1 \dots N]$: Use Cyclic Sort ($O(N)$ time, $O(1)$ space).
3. Monotonic property exists: Replace binary search or sorting with Two Pointers / Sliding Window.
</details>

<details>
<summary>4. "What happens if this recursive method receives an input tree with depth $100,000$?"</summary>

HotSpot JVM default thread stack size is 1MB (`-Xss1m`), which accommodates approximately 5,000 to 10,000 recursive stack frames before throwing a `StackOverflowError`. To handle depth $100,000$, convert the recursion to an iterative loop using an explicit heap-allocated `ArrayDeque` as a stack.
</details>

<details>
<summary>5. "Why did you choose `ArrayDeque` instead of `LinkedList` or `Stack`?"</summary>

1. `java.util.Stack` is legacy synchronized, incurring heavy synchronization lock overhead on every operation.
2. `LinkedList` allocates an independent `Node` object for every pushed element (24 bytes overhead per node) and causes CPU cache misses due to pointer chasing.
3. `ArrayDeque` is backed by a circular contiguous array, has zero per-element allocation overhead, and provides superior CPU L1 cache locality.
</details>

---

## 11. Practice Exercises: The Benchmark Triad

### Exercise 1: Median of Two Sorted Arrays (LeetCode 4, Hard Binary Search)

<details>
<summary>Solution</summary>

```java
public class MedianTwoSortedArrays {
    public double findMedianSortedArrays(int[] nums1, int[] nums2) {
        if (nums1.length > nums2.length) {
            return findMedianSortedArrays(nums2, nums1); // Ensure nums1 is smaller
        }

        int m = nums1.length, n = nums2.length;
        int low = 0, high = m;

        while (low <= high) {
            int partitionX = (low + high) / 2;
            int partitionY = (m + n + 1) / 2 - partitionX;

            int maxLeftX = (partitionX == 0) ? Integer.MIN_VALUE : nums1[partitionX - 1];
            int minRightX = (partitionX == m) ? Integer.MAX_VALUE : nums1[partitionX];

            int maxLeftY = (partitionY == 0) ? Integer.MIN_VALUE : nums2[partitionY - 1];
            int minRightY = (partitionY == n) ? Integer.MAX_VALUE : nums2[partitionY];

            if (maxLeftX <= minRightY && maxLeftY <= minRightX) {
                if ((m + n) % 2 == 0) {
                    return (Math.max(maxLeftX, maxLeftY) + Math.min(minRightX, minRightY)) / 2.0;
                } else {
                    return Math.max(maxLeftX, maxLeftY);
                }
            } else if (maxLeftX > minRightY) {
                high = partitionX - 1;
            } else {
                low = partitionX + 1;
            }
        }

        throw new IllegalArgumentException("Input arrays not sorted.");
    }
}
```
</details>

### Exercise 2: Merge K Sorted Lists (LeetCode 23, Hard K-Way Heap Merge)

<details>
<summary>Solution</summary>

```java
import java.util.PriorityQueue;
import java.util.Comparator;

public class MergeKSortedListsHard {
    public static class ListNode {
        int val;
        ListNode next;
        ListNode(int val) { this.val = val; }
    }

    public ListNode mergeKLists(ListNode[] lists) {
        if (lists == null || lists.length == 0) return null;

        PriorityQueue<ListNode> pq = new PriorityQueue<>(
            lists.length, Comparator.comparingInt(node -> node.val)
        );

        for (ListNode head : lists) {
            if (head != null) pq.offer(head);
        }

        ListNode dummy = new ListNode(0);
        ListNode tail = dummy;

        while (!pq.isEmpty()) {
            ListNode minNode = pq.poll();
            tail.next = minNode;
            tail = tail.next;

            if (minNode.next != null) {
                pq.offer(minNode.next);
            }
        }

        return dummy.next;
    }
}
```
</details>

### Exercise 3: LRU Cache (LeetCode 146, Hard System Pattern / Hash + DLL)

<details>
<summary>Solution</summary>

```java
import java.util.HashMap;
import java.util.Map;

public class LRUCache {
    static class Node {
        int key, val;
        Node prev, next;
        Node(int key, int val) { this.key = key; this.val = val; }
    }

    private final int capacity;
    private final Map<Integer, Node> map = new HashMap<>();
    private final Node head = new Node(0, 0);
    private final Node tail = new Node(0, 0);

    public LRUCache(int capacity) {
        this.capacity = capacity;
        head.next = tail;
        tail.prev = head;
    }

    public int get(int key) {
        Node node = map.get(key);
        if (node == null) return -1;
        moveToHead(node);
        return node.val;
    }

    public void put(int key, int value) {
        Node node = map.get(key);
        if (node != null) {
            node.val = value;
            moveToHead(node);
        } else {
            if (map.size() == capacity) {
                Node lru = tail.prev;
                removeNode(lru);
                map.remove(lru.key);
            }
            Node newNode = new Node(key, value);
            map.put(key, newNode);
            addNode(newNode);
        }
    }

    private void addNode(Node node) {
        node.next = head.next;
        node.prev = head;
        head.next.prev = node;
        head.next = node;
    }

    private void removeNode(Node node) {
        node.prev.next = node.next;
        node.next.prev = node.prev;
    }

    private void moveToHead(Node node) {
        removeNode(node);
        addNode(node);
    }
}
```
</details>

---

## 12. Summary / Quick Reference Card

- **Blind 75 & NeetCode 150**: The ultimate high-yield interview curricula.
- **Pacing Protocol**: 5 min clarify $\to$ 7 min design $\to$ 20 min code $\to$ 8 min dry-run $\to$ 5 min Q&A.
- **Data Structure Invariants**: Always articulate loop and data structure invariants before writing loops.
- **Clean Java**: Avoid raw types, use `ArrayDeque`, enforce boundary defensive checks, and use descriptive names.

---

## 13. Further Reading

- [NeetCode.io: The 150 Practice Roadmap](https://neetcode.io/practice)
- [Blind 75 Curated LeetCode List](https://leetcode.com/discuss/general-discussion/460599/blind-75-leetcode-questions)
- [Cracking the Coding Interview (6th Edition) by Gayle Laakmann McDowell](https://www.crackingthecodinginterview.com/)
- [System Design Interview by Alex Xu](https://bytebytego.com/)
