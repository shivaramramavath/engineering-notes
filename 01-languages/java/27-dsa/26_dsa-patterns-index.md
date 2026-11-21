# Data Structures & Algorithms Patterns Index

> The definitive taxonomy of the 16 foundational algorithmic patterns: problem signatures, invariant conditions, time-space complexities, and decision frameworks.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | All notes in [27-dsa](README.md) (00 through 25) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8+ (Verified on Java 25 LTS) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [The 16 Master Algorithmic Patterns](#2-the-16-master-algorithmic-patterns)
3. [Pattern Decision Framework & Flowchart](#3-pattern-decision-framework--flowchart)
4. [Master Complexity & Trade-Off Matrix](#4-master-complexity--trade-off-matrix)
5. [Pattern Deep Dives & Canonical Code Blueprints](#5-pattern-deep-dives--canonical-code-blueprints)
6. [Best Practices in Pattern Recognition](#6-best-practices-in-pattern-recognition)
7. [Common Mistakes & False Pattern Matches](#7-common-mistakes--false-pattern-matches)
8. [Performance & JVM Optimization Notes](#8-performance--jvm-optimization-notes)
9. [Pattern Combinations & Hybrid Problems](#9-pattern-combinations--hybrid-problems)
10. [Interview Questions & Meta Strategies](#10-interview-questions--meta-strategies)
11. [Practice Exercises & Synthesis Challenges](#11-practice-exercises--synthesis-challenges)
12. [Summary / Master Pattern Cheat Sheet](#12-summary--master-pattern-cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Software engineering technical interviews and algorithmic system design problems do not require memorizing hundreds of disconnected solutions. Over 90% of all algorithmic problems reduce directly to one of **16 core algorithmic patterns**.

Mastering pattern recognition allows you to:
1. **Deconstruct Novel Problems**: Classify an unseen problem into its governing invariant within 60 seconds.
2. **Guarantee Optimal Asymptotics**: Choose the optimal data structure and time-space boundary before writing a single line of code.
3. **Avoid Brute-Force Traps**: Prevent costly dead ends during time-constrained live coding interviews.

---

## 2. The 16 Master Algorithmic Patterns

```
1.  Two Pointers (Converging / Diverging)
2.  Sliding Window (Fixed & Dynamic Bounded Subarrays)
3.  Fast & Slow Pointers (Floyd's Tortoise & Hare Cycle Finding)
4.  Merge Intervals (Interval Scheduling & Disjoint Sets)
5.  Cyclic Sort (Linear $O(N)$ In-Place Index Hashing)
6.  In-Place Reversal of LinkedList (Iterative Pointer Re-wiring)
7.  Tree Breadth-First Search (Level-Order Queue Traversals)
8.  Tree Depth-First Search (Pre/In/Post-Order Recursion & Backtracking)
9.  Two Heaps (Continuous Median & Stream Extremum Balancing)
10. Subsets & Combinations (Breadth & Backtracking Combinatorial Power Sets)
11. Modified Binary Search (Transformed Monotonic Search Spaces)
12. Bitwise XOR & Manipulation (Masking, Cancellation, In-Place Arithmetic)
13. Top 'K' Elements (Bounded Min/Max Priority Queues)
14. K-Way Merge (Multi-Stream Sorted Merging)
15. 0/1 Knapsack & Dynamic Programming (Bounded Subset Sum Optimization)
16. Topological Sort (DAG In-Degree Reduction & Dependency Resolution)
```

---

## 3. Pattern Decision Framework & Flowchart

When presented with an algorithmic problem, evaluate its constraints and inputs against this decision hierarchy:

```
                                  [Problem Input]
                                         |
     +-----------------------------------+-----------------------------------+
     | (Linear Data: Array, String)      | (Non-Linear: Tree, Graph, Intervals)|
     v                                   v
[Is input sorted or orderable?]       [Is it an Interval / Overlap problem?]
  |-- YES -> Two Pointers / Binary Search  |-- YES -> Merge Intervals / Line Sweep
  |-- NO  -> [Subarray / Substring?]       |-- NO  -> [Is it a Tree?]
               |-- Contiguous window?                    |-- Level-by-level? -> Tree BFS
               |     |-- YES -> Sliding Window           |-- Path sums / subtrees? -> Tree DFS
               |-- Cycle in linked list?                 |-- Cyclic dependencies / DAG?
               |     |-- YES -> Fast & Slow Pointers     |     |-- YES -> Topological Sort
               |-- Numbers in range 1..N?                |-- Graph connectivity / paths?
               |     |-- YES -> Cyclic Sort              |     |-- Weighted -> Dijkstra
               |-- Find Top K / Streaming Median?        |     |-- Unweighted -> BFS
                     |-- Median -> Two Heaps             |     |-- Disjoint sets -> DSU
                     |-- Top K -> Min-Heap (size K)
```

---

## 4. Master Complexity & Trade-Off Matrix

| Pattern | Input Type | Standard Time | Auxiliary Space | Key Data Structure |
|---|---|---|---|---|
| **Two Pointers** | Sorted Array | $O(N)$ | $O(1)$ | Two scalar pointers |
| **Sliding Window** | Array / String | $O(N)$ | $O(K)$ or $O(1)$ | Hash table / Array frequencies |
| **Fast & Slow** | Linked List / Array | $O(N)$ | $O(1)$ | Two pointer references |
| **Merge Intervals** | Interval Array | $O(N \log N)$ | $O(N)$ | Sorted Array / Priority Queue |
| **Cyclic Sort** | Array $[1 \dots N]$ | $O(N)$ | $O(1)$ | In-place array swaps |
| **In-Place Reversal** | Linked List | $O(N)$ | $O(1)$ | Three pointer references |
| **Tree BFS** | Binary Tree / Graph | $O(V)$ | $O(W)$ | `ArrayDeque` (Queue) |
| **Tree DFS** | Binary Tree | $O(V)$ | $O(H)$ | Call stack / `ArrayDeque` |
| **Two Heaps** | Stream / Array | $O(N \log N)$ | $O(N)$ | Min-Heap + Max-Heap |
| **Subsets** | Array / String | $O(2^N)$ | $O(N)$ call stack | Backtracking recursion |
| **Modified BS** | Rotated / Monotonic | $O(\log N)$ | $O(1)$ | Scalar pointers `low`, `high` |
| **Bitwise XOR** | Integers | $O(N)$ or $O(1)$ | $O(1)$ | Bitwise primitive operators |
| **Top 'K'** | Unsorted Collection | $O(N \log K)$ | $O(K)$ | Bounded Min-Heap |
| **K-Way Merge** | $K$ Sorted Streams | $O(N \log K)$ | $O(K)$ | Priority Queue of size $K$ |
| **0/1 Knapsack** | Items + Capacity | $O(N \times W)$ | $O(W)$ | 1D Rolling DP Array |
| **Topological Sort**| Directed Graph | $O(V + E)$ | $O(V)$ | In-degree array + Queue |

---

## 5. Pattern Deep Dives & Canonical Code Blueprints

### Blueprint 1: Sliding Window (Dynamic Bounded)
Used for finding the longest or shortest contiguous subarray satisfying constraint $C$.

```java
public class SlidingWindowBlueprint {
    public static int longestWindow(String s) {
        int[] freq = new int[128];
        int left = 0, maxLen = 0;

        for (int right = 0; right < s.length(); right++) {
            char in = s.charAt(right);
            freq[in]++;

            // Shrink window while invalid
            while (/* window condition violated */ freq[in] > 1) {
                char out = s.charAt(left++);
                freq[out]--;
            }

            maxLen = Math.max(maxLen, right - left + 1);
        }

        return maxLen;
    }
}
```

### Blueprint 2: Two Heaps (Running Stream Median)
Maintains two balanced heaps: Max-Heap for lower half, Min-Heap for upper half.

```java
import java.util.*;

public class TwoHeapsBlueprint {
    private final PriorityQueue<Integer> lowerMax = new PriorityQueue<>(Collections.reverseOrder());
    private final PriorityQueue<Integer> upperMin = new PriorityQueue<>();

    public void insert(int val) {
        lowerMax.offer(val);
        upperMin.offer(lowerMax.poll());
        if (upperMin.size() > lowerMax.size()) {
            lowerMax.offer(upperMin.poll());
        }
    }

    public double getMedian() {
        return lowerMax.size() > upperMin.size() ? lowerMax.peek() : (lowerMax.peek() + upperMin.peek()) / 2.0;
    }
}
```

### Blueprint 3: Cyclic Sort (Range $[1 \dots N]$ in $O(N)$ Time & $O(1)$ Space)
Places each number at its correct index (`nums[i] - 1`) in-place.

```java
public class CyclicSortBlueprint {
    public static void sortInPlace(int[] nums) {
        int i = 0;
        while (i < nums.length) {
            int correctIndex = nums[i] - 1;
            if (nums[i] > 0 && nums[i] <= nums.length && nums[i] != nums[correctIndex]) {
                int temp = nums[i];
                nums[i] = nums[correctIndex];
                nums[correctIndex] = temp;
            } else {
                i++;
            }
        }
    }
}
```

---

## 6. Best Practices in Pattern Recognition

1. **State Invariant First**: Before coding, state the invariant that must hold true at every iteration (e.g., "The window $[L, R]$ always contains at most $K$ distinct characters").
2. **Check for Implicit Monotonicity**: If sorting the array doesn't disrupt the answer (e.g., 3Sum, Subarray Sums), sorting first ($O(N \log N)$) often unlocks Two Pointers or Binary Search.
3. **Prefer Flat Arrays Over HashMaps**: In sliding window problems over ASCII strings, `int[128]` runs 10x faster and uses zero heap memory compared to `Map<Character, Integer>`.
4. **Identify Symmetries for Reverse Operations**: In interval problems, sorting by start time solves overlapping union; sorting by end time solves maximum scheduling.

---

## 7. Common Mistakes & False Pattern Matches

| False Assumption | Reality | Correct Pattern |
|---|---|---|
| Using Two Pointers on an unsorted array | Two pointers can only move monotonically if data is sorted | Sort first, or use Hash Map |
| Applying Sliding Window to arrays with negative numbers | Window cannot monotonically expand/shrink when negative numbers alter sums | Prefix Sums + Hash Map |
| Greedy on arbitrary coin denominations | Greedy choice fails on non-canonical currency systems | 1D Dynamic Programming |
| Binary Search requiring strict sorting | Binary search only requires a monotonic **boolean predicate** $P(x)$ | Binary Search on Answer Space |
| Using DFS for Shortest Path in unweighted graphs | DFS explores branches deeply without guarantee of minimum distance | BFS (Breadth-First Search) |

---

## 8. Performance & JVM Optimization Notes

- **Object Creation in Hot Loops**: Avoid creating new `Integer`, `Pair`, or `record` objects inside sliding window or DFS recursion loops. Use primitive variables and flat arrays.
- **Queue Selection**: Always instantiate `new ArrayDeque<>()` for BFS queues and DFS stacks. Never use `java.util.LinkedList` (24 bytes per node overhead).
- **Bitwise Intrinsics**: Use `Integer.bitCount()` and `Integer.numberOfLeadingZeros()` to leverage hardware instruction sets (`POPCNT`, `LZCNT`).

---

## 9. Pattern Combinations & Hybrid Problems

Complex production and interview problems frequently combine two or more patterns:
- **Trie + 2D Grid Backtracking**: Word Search II (Trie prefix pruning guides grid DFS).
- **Binary Search + Sliding Window**: Maximum Average Subarray II.
- **Topological Sort + Dynamic Programming**: Longest Path in a DAG, Parallel Task Scheduling.
- **Monotonic Stack + Dynamic Programming**: Largest Rectangle in Histogram, Trapping Rain Water.
- **Fast & Slow Pointers + In-Place Reversal**: Palindrome Linked List (Find midpoint, reverse second half, compare, restore).

---

## 10. Interview Questions & Meta Strategies

<details>
<summary>1. How do you decide whether a problem requires Sliding Window or Prefix Sums + Hash Table?</summary>

- **Sliding Window** requires that expanding the window monotonically increases the property (e.g., all numbers positive, so sum strictly increases).
- If the array contains **negative numbers**, adding an element might decrease the sum, destroying monotonicity. In such cases, sliding window fails and you must use **Prefix Sums with a Hash Map** ($O(N)$ time, $O(N)$ space).
</details>

<details>
<summary>2. Why does Cyclic Sort achieve $O(N)$ time despite having a while loop?</summary>

In every swap, at least one number is placed into its permanently correct index $nums[i] - 1$. Since an array of length $N$ has only $N$ elements, at most $N - 1$ swaps can ever occur across the entire algorithm. When an element is already in place, pointer $i$ increments. Therefore, the total operations are bounded by $2N = O(N)$.
</details>

<details>
<summary>3. What is "Binary Search on the Answer Space"?</summary>

When finding the minimum or maximum value $X$ satisfying a condition, and checking whether candidate $X$ is valid takes $O(N)$ time via a greedy check or sliding window. If the validity predicate $P(X)$ is monotonic (e.g. valid for all $X \ge \text{ans}$, invalid for all $X < \text{ans}$), binary search finds the optimal value in $O(N \log(\text{range}))$.
</details>

<details>
<summary>4. When should you choose BFS over DFS for tree problems?</summary>

- Use **BFS** when the problem involves levels, nearest neighbors, shortest path, or printing the tree horizontally.
- Use **DFS** when the problem involves root-to-leaf paths, backtracking combinations, subtree validations, or maximum depth.
</details>

<details>
<summary>5. How does the K-Way Merge pattern operate in $O(N \log K)$?</summary>

Initialize a Min-Heap with the first element of each of the $K$ sorted streams. In each step, poll the smallest element from the heap and append it to the result. Then push the next element from that same stream into the heap. The heap size never exceeds $K$, bounding each extraction to $O(\log K)$ for $N$ total elements.
</details>

<details>
<summary>6. How can you detect if a graph problem requires Topological Sort?</summary>

1. The graph is directed.
2. The problem involves dependencies, prerequisites, build orders, or scheduling.
3. It asks for a valid linear sequence or requires cycle detection.
</details>

<details>
<summary>7. What is the Monotonic Stack pattern and when is it used?</summary>

A stack that maintains elements in strictly ascending or descending order. Used to find the **Next Greater Element**, **Next Smaller Element**, or largest rectangular areas under histograms in linear $O(N)$ time.
</details>

<details>
<summary>8. How do you handle live coding under pressure using the Pattern Method?</summary>

1. **Clarify Constraints & Signature**: Identify input size $N$, element ranges, and memory bounds.
2. **Name the Pattern Out Loud**: Announce "This problem matches the Sliding Window pattern because all inputs are positive and we seek the longest valid contiguous subarray."
3. **State the Invariant**: "Window $[L, R]$ will maintain at most $K$ distinct elements."
4. **Draft Blueprint Code**: Write the canonical `while (invalid) { shrink(); }` template.
5. **Trace Edge Cases**: Test empty input, single element, all identical elements, and extremes.
</details>

---

## 11. Practice Exercises & Synthesis Challenges

### Exercise 1: Palindrome Linked List (LeetCode 234, Hybrid Fast/Slow + In-Place Reversal)

<details>
<summary>Solution</summary>

```java
public class PalindromeLinkedList {
    public static class ListNode {
        int val;
        ListNode next;
        ListNode(int val) { this.val = val; }
    }

    public boolean isPalindrome(ListNode head) {
        if (head == null || head.next == null) return true;

        // 1. Fast & Slow to find middle
        ListNode slow = head, fast = head;
        while (fast.next != null && fast.next.next != null) {
            slow = slow.next;
            fast = fast.next.next;
        }

        // 2. In-Place Reversal of second half
        ListNode secondHalf = reverse(slow.next);

        // 3. Compare first and second halves
        ListNode p1 = head, p2 = secondHalf;
        boolean palindrome = true;
        while (p2 != null) {
            if (p1.val != p2.val) {
                palindrome = false;
                break;
            }
            p1 = p1.next;
            p2 = p2.next;
        }

        // 4. Restore original list
        slow.next = reverse(secondHalf);
        return palindrome;
    }

    private ListNode reverse(ListNode head) {
        ListNode prev = null, curr = head;
        while (curr != null) {
            ListNode next = curr.next;
            curr.next = prev;
            prev = curr;
            curr = next;
        }
        return prev;
    }
}
```
</details>

### Exercise 2: Subarray Sum Equals K (LeetCode 560, Prefix Sum + Hash Map)

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class SubarraySumEqualsK {
    public int subarraySum(int[] nums, int k) {
        Map<Integer, Integer> prefixCounts = new HashMap<>();
        prefixCounts.put(0, 1);

        int runningSum = 0;
        int count = 0;

        for (int num : nums) {
            runningSum += num;
            // runningSum - (runningSum - k) = k
            count += prefixCounts.getOrDefault(runningSum - k, 0);
            prefixCounts.put(runningSum, prefixCounts.getOrDefault(runningSum, 0) + 1);
        }

        return count;
    }
}
```
</details>

### Exercise 3: Trapping Rain Water (LeetCode 42, Two Pointers Space-Optimized)

<details>
<summary>Solution</summary>

```java
public class TrappingRainWater {
    public int trap(int[] height) {
        if (height == null || height.length <= 2) return 0;

        int left = 0, right = height.length - 1;
        int leftMax = 0, rightMax = 0;
        int totalWater = 0;

        while (left < right) {
            if (height[left] < height[right]) {
                if (height[left] >= leftMax) {
                    leftMax = height[left];
                } else {
                    totalWater += leftMax - height[left];
                }
                left++;
            } else {
                if (height[right] >= rightMax) {
                    rightMax = height[right];
                } else {
                    totalWater += rightMax - height[right];
                }
                right--;
            }
        }

        return totalWater;
    }
}
```
</details>

---

## 12. Summary / Master Pattern Cheat Sheet

- **Two Pointers**: Sorted arrays; converge from boundaries inwards in $O(N)$.
- **Sliding Window**: Contiguous arrays/strings; expand `right`, shrink `left` on invariant breach.
- **Fast & Slow**: Cycles in linked lists/arrays; collision confirms cycle in $O(N)$ time and $O(1)$ space.
- **Cyclic Sort**: Contiguous range $1 \dots N$; swap each number to `nums[i] - 1` in $O(N)$ time and $O(1)$ space.
- **Two Heaps**: Running stream median; balance Min-Heap and Max-Heap sizes.
- **Top 'K'**: Min-Heap of size $K$ retains the $K$ largest elements in $O(N \log K)$.
- **Topological Sort**: Kahn's algorithm over in-degrees resolves DAG dependencies in $O(V + E)$.

---

## 13. Further Reading

- [Grokking the Coding Interview: Patterns for Coding Questions](https://www.designgurus.io/course/grokking-the-coding-interview)
- [Introduction to Algorithms (CLRS), Part I-VI](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [LeetCode Patterns by Sean Prashad](https://seanprashad.com/leetcode-patterns/)
- [NeetCode 150 Pattern Roadmap](https://neetcode.io/roadmap)
