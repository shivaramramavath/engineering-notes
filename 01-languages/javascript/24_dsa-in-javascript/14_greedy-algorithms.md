# Greedy Algorithms in JavaScript

> A production-grade algorithmic guide to Greedy Algorithms in JavaScript, covering the Greedy Choice Property, Interval Scheduling, Exchange Arguments, and resource optimization.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [10_sorting-algorithms.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/10_sorting-algorithms.md) |
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

In computational problem solving, exploring every possible decision branch using Backtracking or Dynamic Programming can require substantial CPU time and memory tables. However, for a specific class of optimization problems, a much faster approach exists.

A **Greedy Algorithm** builds a solution piece by piece, always making the **locally optimal choice** at each decision point in the hope that these local choices lead to a globally optimal solution.
- **Ultra-Fast Execution**: Does not backtrack or re-evaluate past choices, typically running in **$O(N \log N)$** (sorting bound) or **$O(N)$** linear time.
- **Minimal Space Footprint**: Requires no $O(M \times N)$ dynamic programming matrices or recursive call stacks ($O(1)$ auxiliary memory).
- **Core Real-World Uses**: Network routing (Dijkstra/OSPF), data compression (Huffman coding), CPU task scheduling, and cash denomination dispensing.

---

## 2. Core Concepts

### 1. The Greedy Choice Property
A global optimum can be arrived at by selecting a local optimum. When considering which choice to make, we make the choice that looks best right now without considering results from subproblems.

### 2. Optimal Substructure Prerequisite
A problem exhibits optimal substructure if an optimal solution to the problem contains optimal solutions to subproblems (shared with Dynamic Programming).

### 3. Proving Greedy Correctness (The Exchange Argument)
To prove that a greedy algorithm produces the true global optimum, engineers use an **Exchange Argument** ("Greedy Stays Ahead"):
- Assume an optimal alternative solution $O$ exists that differs from the greedy solution $G$.
- Show that exchanging the first differing choice in $O$ with the greedy choice from $G$ results in a solution that is at least as good as $O$.
- By mathematical induction, $G$ must be globally optimal.

```
Decision Tree Comparison:
Dynamic Programming: Explores all valid sub-branches to find global optimum
                        [ State ]
                       /    |    \
                     [A]   [B]   [C] ──► (Evaluates all three)

Greedy Algorithm:    Commits irreversibly to the best immediate candidate
                        [ State ]
                            |
                           [B] (Only B is explored; A and C discarded forever)
```

---

## 3. How It Works Internally

Most greedy algorithms follow a two-phase execution flow:
1. **Pre-processing (Sorting or Priority Queue)**: Orders candidates by a critical metric (e.g. interval end time, cost-to-weight ratio, deadline). This takes $O(N \log N)$.
2. **Greedy Linear Scan**: Iterates through the sorted candidates, accepting any candidate that satisfies constraints and discarding those that conflict. This takes $O(N)$.

```
Interval Scheduling: Sort by End Time:
Job 1: [====]                   (Ends at 3)  ──► ACCEPT
Job 2:    [========]            (Ends at 7)  ──► REJECT (Overlaps Job 1)
Job 3:       [====]             (Ends at 6)  ──► ACCEPT (Starts at 3 >= 3)
```

---

## 4. Syntax & API Reference

### Universal Interval Scheduling Engine
```javascript
/**
 * Canonical Greedy Interval Scheduling
 * Selects maximum number of non-overlapping intervals.
 * Time: O(N log N) | Space: O(1)
 */
export function scheduleMaxIntervals(intervals) {
  if (intervals.length <= 1) return intervals;

  // 1. Sort by finish time ascending (The Greedy Invariant)
  intervals.sort((a, b) => a[1] - b[1]);

  const selected = [intervals[0]];
  let lastFinishTime = intervals[0][1];

  // 2. Greedy selection
  for (let i = 1; i < intervals.length; i++) {
    const [start, end] = intervals[i];

    // If interval starts after or when previous finishes, accept it
    if (start >= lastFinishTime) {
      selected.push(intervals[i]);
      lastFinishTime = end;
    }
  }

  return selected;
}
```

---

## 5. Practical Examples

### Example 1: Non-Overlapping Intervals (Minimum Removals)
```javascript
export function eraseOverlapIntervals(intervals) {
  if (intervals.length <= 1) return 0;

  // Sort by end time
  intervals.sort((a, b) => a[1] - b[1]);

  let removedCount = 0;
  let lastEnd = intervals[0][1];

  for (let i = 1; i < intervals.length; i++) {
    const [start, end] = intervals[i];

    if (start < lastEnd) {
      // Overlap detected: greedily remove the one with larger end time
      removedCount++;
    } else {
      lastEnd = end;
    }
  }

  return removedCount;
}
```

### Example 2: Jump Game (Greedy Max Reachable Frontier)
```javascript
/**
 * Determine if you can reach the last index.
 * Time Complexity: O(N) | Space Complexity: O(1)
 */
export function canJump(nums) {
  let maxReach = 0;

  for (let i = 0; i < nums.length; i++) {
    // If current index is beyond maximum reachable frontier, impossible
    if (i > maxReach) return false;

    maxReach = Math.max(maxReach, i + nums[i]);

    // Early termination if last index is reachable
    if (maxReach >= nums.length - 1) return true;
  }

  return true;
}

// console.log(canJump([2, 3, 1, 1, 4])); // true
// console.log(canJump([3, 2, 1, 0, 4])); // false (stuck at index 3)
```

### Example 3: Gas Station (Circular Tour Circuit)
```javascript
export function canCompleteCircuit(gas, cost) {
  let totalSurplus = 0;
  let currentSurplus = 0;
  let startStation = 0;

  for (let i = 0; i < gas.length; i++) {
    const netGain = gas[i] - cost[i];
    totalSurplus += netGain;
    currentSurplus += netGain;

    // If current tank drops below 0, station cannot be starting point
    if (currentSurplus < 0) {
      startStation = i + 1; // Greedily reset starting station to next index
      currentSurplus = 0;
    }
  }

  // If total gas >= total cost, a valid starting station is guaranteed to exist
  return totalSurplus >= 0 ? startStation : -1;
}
```

### Example 4: Assign Cookies (Greedy Contentment)
```javascript
export function findContentChildren(g, s) {
  // g: greed factors, s: cookie sizes
  g.sort((a, b) => a - b);
  s.sort((a, b) => a - b);

  let childIdx = 0;
  let cookieIdx = 0;

  // Greedily satisfy child with smallest greed using smallest sufficient cookie
  while (childIdx < g.length && cookieIdx < s.length) {
    if (s[cookieIdx] >= g[childIdx]) {
      childIdx++; // Child satisfied
    }
    cookieIdx++; // Cookie used
  }

  return childIdx;
}
```

### Example 5: Fractional Knapsack (Value-to-Weight Ratio)
```javascript
export function fractionalKnapsack(items, capacity) {
  // items: [{ weight, value }]
  // Sort items descending by value-to-weight ratio
  items.sort((a, b) => b.value / b.weight - a.value / a.weight);

  let totalValue = 0;
  let remainingCapacity = capacity;

  for (const item of items) {
    if (remainingCapacity === 0) break;

    if (item.weight <= remainingCapacity) {
      // Take full item
      totalValue += item.value;
      remainingCapacity -= item.weight;
    } else {
      // Take fractional part
      const fraction = remainingCapacity / item.weight;
      totalValue += item.value * fraction;
      remainingCapacity = 0;
    }
  }

  return totalValue;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Greedy Failing Where DP Is Required (0/1 Knapsack)**:
   In the **0/1 Knapsack Problem** (items cannot be divided), choosing items by highest value-to-weight ratio fails:
   - Capacity: 50. Items: A ($w=10, v=60, \text{ratio}=6$), B ($w=20, v=100, \text{ratio}=5$), C ($w=30, v=120, \text{ratio}=4$).
   - Greedy picks A ($w=10, v=60$) and B ($w=20, v=100$) $\implies$ Total Value = $160$.
   - Optimal choice picks B + C ($w=50, v=220$) $\implies$ Total Value = **$220$**!
   **Takeaway**: If items cannot be split, greedy fails; use Dynamic Programming.
2. **Coin Change Failure with Arbitrary Denominations**:
   Greedy coin change (always picking the largest coin) works for standard canonical currencies (e.g. US coins: 25, 10, 5, 1). However, with denominations `[1, 3, 4]` for amount `6`:
   - Greedy picks $4 + 1 + 1 = 3$ coins.
   - Optimal picks $3 + 3 = 2$ coins.
   Use Dynamic Programming for arbitrary currency sets.
3. **Sorting by Start Time instead of End Time in Interval Scheduling**:
   Sorting intervals by start time fails (e.g. interval `[0, 100]` starts first but blocks all shorter intervals). You must sort by **end time** to leave the maximum remaining time available for subsequent tasks.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Hazard | Production Fix |
| :--- | :--- | :--- |
| Applying Greedy to 0/1 Knapsack | Returns suboptimal results; misses global optimum. | Use Dynamic Programming for indivisible item selection. |
| Sorting by start time in Interval Scheduling | Accepts long intervals that block multiple shorter candidates. | Sort strictly by interval **finish time** (`a[1] - b[1]`). |
| Inverting sorting order | Picks worst candidate first; completely corrupts greedy logic. | Double-check comparator direction (`a - b` vs `b - a`). |
| Greedy coin change on non-canonical denominations | Delivers non-optimal coin combinations. | Use Dynamic Programming table for general coin change. |
| Neglecting tie-breaking in equal metrics | Produces unstable scheduling selections. | Implement multi-attribute comparator tie-breaking. |
| Modifying candidate properties while iterating | Corrupts sorting invariant and produces undefined states. | Clone or preserve original input objects before greedy scan. |

---

## 8. Performance Characteristics

- **Time Complexity**:
  - Sorting Phase: **$O(N \log N)$** using native TimSort.
  - Linear Scan Phase: **$O(N)$**.
  - Total Time: **$O(N \log N)$**.
- **Space Complexity**:
  - **$O(1)$ auxiliary memory** when mutating in-place or iterating over pre-sorted inputs.

---

## 9. Engine Internals

### V8 Cache Efficiency in Single-Pass Greedy Scans
Because greedy algorithms evaluate items sequentially without jumping across pointers or allocating multidimensional DP tables:
- The entire working dataset resides in flat, contiguous memory.
- Modern hardware CPU prefetchers continuously stream next array elements into L1/L2 caches.
- A greedy scan over 1,000,000 items typically completes in under **$2\text{ms}$** in V8.

---

## 10. Best Practices & Design Guidelines

1. **Prove Before You Code**: Verify the exchange argument before assuming a greedy approach is optimal.
2. **Sort by the Right Invariant**:
   - Interval Scheduling $\implies$ Sort by finish time.
   - Fractional Knapsack $\implies$ Sort by $\frac{\text{value}}{\text{weight}}$ ratio.
   - Task Deadlines $\implies$ Sort by earliest deadline.
3. **Fall Back to DP when Choices Have Complex Interdependencies**: If making a local choice permanently ruins future options without an exchange guarantee, use Dynamic Programming.

---

## 11. Real-World Case Study

### Cloud Container Auto-Scaler Resource Packing
**Scenario**: A cloud orchestration engine (Kubernetes-like scheduler) needed to assign incoming batch container jobs to available compute nodes. Each job requested CPU cores and RAM with a strict execution deadline.
**Architecture**:
- Applied a Greedy **First-Fit Decreasing (FFD)** packing heuristic:
  1. Sort incoming containers descending by normalized resource demand ($O(N \log N)$).
  2. For each container, place it in the first node that has sufficient residual capacity.
  3. If no existing node fits the container, provision a new node.
- Proven mathematically to use no more than $\frac{11}{9} \times \text{OPT} + 1$ nodes.

**Outcome**:
- Reduced cloud infrastructure spending by 18% compared to naive round-robin scheduling.
- Scheduling decisions evaluated in $<3\text{ms}$ for batches of 5,000 pods.

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between a Greedy Algorithm and Dynamic Programming?</summary>

- **Greedy Algorithm**: Makes the locally optimal choice at each step without ever reconsidering or backtracking. It never solves all subproblems; it chooses one path and marches forward. Time complexity is typically $O(N \log N)$ or $O(N)$, with $O(1)$ space.
- **Dynamic Programming**: Solves all subproblems, stores their results in a table (memoization/tabulation), and combines optimal subproblem solutions to find the global optimum. It guarantees optimality even when local choices interact with future options, at the expense of higher time ($O(N^2)$) and memory ($O(N)$).
</details>

<details>
<summary>2. Why does the Greedy Choice Property work for Fractional Knapsack but fail for 0/1 Knapsack?</summary>

In Fractional Knapsack, items can be subdivided. Therefore, taking as much of the item with the highest value-per-pound ratio as possible is always optimal; if space remains, you take fractions of the next best. In 0/1 Knapsack, items cannot be broken. Taking an item with a high ratio might leave empty unused space in the knapsack that cannot be filled by another item, whereas taking two items with slightly lower ratios might fill the capacity perfectly and yield a higher total value.
</details>

<details>
<summary>3. Why must intervals be sorted by End Time rather than Start Time in Activity Selection?</summary>

Sorting by end time guarantees that the activity that finishes earliest is chosen first. By finishing as early as possible, it leaves the maximum remaining time available for future non-overlapping activities. If you sort by start time, an activity that starts early but has a huge duration (e.g. $[0, 100]$) would be chosen first, blocking many shorter activities that could have fit within that same interval.
</details>

<details>
<summary>4. What is an Exchange Argument in proving greedy correctness?</summary>

An Exchange Argument is a proof technique used to prove that a greedy algorithm produces an optimal solution:
1. Start with an arbitrary hypothetical optimal solution $O$.
2. Identify the first choice where $O$ differs from the greedy solution $G$.
3. Show that replacing the choice in $O$ with the greedy choice does not decrease the quality/value of the solution.
4. Conclude by induction that the optimal solution can be gradually transformed into the greedy solution without loss of optimality, proving $G$ is optimal.
</details>

<details>
<summary>5. How does Dijkstra's shortest path algorithm exhibit greedy behavior?</summary>

At every step, Dijkstra extracts the unvisited vertex with the minimum known distance from the source using a Priority Queue. It greedily assumes that because all edge weights are non-negative, this current minimum distance can never be improved by taking a longer route through other unvisited vertices. It finalizes the distance immediately and relaxes its neighbors.
</details>

<details>
<summary>6. Under what circumstances does Greedy Coin Change fail?</summary>

Greedy coin change fails when the coin denomination system is **non-canonical**. For example, in a system with coins $[1, 3, 4]$, making change for $6$:
- Greedy picks largest coin ($4$), remaining $2 \implies 4 + 1 + 1 = 3$ coins.
- Optimal choice is $3 + 3 = 2$ coins.
Canonical systems (like US coins: 25, 10, 5, 1) are mathematically structured such that greedy is always guaranteed to produce the minimal coin count.
</details>

<details>
<summary>7. What is the time complexity of the Jump Game greedy algorithm?</summary>

**$O(N)$ linear time and $O(1)$ space**.
The algorithm iterates through the array once, updating a single scalar variable `maxReach = Math.max(maxReach, i + nums[i])`. It requires zero recursion, nested loops, or dynamic programming arrays.
</details>

<details>
<summary>8. How does Kruskal's Minimum Spanning Tree algorithm use a greedy approach?</summary>

Kruskal's algorithm sorts all edges in the graph in ascending order by weight ($O(E \log E)$). It then greedily iterates through the sorted edges, adding an edge to the spanning tree if and only if it does not create a cycle (verified in near-$O(1)$ time using a Disjoint Set Union data structure), halting once $V - 1$ edges are added.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Lemonade Change
**Specification**: At a lemonade stand, each lemonade costs $\$5$. Customers pay with a $\$5$, $\$10$, or $\$20$ bill. You must provide the correct change to each customer in order. Return `true` if you can provide every customer with change, or `false` otherwise.

<details>
<summary>Solution</summary>

```javascript
export function lemonadeChange(bills) {
  let fiveCount = 0;
  let tenCount = 0;

  for (const bill of bills) {
    if (bill === 5) {
      fiveCount++;
    } else if (bill === 10) {
      if (fiveCount === 0) return false;
      fiveCount--;
      tenCount++;
    } else {
      // Bill is 20: Greedily prefer giving 10 + 5 over 5 + 5 + 5
      // (10 bills are less versatile than 5 bills)
      if (tenCount > 0 && fiveCount > 0) {
        tenCount--;
        fiveCount--;
      } else if (fiveCount >= 3) {
        fiveCount -= 3;
      } else {
        return false;
      }
    }
  }

  return true;
}
```
</details>

### Exercise 2: Minimum Number of Arrows to Burst Balloons
**Specification**: Given an array `points` where `points[i] = [xstart, xend]` represents a balloon horizontally on the x-axis, find the minimum number of vertical arrows that must be shot to burst all balloons. An arrow shot at $x$ bursts all balloons with $x_{\text{start}} \le x \le x_{\text{end}}$.

<details>
<summary>Solution</summary>

```javascript
export function findMinArrowShots(points) {
  if (points.length === 0) return 0;

  // Sort by end position ascending
  points.sort((a, b) => a[1] - b[1]);

  let arrows = 1;
  let currentArrowPos = points[0][1];

  for (let i = 1; i < points.length; i++) {
    // If balloon starts after current arrow position, we need a new arrow
    if (points[i][0] > currentArrowPos) {
      arrows++;
      currentArrowPos = points[i][1];
    }
  }

  return arrows;
}
```
</details>
