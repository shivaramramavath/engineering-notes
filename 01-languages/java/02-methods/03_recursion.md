# Recursion

A method is **recursive** when it calls itself, solving a problem by reducing it to a smaller instance of the same problem. Every recursive method needs a **base case** that stops the recursion and a **recursive case** that moves toward it.

```java
static long factorial(int n) {
    if (n < 0) throw new IllegalArgumentException("n must be >= 0");
    if (n <= 1) return 1;               // base case: stops
    return n * factorial(n - 1);        // recursive case: smaller problem
}
```

```
factorial(4)
  = 4 * factorial(3)
        = 3 * factorial(2)
              = 2 * factorial(1)
                    = 1            ◄── base case
              = 2 * 1 = 2
        = 3 * 2 = 6
  = 4 * 6 = 24
```

## The three questions

Before writing a recursive method, answer:

1. **Base case:** for which input(s) is the answer immediate?
2. **Smaller problem:** how do I express the answer in terms of a smaller input?
3. **Progress:** does every call move closer to a base case?

If the answer to 3 is "not always", you have an infinite recursion.

## The call stack

Each call gets its own stack frame ([methods](./00_methods.md#what-happens-at-a-call-the-call-stack)). Frames pile up on the way down and unwind on the way back.

```
 call stack for factorial(3)

 ┌──────────────────┐
 │ factorial(1)  n=1│  returns 1             ▲  unwinding:
 ├──────────────────┤                        │  results flow back up
 │ factorial(2)  n=2│  waits: 2 * ?          │
 ├──────────────────┤                        │
 │ factorial(3)  n=3│  waits: 3 * ?          │
 ├──────────────────┤
 │ main             │
 └──────────────────┘
```

The stack is finite. Too many nested calls throw `StackOverflowError`:

```java
static void forever() { forever(); }     // no base case
forever();                               // StackOverflowError
```

| Fact | Detail |
|------|--------|
| Default depth | Typically thousands to tens of thousands of frames, depending on frame size and `-Xss` |
| Java does **not** optimize tail calls | A "tail-recursive" method still uses one frame per call |
| Space cost | **O(depth)** extra memory for the stack, even if you use no arrays |

## Classic examples

### Sum of digits

```java
static int digitSum(int n) {
    if (n < 10) return n;
    return n % 10 + digitSum(n / 10);
}
digitSum(1234);     // 10
```

### Greatest common divisor (Euclid)

```java
static int gcd(int a, int b) {
    return b == 0 ? a : gcd(b, a % b);
}
```

### Power in O(log n)

```java
static double power(double x, int n) {          // n >= 0
    if (n == 0) return 1;
    double half = power(x, n / 2);
    return n % 2 == 0 ? half * half : half * half * x;
}
```

Computing `half` once is the key: calling `power(x, n/2)` twice would make it O(n).

### Palindrome

```java
static boolean isPalindrome(String s, int l, int r) {
    if (l >= r) return true;
    if (s.charAt(l) != s.charAt(r)) return false;
    return isPalindrome(s, l + 1, r - 1);
}
isPalindrome("level", 0, 4);    // true
```

### Binary search

```java
static int search(int[] a, int key, int lo, int hi) {
    if (lo > hi) return -1;                 // base: empty range
    int mid = lo + (hi - lo) / 2;
    if (a[mid] == key) return mid;
    return a[mid] < key
        ? search(a, key, mid + 1, hi)
        : search(a, key, lo, mid - 1);
}
```

### Tower of Hanoi

```java
static void hanoi(int n, char from, char to, char via) {
    if (n == 0) return;
    hanoi(n - 1, from, via, to);                      // move n-1 out of the way
    System.out.println("Move disk " + n + " " + from + " -> " + to);
    hanoi(n - 1, via, to, from);                      // move them on top
}
```

Moves needed: 2ⁿ - 1.

### Recursion on structures

Recursion fits naturally on nested data: trees, directories, JSON, expression parsers.

```java
static int depth(Node node) {
    if (node == null) return 0;
    return 1 + Math.max(depth(node.left), depth(node.right));
}
```

## Branching recursion and repeated work

```java
static long fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);       // two calls per level
}
```

```
                    fib(5)
                 /          \
            fib(4)          fib(3)
           /     \          /    \
       fib(3)  fib(2)   fib(2) fib(1)
       /   \
   fib(2) fib(1)          ◄── fib(3), fib(2) are recomputed many times
```

This takes **O(2ⁿ)** time: `fib(50)` is already impractical. The same subproblems are solved repeatedly.

### Fix 1: memoization (cache results)

```java
private static final Map<Integer, Long> memo = new HashMap<>();

static long fibMemo(int n) {
    if (n <= 1) return n;
    Long cached = memo.get(n);
    if (cached != null) return cached;
    long result = fibMemo(n - 1) + fibMemo(n - 2);
    memo.put(n, result);
    return result;
}
```

Now O(n) time and O(n) space. Do not use `HashMap.computeIfAbsent` with a function that modifies the same map recursively; it can throw `ConcurrentModificationException`.

### Fix 2: iteration

```java
static long fibIter(int n) {
    long a = 0, b = 1;
    for (int i = 0; i < n; i++) {
        long next = a + b;
        a = b;
        b = next;
    }
    return a;
}
```

O(n) time and O(1) space, with no stack risk. Caching and bottom-up tables are the basis of [dynamic programming](../27-dsa/11-dynamic-programming/).

## Recursion vs iteration

| | Recursion | Iteration |
|---|-----------|-----------|
| Reads best for | Trees, graphs, divide and conquer, backtracking, nested structures | Simple counting, linear scans |
| Memory | O(depth) stack | Usually O(1) |
| Speed | Call overhead | Slightly faster |
| Risk | `StackOverflowError` on deep input | None |
| Java support | No tail-call optimization | Fully supported |

**Rule of thumb:** use recursion when the problem is naturally recursive **and** the depth is bounded (for example, balanced tree depth or about log n). Use a loop for linear problems or when the input can be large.

### Converting recursion to iteration

Any recursion can use an explicit stack:

```java
static int depthIter(Node root) {
    if (root == null) return 0;
    Deque<Node> nodes = new ArrayDeque<>();
    Deque<Integer> depths = new ArrayDeque<>();
    nodes.push(root);
    depths.push(1);
    int best = 0;
    while (!nodes.isEmpty()) {
        Node n = nodes.pop();
        int d = depths.pop();
        best = Math.max(best, d);
        if (n.left != null)  { nodes.push(n.left);  depths.push(d + 1); }
        if (n.right != null) { nodes.push(n.right); depths.push(d + 1); }
    }
    return best;
}
```

The explicit stack lives on the heap, so it does not overflow at normal depths.

## Types of recursion

| Type | Description | Example |
|------|-------------|---------|
| Direct | Method calls itself | `factorial` |
| Indirect (mutual) | `a` calls `b`, `b` calls `a` | `isEven(n)` / `isOdd(n)` |
| Linear | One recursive call per invocation | `factorial`, `sum` |
| Binary / branching | Two or more calls per invocation | `fib`, tree traversal |
| Tail | The recursive call is the last action | `gcd` (no benefit in Java) |

## Complexity

Count **how many calls** and **how much work per call**.

| Method | Time | Stack space |
|--------|------|-------------|
| `factorial(n)` | O(n) | O(n) |
| Naive `fib(n)` | O(2ⁿ) | O(n) |
| Memoized `fib(n)` | O(n) | O(n) |
| `power(x, n)` (halving) | O(log n) | O(log n) |
| Binary search | O(log n) | O(log n) |
| `hanoi(n)` | O(2ⁿ) | O(n) |

More: [27-dsa/00_complexity-analysis.md](../27-dsa/00_complexity-analysis.md). Backtracking problems (permutations, subsets, N-queens) build on this: [27-dsa/10-recursion-and-backtracking](../27-dsa/10-recursion-and-backtracking/).

## Debugging recursion

- Print the arguments on entry, indented by depth, to see the call tree
- Use the IDE debugger and watch the call stack panel ([intellij-idea](../00-setup/04_intellij-idea.md))
- Test the **base case first**, then one level above it, then larger inputs
- A `StackOverflowError` stack trace repeats the same lines: look for the missing or unreachable base case

```java
static long factorial(int n, int depth) {
    System.out.println("  ".repeat(depth) + "factorial(" + n + ")");
    ...
}
```

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| No base case | `StackOverflowError` | Add one |
| Base case never reached (wrong direction: `n + 1`) | `StackOverflowError` | Ensure each call gets smaller |
| Base case covers the wrong range (`n == 0` but input can be negative) | Infinite recursion | Use `n <= 0` or validate input |
| Recomputing the same subproblem | Exponential time | Memoize or iterate |
| Calling the recursive method twice when once suffices (`power`) | Slow | Store the result in a variable |
| Using recursion for a plain loop over a large input | Stack overflow | Iterate |
| `int` overflow in `factorial`/`fib` | Wrong or negative values | `long` (to 20!) or `BigInteger` |
| Mutating shared state without undoing it (backtracking) | Wrong results | Undo changes after each recursive call |
| Assuming Java optimizes tail calls | Still overflows | Rewrite as a loop |
| Catching `StackOverflowError` as a fix | Hides the bug | Fix the recursion or use iteration |

## Key takeaways

- Recursion = base case + a recursive step that makes progress
- Each call uses a stack frame, so depth is limited and Java does not optimize tail calls
- Branching recursion can repeat work: add memoization or switch to iteration
- Prefer loops for linear problems and large inputs; use recursion for trees, divide and conquer and backtracking
- Test base cases first and trace small inputs by hand

**Next:** [03-strings-and-text](../03-strings-and-text/README.md)
