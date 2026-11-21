# Stacks and Queues Algorithmic Patterns in Java

> LIFO and FIFO mechanics, Monotonic Stacks, Monotonic Deques, Min Stack $O(1)$, Circular ArrayDeque bitwise masking, and Sliding Window Maximum in Java.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Advanced |
| **Prerequisites** | [`27-dsa/00_complexity-analysis.md`](00_complexity-analysis.md), [`08-collections/04_queue-and-deque.md`](../08-collections/04_queue-and-deque.md) |
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

Stacks (Last-In, First-Out — LIFO) and Queues (First-In, First-Out — FIFO) represent fundamental linear access-restricted data structures. 

Beyond basic recursion and buffering, advanced variants such as **Monotonic Stacks** and **Monotonic Deques** solve complex non-trivial optimization problems (e.g. Next Greater Element, Largest Rectangle in Histogram, Sliding Window Maximum) in optimal $O(N)$ linear time by eliminating redundant comparisons.

---

## 2. Core Concepts

### 2.1 Monotonic Stack (Increasing vs Decreasing)

A Monotonic Stack maintains its elements in strictly sorted order (either strictly increasing or strictly decreasing):
- **Monotonic Decreasing Stack**: Elements decrease from bottom to top: `[8, 5, 3, 1]`. When an incoming element is larger than `peek()`, smaller elements are popped until invariant is restored.
  - *Identifies*: **Next Greater Element** to the right/left in $O(N)$ total time!
- **Monotonic Increasing Stack**: Elements increase from bottom to top: `[1, 3, 5, 8]`.
  - *Identifies*: **Next Smaller Element** to the right/left!

### 2.2 Monotonic Deque (Sliding Window Maximum)

A double-ended queue (`Deque`) where elements are maintained in decreasing order of value:
- Stores indices of elements.
- When sliding window moves right:
  1. Pop stale indices from the left (`pollFirst()`) that have fallen outside window range $[R - K + 1, R]$.
  2. Pop smaller elements from the right (`pollLast()`) that can never be the maximum again.
  3. The current window maximum is always at `peekFirst()`.

---

## 3. How It Works Internally

### 3.1 Why `ArrayDeque` Beats `java.util.Stack` and `LinkedList`

Java provides three stack/queue implementations:
1. `java.util.Stack`: Legacy subclass of `Vector`. Every method (`push`, `pop`, `peek`) is `synchronized`, incurring unnecessary locking overhead even in single-threaded algorithms.
2. `LinkedList`: Implements `Deque`, but allocates a 24-byte node object for every push and causes pointer chasing cache misses.
3. `ArrayDeque`: Implements circular dynamic array with power-of-two sizing ($2^K$). Index wrapping uses lightning-fast bitwise masking:
   $$\text{tail} = (\text{tail} + 1) \& (\text{elements.length} - 1)$$

```
ArrayDeque Circular Buffer Layout (Capacity = 8, Mask = 7):
Index:    0    1    2    3    4    5    6    7
Data:   [ D ][ E ][   ][   ][   ][ A ][ B ][ C ]
                   ^              ^
                   |              |
                   tail = 2       head = 5
Next push at head: (head - 1) & 7 = 4! Zero modulo division!
```

---

## 4. Syntax & Usage

### 4.1 Modern Idiomatic Stack/Queue Instantiation

```java
import java.util.ArrayDeque;
import java.util.Deque;

// Recommended: Deque interface backed by ArrayDeque
Deque<Integer> stack = new ArrayDeque<>();
stack.push(10);
int top = stack.pop();

Deque<String> queue = new ArrayDeque<>();
queue.offer("task-1");
String first = queue.poll();
```

---

## 5. Practical Examples

### 5.1 Pattern 1: Valid Parentheses Matching (LeetCode 20)
```java
import java.util.ArrayDeque;
import java.util.Deque;

public class ValidParentheses {
    public static boolean isValid(String s) {
        Deque<Character> stack = new ArrayDeque<>();

        for (char c : s.toCharArray()) {
            if (c == '(') stack.push(')');
            else if (c == '{') stack.push('}');
            else if (c == '[') stack.push(']');
            else {
                if (stack.isEmpty() || stack.pop() != c) {
                    return false;
                }
            }
        }
        return stack.isEmpty();
    }
}
```

### 5.2 Pattern 2: Min Stack with Constant Time $O(1)$ Operations (LeetCode 155)
```java
import java.util.ArrayDeque;
import java.util.Deque;

public class MinStack {
    private record Node(int val, int min) {}
    private final Deque<Node> stack = new ArrayDeque<>();

    public void push(int val) {
        int currentMin = stack.isEmpty() ? val : Math.min(val, stack.peek().min());
        stack.push(new Node(val, currentMin));
    }

    public void pop() {
        stack.pop();
    }

    public int top() {
        return stack.peek().val();
    }

    public int getMin() {
        return stack.peek().min();
    }
}
```

### 5.3 Pattern 3: Monotonic Stack — Daily Temperatures (LeetCode 739)
```java
import java.util.ArrayDeque;
import java.util.Deque;

public class DailyTemperatures {
    public static int[] dailyTemperatures(int[] temperatures) {
        int[] results = new int[temperatures.length];
        // Stack stores indices of monotonic decreasing temperatures
        Deque<Integer> stack = new ArrayDeque<>();

        for (int i = 0; i < temperatures.length; i++) {
            while (!stack.isEmpty() && temperatures[i] > temperatures[stack.peek()]) {
                int prevIndex = stack.pop();
                results[prevIndex] = i - prevIndex;
            }
            stack.push(i);
        }
        return results;
    }
}
```

### 5.4 Pattern 4: Monotonic Stack — Next Greater Element I (LeetCode 496)
```java
import java.util.*;

public class NextGreaterElement {
    public static int[] nextGreaterElement(int[] nums1, int[] nums2) {
        Map<Integer, Integer> nextGreaterMap = new HashMap<>();
        Deque<Integer> stack = new ArrayDeque<>();

        for (int num : nums2) {
            while (!stack.isEmpty() && num > stack.peek()) {
                nextGreaterMap.put(stack.pop(), num);
            }
            stack.push(num);
        }

        int[] result = new int[nums1.length];
        for (int i = 0; i < nums1.length; i++) {
            result[i] = nextGreaterMap.getOrDefault(nums1[i], -1);
        }
        return result;
    }
}
```

### 5.5 Pattern 5: Monotonic Stack — Largest Rectangle in Histogram (LeetCode 84)
```java
import java.util.ArrayDeque;
import java.util.Deque;

public class LargestRectangleHistogram {
    public static int largestRectangleArea(int[] heights) {
        Deque<Integer> stack = new ArrayDeque<>();
        int maxArea = 0;
        int n = heights.length;

        for (int i = 0; i <= n; i++) {
            int currentHeight = (i == n) ? 0 : heights[i];

            while (!stack.isEmpty() && currentHeight < heights[stack.peek()]) {
                int height = heights[stack.pop()];
                int width = stack.isEmpty() ? i : i - stack.peek() - 1;
                maxArea = Math.max(maxArea, height * width);
            }
            stack.push(i);
        }
        return maxArea;
    }
}
```

### 5.6 Pattern 6: Monotonic Deque — Sliding Window Maximum (LeetCode 239)
```java
import java.util.ArrayDeque;
import java.util.Deque;

public class SlidingWindowMax {
    public static int[] maxSlidingWindow(int[] nums, int k) {
        if (nums == null || nums.length == 0 || k <= 0) return new int[0];

        int n = nums.length;
        int[] results = new int[n - k + 1];
        int ri = 0;

        // Monotonic decreasing deque storing indices
        Deque<Integer> deque = new ArrayDeque<>();

        for (int i = 0; i < n; i++) {
            // 1. Remove elements outside window [i - k + 1, i]
            if (!deque.isEmpty() && deque.peekFirst() < i - k + 1) {
                deque.pollFirst();
            }

            // 2. Remove smaller elements from back that cannot be max
            while (!deque.isEmpty() && nums[deque.peekLast()] < nums[i]) {
                deque.pollLast();
            }

            // 3. Add current index
            deque.offerLast(i);

            // 4. Record current window maximum
            if (i >= k - 1) {
                results[ri++] = nums[deque.peekFirst()];
            }
        }
        return results;
    }
}
```

---

## 6. Best Practices

1. **Never Use `java.util.Stack`**: Always use `Deque<T> stack = new ArrayDeque<>()`.
2. **Store Indices in Monotonic Stacks**: Storing array indices rather than values allows calculating both distances ($i - \text{prev}$) and querying values (`arr[index]`).
3. **Use Sentinel Values to Flush Monotonic Stacks**: In histogram or area calculations, appending an artificial height of `0` at index $N$ ensures all remaining elements are popped and evaluated cleanly.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Using `Stack` Class** | Synchronized methods introduce lock overhead | Use `ArrayDeque` |
| **Popping Empty Stack** | Calling `pop()` on empty stack throws `NoSuchElementException` | Always guard with `!stack.isEmpty()` |
| **Confusing Stack vs Queue Deque Methods** | Calling `add()` instead of `push()` adds to tail rather than head | Stack idiom: `push()` / `pop()`; Queue idiom: `offer()` / `poll()` |
| **Off-by-One in Histogram Width** | Width formula `i - stack.peek()` omits boundary adjustment | If stack is empty: width = `i`; else: `i - stack.peek() - 1` |

---

## 8. Performance & Security Notes

### 8.1 Zero-Allocation Primitive Stacks
In high-frequency trading or ultra-low-latency code, `ArrayDeque<Integer>` allocates boxed objects. Prefer an unboxed primitive stack (`int[] stack = new int[capacity]; int top = -1;`) for 10x higher throughput and zero GC allocation.

---

## 9. Comparison / When to Use What

| Pattern | Data Structure | Invariant | Solves |
| :--- | :--- | :--- | :--- |
| **Monotonic Stack (Decreasing)** | `Deque<Integer>` | Stack values decrease | Next Greater Element, Daily Temperatures, Stock Span |
| **Monotonic Stack (Increasing)** | `Deque<Integer>` | Stack values increase | Next Smaller Element, Largest Rectangle in Histogram |
| **Monotonic Deque** | `Deque<Integer>` | Front is always max | Sliding Window Maximum, Constrained Subsequence Sum |
| **Min/Max Stack** | Pair Stack / Dual Stack | Auxiliary keeps min | Constant-time $O(1)$ `getMin()` retrieval |

---

## 10. Interview Questions

<details>
<summary>1. Why does a Monotonic Stack solve Next Greater Element in $O(N)$ time?</summary>
Although there is a `while` loop inside the `for` loop, each element in the input array is pushed onto the stack exactly once and popped from the stack at most once. The total number of push and pop operations across all iterations is at most $2N$, guaranteeing $O(N)$ linear time.
</details>

<details>
<summary>2. Why does the Java documentation recommend `ArrayDeque` over `Stack`?</summary>
`java.util.Stack` inherits from `Vector`, meaning every operation acquires and releases a synchronized monitor lock. `ArrayDeque` is unsynchronized, backed by a circular array, does not allocate node wrapper objects, and provides faster $O(1)$ operations with better cache locality.
</details>

<details>
<summary>3. How does `ArrayDeque` implement fast circular wrapping?</summary>
It forces its capacity to always be a power of two ($2^K$). Wrapping indices uses bitwise AND masking `(index + 1) & (length - 1)` which executes in a single CPU clock cycle, avoiding costly integer modulo division.
</details>

<details>
<summary>4. How can you implement a Queue using two Stacks?</summary>
Use `inStack` for enqueuing and `outStack` for dequeuing. When `dequeue()` is called, if `outStack` is empty, pop all elements from `inStack` and push them to `outStack` (reversing their order to FIFO). Amortized time per operation is $O(1)$.
</details>

<details>
<summary>5. How does a Min Stack achieve $O(1)$ `getMin()` without sorting?</summary>
Each stack entry stores a pair: the element value and the running minimum up to that point: `new Node(val, Math.min(val, currentMin))`. When an element is popped, the previous minimum is automatically restored.
</details>

<details>
<summary>6. Explain the logic of Largest Rectangle in Histogram.</summary>
Maintain a monotonic increasing stack of bar indices. When a shorter bar is encountered, the bar at the top of the stack has found its right boundary (current index) and left boundary (the element below it on the stack). We pop it and calculate its maximal rectangular area: $\text{height} \times (\text{right} - \text{left} - 1)$.
</details>

<details>
<summary>7. What happens when an `ArrayDeque` becomes full?</summary>
It doubles its internal array capacity (`elements = Arrays.copyOf(elements, newCapacity)`) and realigns the head and tail pointers to contiguous positions.
</details>

<details>
<summary>8. Can `ArrayDeque` store `null` elements?</summary>
No. `ArrayDeque` prohibits `null` elements and throws `NullPointerException` on `offer(null)` or `push(null)`. This design choice allows methods like `poll()` to use `null` as a sentinel indicating that the queue is empty.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Implement Queue using Stacks (LeetCode 232)</summary>

```java
import java.util.ArrayDeque;
import java.util.Deque;

public class MyQueue {
    private final Deque<Integer> in = new ArrayDeque<>();
    private final Deque<Integer> out = new ArrayDeque<>();

    public void push(int x) { in.push(x); }

    public int pop() {
        peek();
        return out.pop();
    }

    public int peek() {
        if (out.isEmpty()) {
            while (!in.isEmpty()) out.push(in.pop());
        }
        return out.peek();
    }

    public boolean empty() { return in.isEmpty() && out.isEmpty(); }
}
```
</details>

<details>
<summary>Exercise 2: Evaluate Reverse Polish Notation (LeetCode 150)</summary>

```java
import java.util.ArrayDeque;
import java.util.Deque;

public class EvalRPN {
    public static int evalRPN(String[] tokens) {
        Deque<Integer> stack = new ArrayDeque<>();
        for (String t : tokens) {
            switch (t) {
                case "+" -> stack.push(stack.pop() + stack.pop());
                case "-" -> {
                    int b = stack.pop();
                    stack.push(stack.pop() - b);
                }
                case "*" -> stack.push(stack.pop() * stack.pop());
                case "/" -> {
                    int b = stack.pop();
                    stack.push(stack.pop() / b);
                }
                default -> stack.push(Integer.parseInt(t));
            }
        }
        return stack.pop();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **`ArrayDeque`**: Preferred default over `Stack` and `LinkedList`. Power-of-2 bitwise mask wrapping.
- **Monotonic Stack**: Finds Next Greater/Smaller element in $O(N)$ linear time.
- **Monotonic Deque**: Maintains maximum/minimum of a sliding window in $O(N)$ time.
- **Min Stack**: Stores `(val, runningMin)` pair for $O(1)$ constant time minimum retrieval.
- **Sentinel**: Push `0` height at end of histogram to flush stack without trailing loops.

---

## 13. Further Reading

- [Algorithms, 4th Edition: Stacks and Queues](https://algs4.cs.princeton.edu/13stacks/)
- [OpenJDK Source Code: `java.util.ArrayDeque`](https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/util/ArrayDeque.java)
"""

write_dsa_note("03_stacks-and-queues.md", c03)
