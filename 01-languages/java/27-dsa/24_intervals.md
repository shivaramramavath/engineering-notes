# Interval Algorithms

> Systematic patterns for interval scheduling, merging overlapping spans, line-sweep chronologies, and calendar conflict resolution.

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

An **Interval** represents a contiguous range of values between a start boundary and an end boundary $[start, end]$.

Interval problems arise ubiquitously in real-world engineering:
- **Calendar Scheduling (Google Calendar / Outlook)**: Detecting double-bookings and computing shared availability.
- **Resource Allocation**: Finding the minimum number of conference rooms or servers needed to service concurrent workloads.
- **Operating Systems**: Memory segment allocation, coalescing free memory blocks, and thread CPU timeslices.
- **Genomic Sequencing**: Identifying overlapping gene expressions along DNA strands.

---

## 2. Core Concepts

### 1. The 6 Relative Interval Configurations
Given two intervals $A$ and $B$:
1. $A$ completely precedes $B$: $A_{end} < B_{start}$
2. $A$ overlaps $B$ on left: $A_{start} < B_{start} \le A_{end} \le B_{end}$
3. $A$ encloses $B$: $A_{start} \le B_{start} \le B_{end} \le A_{end}$
4. $B$ encloses $A$: $B_{start} \le A_{start} \le A_{end} \le B_{end}$
5. $A$ overlaps $B$ on right: $B_{start} \le A_{start} \le B_{end} \le A_{end}$
6. $A$ completely succeeds $B$: $B_{end} < A_{start}$

Two intervals **overlap if and only if**:
$$\max(A_{start}, B_{start}) \le \min(A_{end}, B_{end})$$

### 2. Core Algorithmic Archetypes
- **Merge Overlaps**: Sort by start time ascending. If current interval's start $\le$ previous interval's end, merge by extending end: $\max(prev_{end}, curr_{end})$.
- **Line-Sweep (Chronological Event Processing)**: Split each interval $[s, e]$ into two discrete events: $(s, +1)$ (workload starts) and $(e, -1)$ (workload finishes). Sort all events by timestamp and compute running prefix sums.
- **Min-Heap Scheduling**: Sort intervals by start time. Maintain a Min-Heap of end times of active rooms. If heap's earliest end time $\le$ current interval's start time, reuse the room (`poll()`); otherwise, allocate a new room.

---

## 3. How It Works Internally

### Sweep-Line vs Priority Queue for Room Allocation

Line-Sweep Technique ($2N$ Discrete Events):
```
Timeline:    0 --- 5 --- 10 --- 15 --- 20 --- 30
Intervals:  [0 ---------- 30]
                  [5 - 10]
                       [15 --- 20]

Events:
(0, +1)  -> active = 1 (Peak = 1)
(5, +1)  -> active = 2 (Peak = 2)
(10, -1) -> active = 1
(15, +1) -> active = 2 (Peak = 2)
(20, -1) -> active = 1
(30, -1) -> active = 0
Maximum concurrent rooms = 2.
```
Memory footprint is $O(N)$ with contiguous array sorting, delivering top cache line performance.

---

## 4. Syntax & Usage

### Standard Interval Merging Template

```java
import java.util.*;

public class IntervalMergeTemplate {
    public static int[][] merge(int[][] intervals) {
        if (intervals.length <= 1) return intervals;

        // Sort by start time ascending
        Arrays.sort(intervals, Comparator.comparingInt(a -> a[0]));

        List<int[]> merged = new ArrayList<>();
        int[] current = intervals[0];
        merged.add(current);

        for (int i = 1; i < intervals.length; i++) {
            int[] next = intervals[i];
            if (next[0] <= current[1]) {
                // Overlapping: expand end boundary
                current[1] = Math.max(current[1], next[1]);
            } else {
                // Disjoint: begin new interval
                current = next;
                merged.add(current);
            }
        }

        return merged.toArray(new int[merged.size()][]);
    }
}
```

---

## 5. Practical Examples

### Example 1: Merge Intervals (LeetCode 56)

```java
import java.util.*;

public class MergeIntervals {
    public int[][] merge(int[][] intervals) {
        if (intervals.length <= 1) return intervals;

        Arrays.sort(intervals, (a, b) -> Integer.compare(a[0], b[0]));
        List<int[]> result = new ArrayList<>();

        int start = intervals[0][0];
        int end = intervals[0][1];

        for (int i = 1; i < intervals.length; i++) {
            if (intervals[i][0] <= end) {
                end = Math.max(end, intervals[i][1]);
            } else {
                result.add(new int[]{start, end});
                start = intervals[i][0];
                end = intervals[i][1];
            }
        }
        result.add(new int[]{start, end});

        return result.toArray(new int[result.size()][]);
    }
}
```

### Example 2: Insert Interval (LeetCode 57)

```java
import java.util.*;

public class InsertInterval {
    public int[][] insert(int[][] intervals, int[] newInterval) {
        List<int[]> result = new ArrayList<>();
        int i = 0, n = intervals.length;

        // 1. Add all intervals ending before newInterval starts
        while (i < n && intervals[i][1] < newInterval[0]) {
            result.add(intervals[i++]);
        }

        // 2. Merge all overlapping intervals
        while (i < n && intervals[i][0] <= newInterval[1]) {
            newInterval[0] = Math.min(newInterval[0], intervals[i][0]);
            newInterval[1] = Math.max(newInterval[1], intervals[i][1]);
            i++;
        }
        result.add(newInterval);

        // 3. Add all remaining intervals starting after newInterval ends
        while (i < n) {
            result.add(intervals[i++]);
        }

        return result.toArray(new int[result.size()][]);
    }
}
```

### Example 3: Meeting Rooms II (LeetCode 253, Dual Pointers Sweep)

```java
import java.util.Arrays;

public class MeetingRoomsII {
    public int minMeetingRooms(int[][] intervals) {
        int n = intervals.length;
        int[] starts = new int[n];
        int[] ends = new int[n];

        for (int i = 0; i < n; i++) {
            starts[i] = intervals[i][0];
            ends[i] = intervals[i][1];
        }

        Arrays.sort(starts);
        Arrays.sort(ends);

        int rooms = 0;
        int endPtr = 0;

        for (int i = 0; i < n; i++) {
            if (starts[i] < ends[endPtr]) {
                // A meeting starts before the earliest meeting finishes -> allocate room
                rooms++;
            } else {
                // A room freed up -> reuse it
                endPtr++;
            }
        }

        return rooms;
    }
}
```

### Example 4: Interval List Intersections (LeetCode 986)

```java
import java.util.*;

public class IntervalIntersections {
    public int[][] intervalIntersection(int[][] firstList, int[][] secondList) {
        List<int[]> intersections = new ArrayList<>();
        int i = 0, j = 0;

        while (i < firstList.length && j < secondList.length) {
            int low = Math.max(firstList[i][0], secondList[j][0]);
            int high = Math.min(firstList[i][1], secondList[j][1]);

            // Valid overlap
            if (low <= high) {
                intersections.add(new int[]{low, high});
            }

            // Advance the pointer whose interval ends earlier
            if (firstList[i][1] < secondList[j][1]) {
                i++;
            } else {
                j++;
            }
        }

        return intersections.toArray(new int[intersections.size()][]);
    }
}
```

### Example 5: Employee Free Time (LeetCode 759)

```java
import java.util.*;

public class EmployeeFreeTime {
    public record Interval(int start, int end) {}

    public List<Interval> employeeFreeTime(List<List<Interval>> schedule) {
        List<Interval> allIntervals = new ArrayList<>();
        for (List<Interval> list : schedule) {
            allIntervals.addAll(list);
        }

        // Sort all intervals across all employees by start time
        allIntervals.sort(Comparator.comparingInt(Interval::start));

        List<Interval> freeTime = new ArrayList<>();
        int currentEnd = allIntervals.get(0).end();

        for (int i = 1; i < allIntervals.size(); i++) {
            Interval next = allIntervals.get(i);
            if (next.start() > currentEnd) {
                // Disjoint gap: mutual free time for everyone!
                freeTime.add(new Interval(currentEnd, next.start()));
            }
            currentEnd = Math.max(currentEnd, next.end());
        }

        return freeTime;
    }
}
```

---

## 6. Best Practices

1. **Sort by Start Time vs End Time**:
   - For **Merging Overlaps** and **Room Allocation**: Sort by **start time** ascending.
   - For **Maximum Non-Overlapping Scheduling**: Sort by **end time** ascending.
2. **Use Two Pointers for Meeting Rooms**: In Meeting Rooms II, sorting `starts[]` and `ends[]` independently runs in $O(N \log N)$ time and $O(N)$ space with zero heap allocation overhead, running 3x faster than `PriorityQueue`.
3. **Use `Integer.compare` in Comparators**: Prevent integer overflow when intervals contain large or negative coordinates.
4. **Boundary Inclusivity**: Always clarify whether $[1, 2]$ and $[2, 3]$ overlap. In meeting rooms, a meeting starting at 2 can reuse a room freed at 2 (`start >= end`). In other contexts, boundaries are closed.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Comparator subtraction: `a[0] - b[0]` | Overflows to positive/negative values when coordinates cross zero | Use `Integer.compare(a[0], b[0])`. |
| Advancing both pointers on intersection | Skips overlapping intervals spanning multiple segments | Advance only the pointer with smaller end: `first[i][1] < second[j][1] ? i++ : j++`. |
| Forgetting to flush last interval | Last merged segment is omitted from result | Flush `new int[]{start, end}` after loop termination. |
| Inverting sorting order | Breaks sequential timeline traversal | Carefully verify whether start or end is primary sort key. |

---

## 8. Performance & Security Notes

- **Time Complexity**:
  - Merge Intervals: $O(N \log N)$ for sorting, $O(N)$ merge sweep.
  - Insert Interval (already sorted): Strictly $O(N)$ linear time.
  - Meeting Rooms II: $O(N \log N)$ sort, $O(N)$ two-pointer scan.
- **Space Complexity**: $O(N)$ for result storage and sorting arrays.

---

## 9. Comparison / When to Use What

| Pattern | Method | Time Complexity | Best For |
|---|---|---|---|
| **Merge Overlaps** | Sort by start + linear pass | $O(N \log N)$ | Coalescing blocks, union of ranges |
| **Insert Interval** | 3-phase linear scan | $O(N)$ | Adding intervals into pre-sorted sets |
| **Max Concurrent Rooms** | Two arrays (start/end) | $O(N \log N)$ | Peak simultaneous workload |
| **Max Non-Overlapping** | Sort by end + greedy pick | $O(N \log N)$ | Interval scheduling, burst balloons |

---

## 10. Interview Questions

<details>
<summary>1. How do you check if any two intervals in a collection overlap in $O(N \log N)$ time?</summary>

Sort intervals by start time ascending. Then perform a single pass comparing adjacent elements: if `intervals[i][0] < intervals[i - 1][1]`, an overlap exists.
</details>

<details>
<summary>2. Why does sorting start times and end times independently work for Meeting Rooms II?</summary>

Because room requirements only care about how many meetings have started versus how many have finished by any given time. The exact identity of which meeting began and which ended is irrelevant to the total count of active rooms.
</details>

<details>
<summary>3. How does the Line-Sweep algorithm handle ties when an end event coincides with a start event?</summary>

If intervals are semi-open $[start, end)$, the meeting at $end$ finishes before the new one begins. Therefore, end events ($-1$) must be processed **before** start events ($+1$). If intervals are closed $[start, end]$, start events must be processed first.
</details>

<details>
<summary>4. How can you insert an interval into a collection of disjoint intervals in $O(\log N)$ time?</summary>

Use a self-balancing binary search tree (like Java's `TreeMap<Integer, Integer>`) where keys are start times and values are end times. Binary search locates predecessor and successor intervals in $O(\log N)$ to merge only neighboring spans.
</details>

<details>
<summary>5. What is the Interval Tree data structure?</summary>

An Interval Tree is an augmented balanced BST (Red-Black Tree) where each node represents an interval and stores the maximum endpoint value `maxEnd` in its entire subtree. It allows finding all intervals that overlap a query interval $[q_l, q_r]$ in $O(\log N + K)$ time where $K$ is the number of matching intervals.
</details>

<details>
<summary>6. How do you find the mutual intersection of three or more interval lists?</summary>

Generalize the two-pointer approach: track pointers for each list. The mutual intersection starts at $\max(start_1, start_2, \dots)$ and ends at $\min(end_1, end_2, \dots)$. Advance the pointer of the interval with the minimum end time.
</details>

<details>
<summary>7. How do you design a streaming Calendar system (MyCalendar)?</summary>

Use `TreeMap<Integer, Integer>`. To book $[s, e)$:
1. Find `prev = tree.floorKey(s)`. If `prev != null && tree.get(prev) > s`, return false.
2. Find `next = tree.ceilingKey(s)`. If `next != null && next < e`, return false.
3. Otherwise, `tree.put(s, e)` and return true in $O(\log N)$ time.
</details>

<details>
<summary>8. How do you compute the total covered length of a union of intervals?</summary>

Sort intervals by start time. Merge overlapping intervals. Sum the lengths of the resulting disjoint intervals: $\sum (end_i - start_i)$.
</details>

---

## 11. Practice Exercises

### Exercise 1: My Calendar I (LeetCode 729, TreeMap)

<details>
<summary>Solution</summary>

```java
import java.util.TreeMap;

public class MyCalendar {
    private final TreeMap<Integer, Integer> calendar = new TreeMap<>();

    public boolean book(int start, int end) {
        Integer prevStart = calendar.floorKey(start);
        if (prevStart != null && calendar.get(prevStart) > start) {
            return false; // Overlaps with previous event
        }

        Integer nextStart = calendar.ceilingKey(start);
        if (nextStart != null && nextStart < end) {
            return false; // Overlaps with following event
        }

        calendar.put(start, end);
        return true;
    }
}
```
</details>

### Exercise 2: Summary Ranges (LeetCode 228, Continuous Segment Grouping)

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class SummaryRanges {
    public List<String> summaryRanges(int[] nums) {
        List<String> result = new ArrayList<>();
        int i = 0, n = nums.length;

        while (i < n) {
            int start = nums[i];
            while (i + 1 < n && nums[i + 1] == nums[i] + 1) {
                i++;
            }

            if (start == nums[i]) {
                result.add(String.valueOf(start));
            } else {
                result.add(start + "->" + nums[i]);
            }
            i++;
        }

        return result;
    }
}
```
</details>

### Exercise 3: Car Pooling (LeetCode 1094, Difference Array / Sweep-Line)

<details>
<summary>Solution</summary>

```java
public class CarPooling {
    public boolean carPooling(int[][] trips, int capacity) {
        int[] stops = new int[1001];

        for (int[] t : trips) {
            int passengers = t[0], from = t[1], to = t[2];
            stops[from] += passengers;
            stops[to] -= passengers; // Drop off at 'to'
        }

        int currentPassengers = 0;
        for (int p : stops) {
            currentPassengers += p;
            if (currentPassengers > capacity) {
                return false;
            }
        }

        return true;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Overlap Condition**: Two intervals $A$ and $B$ overlap $\iff \max(A_s, B_s) \le \min(A_e, B_e)$.
- **Merging**: Sort by start ascending; update `end = Math.max(end, next[1])`.
- **Insert**: 3 stages: before, merging overlaps, after.
- **Room Allocation**: Two-pointer sweep over sorted `starts[]` and `ends[]` in $O(N \log N)$.
- **Calendar Queries**: `TreeMap.floorKey` and `TreeMap.ceilingKey` validate in $O(\log N)$.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 14.3: Interval Trees](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Baeldung: Guide to Interval Overlap in Java](https://www.baeldung.com/cs/intervals-overlap)
- [LeetCode Intervals Study Tag](https://leetcode.com/tag/intervals/)
