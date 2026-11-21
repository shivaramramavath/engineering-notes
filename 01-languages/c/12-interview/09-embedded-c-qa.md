# Embedded and Bare-Metal C Technical Interview Guide (`09-embedded-c-qa`)

> Master bare-metal and microcontroller C interview concepts: volatile hardware registers, memory-mapped I/O, interrupt service routines (ISRs), and linker sections.

| Metadata | Details |
| :--- | :--- |
| **Guide** | `09-embedded-c-qa` |
| **Level** | Advanced |
| **Standard Baseline** | C11 / Bare-Metal / ARM Cortex-M / RISC-V |

---

## 1. Volatile and Memory-Mapped I/O

<details>
<summary><strong>Q1: What does the `volatile` qualifier guarantee, and where is it mandatory in embedded systems?</strong></summary>

`volatile` informs the compiler that an object's value can be modified by hardware or external agents outside the compiler's knowledge.
The compiler is prohibited from:
1. Caching the variable in a CPU register.
2. Reordering volatile accesses relative to sequence points.
3. Eliminating reads or writes as dead code.

**Mandatory Scenarios:**
- Memory-Mapped I/O (MMIO) hardware registers.
- Variables modified inside an Interrupt Service Routine (ISR) and checked in the main loop.
- Memory shared across threads without explicit locks or atomics.
</details>

<details>
<summary><strong>Q2: How do you access a hardware control register at physical address `0x40021000`?</strong></summary>

```c
#define GPIO_MODER (*(volatile uint32_t*)0x40021000UL)

/* Set bit 5 */
GPIO_MODER |= (1U << 5);
```
</details>

---

## 2. Interrupts and Linker Layout

<details>
<summary><strong>Q3: What constraints apply to an Interrupt Service Routine (ISR) in C?</strong></summary>

1. **No Arguments or Return Values:** Hardware interrupts cannot pass parameters or receive return values (`void ISR_Handler(void)`).
2. **Never Call Non-Reentrant Functions:** Banned: `malloc()`, `free()`, `printf()`.
3. **Keep Execution Minimal:** Defer heavy processing to background tasks via flags or ring buffers.
4. **Shared State Must Be Volatile:** Any variable updated in an ISR and read elsewhere must be qualified with `volatile`.
</details>

<details>
<summary><strong>Q4: Explain the primary memory sections defined in an embedded linker script (`.text`, `.rodata`, `.data`, `.bss`).</strong></summary>

- **`.text` (Flash / ROM):** Executable machine instructions and interrupt vector tables.
- **`.rodata` (Flash / ROM):** Immutable constants and string literals.
- **`.data` (RAM, initialized from Flash):** Initialized global and static variables. Startup code (`Reset_Handler`) copies initial values from Flash to RAM.
- **`.bss` (RAM, zero-initialized):** Uninitialized global and static variables. Startup code zeroes this memory region before invoking `main()`.
</details>

---

[← Undefined Behavior Puzzles](./08-undefined-behavior-puzzles.md) | [Interview Index](./README.md) | [Next Module: Cheatsheets →](../13-cheatsheets/README.md)
