# C Fundamentals Technical Interview Guide (`01-c-basics-qa`)

> Master core C language interview questions: storage classes, linkage, type conversions, sequence points, and preprocessor mechanics.

| Metadata | Details |
| :--- | :--- |
| **Guide** | `01-c-basics-qa` |
| **Level** | All Levels |
| **Standard Baseline** | C11 / C17 / C23 |

---

## 1. Storage Classes, Scopes, and Linkage

<details>
<summary><strong>Q1: What are the four storage durations defined in ISO C, and how do they differ?</strong></summary>

The four storage durations in ISO C (Section 6.2.4) are:
1. **Automatic:** Allocated at block entry, destroyed at block exit (local variables, function arguments). Stored in CPU registers or on the call stack frame.
2. **Static:** Allocated at program startup, persists until termination (`static` locals, global variables). Stored in `.data` (initialized) or `.bss` (zero-initialized).
3. **Allocated:** Explicitly managed by dynamic allocators (`malloc`, `calloc`, `realloc`). Lifetime persists until `free()`. Stored on the heap.
4. **Thread (C11):** Declared with `_Thread_local` or `thread_local`. Lifetime spans the duration of the thread.
</details>

<details>
<summary><strong>Q2: What is the difference between internal and external linkage?</strong></summary>

- **External Linkage (`extern` or default non-static globals):** Symbol is published in the global symbol table. Can be referenced by any translation unit across the executable.
- **Internal Linkage (`static` at file scope):** Symbol is marked local to the current translation unit (`STB_LOCAL`). Invisible to other `.c` files, eliminating naming collisions.
- **No Linkage (Local block variables):** Identifier refers solely to a unique local entity within its lexical scope.
</details>

<details>
<summary><strong>Q3: What does the `static` keyword mean in C across different contexts?</strong></summary>

1. **At File Scope:** Gives functions and global variables *internal linkage*, restricting visibility strictly to the current translation unit.
2. **Inside a Block / Function:** Gives local variables *static storage duration*. The variable is initialized once at startup and retains its value across function calls.
3. **In Array Parameter Declarations (C99):** In `void foo(int arr[static 10])`, informs the compiler that `arr` points to an array containing at least 10 non-null elements, enabling aggressive loop vectorization.
</details>

---

## 2. Type System and Promotions

<details>
<summary><strong>Q4: Explain Integer Promotions and the "Usual Arithmetic Conversions" in C.</strong></summary>

In C, integer types smaller than `int` (`char`, `signed char`, `unsigned char`, `short`, `unsigned short`, `bool`) are automatically promoted to `int` (or `unsigned int` if `int` cannot represent all values) before any arithmetic or bitwise operation.

The **Usual Arithmetic Conversions** determine the common real type for binary operators:
1. If either operand is `long double`, `double`, or `float`, the other is converted to match floating types.
2. Otherwise, both undergo integer promotions.
3. If types match, no further conversion occurs.
4. If both have the same signedness, the smaller rank converts to the higher rank.
5. If the unsigned operand has equal or higher rank, the signed operand converts to unsigned!
*(Warning: `if (-1 < 1U)` evaluates to `false` because `-1` converts to `UINT_MAX`!)*
</details>

<details>
<summary><strong>Q5: What is the exact difference between `typedef` and `#define` for types?</strong></summary>

- `typedef` is processed by the compiler front-end and respects lexical scoping rules. It creates a true type alias with proper pointer binding.
- `#define` is a textual preprocessor substitution without semantic awareness.
Example hazard:
```c
#define PINT int*
PINT a, b; /* Expands to: int* a, b; -> 'a' is pointer, 'b' is plain int! */

typedef int* IntPtr;
IntPtr x, y; /* Both 'x' and 'y' are pointers to int! */
```
</details>

---

## 3. Sequence Points and Preprocessor

<details>
<summary><strong>Q6: What is a Sequence Point (C11 "sequenced before" relation), and why does `i = i++` invoke UB?</strong></summary>

A sequence point represents a point in program execution where all side effects of previous evaluations are guaranteed to be complete, and no side effects of subsequent evaluations have occurred.
In C11 (Section 6.5#2): *If a side effect on a scalar object is unsequenced relative to either a different side effect on the same scalar object or a value computation using the value of the same scalar object, the behavior is undefined.*

In `i = i++`:
- Modification of `i` via post-increment (`i++`)
- Modification of `i` via assignment (`=`)
Both side effects occur unsequenced within the same full expression, triggering Undefined Behavior.
</details>

<details>
<summary><strong>Q7: Why must multi-statement macros always be wrapped in `do { ... } while(0)`?</strong></summary>

Wrapping in `do { ... } while(0)` forces the macro expansion to behave syntactically like a single statement that safely accepts a trailing semicolon:
```c
#define DO_SOMETHING(x) do { step1(x); step2(x); } while(0)

if (condition)
    DO_SOMETHING(val);
else
    fallback();
```
Without `do { ... } while(0)`, expanding braces `{ step1; step2; };` introduces an unexpected semicolon that breaks the `else` clause binding.
</details>

<details>
<summary><strong>Q8: What are `#` and `##` operators in the C preprocessor?</strong></summary>

- `#` (Stringification): Converts a macro parameter into a string literal (e.g., `#x` with `x = 42` becomes `"42"`).
- `##` (Token Pasting): Concatenates two preprocessor tokens into a single valid syntactic token (e.g., `cmd_ ## name` with `name = run` becomes `cmd_run`).
</details>

---

[← Interview Index](./README.md) | [Next: Pointers & Memory QA →](./02-pointers-and-memory-qa.md)
