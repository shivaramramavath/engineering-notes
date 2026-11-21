# Enumerations and Bitmask Flags: Type Safety, Bitwise Logic, and C23

> Using enums and bit flags in C: state machine modeling, bitmask combination logic, type safety limitations, underlying representation, and ISO C23 fixed underlying types.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Operators](../01-fundamentals/05-operators.md), [Structs, Unions, and Enums](./06-structs-unions-enums.md), [Bit-Fields](./16-bit-fields.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | ISO C11 / C17 / C23 |

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
9. [Comparison: When to Use What](#9-comparison-when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Magic numbers (such as `0`, `1`, `42`, or `0x80`) scattered across source code make programs unreadable and error-prone. In C, **Enumerations (`enum`)** assign human-readable symbolic names to integer constants.

Enums serve two distinct design patterns in systems engineering:
1. **Discrete State Modeling**: Mutually exclusive states in finite state machines (e.g., `STATE_IDLE`, `STATE_CONNECTING`, `STATE_CONNECTED`).
2. **Bitmask Flags**: Sets of combinable boolean options defined as powers of two (`1 << 0`, `1 << 1`, `1 << 2`), combined using bitwise OR (`|`) and queried using bitwise AND (`&`).

While enums look like strongly-typed types in other languages, in C they are intimately tied to standard integers. Understanding their underlying types, conversion rules, and the new ISO C23 fixed underlying type syntax is vital for writing portable, robust systems software.

---

## 2. Core Concepts

### Discrete Enumeration Constants
By default, the first enumerator has value `0`, and each subsequent enumerator increments by `1`:
```c
enum TrafficLight {
    LIGHT_RED,    /* 0 */
    LIGHT_YELLOW, /* 1 */
    LIGHT_GREEN   /* 2 */
};
```
Explicit integer values can be assigned to any enumerator:
```c
enum HttpStatus {
    HTTP_OK        = 200,
    HTTP_NOT_FOUND = 404,
    HTTP_ERROR     = 500
};
```

### Bitmask Flags Pattern
When options are not mutually exclusive, declare enumerators using bit shifts (`1 << N`):
```c
typedef enum FilePermission {
    PERM_READ    = 1 << 0, /* 0x01 */
    PERM_WRITE   = 1 << 1, /* 0x02 */
    PERM_EXECUTE = 1 << 2, /* 0x04 */
    PERM_ADMIN   = 1 << 3  /* 0x08 */
} FilePermission;
```
Combining flags:
- **Set/Grant Flag**: `perms |= PERM_EXECUTE;`
- **Clear/Revoke Flag**: `perms &= ~PERM_WRITE;`
- **Toggle/Flip Flag**: `perms ^= PERM_READ;`
- **Check/Query Flag**: `if ((perms & PERM_READ) == PERM_READ)`

### Type Safety Reality in C
Unlike C++, where enums are strictly typed and cannot be implicitly assigned arbitrary integers without a cast, in ISO C:
- Enumeration constants have type `int`.
- An enum variable can be assigned any integer value without a compiler error:
  ```c
  enum TrafficLight light = 999; /* Compiles without error in standard C! */
  ```
Compilers like GCC and Clang provide warning flags (`-Wenum-conversion`, `-Wswitch-enum`) to help enforce stricter enum hygiene.

---

## 3. How It Works Internally

### Bitmask State Representation

```
Bitmask State: uint8_t perms = PERM_READ | PERM_WRITE; (0x03)

 Bit Index:   7     6     5     4     3          2             1           0
 Flag:       ---   ---   ---   ---  ADMIN     EXECUTE        WRITE       READ
 Value:       0     0     0     0     0          0             1           1

Bitwise Operations:
 1. Query READ: (perms & 0x01) == 0x01 -> TRUE
 2. Grant EXEC:  perms |= (1 << 2)     -> 0x07 (0000 0111)
 3. Revoke WRITE perms &= ~(1 << 1)    -> 0x05 (0000 0101)
```

In compiled assembly, enum lookups and bitmask tests map directly to single fast ALU instructions: `test`, `and`, `or`, and `xor`.

---

## 4. Syntax & Usage

```c
#include <stdio.h>
#include <stdbool.h>

typedef enum WindowFlags {
    WIN_RESIZABLE = 1 << 0,
    WIN_FULLSCREEN = 1 << 1,
    WIN_BORDERLESS = 1 << 2
} WindowFlags;

void configure_window(int flags) {
    if (flags & WIN_FULLSCREEN) {
        /* Enable fullscreen mode */
    }
}
```

> [!NOTE]
> ### ISO C23 Enums with Fixed Underlying Types
> In C11/C17, the compiler chooses an integer type (usually `int` or `unsigned int`) large enough to represent all enumerators, causing ABI discrepancies across platforms. ISO C23 introduces **Fixed Underlying Types** (matching C++11):
> ```c
> enum Color : uint8_t { RED, GREEN, BLUE };
> ```
> This guarantees that `sizeof(enum Color) == 1`, reducing memory footprint in structures and establishing stable binary interfaces.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates bitmask flag creation, bitwise OR combination, bitwise AND query verification, flag granting, and flag revoking.

```c
#include <stdio.h>
#include <stdbool.h>

/* Bitmask flags enumeration */
typedef enum FilePermission {
    PERM_READ    = 1 << 0, /* 0x01 */
    PERM_WRITE   = 1 << 1, /* 0x02 */
    PERM_EXECUTE = 1 << 2, /* 0x04 */
    PERM_ADMIN   = 1 << 3  /* 0x08 */
} FilePermission;

static bool has_permission(int user_perms, FilePermission req) {
    return (user_perms & req) == req;
}

int main(void) {
    printf("=== C Enums and Flags Demo ===\n");

    /* Combine permissions with bitwise OR */
    int current_perms = PERM_READ | PERM_WRITE;
    printf("Permissions bitmask: 0x%02X\n", current_perms);

    printf("Has READ: %s\n", has_permission(current_perms, PERM_READ) ? "YES" : "NO");
    printf("Has WRITE: %s\n", has_permission(current_perms, PERM_WRITE) ? "YES" : "NO");
    printf("Has EXECUTE: %s\n", has_permission(current_perms, PERM_EXECUTE) ? "YES" : "NO");

    /* Grant EXECUTE */
    current_perms |= PERM_EXECUTE;
    printf("After granting EXECUTE: 0x%02X (Has EXECUTE: %s)\n",
           current_perms, has_permission(current_perms, PERM_EXECUTE) ? "YES" : "NO");

    /* Revoke WRITE */
    current_perms &= ~PERM_WRITE;
    printf("After revoking WRITE: 0x%02X (Has WRITE: %s)\n",
           current_perms, has_permission(current_perms, PERM_WRITE) ? "YES" : "NO");

    return 0;
}
```

```text
=== C Enums and Flags Demo ===
Permissions bitmask: 0x03
Has READ: YES
Has WRITE: YES
Has EXECUTE: NO
After granting EXECUTE: 0x07 (Has EXECUTE: YES)
After revoking WRITE: 0x05 (Has WRITE: NO)
```

---

## 6. Best Practices

1. **Use `enum` Over `#define` for Grouped Constants**: Enums are visible to GDB debuggers, enable compiler switch-case coverage checking (`-Wswitch`), and group related states logically.
2. **Always Enable `-Wswitch` and `-Wswitch-enum`**: When switching over an enum, the compiler will warn if any enum constant is missing from the `switch` statements.
3. **Always Check Bitmask Flags with `(val & FLAG) == FLAG`**: Writing `if (val & FLAG)` works for single-bit flags, but fails if `FLAG` is a multi-bit mask. Always test equality with the target mask.
4. **Define a Clear/None Constant**: Include an explicit `0` value (e.g., `PERM_NONE = 0`) to document default uninitialized states.
5. **Use C23 Fixed-Type Enums in Serialized Structs**: In network packet or storage structures, use `enum Mode : uint8_t` (or explicit `uint8_t` variables) to prevent 32-bit compiler padding.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Testing multi-bit mask with `if (val & MASK)`** | Evaluates to true if *any* single bit matches, even if other required bits are clear. | Test `if ((val & MASK) == MASK)`. |
| **Clearing flag with `val &= ~FLAG` using wrong type** | If `FLAG` is 32-bit and `val` is 64-bit, `~FLAG` sign-extends, corrupting upper bits. | Use unsigned literal shifts: `1ULL << N`. |
| **Assuming enums are type-safe in C** | C compilers allow assigning arbitrary integers to enum variables without error. | Enable `-Wenum-conversion` and write runtime bounds assertions. |
| **Omitting parentheses around bitwise operations** | `if (val & FLAG == FLAG)` parses as `val & (FLAG == FLAG)` due to operator precedence! | Always parenthesize: `if ((val & FLAG) == FLAG)`. |
| **Sequential numbers for bit flags** | Defining flags as `1, 2, 3, 4`: Value `3` is `1 | 2`, causing overlapping flag collisions. | Use powers of two: `1 << 0, 1 << 1, 1 << 2, 1 << 3`. |
| **Missing `default` in non-exhaustive switch** | If a new enum constant is added later, unhandled cases produce undefined program state. | Handle all cases or provide an explicit `default: abort();`. |

---

## 8. Performance & Security Notes

### Zero Overhead Abstraction
Enumeration constants are resolved entirely during translation. In assembly, enum names disappear completely, replaced with immediate numerical constants (`$0x01`). There is **zero memory lookup or pointer overhead**.

### Security: Enum Value Out-of-Bounds in Switch Jumps
Optimizing compilers compile dense `switch` statements into **jump tables**. If an attacker can inject an arbitrary out-of-range integer into an enum variable, and the compiler assumes all enum values are within known bounds, the jump table calculation can jump to arbitrary unmapped memory addresses. Always validate input integers against valid enum ranges before dispatching.

---

## 9. Comparison: When to Use What

| Technique | Debugger Symbol? | Switch Coverage Checking? | Memory Overhead | Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Discrete Enum** | Yes (Prints name in GDB) | Yes (`-Wswitch`) | 0 (Compile-time) | State machines, error codes, modes |
| **Bitmask Enum (`1 << N`)** | Yes | No (Bitwise combinations) | 0 (Compile-time) | Combinable flags, permissions, options |
| **Object Macro (`#define`)** | No | No | 0 | Header guards, feature flags |
| **Bit-Field Struct** | Yes | No | Unused padding | Low-level hardware register models |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the type of an enumeration constant in ISO C vs. C++?</strong></summary>

In ISO C (§6.7.2.2), all enumeration constants have type **`int`**, regardless of the underlying type chosen for the enum variable itself. In C++, enumeration constants have the type of the enumeration itself (`enum Type`), enforcing strict type checking and forbidding implicit conversions to integers without static casts.
</details>

<details>
<summary><strong>2. Why does <code>if (flags &amp; MASK == MASK)</code> fail to compile or execute incorrectly?</strong></summary>

In the C operator precedence hierarchy, the equality operator `==` has higher precedence than the bitwise AND operator `&`. Therefore, `flags & MASK == MASK` is evaluated as `flags & (MASK == MASK)`, which evaluates to `flags & 1`. Always write `((flags & MASK) == MASK)`.
</details>

<details>
<summary><strong>3. How does the compiler decide the size of an <code>enum</code> variable in C11/C17?</strong></summary>

In C11/C17 (§6.7.2.2), the standard states that the underlying type is an implementation-defined integer type capable of representing all enumerator values. Typically, compilers default to `int` or `unsigned int` (4 bytes).
</details>

<details>
<summary><strong>4. How do fixed underlying types in ISO C23 improve embedded software?</strong></summary>

By declaring `enum Status : uint8_t { ... };`, the developer explicitly forces the compiler to allocate exactly 1 byte for the enum. This guarantees that struct layouts, memory padding, and network ABI definitions remain identical across 8-bit, 32-bit, and 64-bit microcontroller compilers.
</details>

<details>
<summary><strong>5. How do you toggle a specific bit flag in a bitmask?</strong></summary>

Using the bitwise XOR operator (`^`):
`flags ^= FLAG_NAME;`
If the flag bit was `1`, `1 ^ 1` sets it to `0`. If it was `0`, `0 ^ 1` sets it to `1`.
</details>

<details>
<summary><strong>6. Can an enum have multiple enumerators with the exact same value?</strong></summary>

Yes. It is common to define aliases:
```c
enum Color {
    COLOR_RED = 1,
    COLOR_BLUE = 2,
    COLOR_PRIMARY_FIRST = COLOR_RED
};
```
</details>

<details>
<summary><strong>7. What compiler warning ensures all enum values are handled in a switch statement?</strong></summary>

`-Wswitch` warns if an enum value is not handled in a `switch` statement that lacks a `default` case. `-Wswitch-enum` warns if any enum value is missing, even if a `default` case is present.
</details>

<details>
<summary><strong>8. Can an enum constant be negative in C?</strong></summary>

Yes. Because enumeration constants have type `int`, negative values like `STATUS_ERROR = -1` are valid and standard.
</details>

---

## 11. Practice Exercises

### Exercise 1: State Machine Validator (Easy)
Define an enum representing the state of a TCP connection (`TCP_CLOSED`, `TCP_LISTEN`, `TCP_SYN_SENT`, `TCP_ESTABLISHED`). Write a function `const char* tcp_state_name(enum TcpState s)` returning the string name.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>

typedef enum {
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_ESTABLISHED
} TcpState;

const char* tcp_state_name(TcpState s) {
    switch (s) {
        case TCP_CLOSED:      return "CLOSED";
        case TCP_LISTEN:      return "LISTEN";
        case TCP_SYN_SENT:    return "SYN_SENT";
        case TCP_ESTABLISHED: return "ESTABLISHED";
        default:              return "UNKNOWN";
    }
}
```
</details>

### Exercise 2: Bitmask Flag Manipulator (Medium)
Write three helper functions: `grant_perm(int *mask, int perm)`, `revoke_perm(int *mask, int perm)`, and `check_perm(int mask, int perm)`.

<details>
<summary>View Solution</summary>

```c
#include <stdbool.h>

void grant_perm(int *mask, int perm) {
    if (mask) *mask |= perm;
}

void revoke_perm(int *mask, int perm) {
    if (mask) *mask &= ~perm;
}

bool check_perm(int mask, int perm) {
    return (mask & perm) == perm;
}
```
</details>

### Exercise 3: Dynamic Flag String Formatter (Hard)
Given a bitmask of file permissions (`READ = 1`, `WRITE = 2`, `EXEC = 4`), write a function `char* format_permissions(int perms, char buf[4])` that returns a UNIX-style string (`"rwx"`, `"r--"`, `"-w-"`).

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>

#define P_READ  (1 << 0)
#define P_WRITE (1 << 1)
#define P_EXEC  (1 << 2)

char* format_permissions(int perms, char buf[4]) {
    buf[0] = (perms & P_READ)  ? 'r' : '-';
    buf[1] = (perms & P_WRITE) ? 'w' : '-';
    buf[2] = (perms & P_EXEC)  ? 'x' : '-';
    buf[3] = '\0';
    return buf;
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Enum definition */
typedef enum State { IDLE, RUNNING, STOPPED } State;

/* Bitmask definitions */
typedef enum Flags {
    FLAG_A = 1 << 0, /* 0x01 */
    FLAG_B = 1 << 1, /* 0x02 */
    FLAG_C = 1 << 2  /* 0x04 */
} Flags;

/* Bitwise Idioms */
flags |= FLAG_A;               /* Set */
flags &= ~FLAG_A;              /* Clear */
flags ^= FLAG_A;               /* Toggle */
if ((flags & FLAG_A) == FLAG_A)/* Query */

/* ISO C23 Fixed Underlying Type */
enum PacketType : uint8_t { DATA, ACK, NAK };
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §6.7.2.2 "Enumeration specifiers".
- ISO/IEC 9899:2024 (C23 Standard), §6.7.2.2 "Enumeration specifiers" (Fixed underlying types).
- *The Practice of Programming* by Brian Kernighan & Rob Pike (Style and Constants).
- GCC Compiler Warnings: `-Wswitch`, `-Wswitch-enum`, `-Wenum-conversion`.

---

[← Previous: Bit-Fields](./16-bit-fields.md) | [Section Index](./README.md) | [Next: Designated Initializers & Compound Literals →](./18-designated-initializers-and-compound-literals.md)
