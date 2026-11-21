# Bit-Fields: Memory Packing, Hardware Registers, and Portability

> Low-level bit-level data modeling in C: bit-field syntax, memory layout, integer promotions, compiler alignment rules, and explicit bitmask alternatives.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Concepts |
| **Prerequisites** | [Operators](../01-fundamentals/05-operators.md), [Structs, Unions, and Enums](./06-structs-unions-enums.md), [Integer Promotions](../01-fundamentals/13-integer-promotions-and-usual-arithmetic-conversions.md) |
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

In standard C, the smallest addressable unit of memory is a `char` (typically 8 bits). However, communication protocols, hardware control registers, and compressed file formats often require flags and integers of non-standard bit widths:
- An IP packet header contains a 4-bit version field and a 4-bit header length.
- A microcontroller register contains 1-bit enable flags, 3-bit mode selectors, and 2-bit interrupt status bits.

To avoid writing repetitive manual bitwise shift and mask expressions (`(val >> 4) & 0x0F`), C provides **Bit-Fields**: structure members declared with explicit bit widths. While bit-fields offer clean syntax, their underlying memory layout, endianness ordering, and word boundary packing are notoriously **implementation-defined**. Understanding these constraints is essential for systems and embedded firmware developers.

---

## 2. Core Concepts

### Bit-Field Declaration Syntax
A bit-field is declared inside a `struct` or `union` by following the member identifier with a colon and the bit width:
```c
struct StatusRegister {
    unsigned int enable     : 1; /* 1 bit (0 or 1) */
    unsigned int mode       : 3; /* 3 bits (0 to 7) */
    unsigned int reserved   : 4; /* 4 bits padding */
};
```

### Standard Type Rules
In ISO C (§6.7.2.1), bit-fields may be declared with:
- `unsigned int`
- `signed int`
- `_Bool` (or `bool` in C23)
- An implementation-defined set of other integer types (most compilers support `uint8_t`, `uint16_t`, `uint64_t`).

### Zero-Width Bit-Fields
A special unnamed bit-field with width `0` instructs the compiler to immediately pad and force alignment to the start of the next storage unit (e.g., the next 32-bit machine word):
```c
struct Packed {
    unsigned int a : 4;
    unsigned int   : 0; /* Force alignment to next unsigned int boundary */
    unsigned int b : 4;
};
```

### The Address-Of Restriction
Because individual bits within a byte do not have discrete memory addresses in computer hardware, **it is strictly illegal to take the address of a bit-field member**:
```c
struct StatusRegister s;
int *p = &s.enable; /* COMPILE ERROR: cannot take address of bit-field */
```

---

## 3. How It Works Internally

### Packing and Endianness Non-Portability
How the compiler places bit-fields into storage units is completely implementation-defined:
- On **Little-Endian** systems (x86, ARM little-endian), compilers typically pack bit-fields from the Least Significant Bit (LSB) toward the Most Significant Bit (MSB).
- On **Big-Endian** systems (PowerPC, SPARC, network byte order), compilers typically pack from the MSB toward the LSB.

```
Layout of: struct { unsigned int a:4; unsigned int b:4; unsigned int c:8; };

Little-Endian (x86): Packed LSB -> MSB
 Byte 0:
 7        4 3        0
┌──────────┬──────────┐
│   b (4)  │   a (4)  │
└──────────┴──────────┘
 ◄── MSB       LSB ──►

Big-Endian (Network Order): Packed MSB -> LSB
 Byte 0:
 7        4 3        0
┌──────────┬──────────┐
│   a (4)  │   b (4)  │
└──────────┴──────────┘
```

Because of this architectural variance, **using raw bit-fields to map binary network packets or disk structures is dangerous and non-portable**.

---

## 4. Syntax & Usage

```c
#include <stdio.h>
#include <stdint.h>

typedef struct HardwareCtrl {
    uint32_t power_on    : 1;
    uint32_t reset_ready : 1;
    uint32_t clock_div   : 6;
    uint32_t tx_fifo_lvl : 8;
    uint32_t reserved    : 16;
} HardwareCtrl;
```

> [!NOTE]
> ### Signed 1-Bit Bit-Fields: The `-1` Surprise
> Declaring a signed 1-bit bitfield:
> `struct { int flag : 1; } s; s.flag = 1;`
> In two's complement, a 1-bit signed integer has only two representable values: `0` and `-1` (the sign bit represents $-2^0$). Setting `s.flag = 1` overflows and results in `s.flag == -1`! Always declare single-bit flags as `unsigned int` or `bool`.

---

## 5. Practical Examples

### Complete Demonstration Program
The following program demonstrates bit-field allocation, member inspection, and masking to prevent overflow.

```c
#include <stdio.h>
#include <stdint.h>

/* Bit-field structure modeling hardware network packet header */
typedef struct PacketHeader {
    unsigned int version     : 4;  /* 4 bits (0-15) */
    unsigned int header_len  : 4;  /* 4 bits (0-15) */
    unsigned int dscp        : 6;  /* 6 bits (0-63) */
    unsigned int ecn         : 2;  /* 2 bits (0-3)  */
    unsigned int total_len   : 16; /* 16 bits */
} PacketHeader;

int main(void) {
    printf("=== C Bit-Fields Demo ===\n");

    PacketHeader hdr = {0};
    hdr.version = 4;
    hdr.header_len = 5;
    hdr.dscp = 46; /* Expedited Forwarding */
    hdr.ecn = 1;
    hdr.total_len = 1500;

    printf("PacketHeader sizeof: %zu bytes\n", sizeof(PacketHeader));
    printf("Header fields: Version=%u, IHL=%u, DSCP=%u, ECN=%u, Length=%u\n",
           hdr.version, hdr.header_len, hdr.dscp, hdr.ecn, hdr.total_len);

    /* Demonstrate dynamic runtime truncation masking in bitfields */
    unsigned int raw_input = 17; /* 17 = 0b10001; mask to 4 bits */
    hdr.version = raw_input & 0x0F;
    printf("4-bit version masked from %u: %u\n", raw_input, hdr.version);

    return 0;
}
```

```text
=== C Bit-Fields Demo ===
PacketHeader sizeof: 4 bytes
Header fields: Version=4, IHL=5, DSCP=46, ECN=1, Length=1500
4-bit version masked from 17: 1
```

### Real-World Example 2: Microcontroller Peripheral Control Register Union
In embedded firmware, peripheral registers are frequently modeled by pairing a 32-bit raw integer word with a bit-field struct inside a union, enabling both single-bit flag toggling and bulk 32-bit DMA bus writes:

```c
#include <stdint.h>

typedef union UartControlReg {
    uint32_t raw_word;
    struct {
        uint32_t rx_enable   : 1; /* Bit 0: Receiver Enable */
        uint32_t tx_enable   : 1; /* Bit 1: Transmitter Enable */
        uint32_t parity_mode : 2; /* Bits 2-3: 00=None, 01=Odd, 10=Even */
        uint32_t stop_bits   : 1; /* Bit 4: 0=1 stop bit, 1=2 stop bits */
        uint32_t baud_rate   : 11;/* Bits 5-15: Baud divider */
        uint32_t reserved    : 16;/* Bits 16-31 */
    } fields;
} UartControlReg;

void init_uart(volatile uint32_t *hardware_address) {
    UartControlReg reg = {0};
    reg.fields.rx_enable = 1;
    reg.fields.tx_enable = 1;
    reg.fields.baud_rate = 1152; /* Set 115200 baud divisor */
    
    /* Write entire 32-bit register atomically to hardware bus */
    *hardware_address = reg.raw_word;
}
```

### Real-World Example 3: Portable Network Header Serialization via Shift & Mask
To eliminate the endianness and ABI variance of bit-fields when sending data over network sockets, production protocol stacks use explicit bitwise shift and mask expressions:

```c
#include <stdint.h>

/* Serialize IP header byte: version (4 bits) + IHL (4 bits) */
uint8_t pack_ip_version_ihl(uint8_t version, uint8_t ihl) {
    return ((version & 0x0F) << 4) | (ihl & 0x0F);
}

/* Deserialize IP header byte */
void unpack_ip_version_ihl(uint8_t byte, uint8_t *version, uint8_t *ihl) {
    if (version) *version = (byte >> 4) & 0x0F;
    if (ihl)     *ihl     = byte & 0x0F;
}
```


---

## 6. Best Practices

1. **Always Use `unsigned int` or `bool` for 1-Bit Flags**: Never declare `int flag : 1;`. In two's complement, a 1-bit signed integer can only represent `0` or `-1`.
2. **Never Rely on Bit-Fields for Wire Protocols or File Formats**: Bit-field member ordering across endian boundaries is undefined. Use explicit integer types (`uint16_t`, `uint32_t`) and portable bitwise shifts (`>>`, `<<`, `&`, `|`) instead.
3. **Beware of Word Boundaries**: Compilers do not split a bit-field across standard allocation boundaries (e.g., crossing a 32-bit boundary) unless packed with compiler attributes.
4. **Never Take the Address of a Bit-Field**: Attempting `&s.field` causes a compile error. If a pointer is required, copy the value to an ordinary local variable first.
5. **Mask Dynamic Values Before Assignment**: To prevent undefined behavior or silent integer truncation, mask inputs with `& ((1U << width) - 1)` before assigning.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| **Declaring `signed int flag : 1`** | A signed 1-bit field holds only `0` and `-1`. Assigning `1` overflows to `-1`. | Declare as `unsigned int flag : 1` or `bool flag : 1`. |
| **Attempting to take address `&s.field`** | Hardware does not provide byte addresses for bit slices; illegal C syntax. | Copy to a temporary variable and take the address of that variable. |
| **Using bit-fields across network sockets** | Big-endian and little-endian CPUs pack bits in inverted order, corrupting protocol headers. | Use standard integers with bitwise masks (`<<`, `>>`, `&`). |
| **Assuming bit-fields prevent concurrent data races** | Modifying adjacent bit-fields in separate threads causes data races because the CPU updates the whole byte/word. | Protect the entire containing struct with a mutex or use atomic integers. |
| **Implicit truncation without warning** | Assigning `32` to a 5-bit field silently truncates high bits in runtime variables. | Validate inputs before assigning or mask explicitly. |
| **Passing bit-field to function expecting pointer** | `scanf("%u", &s.val)` fails to compile. | Read into a temporary local variable, then assign to the bit-field. |

---

## 8. Performance & Security Notes

### Assembly Overhead: Bit Extraction
Accessing an ordinary integer `s.val` emits a single direct load instruction. Accessing a bit-field requires the compiler to emit instructions to load the enclosing word, shift right, and mask off the unwanted bits:
```nasm
; Accessing s.dscp (6 bits at offset 8):
mov   eax, DWORD PTR [rdi]
shr   eax, 8
and   eax, 63
```
While bit-fields save memory, reading and writing them introduces slight instruction overhead.

### Security: Non-Atomic Word Tearing
If Thread 1 updates `s.flag_a` while Thread 2 simultaneously updates `s.flag_b`, both threads read, modify, and write back the **same 32-bit machine word**. Without synchronization, Thread 2's write will silently overwrite Thread 1's modification, causing hard-to-diagnose data race bugs.

---

## 9. Comparison: When to Use What

| Technique | Syntax Readability | Portability Across CPUs | Wire Protocol Safe? | Concurrency Safe? |
| :--- | :--- | :--- | :--- | :--- |
| **Bit-Fields (`field:3`)** | High (`s.mode = 2`) | Poor (Endian dependent) | No | No (Shared word) |
| **Bitwise Masks (`&`, `|`)** | Moderate (`s |= MASK`) | 100% Portable | Yes | Yes (Can use atomics) |
| **Byte Arrays (`uint8_t[]`)** | Manual | 100% Portable | Yes | Safe per byte |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is <code>&amp;s.my_bitfield</code> a compile-time constraint violation in ISO C?</strong></summary>

Standard computer architectures utilize byte-addressable memory. The smallest addressable unit that a pointer can reference is a byte (`char`). Because a bit-field occupies a sub-byte slice, hardware has no physical pointer representation for individual bit offsets within a byte.
</details>

<details>
<summary><strong>2. What is the effect of an unnamed bit-field with width zero (<code>unsigned int : 0;</code>)?</strong></summary>

It instructs the compiler to stop packing members into the current storage unit and force alignment to the boundary of the next adjacent storage unit (e.g., the next 32-bit word).
</details>

<details>
<summary><strong>3. What values can a <code>signed int b : 2;</code> bit-field hold?</strong></summary>

In standard two's complement representation, a 2-bit signed integer has a range from $-2^{2-1}$ to $2^{2-1} - 1$, which is $[-2, 1]$ (values: $-2, -1, 0, 1$).
</details>

<details>
<summary><strong>4. Why should bit-fields not be used to define packet headers in cross-platform network software?</strong></summary>

ISO C leaves the ordering of bit-fields within a storage unit (MSB-to-LSB vs. LSB-to-MSB) completely implementation-defined. A packet header defined with bit-fields compiled on an x86 PC (little-endian) will have its bit order reversed compared to the same struct compiled on a network router (big-endian), corrupting protocol communication.
</details>

<details>
<summary><strong>5. What happens if you assign a value that exceeds a bit-field's width?</strong></summary>

If the bit-field is `unsigned`, the value is converted modulo $2^{\text{width}}$, effectively keeping only the lowest bits. If the bit-field is `signed`, assigning an out-of-range value is implementation-defined or raises an implementation-defined signal.
</details>

<details>
<summary><strong>6. Can bit-fields be declared inside unions?</strong></summary>

Yes. A common embedded pattern defines a struct of bit-fields inside a union with a raw integer (`uint32_t raw;`), allowing developers to access individual fields by name or read/write the entire 32-bit register in a single operation.
</details>

<details>
<summary><strong>7. How does structure padding affect a struct containing only bit-fields?</strong></summary>

The compiler allocates one or more storage units (e.g., 32-bit words) to hold the bit-fields. If the total number of bits is 17, the compiler must allocate a 32-bit word, resulting in `sizeof` equal to 4 bytes (with 15 unused padding bits).
</details>

<details>
<summary><strong>8. Can an array of bit-fields be declared in C?</strong></summary>

No. Arrays in C are collections of objects whose addresses can be indexed. Because bit-fields cannot have memory addresses, declaring an array of bit-fields (e.g., `unsigned int flags[10] : 1;`) is strictly illegal.
</details>

---

## 11. Practice Exercises

### Exercise 1: RGB565 Color Packer (Easy)
Define a bit-field structure `RGB565` that packs 5 bits of Red, 6 bits of Green, and 5 bits of Blue into a 16-bit word.

<details>
<summary>View Solution</summary>

```c
#include <stdint.h>

typedef struct {
    uint16_t red   : 5;
    uint16_t green : 6;
    uint16_t blue  : 5;
} RGB565;
```
</details>

### Exercise 2: Hardware Register Union (Medium)
Create a union that pairs a 32-bit raw integer `uint32_t word` with a bit-field struct representing a Timer Control Register (enable: 1 bit, mode: 3 bits, prescaler: 8 bits, count: 20 bits).

<details>
<summary>View Solution</summary>

```c
#include <stdint.h>

typedef union {
    uint32_t raw;
    struct {
        uint32_t enable    : 1;
        uint32_t mode      : 3;
        uint32_t prescaler : 8;
        uint32_t count     : 20;
    } bits;
} TimerRegister;
```
</details>

### Exercise 3: Portable Bitmask Extraction Macros (Hard)
Instead of non-portable bit-fields, write a set of robust preprocessor macros `GET_BITS(val, shift, mask)` and `SET_BITS(target, val, shift, mask)` to manipulate arbitrary bit slices portably across any CPU architecture.

<details>
<summary>View Solution</summary>

```c
#include <stdio.h>
#include <stdint.h>

#define GET_BITS(val, shift, mask) \
    (((val) >> (shift)) & (mask))

#define SET_BITS(target, val, shift, mask) \
    ((target) = ((target) & ~((mask) << (shift))) | (((val) & (mask)) << (shift)))

int main(void) {
    uint32_t reg = 0;
    /* Set 4-bit field at bit offset 8 with mask 0x0F to value 12 */
    SET_BITS(reg, 12, 8, 0x0F);
    printf("Register: 0x%08X\n", reg);
    printf("Extracted value: %u\n", GET_BITS(reg, 8, 0x0F));
    return 0;
}
```
</details>

---

## 12. Summary & Cheat Sheet

```c
/* Bit-field declaration */
struct Flags {
    unsigned int active : 1; /* 1 bit (0 or 1) */
    unsigned int mode   : 3; /* 3 bits (0 to 7) */
    unsigned int        : 0; /* Align to next word boundary */
    unsigned int code   : 4;
};

/* Prohibitions */
&s.active;           /* ILLEGAL: Cannot take address */
unsigned int f[4]:1; /* ILLEGAL: No arrays of bit-fields */

/* Portable Alternative: Bitwise Operators */
#define FLAG_A (1U << 0)
#define FLAG_B (1U << 1)
```

---

## 13. Further Reading
- ISO/IEC 9899:2018 (C17 Standard), §6.7.2.1 "Structure and union specifiers" (Bit-fields).
- ISO/IEC 9899:2024 (C23 Standard), §6.7.2.1 "Structure and union specifiers".
- *The Lost Art of Structure Packing* by Eric S. Raymond.
- Embedded.com: *Bit-fields vs. Bitmasks in Embedded Systems*.

---

[← Previous: Safe String Handling & string.h](./15-string-h-functions-and-safe-alternatives.md) | [Section Index](./README.md) | [Next: Enums & Flags →](./17-enums-and-flags.md)
