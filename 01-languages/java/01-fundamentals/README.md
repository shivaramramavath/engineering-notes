# 01 - Fundamentals

The building blocks of every Java program: syntax, data, operators, decisions, repetition and basic input/output. Everything later in the repo depends on these, and most "tricky Java" interview questions start here.

```
syntax ─► variables & types ─► operators ─► casting ─► wrappers
                                                          │
console I/O ◄─ numeric precision ◄─ arrays ◄─ loops ◄─ conditionals
```

## Prerequisites

[00-setup](../00-setup/README.md) completed: you can compile and run a Java file.

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 0 | [00_syntax-and-program-structure.md](./00_syntax-and-program-structure.md) | Program anatomy, `main`, comments, naming, compile vs runtime errors |
| 1 | [01_variables-and-data-types.md](./01_variables-and-data-types.md) | Primitives, reference types, literals, scope, defaults |
| 2 | [02_operators.md](./02_operators.md) | Arithmetic, logical, bitwise, precedence, short-circuiting |
| 3 | [03_type-casting-and-conversion.md](./03_type-casting-and-conversion.md) | Widening, narrowing, numeric promotion, string conversion |
| 4 | [04_wrapper-classes-and-autoboxing.md](./04_wrapper-classes-and-autoboxing.md) | `Integer` and friends, the cache, `null` unboxing |
| 5 | [05_conditionals.md](./05_conditionals.md) | `if`, ternary, classic `switch` |
| 6 | [06_loops.md](./06_loops.md) | `for`, `while`, `do-while`, for-each, `break`/`continue` |
| 7 | [07_arrays.md](./07_arrays.md) | 1D and 2D arrays, copying, the `Arrays` class |
| 8 | [08_numeric-precision-and-math.md](./08_numeric-precision-and-math.md) | Floating-point traps, `Math`, `BigDecimal`, overflow |
| 9 | [09_console-input-output.md](./09_console-input-output.md) | `Scanner`, `printf`, command-line arguments |

## Practice

| After file | Try |
|------------|-----|
| 01 | Print the min and max of every primitive type |
| 02 | Predict the output of 10 expressions in [JShell](../00-setup/03_jshell.md), then check |
| 05 | Write a grade calculator and a leap-year checker |
| 06 | FizzBuzz, prime test, multiplication table, digit sum, reverse a number |
| 07 | Find min/max, reverse in place, rotate, remove duplicates, matrix transpose |
| 08 | Compute a bank balance with `BigDecimal` and with `double`; compare the results |
| 09 | A menu-driven calculator that reads from the console |

**Project:** [26-projects/01-cli-application](../26-projects/01-cli-application/) fits here once you have also read `02-methods` and `03-strings-and-text`.

## You are done when you can

- [ ] Explain why `byte b = 10; b = b + 1;` does not compile but `b += 1;` does
- [ ] Explain why `0.1 + 0.2 != 0.3` and what to use for money
- [ ] Explain why `Integer a = 127, b = 127;` gives `a == b` but 128 does not
- [ ] Write nested loops over a 2D array without index errors
- [ ] Read integers and lines from the console without the `nextLine()` newline bug
- [ ] Predict `5 / 2`, `-5 % 3`, `'a' + 1`, `1 + 2 + "3"` and `(int) 3.99` without running them

## Key takeaways

- Java is statically typed: every variable has a type known at compile time
- Eight primitives hold values; everything else is a reference (or `null`)
- Most beginner bugs here come from integer division, overflow, implicit casts, `==` on objects, and floating-point equality

**Next:** [02-methods](../02-methods/README.md)
