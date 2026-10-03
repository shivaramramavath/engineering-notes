# 02 - Methods

Methods are how you name, reuse and organize behavior. This folder also covers the two most common sources of confusion at this stage: **what really happens when you pass arguments**, and **how a method can call itself**.

```
methods ─► pass-by-value ─► overloading & varargs ─► recursion
```

## Prerequisites

[01-fundamentals](../01-fundamentals/README.md), mainly variables, operators, conditionals, loops and arrays.

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 0 | [00_methods.md](./00_methods.md) | Declaring and calling methods, parameters, return values, scope, good design |
| 1 | [01_pass-by-value.md](./01_pass-by-value.md) | Why Java is always pass-by-value, including for objects |
| 2 | [02_overloading-and-varargs.md](./02_overloading-and-varargs.md) | Same name, different parameters; overload resolution; `T...` |
| 3 | [03_recursion.md](./03_recursion.md) | Base case, call stack, memoization, recursion vs iteration |

## Practice

| After file | Try |
|------------|-----|
| 00 | Write `isPrime(int)`, `max(int[])`, `average(int[])`, `isLeapYear(int)`; keep each under 15 lines |
| 01 | Predict the output of the examples before running them; write a `swap` that works for two `int`s |
| 02 | Write `sum(int...)`, `max(int, int...)`; add overloads and predict which one runs for `m(5)`, `m('a')`, `m(null)` |
| 03 | Factorial, Fibonacci (naive, then memoized), `gcd`, `power(x, n)` in O(log n), palindrome check, Tower of Hanoi |

**Project:** [26-projects/01-cli-application](../26-projects/01-cli-application/) becomes comfortable once you finish this folder and `03-strings-and-text`.

## You are done when you can

- [ ] Split a 50-line `main` into small methods that each do one thing
- [ ] Explain in two sentences why a `swap(int a, int b)` method cannot swap the caller's variables
- [ ] Explain why changing `person.name` inside a method is visible to the caller but `person = new Person()` is not
- [ ] Predict which overload is chosen for `m(5)` when `m(long)`, `m(Integer)` and `m(Object)` exist
- [ ] Write a recursive method with a correct base case and explain its call stack
- [ ] Say when to prefer iteration over recursion in Java

## Key takeaways

- A method = name + parameter list + return type + body; the name and parameter types form its **signature**
- Java passes **everything by value**; for objects the value is a copy of the reference
- Overloads are chosen at **compile time** from the static types of the arguments
- Recursion needs a base case and consumes stack space; Java does not optimize tail calls

**Next:** [03-strings-and-text](../03-strings-and-text/README.md)
