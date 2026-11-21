# Structs, Unions, and Enums

> Composite and user-defined types in C: memory layouts, alignment padding, discriminated unions, and enumeration constants.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Pointers](./03-pointers.md), [Declarations and Typedef](./04-declarations-typedef.md), [Type Qualifiers](./05-type-qualifiers.md) |
| **Time to Read** | 30 minutes |
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

Real-world software models complex entities containing multiple attributes. C provides three core mechanisms for defining custom data types:
1. **`struct` (Structure)**: Aggregates heterogeneous data members stored contiguously in memory. Each member occupies its own distinct offset.
2. **`union`**: Overlays heterogeneous data members within the **same shared memory region**. The size of a union is determined by its largest member.
3. **`enum` (Enumeration)**: Defines a named type representing a discrete set of integer constants, improving readability and self-documentation over magic numbers.

Together with pointers, these constructs enable object-oriented abstractions, abstract data types (ADTs), protocol packet serialization, tagged variant types, and operating system control blocks.

---

## 2. Core Concepts

### Structures (`struct`)
A structure groups variables under a single name. Members are laid out in the order declared.
- **Member Access**: The direct member operator `.` is used on lvalues (`hdr.magic`), while the arrow operator `->` dereferences a pointer (`p->magic` is sugar for `(*p).magic`).
- **Value Semantics**: Structures are first-class values in C. Assigning `struct A = struct B;` performs a shallow byte-for-byte copy of all members, including embedded arrays. Passing a structure by value copies the entire structure onto the call stack.
- **No Equality Operator**: C does not provide `==` or `!=` for structures because alignment padding bytes contain arbitrary, uninitialized values. Member-by-member comparison is required.

### Unions (`union`)
A union stores different types in the same physical memory space:
- All members share the base address (`(void *)&u == (void *)&u.member`).
- Writing to one member overwrites the contents of all other members.
- **Type Punning**: In ISO C (Annex J / §6.5.2.3 footnote 95), reading from a union member other than the one last written is explicitly permitted to inspect the binary representation of data (unlike C++ where it is undefined behavior).
- **Tagged Unions (Variants)**: A common architectural pattern pairs an enum tag with a union inside a wrapper struct to safely model polymorphic values.

### Enumerations (`enum`)
An enumeration assigns symbolic names to integer constants:
- By default, the first enumerator is `0`, and each subsequent enumerator increments by `1`.
- Explicit values can be assigned to create bitmasks or error codes.
- In C11/C17, enumeration constants have type `int`.

---

## 3. How It Works Internally

### Alignment, Padding, and `offsetof`

Compilers insert invisible **padding bytes** between struct members to align each member to its natural machine boundary (typically 4 bytes for 32-bit integers, 8 bytes for 64-bit pointers and doubles).

```
Structure Alignment Example:
struct Header {
    uint8_t  magic;       /* 1 byte  at offset 0 */
                          /* 3 bytes padding (invisible) */
    uint32_t payload_len; /* 4 bytes at offset 4 */
    uint16_t checksum;    /* 2 bytes at offset 8 */
                          /* 2 bytes padding at end to align whole struct to multiple of 4 */
};

Memory Layout (Total Size = 12 Bytes):
 0        1        2        3        4        5        6        7
┌────────┬────────┬────────┬────────┬────────┬────────┬────────┬────────┐
│ magic  │  PAD   │  PAD   │  PAD   │      payload_len (4 bytes)      │
├────────┴────────┴────────┴────────┼────────┴────────┴────────┴────────┤
│     checksum (2 bytes)    │  PAD   │  PAD   │ (Total: 12 bytes)       │
└───────────────────────────────────┴────────┴─────────────────────────┘
 8        9        10       11
```

The standard macro `offsetof(type, member)` defined in `<stddef.h>` returns the byte offset of a member from the start of the structure.

### Discriminated Union Memory Overlay

```
DynamicValue Union Layout:
┌─────────────────────────┬─────────────────────────────────────────────────┐
│ type (ValueType enum)   │ data (Union of int64_t, double, char as_str[16])│
│ 4 bytes (+ 4 pad)       │ 16 bytes shared storage                         │
└─────────────────────────┴─────────────────────────────────────────────────┘
Offset 0                   Offset 8                                   Offset 24
```

---

## 4. Syntax & Usage

### Structs, Flexible Array Members, and Bit-Fields
```c
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* Structure with Flexible Array Member (C99) */
typedef struct DynamicPacket {
    size_t length;
    uint8_t payload[]; /* Must be the last member; zero footprint in sizeof */
} DynamicPacket;

/* Bit-fields: Packed status flags */
typedef struct RegisterFlags {
    uint8_t enable     : 1;
    uint8_t mode       : 3;
    uint8_t error_code : 4;
} RegisterFlags;
```

> [!NOTE]
> ### ISO C23 Updates: Fixed-Type Enums and Empty Initializers
> - **Enums with Fixed Underlying Types**: C23 allows specifying the exact integer type backing an enumeration, e.g., `enum Color : uint8_t { RED, GREEN, BLUE };`. This guarantees memory footprint and ABI consistency across platforms.
> - **Empty Initializer `{}`**: C23 allows `struct Header h = {};` to zero-initialize all members, bringing parity with C++. In C11/C17, `{0}` was required.
> - **Enhanced Attribute Specifiers**: `[[deprecated]]` and `[[maybe_unused]]` can be applied directly to struct definitions, enum constants, and members.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates enum constants, struct padding measurement using `offsetof`, designated initializers, discriminated unions, and flexible array member calculations.

```c
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Enum definition with custom values */
typedef enum Status {
    STATUS_OK = 0,
    STATUS_PENDING = 1,
    STATUS_ERROR = -1
} Status;

/* Struct alignment and offset demonstration */
typedef struct Header {
    uint8_t  magic;       /* 1 byte */
    /* 3 bytes padding */
    uint32_t payload_len; /* 4 bytes */
    uint16_t checksum;    /* 2 bytes */
    /* 2 bytes padding at end to align whole struct */
} Header;

/* Discriminated (tagged) union */
typedef enum ValueType {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING
} ValueType;

typedef struct DynamicValue {
    ValueType type;
    union {
        int64_t     as_int;
        double      as_float;
        char        as_str[16];
    } data;
} DynamicValue;

/* Flexible Array Member */
typedef struct Packet {
    size_t length;
    uint8_t payload[]; /* Flexible array member: C99 onwards */
} Packet;

int main(void) {
    printf("=== C Structs, Unions, and Enums Demo ===\n");

    /* 1. Enum usage */
    Status s = STATUS_PENDING;
    printf("Enum Status value: %d\n", (int)s);

    /* 2. Struct layout and offsetof */
    printf("Header sizeof: %zu bytes\n", sizeof(Header));
    printf("Header.magic offset: %zu\n", offsetof(Header, magic));
    printf("Header.payload_len offset: %zu\n", offsetof(Header, payload_len));
    printf("Header.checksum offset: %zu\n", offsetof(Header, checksum));

    /* Designated initializer */
    Header hdr = {
        .magic = 0xAA,
        .payload_len = 1024,
        .checksum = 0xF00D
    };
    printf("Header initialized: magic=0x%X, len=%u, sum=0x%X\n",
           hdr.magic, (unsigned int)hdr.payload_len, hdr.checksum);

    /* 3. Discriminated Union */
    DynamicValue v1;
    v1.type = VAL_INT;
    v1.data.as_int = 9876543210LL;

    DynamicValue v2;
    v2.type = VAL_STRING;
    strncpy(v2.data.as_str, "Hello Union", sizeof(v2.data.as_str) - 1);
    v2.data.as_str[sizeof(v2.data.as_str) - 1] = '\0';

    printf("DynamicValue sizeof: %zu bytes\n", sizeof(DynamicValue));
    if (v1.type == VAL_INT) {
        printf("v1 integer: %lld\n", (long long)v1.data.as_int);
    }
    if (v2.type == VAL_STRING) {
        printf("v2 string: %s\n", v2.data.as_str);
    }

    /* 4. Flexible Array Member sizeof check */
    printf("Packet header base sizeof: %zu bytes\n", sizeof(Packet));

    return 0;
}
```

```text
=== C Structs, Unions, and Enums Demo ===
Enum Status value: 1
Header sizeof: 12 bytes
Header.magic offset: 0
Header.payload_len offset: 4
Header.checksum offset: 8
Header initialized: magic=0xAA, len=1024, sum=0xF00D
DynamicValue sizeof: 24 bytes
v1 integer: 9876543210
v2 string: Hello Union
Packet header base sizeof: 4 bytes
```

---

## 6. Best Practices

1. **Reorder Struct Members to Minimize Padding**: Order members in descending order of size/alignment (e.g., 8-byte pointers first, then 4-byte integers, 2-byte shorts, and 1-byte chars). This avoids wasted padding bytes.
2. **Never Use `memcmp` to Compare Structs**: Structures contain uninitialized padding bytes with undefined contents. Comparing structs with `memcmp(&a, &b, sizeof a)` produces false negatives. Write a dedicated comparison function.
3. **Always Tag Unions**: Never use bare unions unless implementing specialized low-level bit-punning. Always enclose unions inside a struct containing an enum tag indicating which variant is currently active.
4. **Use Designated Initializers**: Always initialize structs and unions using C99 designated initializers (`.member = val`). This ensures unmentioned members are safely zero-initialized and protects against member reordering bugs.
5. **Pass Structs Larger Than 16 Bytes by `const` Pointer**: Passing large structs by value causes substantial stack memory copying overhead. Pass `const MyStruct *ptr` instead.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Comparing structs with `memcmp`** | Padding bytes hold arbitrary values; two structs with identical members can evaluate as not equal. | Compare each member explicitly with a custom comparator function. |
| **Assuming struct size is the sum of member sizes** | Compilers insert padding bytes for alignment. `sizeof(struct)` is often significantly larger than the sum. | Use `sizeof(struct Type)` and inspect member offsets with `offsetof()`. |
| **Reading inactive union members without caution** | While type-punning is permitted in C, interpreting inactive members of different sizes or alignments can cause garbage reads. | Use a tagged union to verify the active variant before reading. |
| **Allocating Flexible Array Members incorrectly** | Forgetting to add the dynamic array payload size to `sizeof(struct)` causes buffer overflow when accessing `payload[i]`. | Allocate `malloc(sizeof(struct Packet) + n * sizeof(uint8_t))`. |
| **Relying on bit-field memory order** | Bit-field ordering (LSB vs MSB) and boundary crossing are implementation-defined and endian-dependent. | Use explicit bitwise shift and mask operations on standard integer types for wire formats. |
| **Passing large structs by value in recursive calls** | Repeatedly copying megabytes of struct data onto the stack quickly exhausts thread stack memory (`SIGSEGV`). | Pass pointers to structs (`const LargeStruct *s`). |

---

## 8. Performance & Security Notes

### Structure Padding and Cache Footprint
In high-performance systems programming, improper struct layout degrades cache efficiency:
```c
/* Suboptimal layout: 24 bytes on 64-bit systems due to padding */
struct Suboptimal {
    char a;       /* 1 byte + 7 bytes pad */
    double b;     /* 8 bytes */
    char c;       /* 1 byte + 7 bytes pad */
};

/* Optimized layout: 16 bytes (33% memory reduction) */
struct Optimized {
    double b;     /* 8 bytes */
    char a;       /* 1 byte */
    char c;       /* 1 byte + 6 bytes trailing pad */
};
```
Packing members tightly allows more instances to fit within a single 64-byte L1 CPU cache line, dramatically reducing cache misses.

### Security: Padding Information Leaks
When copying a kernel or network structure to user space or across a socket, uninitialized padding bytes can leak residual stack or kernel memory containing sensitive keys or passwords. Always clear structs with `memset(&s, 0, sizeof s)` before serializing.

---

## 9. Comparison: When to Use What

| Feature | `struct` | `union` | `enum` |
| :--- | :--- | :--- | :--- |
| **Primary Goal** | Group related heterogeneous fields | Overlay mutually exclusive fields | Name discrete integer states |
| **Memory Allocation** | Sum of member sizes + padding | Size of the largest member | Size of underlying integer (usually `int`) |
| **Member Storage** | Independent simultaneous storage | Shared overlapping storage | N/A (compile-time numeric constants) |
| **Typical Use Cases** | Domain entities, vectors, packets | Variant types, binary reinterpretation | State machine states, error codes, flags |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the Flexible Array Member in C, and what are its syntax rules?</strong></summary>

Introduced in C99, a flexible array member is an incomplete array type declared as the very last member of a structure: `struct Buffer { size_t len; uint8_t data[]; };`.
Rules:
1. The struct must contain at least one other named member.
2. The flexible array must be the last member.
3. `sizeof(struct Buffer)` ignores the flexible array member completely.
4. Memory is allocated dynamically: `malloc(sizeof(struct Buffer) + count * sizeof(uint8_t))`.
</details>

<details>
<summary><strong>2. Why is type punning through a union valid in C but undefined behavior in C++?</strong></summary>

In ISO C (§6.5.2.3, footnote 95), if a union object contains several members, a read through a member other than the one last stored into inspects the object representation of the value. In C++, the strict aliasing and object lifetime rules dictate that reading a non-active union member is strictly undefined behavior, requiring `std::bit_cast` or `memcpy` instead.
</details>

<details>
<summary><strong>3. What does <code>#pragma pack(1)</code> or <code>__attribute__((packed))</code> do, and what are the trade-offs?</strong></summary>

It instructs the compiler to omit all alignment padding bytes, packing struct members contiguously.
- Advantage: Exactly matches external binary wire protocols or file formats.
- Trade-off: Accessing unaligned members causes severe CPU performance degradation or fatal `SIGBUS` faults on architectures (e.g., ARM, SPARC) that forbid unaligned memory loads.
</details>

<details>
<summary><strong>4. How does the compiler determine the alignment and size of a struct?</strong></summary>

The alignment of a struct is the maximum alignment of any of its individual members. The total size of the struct is rounded up to an integer multiple of that struct's alignment by adding trailing padding bytes. This ensures that in an array of structures, every element starts on a properly aligned boundary.
</details>

<details>
<summary><strong>5. What is the difference between an anonymous struct and a tagged struct?</strong></summary>

A tagged struct has an identifier following `struct`: `struct Node { int val; };`. This allows self-referential pointers (`struct Node *next;`).
An anonymous struct omits the tag: `struct { int x, y; } point;`. It cannot be referenced elsewhere unless given a type alias via `typedef`. C11 also introduced anonymous struct members inside unions to create flat member access.
</details>

<details>
<summary><strong>6. Can an enum have negative values in C?</strong></summary>

Yes. Enumeration constants are signed `int` values by default, so definitions such as `enum Error { ERR_IO = -1, ERR_MEM = -2 };` are valid and standard.
</details>

<details>
<summary><strong>7. How do you implement an opaque struct (encapsulation) in C?</strong></summary>

In the public header file (`foo.h`), forward-declare the struct without defining its body: `typedef struct Foo Foo;`. Provide creation and manipulation functions operating on `Foo *`. Define the actual struct definition exclusively inside the private implementation file (`foo.c`). This prevents consumers from accessing internal members or depending on struct layout.
</details>

<details>
<summary><strong>8. What are the rules regarding self-referential structures?</strong></summary>

A structure cannot contain an instance of itself by value because its size would be infinite and cannot be calculated at compile time. However, it can contain a pointer to itself (`struct Node *next;`) because all data pointers have a known, fixed machine size regardless of what they point to.
</details>

---

## 11. Practice Exercises

### Exercise 1: Struct Padding Calculator (Easy)
Define a struct containing a `char`, an `int`, a `short`, and a `double`. Reorder the members to minimize size, and print the `sizeof` before and after.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>

struct Unpacked {
    char a;    /* 1 byte + 3 pad */
    int b;     /* 4 bytes */
    short c;   /* 2 bytes + 6 pad */
    double d;  /* 8 bytes */
};             /* Total: 24 bytes */

struct Packed {
    double d;  /* 8 bytes */
    int b;     /* 4 bytes */
    short c;   /* 2 bytes */
    char a;    /* 1 byte + 1 pad */
};             /* Total: 16 bytes */

int main(void) {
    printf("Unpacked size: %zu\n", sizeof(struct Unpacked));
    printf("Packed size:   %zu\n", sizeof(struct Packed));
    return 0;
}
```
</details>

### Exercise 2: Discriminated Variant JSON Value (Medium)
Implement a tagged union representing a JSON value capable of holding either a `NULL` value, a `double` number, a `bool`, or a heap-allocated dynamic string. Write an initialization and print function.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef enum { JSON_NULL, JSON_NUMBER, JSON_BOOL, JSON_STRING } JsonType;

typedef struct {
    JsonType type;
    union {
        double num;
        bool boolean;
        char *str;
    } as;
} JsonValue;

void json_print(const JsonValue *val) {
    switch (val->type) {
        case JSON_NULL:   printf("null\n"); break;
        case JSON_NUMBER: printf("%f\n", val->as.num); break;
        case JSON_BOOL:   printf("%s\n", val->as.boolean ? "true" : "false"); break;
        case JSON_STRING: printf("\"%s\"\n", val->as.str); break;
    }
}
```
</details>

### Exercise 3: Dynamic Packet with Flexible Array Member (Hard)
Implement an allocator and serializer for a network packet using a Flexible Array Member. The packet contains a 32-bit ID, a 16-bit payload length, and a flexible array `uint8_t payload[]`.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t id;
    uint16_t length;
    uint8_t  payload[];
} Packet;

Packet* packet_create(uint32_t id, const uint8_t *data, uint16_t len) {
    Packet *p = malloc(sizeof(Packet) + len);
    if (!p) return NULL;
    p->id = id;
    p->length = len;
    if (data && len > 0) {
        memcpy(p->payload, data, len);
    }
    return p;
}

void packet_destroy(Packet *p) {
    free(p);
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Struct definition and designated initialization */
struct Point { int x, y; };
struct Point p = { .x = 10, .y = 20 };

/* Tagged union (discriminated variant) */
struct Variant {
    enum { VAR_INT, VAR_STR } tag;
    union {
        int i;
        char s[16];
    } val;
};

/* Flexible array member allocation */
struct Msg { size_t n; char buf[]; };
struct Msg *m = malloc(sizeof *m + data_len);

/* Member offset inspection */
size_t off = offsetof(struct MyStruct, member_name);
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §6.7.2.1 "Structure and union specifiers".
- ISO/IEC 9899:2024 (C23 Standard), §6.7.2.2 "Enumeration specifiers" (Fixed underlying types).
- *The C Programming Language* (2nd ed.) by Kernighan & Ritchie (Chapter 6: "Structures").
- ESR's Guide: *The Lost Art of Structure Packing* (Eric S. Raymond).

---

[← Previous: Type Qualifiers](./05-type-qualifiers.md) | [Section Index](./README.md) | [Next: Scope & Storage Classes →](./07-scope-storage-classes.md)
