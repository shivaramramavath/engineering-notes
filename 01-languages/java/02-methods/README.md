# Module 02: Methods, Parameter Evaluation, Overloading, and Recursion

> Procedural abstraction and execution dynamics in the JVM: method anatomy, stack frame allocation, strict pass-by-value semantics, the three-phase overload resolution protocol, variable-arity parameters (`T...`), call stack limits, and functional trampolines.

| Metadata | Value |
| :--- | :--- |
| **Track Section** | 01-languages / java |
| **Module Number** | 02 |
| **Prerequisites** | [01-fundamentals](../01-fundamentals/README.md) |
| **Baseline Runtime** | Oracle JDK 25 LTS / Eclipse Temurin 25 LTS |
| **Core Concepts** | Stack Frames, `invokevirtual` vs `invokestatic`, Pass-by-Value, Overload Resolution, TCO limits |

---

## 1. Architectural Overview & Conceptual Flow

```
[ 00 Method Declarations ] ──► [ 01 Pass-by-Value ]
  (Signature, Stack Frames,      (Bit copying, Reference copying,
   JVM Invocation Opcodes)        Reassignment vs Mutation)
           │
           ▼
[ 02 Overloading & Varargs ] ──► [ 03 Recursion Mechanics ]
  (Three-Phase Resolution,         (Call stack depth, No native TCO,
   Array desugaring, @SafeVarargs)  Memoization, Trampoline Pattern)
```

---

## 2. Topic Directory & Navigation

| # | Topic Document | Primary Engineering Focus | Target Lines |
|---|---|---|---|
| 00 | [00_methods.md](./00_methods.md) | Method anatomy, signatures, return rules, JVM stack frames (Local Variable Array, Slot 0 `this`), the 5 invocation opcodes (`invokestatic`, `invokevirtual`, `invokespecial`, `invokeinterface`, `invokedynamic`), and HotSpot C2 method inlining. | 580 |
| 01 | [01_pass-by-value.md](./01_pass-by-value.md) | Unconditional pass-by-value law: primitive bit copying vs reference pointer copying, why `swap(a, b)` fails in Java, defensive copying against reference hijacking, and immutable records. | 592 |
| 02 | [02_overloading-and-varargs.md](./02_overloading-and-varargs.md) | Compile-time static binding, the JLS §15.12.2 three-phase resolution protocol (Widening $\to$ Boxing $\to$ Varargs), ambiguous calls with `null`, generic heap pollution, and `@SafeVarargs`. | 577 |
| 03 | [03_recursion.md](./03_recursion.md) | Base case vs recursive step, stack frame memory growth, `StackOverflowError` limits (`-Xss`), lack of native Tail Call Optimization in HotSpot, memoization, and the functional Trampoline pattern. | 595 |

---

## 3. Core Engineering Competency Verification

You have mastered Java Methods when you can definitively:
- [x] Trace stack frame allocation during an instance method call, explaining why Slot 0 is reserved for `this`.
- [x] Prove why writing `p = new Person()` inside a method has zero effect on the caller's reference variable, while `p.setName()` mutates the shared heap object.
- [x] Predict the exact compile-time overload chosen across complex competing signatures (primitive widening vs boxing vs varargs).
- [x] Explain why `Arrays.asList(new int[]{1, 2, 3}).size()` returns `1` instead of `3`.
- [x] Explain the architectural reasons why the HotSpot JVM does not implement Tail Call Optimization (TCO).
- [x] Implement the functional Trampoline pattern to execute deeply recursive algorithms with $O(1)$ constant call stack space.

---

| [← Previous Module: 01-fundamentals](../01-fundamentals/README.md) | [Next Module: 03-strings-and-text →](../03-strings-and-text/README.md) |
