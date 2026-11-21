# Greedy Algorithms

> Locally optimal decision making for global optimization: exchange argument proofs, interval scheduling, matroid theory, and multi-pass strategies.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01_arrays-and-strings.md](01_arrays-and-strings.md), [10_heaps-and-priority-queues.md](10_heaps-and-priority-queues.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java Version** | Java 8+ (Verified on Java 25 LTS) |

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

A **Greedy Algorithm** builds up a solution piece by piece, always choosing the next piece that offers the most immediate, locally optimal benefit without ever reconsidering past choices.

### When Is Greedy Correct?
Unlike Dynamic Programming (which explores all plausible subproblems) or Backtracking (which tries and unwinds choices), Greedy makes a irrevocable choice at each step. A greedy algorithm yields a globally optimal solution **if and only if** the problem satisfies two strict properties:
1. **Greedy Choice Property**: A globally optimal solution can be arrived at by making locally optimal (greedy) choices without needing to evaluate future consequences or backtrack.
2. **Optimal Substructure**: An optimal solution to the problem contains within it optimal solutions to subproblems.

If either property fails, greedy heuristics produce sub-optimal solutions (e.g. 0/1 Knapsack or Coin Change with non-canonical denominations).

---

## 2. Core Concepts

### 1. The Exchange Argument Proof Technique
The standard mathematical method for proving a greedy algorithm's correctness:
1. Assume there exists an arbitrary optimal solution $O$ that differs from the greedy solution $G$.
2. Identify the first choice where $O$ deviates from $G$.
3. "Exchange" the choice in $O$ with the greedy choice from $G$.
4. Prove that this exchange does not decrease the solution's quality (cost or value).
5. By induction, $O$ can be transformed step-by-step into $G$ without losing optimality, proving $G$ is also optimal.

### 2. Classic Greedy Archetypes
- **Activity Selection / Interval Scheduling**: Always select the activity with the **earliest finish time** that is compatible with previously selected activities.
- **Huffman Coding**: Merges the two lowest-frequency trees repeatedly using a min-heap to construct optimal prefix-free codes.
- **Two-Pass Greedy**: Enforcing directional constraints by scanning left-to-right, then right-to-left (e.g., Candy problem).
- **Fractional Knapsack**: Sort items by value-to-weight ratio $v_i / w_i$ descending and take fractions greedily.

---

## 3. How It Works Internally

### Performance Profile of Greedy Solutions
```
Input Data
   |
   v
Sorting Step: Arrays.sort(intervals, Comparator.comparingInt(a -> a[1]))
   |  Dual-Pivot Quicksort for primitives O(N log N)
   |  TimSort for Object arrays O(N log N)
   v
Linear Scan: Single pass O(N) loop maintaining 1 or 2 scalar variables
   |  (e.g., currentEnd, farthestReach, totalGas)
   v
Result: Total Time O(N log N) or O(N), Space O(1) auxiliary
```
Greedy algorithms avoid heap allocations, recursion call frames, and 2D tables, running at peak CPU hardware speed.

---

## 4. Syntax & Usage

### Interval Scheduling Greedy Template

```java
import java.util.Arrays;
import java.util.Comparator;

public class GreedyIntervals {
    public static int maxNonOverlapping(int[][] intervals) {
        if (intervals == null || intervals.length == 0) return 0;

        // Sort by end time ascending
        Arrays.sort(intervals, Comparator.comparingInt(a -> a[1]));

        int count = 1;
        int currentEnd = intervals[0][1];

        for (int i = 1; i < intervals.length; i++) {
            if (intervals[i][0] >= currentEnd) {
                count++;
                currentEnd = intervals[i][1];
            }
        }

        return count;
    }
}
```

---

## 5. Practical Examples

### Example 1: Jump Game I & II (LeetCode 55, 45)

```java
public class JumpGame {
    // Jump Game I: Can we reach the last index?
    public boolean canJump(int[] nums) {
        int farthest = 0;
        for (int i = 0; i < nums.length; i++) {
            if (i > farthest) return false; // Trapped!
            farthest = Math.max(farthest, i + nums[i]);
            if (farthest >= nums.length - 1) return true;
        }
        return true;
    }

    // Jump Game II: Minimum jumps to reach the last index
    public int jump(int[] nums) {
        int jumps = 0;
        int currentWindowEnd = 0;
        int farthest = 0;

        for (int i = 0; i < nums.length - 1; i++) {
            farthest = Math.max(farthest, i + nums[i]);
            // When we exhaust current jump window, make a new jump
            if (i == currentWindowEnd) {
                jumps++;
                currentWindowEnd = farthest;
                if (currentWindowEnd >= nums.length - 1) break;
            }
        }

        return jumps;
    }
}
```

### Example 2: Gas Station (LeetCode 134)

```java
public class GasStation {
    public int canCompleteCircuit(int[] gas, int[] cost) {
        int totalSurplus = 0;
        int currentSurplus = 0;
        int startStation = 0;

        for (int i = 0; i < gas.length; i++) {
            int net = gas[i] - cost[i];
            totalSurplus += net;
            currentSurplus += net;

            // If current surplus drops below 0, no station from startStation to i can be start!
            if (currentSurplus < 0) {
                startStation = i + 1;
                currentSurplus = 0;
            }
        }

        return totalSurplus >= 0 ? startStation : -1;
    }
}
```

### Example 3: Non-overlapping Intervals (LeetCode 435)

```java
import java.util.Arrays;
import java.util.Comparator;

public class EraseOverlapIntervals {
    public int eraseOverlapIntervals(int[][] intervals) {
        if (intervals.length == 0) return 0;
        // Sort by end time ascending
        Arrays.sort(intervals, Comparator.comparingInt(a -> a[1]));

        int nonOverlappingCount = 1;
        int currentEnd = intervals[0][1];

        for (int i = 1; i < intervals.length; i++) {
            if (intervals[i][0] >= currentEnd) {
                nonOverlappingCount++;
                currentEnd = intervals[i][1];
            }
        }

        return intervals.length - nonOverlappingCount;
    }
}
```

### Example 4: Candy (LeetCode 135, Two-Pass Greedy)

```java
import java.util.Arrays;

public class CandyDistribution {
    public int candy(int[] ratings) {
        int n = ratings.length;
        int[] candies = new int[n];
        Arrays.fill(candies, 1);

        // Pass 1: Left to right (higher than left neighbor gets more)
        for (int i = 1; i < n; i++) {
            if (ratings[i] > ratings[i - 1]) {
                candies[i] = candies[i - 1] + 1;
            }
        }

        // Pass 2: Right to left (higher than right neighbor gets more)
        for (int i = n - 2; i >= 0; i--) {
            if (ratings[i] > ratings[i + 1]) {
                candies[i] = Math.max(candies[i], candies[i + 1] + 1);
            }
        }

        int total = 0;
        for (int c : candies) total += c;
        return total;
    }
}
```

### Example 5: Task Scheduler (LeetCode 621, Frequency Math / Greedy)

```java
import java.util.Arrays;

public class TaskSchedulerGreedy {
    public int leastInterval(char[] tasks, int n) {
        int[] freq = new int[26];
        for (char c : tasks) freq[c - 'A']++;
        Arrays.sort(freq);

        int maxFreq = freq[25];
        int numMaxFreqTasks = 1;

        // Count how many tasks share the maximum frequency
        for (int i = 24; i >= 0; i--) {
            if (freq[i] == maxFreq) {
                numMaxFreqTasks++;
            } else {
                break;
            }
        }

        // (maxFreq - 1) frames of size (n + 1) plus final tasks
        int partCount = maxFreq - 1;
        int emptySlots = partCount * (n - (numMaxFreqTasks - 1));
        int availableTasks = tasks.length - (maxFreq * numMaxFreqTasks);
        int idles = Math.max(0, emptySlots - availableTasks);

        return tasks.length + idles;
    }
}
```

---

## 6. Best Practices

1. **Sort by End Time, Not Start Time for Interval Scheduling**: Sorting by start time fails on long intervals that start early and block multiple short intervals. Sorting by end time leaves maximal room for remaining activities.
2. **Look for Invariant Properties in Circular Problems**: In the Gas Station problem, if total gas $\ge$ total cost, a valid starting station is guaranteed to exist.
3. **Use Primitive Comparators**: Always write `Comparator.comparingInt(a -> a[1])` to prevent integer subtraction overflows `(a, b) -> a[1] - b[1]`.
4. **Identify Independence**: Ensure that making a greedy choice at step $k$ does not invalidate future valid moves or close off optimal paths.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Assuming greedy solves 0/1 Knapsack | Items cannot be divided; greedy ratio leaves empty gaps | Use Dynamic Programming ($O(NW)$). |
| Sorting intervals by start time for max scheduling | An early starting long task blocks several short ones | Sort intervals by **end time** ascending. |
| Comparator overflow `a[1] - b[1]` | Numeric overflow with negative numbers | Use `Integer.compare(a[1], b[1])`. |
| Single pass when two directional constraints exist | Candy requires checking both left and right neighbors | Use two independent passes (left-to-right, then right-to-left). |

---

## 8. Performance & Security Notes

- **Time Complexity**: Typically $O(N \log N)$ if sorting is required; $O(N)$ if already sorted or solved via single-pass tracking.
- **Space Complexity**: Strictly $O(1)$ auxiliary space in most cases (or $O(N)$ for sorting/result arrays).
- **Denial of Service (Sorting Quicksort Degeneration)**: Java's `Arrays.sort(int[])` uses Dual-Pivot Quicksort. Adversarially crafted arrays can cause quadratic time $O(N^2)$ in older JDKs. Java 14+ mitigates this with introsort fallbacks to heapsort.

---

## 9. Comparison / When to Use What

| Criteria | Greedy Algorithm | Dynamic Programming | Backtracking |
|---|---|---|---|
| **Optimality Guarantee** | Requires strict proof | Always optimal if DP applies | Always optimal (exhaustive) |
| **Subproblem Overlap** | Irrelevant (never backtracks) | Essential | Not required |
| **Time Complexity** | Fastest: $O(N)$ or $O(N \log N)$ | Polynomial: $O(N^2)$ or $O(N^3)$ | Exponential: $O(2^N)$ or $O(N!)$ |
| **Space Overhead** | $O(1)$ auxiliary | $O(N)$ to $O(N^2)$ table | $O(N)$ call stack |

---

## 10. Interview Questions

<details>
<summary>1. When does a greedy algorithm fail to find the optimal solution?</summary>

A greedy algorithm fails when local decisions restrict future options from reaching the global optimum. Classic examples:
1. **0/1 Knapsack**: Picking the highest value-to-weight ratio item may leave unused capacity that could have been filled with two higher-value items.
2. **Coin Change**: With denominations $\{1, 3, 4\}$ and amount 6, greedy picks $4 + 1 + 1$ (3 coins), whereas optimal is $3 + 3$ (2 coins).
</details>

<details>
<summary>2. How do you prove a greedy algorithm using an Exchange Argument?</summary>

1. State that an optimal solution $OPT$ exists.
2. If $OPT == GREEDY$, the greedy solution is optimal.
3. If $OPT \neq GREEDY$, identify the first choice where they diverge.
4. Replace the choice in $OPT$ with the greedy choice.
5. Prove that the total cost or benefit in $OPT$ is preserved or improved.
6. Conclude by induction that $GREEDY$ is optimal.
</details>

<details>
<summary>3. Why does sorting by end time solve Activity Selection, but sorting by duration fails?</summary>

Sorting by duration fails when a short activity in the middle blocks two activities on either side:
Activity A: $[0, 10]$
Activity B: $[9, 13]$ (short duration = 4)
Activity C: $[12, 20]$
Picking B by shortest duration allows only 1 activity. Picking A and C yields 2 activities. Sorting by end time guarantees leaving maximal remaining time.
</details>

<details>
<summary>4. What is a Matroid and how does it relate to greedy algorithms?</summary>

A Matroid is an abstract combinatorial structure $(S, \mathcal{I})$ that satisfies the hereditary and exchange properties. Rado-Edmonds Theorem proves that **a greedy algorithm is guaranteed to produce an optimal solution for all weight functions if and only if the underlying structure is a matroid**. (e.g., Kruskal's MST operates on the Graphic Matroid).
</details>

<details>
<summary>5. How does Huffman Coding use a greedy strategy?</summary>

Huffman coding builds an optimal prefix-free tree from the bottom up. At each step, it greedily merges the two trees with the lowest frequencies. This minimizes the weighted external path length $\sum f_i \times d_i$.
</details>

<details>
<summary>6. How do you solve the Jump Game in $O(N)$ time instead of $O(N^2)$ DP?</summary>

Maintain a scalar variable `farthestReach`. As we scan from index 0 to $N-1$, if $i > \text{farthestReach}$, we are stuck. Otherwise, update $\text{farthestReach} = \max(\text{farthestReach}, i + nums[i])$. This eliminates the nested inner loop, dropping runtime from $O(N^2)$ to $O(N)$.
</details>

<details>
<summary>7. Can greedy algorithms be used for NP-Hard problems?</summary>

Yes, as approximation algorithms. For example:
- Metric TSP: Christofides or 2-approximation using MST.
- Set Cover: Greedy selection gives a $(1 + \ln N)$ approximation, which is the mathematically best polynomial-time approximation possible unless $P = NP$.
</details>

<details>
<summary>8. Why is two-pass greedy necessary in the Candy problem?</summary>

Because each child's candy count depends symmetrically on **both** their left neighbor and right neighbor. A single pass in one direction can only resolve constraints with one neighbor. Running left-to-right satisfies the left constraint, and running right-to-left (taking `Math.max`) satisfies the right constraint without violating the first.
</details>

---

## 11. Practice Exercises

### Exercise 1: Minimum Number of Arrows to Burst Balloons (LeetCode 452)
Find minimum arrows shot perpendicularly to burst all interval balloons.

<details>
<summary>Solution</summary>

```java
import java.util.Arrays;

public class BurstBalloonsArrows {
    public int findMinArrowShots(int[][] points) {
        if (points.length == 0) return 0;

        // Sort by end coordinate ascending using Integer.compare
        Arrays.sort(points, (a, b) -> Integer.compare(a[1], b[1]));

        int arrows = 1;
        int currentArrowPos = points[0][1];

        for (int i = 1; i < points.length; i++) {
            if (points[i][0] > currentArrowPos) {
                arrows++;
                currentArrowPos = points[i][1];
            }
        }

        return arrows;
    }
}
```
</details>

### Exercise 2: Queue Reconstruction by Height (LeetCode 406)
Reconstruct queue of `[height, k]` where `k` is number of people in front who have height $\ge$ this person.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class QueueReconstruction {
    public int[][] reconstructQueue(int[][] people) {
        // Sort height descending; on height tie, sort k ascending
        Arrays.sort(people, (a, b) -> {
            if (a[0] != b[0]) return Integer.compare(b[0], a[0]);
            return Integer.compare(a[1], b[1]);
        });

        List<int[]> result = new ArrayList<>();
        // Greedily insert each person at index k
        for (int[] p : people) {
            result.add(p[1], p);
        }

        return result.toArray(new int[people.length][]);
    }
}
```
</details>

### Exercise 3: Partition Labels (LeetCode 763)
Partition a string into as many parts as possible such that each letter appears in at most one part.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class PartitionLabels {
    public List<Integer> partitionLabels(String s) {
        int[] lastIndex = new int[26];
        for (int i = 0; i < s.length(); i++) {
            lastIndex[s.charAt(i) - 'a'] = i;
        }

        List<Integer> partitions = new ArrayList<>();
        int start = 0;
        int maxEnd = 0;

        for (int i = 0; i < s.length(); i++) {
            maxEnd = Math.max(maxEnd, lastIndex[s.charAt(i) - 'a']);
            if (i == maxEnd) {
                partitions.add(i - start + 1);
                start = i + 1;
            }
        }

        return partitions;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Requisites**: Greedy Choice Property and Optimal Substructure.
- **Interval Scheduling**: Always sort by end coordinate ascending: `Comparator.comparingInt(a -> a[1])`.
- **Space & Speed**: Greedy algorithms run in $O(N \log N)$ or $O(N)$ with $O(1)$ space.
- **Two-Pass Technique**: Satisfies dual directional constraints by sweeping left-to-right then right-to-left.
- **Comparator Safety**: Always use `Integer.compare(a, b)` instead of subtraction to prevent overflow.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 16: Greedy Algorithms](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Korte & Vygen: Combinatorial Optimization: Theory and Algorithms (Matroid Theory)](https://link.springer.com/book/10.1007/978-3-662-56039-6)
- [Baeldung: Greedy Algorithms in Java](https://www.baeldung.com/java-greedy-algorithms)
- [LeetCode Greedy Study Plan](https://leetcode.com/tag/greedy/)
