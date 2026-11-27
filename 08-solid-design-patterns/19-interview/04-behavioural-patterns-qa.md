# Behavioural Patterns Q&A

<details><summary>1. Strategy vs State — what's the real difference?</summary>
Both delegate to a polymorphic object, but **Strategy** is chosen by the caller and swaps an algorithm (`QuickSort` vs `MergeSort`), while **State** models an object's internal lifecycle where states trigger transitions to other states (`Draft -> Paid -> Shipped`).
</details>

<details><summary>2. How does Observer cause memory leaks, and how do you prevent it?</summary>
The Subject holds strong references to Observers; if an Observer isn't unsubscribed on destroy/unmount, it can't be garbage-collected (Lapsed Listener Problem). Always unsubscribe (`dispose()` / `AbortController`) or use weak references.
</details>

<details><summary>3. Why do modern Java 25, TS 5.8, and Python 3.14 rarely need the GoF Visitor pattern?</summary>
`sealed` interfaces / discriminated unions + exhaustive `switch`/`match` let you add new operations over a closed set of variants in one function without double-dispatch boilerplate.
</details>

<details><summary>4. Command vs Strategy?</summary>
**Command** encapsulates an *action + its arguments + receiver* as an object (for undo, queuing, logging). **Strategy** encapsulates *how* a task is performed.
</details>

<details><summary>5. Chain of Responsibility vs Onion Middleware?</summary>
Classic CoR usually passes a request until *one* handler handles it; Onion Middleware wraps `await next()` so every layer runs pre- and post-processing around the core handler.
</details>
