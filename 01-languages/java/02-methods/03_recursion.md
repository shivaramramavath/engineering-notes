# Recursion: Call Stack Mechanics, Tail Calls, and Trampolines in Java

> Recursion solves complex hierarchical and combinatorial problems by decomposing algorithms into self-referential base and recursive cases, governed directly by JVM thread stack allocation boundaries, lack of native Tail Call Optimization (TCO), and functional trampolines.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_methods.md](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/00_methods.md) |
| **Time to Read** | 30 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (HotSpot Thread Stack Limits & Virtual Threads) |

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

### Problem Statement
Certain algorithmic problems—such as traversing nested JSON trees, searching graph nodes, navigating filesystem hierarchies, parsing abstract syntax trees (ASTs), and executing divide-and-conquer sorting (MergeSort, QuickSort)—are naturally inductive and self-similar. Solving these problems using purely iterative loops requires explicitly instantiating and managing an in-memory stack data structure, resulting in complex, bug-prone code.

### Historical Context & Design Principles
Recursion solves this by using the runtime system's own call stack as the tracking data structure:
- **Base Case:** The halting condition that returns a direct result without further recursive calls.
- **Recursive Case:** Decomposes the input into one or more strictly smaller sub-problems, invoking the method recursively until the base case is reached.
- **The HotSpot Limitation:** Unlike languages designed for pure functional programming (Scheme, Haskell, Scala), the standard Oracle HotSpot JVM does **not** perform native Tail Call Optimization (TCO). Every recursive call allocates a physical stack frame. Deep recursion without a bound causes an unrecoverable `java.lang.StackOverflowError`.

### Real-World Relevance
Recursion is indispensable for tree and graph processing, recursive-descent parsing, and backtracking search (e.g., Sudoku solvers, pathfinders). Senior engineers must understand how to balance the conceptual elegance of recursion against stack memory limits, knowing when to memoize, when to convert to iteration, and how to use functional trampolines.

---

## 2. Core Concepts

### The Anatomy of Recursion

```
+---------------------------------------------------------------------------------+
|                               Recursive Method Anatomy                          |
|                                                                                 |
|   long factorial(int n) {                                                       |
|       // 1. Guard Clause / Precondition                                         |
|       if (n < 0) throw new IllegalArgumentException("n must be >= 0");          |
|                                                                                 |
|       // 2. Base Case (Halting Condition)                                       |
|       if (n <= 1) return 1L;                                                    |
|                                                                                 |
|       // 3. Recursive Step (Progress toward base case)                          |
|       return n * factorial(n - 1);                                             |
|   }                                                                             |
+---------------------------------------------------------------------------------+
```

### Recursion Classifications
1. **Linear Recursion:** A method makes at most one recursive call per invocation (e.g., linear search, factorial).
2. **Tail Recursion:** The recursive call is the **very last operation** executed by the method. The return value of the recursive call is returned directly without further computation.
   - *Non-Tail:* `return n * factorial(n - 1);` (Must wait for child to return before multiplying by `n`).
   - *Tail-Recursive:* `return factorialHelper(n - 1, acc * n);` (Child return value is directly returned).
3. **Tree (Binary/Multi-Branch) Recursion:** A method makes two or more recursive calls per invocation (e.g., naive Fibonacci $F(n) = F(n-1) + F(n-2)$, tree traversals). Time complexity explodes exponentially ($O(2^N)$) unless pruned via memoization.

---

## 3. How It Works Internally

### Stack Frame Growth and Unwinding

```
Calling factorial(4):

 [ Thread Stack (Pushing Frames Downward) ]
 ┌─────────────────────────────────────────┐
 │ Frame 1: factorial(4) -> waits for (3)  │  (Memory allocated for Slot 0: n=4)
 ├─────────────────────────────────────────┤
 │ Frame 2: factorial(3) -> waits for (2)  │  (Memory allocated for Slot 0: n=3)
 ├─────────────────────────────────────────┤
 │ Frame 3: factorial(2) -> waits for (1)  │  (Memory allocated for Slot 0: n=2)
 ├─────────────────────────────────────────┤
 │ Frame 4: factorial(1) -> returns 1L     │  <-- BASE CASE REACHED!
 └─────────────────────────────────────────┘
                      │
                      ▼
 [ Thread Stack Unwinding (Popping Frames) ]
 Frame 4 returns 1L  ──► Frame 3 computes: 2 * 1L = 2L (Frame 4 Popped)
 Frame 3 returns 2L  ──► Frame 2 computes: 3 * 2L = 6L (Frame 3 Popped)
 Frame 2 returns 6L  ──► Frame 1 computes: 4 * 6L = 24L (Frame 2 Popped)
 Frame 1 returns 24L ──► Caller receives 24L (Frame 1 Popped)
```

### The `StackOverflowError` Boundary
Each thread in the JVM has a fixed call stack size configured via the `-Xss` flag (default is typically 1 MB on 64-bit systems).
- A typical stack frame consumes between 32 and 64 bytes depending on local variable slots and operand stack depth.
- In a default 1 MB stack, a thread can typically sustain approximately **10,000 to 15,000 recursive frames** before running out of stack space and throwing `java.lang.StackOverflowError`.

### Why Doesn't HotSpot Implement Tail Call Optimization (TCO)?
In functional runtimes, tail-recursive calls reuse the existing stack frame instead of allocating a new one. The HotSpot JVM historically chose **not** to implement TCO for two major architectural reasons:
1. **Security & Permissions Checks:** Legacy security mechanisms (`SecurityManager`, stack-walking access controllers) inspect the exact call stack frames to verify caller privileges. Collapsing frames via TCO would hide callers from the security audit trail.
2. **Stack Trace Fidelity:** Diagnostic utilities and exceptions (`e.printStackTrace()`, `Thread.getStackTrace()`) rely on exact 1:1 line-by-line frame representations for debugging.

---

## 4. Syntax & Usage

### Tail-Recursive Accumulator Pattern
To convert a non-tail recursive method into tail-recursive form, pass an **accumulator** argument that carries the intermediate state forward:

```java
// Public entry point
public static long factorial(int n) {
    if (n < 0) throw new IllegalArgumentException();
    return factorialTail(n, 1L);
}

// Tail-recursive helper: recursive call is the absolute final expression
private static long factorialTail(int n, long accumulator) {
    if (n <= 1) {
        return accumulator; // Base case returns accumulated result directly
    }
    return factorialTail(n - 1, n * accumulator);
}
```

### Top-Down Dynamic Programming (Memoization)
To prevent exponential $O(2^N)$ tree recursion, store already-computed results in a cache:

```java
public class MemoizedFibonacci {
    private static final Map<Integer, Long> CACHE = new HashMap<>();

    public static long fib(int n) {
        if (n <= 0) return 0;
        if (n == 1) return 1;

        // Check memoization cache before branching
        if (CACHE.containsKey(n)) {
            return CACHE.get(n);
        }

        long result = fib(n - 1) + fib(n - 2);
        CACHE.put(n, result);
        return result;
    }
}
```

---

## 5. Practical Examples

### Example 1: Tree Traversal (Directory File Search)
- **Goal:** Recursively traverse a nested directory structure to find all `.java` files without manual stack management.
```java
import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class RecursiveFileSearch {
    public static void findJavaFiles(File directory, List<File> accumulator) {
        if (directory == null || !directory.exists()) return;

        File[] files = directory.listFiles();
        if (files == null) return; // Base case: empty or unreadable folder

        for (File file : files) {
            if (file.isDirectory()) {
                // Recursive step: traverse child subdirectory
                findJavaFiles(file, accumulator);
            } else if (file.getName().endsWith(".java")) {
                accumulator.add(file);
            }
        }
    }

    public static void main(String[] args) {
        List<File> javaFiles = new ArrayList<>();
        findJavaFiles(new File("src"), javaFiles);
        System.out.println("Discovered Java source files count: " + javaFiles.size());
    }
}
```
- **Console Output:**
```text
Discovered Java source files count: 0
```
- **Explanation:** Recursion naturally mirrors hierarchical recursive structures. Each directory branch navigates downward until leaf files are processed, then automatically backtracks up the call stack.

---

### Example 2: Detecting `StackOverflowError` Depth
- **Goal:** Write an empirical diagnostic utility to measure the exact stack frame depth your JVM environment can support before crashing.
```java
public class StackDepthProbe {
    private static int depth = 0;

    public static void recurse() {
        depth++;
        recurse(); // Infinite recursion without base case!
    }

    public static void main(String[] args) {
        try {
            recurse();
        } catch (StackOverflowError e) {
            System.out.println("JVM Stack Exhausted!");
            System.out.println("Maximum Sustainable Stack Depth: " + depth + " frames");
        }
    }
}
```
- **Console Output:**
```text
JVM Stack Exhausted!
Maximum Sustainable Stack Depth: 10480 frames
```
- **Explanation:** Demonstrates that stack space is finite. When recursive depth exceeds the memory allocated by `-Xss` (here $\approx 10,480$ frames), the JVM throws `StackOverflowError`.

---

### Example 3: Edge Case — Exponential Tree Recursion vs. Linear Dynamic Programming
- **Goal:** Compare execution time between naive exponential Fibonacci $O(2^N)$ and linear memoized calculation $O(N)$ for $N = 45$.
```java
public class FibonacciComparison {
    // Naive Exponential O(2^N)
    public static long naiveFib(int n) {
        if (n <= 1) return n;
        return naiveFib(n - 1) + naiveFib(n - 2);
    }

    // Linear Iterative / Dynamic Programming O(N)
    public static long fastFib(int n) {
        if (n <= 1) return n;
        long prev = 0, curr = 1;
        for (int i = 2; i <= n; i++) {
            long next = prev + curr;
            prev = curr;
            curr = next;
        }
        return curr;
    }

    public static void main(String[] args) {
        long start = System.nanoTime();
        long fastResult = fastFib(45);
        long fastDuration = (System.nanoTime() - start) / 1_000;

        System.out.printf("Fast O(N) Result:  %d (Elapsed: %d µs)%n", fastResult, fastDuration);

        start = System.nanoTime();
        long naiveResult = naiveFib(45); // Takes billions of recursive calls!
        long naiveDuration = (System.nanoTime() - start) / 1_000_000;

        System.out.printf("Naive O(2^N) Result: %d (Elapsed: %d ms)%n", naiveResult, naiveDuration);
    }
}
```
- **Console Output:**
```text
Fast O(N) Result:  1134903170 (Elapsed: 4 µs)
Naive O(2^N) Result: 1134903170 (Elapsed: 4210 ms)
```
- **Explanation:** For $N = 45$, naive tree recursion executes over $2.2$ billion method calls, requiring over 4 seconds of CPU time. The iterative/memoized version executes in 4 microseconds (a 1,000,000x speedup!).

---

### Example 4: Failure Case — Recursive Binary Search Missing Base Case Bounds
- **Goal:** Demonstrate an infinite recursion bug caused by bad index calculation.
```java
public class BrokenBinarySearch {
    // BUGGY: Does not check if low > high!
    public static int badSearch(int[] arr, int target, int low, int high) {
        int mid = (low + high) / 2;
        if (arr[mid] == target) return mid;
        if (arr[mid] > target) {
            return badSearch(arr, target, low, mid - 1);
        } else {
            return badSearch(arr, target, mid + 1, high);
        }
    }

    public static void main(String[] args) {
        int[] data = { 2, 4, 6, 8, 10 };
        try {
            // Target 5 does not exist; low crosses high, causing index crash or infinite loop
            badSearch(data, 5, 0, data.length - 1);
        } catch (ArrayIndexOutOfBoundsException | StackOverflowError e) {
            System.out.println("Caught failure: Missing 'low > high' base case!");
        }
    }
}
```
- **Console Output:**
```text
Caught failure: Missing 'low > high' base case!
```
- **Remedy:** Every binary search must have the terminating base case: `if (low > high) return -1;`.

---

### Example 5: High-Performance / Modern Idiom — Functional Trampolining (Simulated TCO)
- **Goal:** Execute deeply recursive algorithms (e.g., 100,000 iterations) with $O(1)$ constant stack space using the **Trampoline Pattern**.
```java
public class TrampolineDemo {
    @FunctionalInterface
    interface Trampoline<T> {
        Trampoline<T> bounce();
        default boolean isComplete() { return false; }
        default T result() { throw new UnsupportedOperationException(); }

        static <T> Trampoline<T> done(T value) {
            return new Trampoline<>() {
                public Trampoline<T> bounce() { return this; }
                public boolean isComplete() { return true; }
                public T result() { return value; }
            };
        }

        default T compute() {
            Trampoline<T> current = this;
            while (!current.isComplete()) {
                current = current.bounce();
            }
            return current.result();
        }
    }

    public static Trampoline<Long> factorialTrampoline(long n, long acc) {
        if (n <= 1) return Trampoline.done(acc);
        // Returns a lambda supplier instead of executing recursive call on call stack!
        return () -> factorialTrampoline(n - 1, acc * n);
    }

    public static void main(String[] args) {
        // Executes 20 recursions on the heap via while loop; stack depth is exactly 1!
        long result = factorialTrampoline(20, 1).compute();
        System.out.println("Trampoline Factorial(20): " + result);
    }
}
```
- **Console Output:**
```text
Trampoline Factorial(20): 2432902008176640000
```
- **Explanation:** A Trampoline wraps each recursive step into a lazy lambda (`Supplier`). The `while` loop repeatedly invokes the lambda on the heap, unwinding the call stack on every step, enabling infinite recursive depth without `StackOverflowError`.

---

## 6. Best Practices

1. **Always Define and Test the Base Case First:** Before writing a single line of recursive logic, write the base case halting condition and test that it triggers properly.
2. **Ensure Every Recursive Step Makes Measurable Progress:** The argument passed to the child invocation must strictly move closer to the base case (e.g., $n - 1$, $high - 1$, or sublist).
3. **Prefer Iteration for Simple Linear Reductions:** For simple countdowns, factorial, or array summing, use standard `for` loops or Streams. Iteration in Java is faster, uses $O(1)$ stack space, and cannot overflow.
4. **Use Memoization for Overlapping Subproblems:** If a recursive algorithm calls itself with the same arguments repeatedly (like Fibonacci or Knapsack), use a cache to convert exponential $O(2^N)$ time to linear $O(N)$ time.
5. **Set `-Xss` Appropriately for Deep Structural Traversals:** If your application must process deeply nested hierarchies (such as 50,000-deep JSON trees), increase thread stack size via `-Xss4m` or refactor to an iterative stack.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Missing Base Case** | Method calls itself unconditionally until thread stack memory is exhausted. | Always write `if (termination) return;` at top of method. |
| **Non-Progressing Recursive Step** | Passing `n` instead of `n - 1` never reaches the base case, causing stack overflow. | Verify that argument strictly moves toward halting bound. |
| **Assuming Java Has Native Tail-Call Optimization** | Tail-recursive code still allocates a stack frame on every call in HotSpot. | Use iterative loops or a functional Trampoline. |
| **Tree Recursion Without Memoization** | Identical subproblems are re-evaluated billions of times, freezing the application. | Cache intermediate results in a `Map` or array. |
| **Stack Overflow on Large Production Inputs** | A method that works on small test arrays crashes on a 100,000-item customer dataset. | Benchmark with realistic production scale; convert to iteration if deep. |
| **Returning Uninitialized Value from Base Case** | Returning `0` instead of `1` in multiplicative recursion (e.g., factorial) collapses all results to 0. | Double-check identity elements ($0$ for sum, $1$ for product). |

---

## 8. Performance & Security Notes

### Stack Frame Overhead
Each recursive invocation pushes a full stack frame (minimum 32 bytes). For $10,000$ iterations, this consumes $640 \text{ KB}$ of contiguous thread stack RAM. In high-concurrency environments with thousands of threads, allocating large stacks (`-Xss`) can lead to OS virtual memory exhaustion.

### Virtual Threads (Project Loom) and Recursion
In Java 21+ / 25, **Virtual Threads** do not allocate massive fixed OS stack memory. Instead, virtual thread stack frames are allocated as dynamic slice objects on the Java heap. While this allows millions of virtual threads to coexist, a single virtual thread that enters infinite recursion will still be terminated by `StackOverflowError` to protect heap memory from being overwhelmed.

---

## 9. Comparison / When to Use What

| Technique | Readability on Trees | Stack Space Complexity | Heap Memory Allocation | Risk of Stack Overflow |
| :--- | :--- | :---: | :---: | :---: |
| **Standard Recursion** | **Exceptional** (Clean & Intuitive) | $O(N)$ stack frames | **Zero** | **High** (if $N > 10,000$) |
| **Iterative Loop** | Moderate to Complex | **$O(1)$ constant** | Zero | **None** |
| **Memoized Recursion** | High | $O(N)$ stack frames | $O(N)$ cache objects | Moderate |
| **Functional Trampoline** | Moderate | **$O(1)$ constant** | $O(N)$ lambda objects | **None** |
| **Explicit `ArrayDeque` Stack** | Moderate | **$O(1)$ call stack** | $O(N)$ heap objects | **None** |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between Head Recursion and Tail Recursion?</strong></summary>

**Answer:**  
- **Head Recursion:** The method makes its recursive call *before* performing its own processing. The computation is performed during the unwinding phase as stack frames return.
- **Tail Recursion:** The method performs its calculations *first* and makes the recursive call as its very last action, passing the intermediate result via an accumulator parameter. No work remains to be done after the recursive call returns.
</details>

<details>
<summary><strong>2. Does the standard Oracle HotSpot JVM support Tail Call Optimization (TCO)? Why or why not?</strong></summary>

**Answer:**  
No. HotSpot does not optimize tail calls. Every recursive call pushes a new stack frame onto the thread's call stack. The OpenJDK team has kept this design intentionally because collapsing stack frames would break JDK stack-walking security checks (which verify caller permissions) and would distort stack traces produced by exceptions and debuggers.
</details>

<details>
<summary><strong>3. What is the default stack size in HotSpot, and what command-line flag adjusts it?</strong></summary>

**Answer:**  
On modern 64-bit JVMs, the default thread stack size is typically **1 MB** (1024 KB). It can be configured at JVM launch using the **`-Xss`** flag (e.g., `-Xss2m` for 2 megabytes, or `-Xss512k` for 512 kilobytes).
</details>

<details>
<summary><strong>4. Why is naive recursive Fibonacci $O(2^N)$ while iterative Fibonacci is $O(N)$?</strong></summary>

**Answer:**  
Naive recursive Fibonacci (`fib(n) = fib(n-1) + fib(n-2)`) forms a binary tree of method calls. For each level of depth, the number of calls doubles, resulting in $2^0 + 2^1 + 2^2 + \dots + 2^N \approx 2^{N+1} - 1$ total calls. The vast majority of these calls re-evaluate identical subproblems (e.g., `fib(3)` is computed millions of times). The iterative version simply computes each step once sequentially from $2$ to $N$, requiring only $N - 1$ additions.
</details>

<details>
<summary><strong>5. How can any recursive algorithm be converted into an iterative algorithm?</strong></summary>

**Answer:**  
By utilizing an explicit stack data structure (such as `java.util.ArrayDeque`) allocated on the Java heap. Instead of relying on the operating system call stack to store return addresses and local variables, the developer manually pushes and pops state frames to and from the `ArrayDeque` inside a `while (!stack.isEmpty())` loop. This eliminates the risk of `StackOverflowError` because the heap is typically gigabytes in size compared to the 1 MB thread stack.
</details>

<details>
<summary><strong>6. What is a Trampoline in functional Java programming?</strong></summary>

**Answer:**  
A Trampoline is an architectural pattern that simulates Tail Call Optimization in languages that lack native JVM TCO. Instead of executing the tail call directly on the call stack, the recursive function returns a lazy lambda (a `Supplier` or `Trampoline` instance). A top-level iterative `while` loop repeatedly invokes the returned lambda, executing each recursive step in an iterative loop on the heap, ensuring that the thread call stack depth never exceeds 1.
</details>

<details>
<summary><strong>7. How do Virtual Threads in Java 21+ / 25 handle recursive call stacks?</strong></summary>

**Answer:**  
Traditional platform threads have a fixed-size contiguous memory block (1 MB) allocated in OS virtual memory. Virtual Threads (Project Loom) store their call stack frames as chunk objects allocated on the Java heap. When a virtual thread unmounts during blocking I/O, its stack frames are copied to the heap. While virtual threads avoid wasting 1 MB of native memory per thread, deep infinite recursion still triggers a `StackOverflowError` when the virtual thread's cumulative stack size exceeds safety thresholds.
</details>

<details>
<summary><strong>8. What is the difference between Top-Down Dynamic Programming and Bottom-Up Dynamic Programming?</strong></summary>

**Answer:**  
- **Top-Down (Memoized Recursion):** Starts with the original high-level problem and recursively breaks it down, caching the result of each subproblem in a lookup table as it returns. Only evaluates subproblems that are actually needed.
- **Bottom-Up (Tabulation / Iteration):** Starts from the smallest base cases ($0, 1$) and iteratively builds up a table of solutions in sequential order until reaching the target problem, completely avoiding recursion and stack frames.
</details>

---

## 11. Practice Exercises

### Exercise 1: Recursive String Palindrome Checker (Easy)
- **Task:** Implement a recursive method `boolean isPalindrome(String s)` that ignores case and non-alphanumeric characters, returning `true` if the string reads identically forwards and backwards.
<details>
<summary>Solution</summary>

```java
public class PalindromeChecker {
    public static boolean isPalindrome(String s) {
        if (s == null) return false;
        String clean = s.replaceAll("[^a-zA-Z0-9]", "").toLowerCase();
        return checkHelper(clean, 0, clean.length() - 1);
    }

    private static boolean checkHelper(String s, int left, int right) {
        // Base case 1: pointers have met or crossed
        if (left >= right) return true;
        // Base case 2: mismatch found
        if (s.charAt(left) != s.charAt(right)) return false;
        // Recursive step
        return checkHelper(s, left + 1, right - 1);
    }

    public static void main(String[] args) {
        System.out.println(isPalindrome("A man, a plan, a canal: Panama")); // true
        System.out.println(isPalindrome("race a car")); // false
    }
}
```
</details>

### Exercise 2: Greatest Common Divisor via Euclidean Algorithm (Medium)
- **Task:** Implement Euclid's algorithm recursively: $\gcd(a, b) = \gcd(b, a \pmod b)$ with base case $\gcd(a, 0) = a$.
<details>
<summary>Solution</summary>

```java
public class EuclideanGcd {
    public static int gcd(int a, int b) {
        a = Math.abs(a);
        b = Math.abs(b);
        // Base case: when remainder is 0, a is the GCD
        if (b == 0) return a;
        // Tail-recursive step
        return gcd(b, a % b);
    }

    public static void main(String[] args) {
        System.out.println("gcd(48, 18):  " + gcd(48, 18));  // 6
        System.out.println("gcd(101, 10): " + gcd(101, 10)); // 1
    }
}
```
</details>

### Exercise 3: Recursive Object Tree Depth Calculator (Hard)
- **Task:** Define a composite `TreeNode` class with a `List<TreeNode> children`. Implement a method `int maxTreeDepth(TreeNode root)` that calculates the maximum depth of an arbitrary n-ary tree, handling empty and single-node trees safely.
<details>
<summary>Solution</summary>

```java
import java.util.ArrayList;
import java.util.List;

public class TreeDepthExercise {
    static class TreeNode {
        String name;
        List<TreeNode> children = new ArrayList<>();

        TreeNode(String name) { this.name = name; }
        TreeNode addChild(TreeNode child) {
            children.add(child);
            return this;
        }
    }

    public static int maxTreeDepth(TreeNode root) {
        if (root == null) return 0;
        if (root.children.isEmpty()) return 1;

        int maxChildDepth = 0;
        for (TreeNode child : root.children) {
            int depth = maxTreeDepth(child);
            if (depth > maxChildDepth) {
                maxChildDepth = depth;
            }
        }
        return 1 + maxChildDepth;
    }

    public static void main(String[] args) {
        TreeNode root = new TreeNode("Root");
        TreeNode c1 = new TreeNode("C1");
        TreeNode c2 = new TreeNode("C2");
        TreeNode c1Sub = new TreeNode("C1-Sub");

        root.addChild(c1).addChild(c2);
        c1.addChild(c1Sub);

        System.out.println("Max Tree Depth: " + maxTreeDepth(root)); // 3
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Anatomy:** Every recursive function must have a **Base Case** (stops recursion) and a **Recursive Step** (progresses toward base case).
- **No Native TCO:** HotSpot does **not** optimize tail recursion; every call pushes a physical stack frame.
- **Stack Limit:** Default thread stack size (`-Xss1m`) supports $\approx 10,000$ to $15,000$ frames before `StackOverflowError`.
- **Tree Recursion Hazard:** Naive branch recursion (Fibonacci) takes exponential $O(2^N)$ time; always use **Memoization** or iteration.
- **Trampoline Pattern:** Eliminates stack overflow by bouncing recursive steps as lambda suppliers within a while loop.
- **When to Use:** Ideal for hierarchical tree traversals, divide-and-conquer, and backtracking. Prefer iteration for linear accumulation.

---

## 13. Further Reading

- [The Java Language Specification (Java SE 25 Edition) - Chapter 14.17: The return Statement](https://docs.oracle.com/javase/specs/jls/se25/html/jls-14.html#jls-14.17)
- [Introduction to Algorithms (CLRS) - Chapter 4: Divide-and-Conquer](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [JEP 444: Virtual Threads (Stack Management)](https://openjdk.org/jeps/444)

---

| [← Previous: 02_overloading-and-varargs.md](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/02_overloading-and-varargs.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/README.md) | [Next Module: 03-strings-and-text →](file:///E:/Learning/engineering-notes/01-languages/java/03-strings-and-text/README.md) |
