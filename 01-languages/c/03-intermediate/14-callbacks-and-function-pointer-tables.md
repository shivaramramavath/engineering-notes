# 14 Callbacks & Dispatch Tables

> Master inversion of control, event-driven architectures, state machines, jump tables, and secure function pointer dispatch in C.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [02-core-concepts/03-pointers.md](../02-core-concepts/03-pointers.md), [03-intermediate/01-function-pointers.md](01-function-pointers.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | C11 / C17 / C23 |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally](#3-how-it-works-internally)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why

In procedural programming, control flow is typically direct and static: function A explicitly calls function B, which calls function C. However, modern software systems require decoupled, dynamic control flow:
- **Inversion of Control (IoC)**: Frameworks (GUI libraries, network event loops) invoke user-defined code when events occur (e.g., button clicked, packet received).
- **Table-Driven Design**: Replacing massive, unwieldy `switch` or `if-else` blocks with tabular mappings between commands and handler functions.
- **State Machines**: Finite State Automata (FSA) where transitions between states execute specific entry, exit, or transition actions.
- **Extensible Plugin Systems**: Allowing third-party modules to register callback hooks without modifying the host core binary.

By mastering callbacks with context handles (`void *user_data`) and dispatch tables stored in read-only memory, C developers can build modular, high-throughput systems that scale cleanly.

---

## 2. Core Concepts

### 2.1 The Callback Context Pattern (`void *ctx`)
A callback function pointer allows a library to execute caller code. However, a bare function pointer cannot carry state:
```c
/* POOR DESIGN: No context pointer */
typedef void (*ButtonCallback)(void);
```
If a button click needs to know which window or database connection to update, a bare callback forces the developer to rely on dangerous global variables.

The professional standard is the **Context Carrier Callback Pattern**:
```c
/* PROFESSIONAL DESIGN: Explicit context carrier */
typedef void (*ButtonCallback)(void *user_data);

void button_set_callback(Button *btn, ButtonCallback cb, void *user_data);
```
The caller passes a pointer to their own state struct as `user_data`. When the button fires, it passes `user_data` directly into the callback, achieving complete encapsulation without global variables.

### 2.2 Function Pointer Dispatch Tables (Jump Tables)
A dispatch table is an array of structures associating keys (command names, opcode integers, event types) with function pointers:
```c
typedef struct {
    const char *command_name;
    CommandHandler handler;
    const char *help_description;
} CommandEntry;

static const CommandEntry DISPATCH_TABLE[] = {
    {"start", handle_start, "Start service"},
    {"stop",  handle_stop,  "Stop service"},
    {"status", handle_status, "Query service status"}
};
```

### 2.3 State Machines with Action Tables
A state machine transition table maps `(Current State, Event) -> (Next State, Action Callback)`. This converts complex stateful logic into a deterministic, auditable two-dimensional matrix.

---

## 3. How It Works Internally

### Indirect Calls in Assembly & Branch Prediction
A direct function call emits a static assembly instruction:
```assembly
call print_message     ; Direct call: Target address hardcoded in instruction
```
An indirect call through a function pointer dereferences a memory location or register:
```assembly
mov  rax, QWORD PTR [rdi + 8]   ; Load function address from table
call rax                       ; Indirect call
```

```
CPU Indirect Branch Execution Pipeline:
┌───────────────────────────────────────┐
│ Indirect Call: call rax               │
├───────────────────────────────────────┤
│ Branch Target Buffer (BTB) Lookup:    │
│ Predicts target address based on      │
│ execution history                     │
├───────────────────────────────────────┤
│ Target Matches History:               │
│ -> 0-cycle speculative execution      │
├───────────────────────────────────────┤
│ Target Mispredicted / Dynamic Change: │
│ -> Pipeline Flush (15-20 cycle penalty│
└───────────────────────────────────────┘
```

Modern CPU microarchitectures utilize sophisticated **Branch Target Buffers (BTB)** to predict indirect jump targets. If a dispatch table consistently invokes the same handler, the indirect call incurs almost zero latency penalty.

---

## 4. Syntax & Usage

### 4.1 Typedefs for Readability
Function pointer syntax is notoriously difficult to parse without typedefs. Always declare a clean typedef:
```c
/* Typedef for an event callback */
typedef int (*EventCallback)(int event_id, void *payload, void *user_data);
```

### 4.2 Array-Indexed Direct Jump Tables
When event IDs or opcodes are contiguous integers (`0, 1, 2, ...`), array indexing provides $O(1)$ constant-time dispatch without string comparisons:
```c
typedef void (*OpcodeHandler)(CpuState *cpu);

static const OpcodeHandler OPCODE_TABLE[256] = {
    [0x00] = handle_nop,
    [0x01] = handle_load,
    [0x02] = handle_store,
    [0x03] = handle_add
};

void execute_instruction(CpuState *cpu, uint8_t opcode) {
    OpcodeHandler handler = OPCODE_TABLE[opcode];
    if (handler) {
        handler(cpu);
    } else {
        handle_illegal_instruction(cpu, opcode);
    }
}
```

---

## 5. Practical Examples

### Comprehensive Example: Command Dispatch Table with Context
The following program demonstrates an extensible table-driven command processor passing an application context struct into callbacks.

```c
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Context struct passed to callbacks */
typedef struct {
    int accumulator;
    bool debug_mode;
} AppContext;

/* Command handler function pointer signature */
typedef void (*CommandHandler)(AppContext *ctx, const char *arg);

typedef struct {
    const char *command_name;
    CommandHandler handler;
    const char *help_text;
} CommandEntry;

static void handle_add(AppContext *ctx, const char *arg) {
    int val = 0;
    if (sscanf(arg, "%d", &val) == 1) {
        ctx->accumulator += val;
        if (ctx->debug_mode) {
            printf("[DEBUG] Added %d, accumulator is now %d\n", val, ctx->accumulator);
        }
    }
}

static void handle_reset(AppContext *ctx, const char *arg) {
    (void)arg;
    ctx->accumulator = 0;
    if (ctx->debug_mode) {
        printf("[DEBUG] Reset accumulator to 0\n");
    }
}

static void handle_status(AppContext *ctx, const char *arg) {
    (void)arg;
    printf("[STATUS] Current accumulator value = %d\n", ctx->accumulator);
}

/* Dispatch table */
static const CommandEntry COMMAND_TABLE[] = {
    {"add",    handle_add,    "Add integer to accumulator"},
    {"reset",  handle_reset,  "Reset accumulator to zero"},
    {"status", handle_status, "Display current accumulator status"}
};

static const size_t COMMAND_COUNT = sizeof(COMMAND_TABLE) / sizeof(COMMAND_TABLE[0]);

static bool dispatch_command(AppContext *ctx, const char *cmd, const char *arg) {
    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(COMMAND_TABLE[i].command_name, cmd) == 0) {
            COMMAND_TABLE[i].handler(ctx, arg);
            return true;
        }
    }
    return false;
}

int main(void) {
    printf("=== Callbacks and Dispatch Tables ===\n");
    AppContext ctx = {0, true};

    printf("Executing command dispatch sequence:\n");
    dispatch_command(&ctx, "add", "15");
    dispatch_command(&ctx, "add", "25");
    dispatch_command(&ctx, "status", "");
    dispatch_command(&ctx, "reset", "");
    dispatch_command(&ctx, "status", "");

    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 14-callbacks-and-function-pointer-tables.c -o 14-callbacks-and-function-pointer-tables.exe
./14-callbacks-and-function-pointer-tables.exe
```

```text
=== Callbacks and Dispatch Tables ===
Executing command dispatch sequence:
[DEBUG] Added 15, accumulator is now 15
[DEBUG] Added 25, accumulator is now 40
[STATUS] Current accumulator value = 40
[DEBUG] Reset accumulator to 0
[STATUS] Current accumulator value = 0
```

---

## 6. Best Practices

1. **Place Dispatch Tables in Read-Only Memory (`static const`)**: Marking tables `static const` places them in the `.rodata` section of the binary, preventing memory corruption or malicious function pointer overwrites.
2. **Always Pass Context Pointers (`void *user_data`)**: Never design a callback API that omits user context. It is trivial for callers to ignore `user_data` if unused, but impossible to pass context without it.
3. **Verify Nullness Before Invoking Callbacks**: If callback registration is optional, always verify `if (handler != NULL)` before calling. Dereferencing a NULL function pointer triggers an immediate crash.
4. **Use Designated Initializers for Jump Tables**: For opcode or enum tables, use C99 designated initializers (`[STATE_IDLE] = handle_idle`) to make table ordering robust against enum renumbering.
5. **Enforce Boundary Checks on Indexed Tables**: If indexing directly by integer opcode, verify `opcode < TABLE_SIZE` before dereferencing to prevent out-of-bounds execution.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| Storing dispatch tables in mutable RAM | Vulnerable to memory corruption or security exploitation (arbitrary code injection). | Mark tables `static const`. |
| Missing `user_data` context | Forces callers to use global variables, breaking multi-threaded safety and reentrancy. | Always include `void *user_data` in callback signatures. |
| Calling NULL function pointer | If an uninitialized or optional callback is invoked, program crashes with a segmentation fault. | Check `if (cb != NULL) cb(...);`. |
| Indexing jump table without bounds check | Supplying an out-of-range opcode reads garbage pointers from memory and jumps to them. | Verify `if (opcode < TABLE_LEN)` before table lookup. |
| Registering callback with stack address | If the callback context points to a local stack variable that goes out of scope, callback dereferences garbage. | Ensure `user_data` has static, global, or heap lifetime. |
| Mismatched calling convention | On 32-bit Windows, mixing `__cdecl` and `__stdcall` corrupts the stack frame on return. | Ensure typedefs match function definitions exactly. |

---

## 8. Performance & Security Notes

- **Indirect Branch Target Injection (Spectre Variant 2)**: Speculative execution attacks exploit indirect branches through function pointers. Modern operating systems and compilers mitigate this with "retpolines" (return trampolines), which introduce a slight execution penalty for indirect calls.
- **W^X (Write XOR Execute) & Read-Only Tables**: Function pointer tables stored in writable memory (`.data` or `.bss`) are prime targets for buffer overflow exploits: an attacker overwrites a function pointer with the address of shellcode or `system()`. Placing tables in `.rodata` (`const`) guarantees hardware page write protection.
- **Compiler Jump Tables**: Modern compilers automatically convert large, dense `switch` statements into assembly jump tables. Writing explicit function pointer tables is advantageous when handlers reside in external modules or require runtime reconfiguration.

---

## 9. Comparison / When to Use What

| Architecture | Performance | Memory Overhead | Extensibility | Reconfiguration |
| :--- | :--- | :--- | :--- | :--- |
| **`switch` Statement** | Optimal (Compiler generated jump table) | Minimal | Low (Requires modifying source code) | Compile-time only |
| **`static const` Dispatch Table** | High (1 memory deref + indirect call) | Low (Size of pointer array) | High (Clean table structure) | Compile-time |
| **Dynamic Callback Registry** | Moderate (List/Hash traversal) | Moderate (Dynamic allocations) | Maximum (Plugins register at runtime) | Runtime |
| **C++ Virtual Methods (Vtables)** | High | Moderate (Vptr per object) | High | Compile-time |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is the `void *user_data` parameter essential in C callback APIs?</strong></summary>

In C, functions cannot capture lexical scope (closures) like in JavaScript, Python, or C++. A plain function pointer carries only the code address, not data. If a callback needs to operate on a specific object or state, omitting a context pointer forces the application to store that state in global variables. This breaks thread safety, prevents multiple instances of the subsystem from coexisting, and eliminates reentrancy. Providing `void *user_data` allows the caller to pass arbitrary state into the callback cleanly.
</details>

<details>
<summary><strong>2. How does the CPU execute an indirect function call at the machine instruction level?</strong></summary>

A direct call (`call target`) encodes the relative offset to the target function directly into the machine instruction. An indirect call (`call rax` or `call [table + rdx*8]`) requires the CPU to first load the 64-bit address from a register or memory location into the Instruction Pointer (`RIP`). Because the destination is not fixed at compile time, the CPU consults its Branch Target Buffer (BTB) to predict the target address for speculative execution.
</details>

<details>
<summary><strong>3. What security risk is introduced by storing function pointer tables in writable memory?</strong></summary>

If a function pointer table is stored in writable memory (`.data` or `.bss`), any buffer overflow, format string flaw, or use-after-free vulnerability in the application can allow an attacker to overwrite one of the function pointers with an arbitrary address (such as injected shellcode or `system()`). When the application subsequently dispatches a command, it jumps directly to the attacker's payload. Storing dispatch tables as `static const` places them in write-protected memory pages (`.rodata`).
</details>

<details>
<summary><strong>4. When should you prefer a `switch` statement over an explicit function pointer dispatch table?</strong></summary>

A `switch` statement should be preferred when:
1. The set of cases is small, fixed, and completely known at compile-time.
2. The operations performed in each case are small (1–3 lines).
3. The compiler can automatically optimize dense cases into jump tables without the syntactic overhead of declaring separate helper functions for every branch.
Dispatch tables should be preferred when handlers are modular, when tables need to be dynamically populated or extended by plugins, or when commands are strings.
</details>

<details>
<summary><strong>5. How do you implement a state machine using a two-dimensional dispatch table?</strong></summary>

Define enums for States (`State`) and Events (`Event`). Create a 2D array of state transition entries:
`TransitionTable[NUM_STATES][NUM_EVENTS]`.
Each entry contains the `next_state` and an action function pointer `void (*action)(Context *ctx)`. When an event occurs:
```c
Transition t = TransitionTable[current_state][event];
if (t.action) t.action(ctx);
current_state = t.next_state;
```
This guarantees $O(1)$ constant-time transitions without nested conditionals.
</details>

<details>
<summary><strong>6. What is a retpoline and why was it introduced for indirect calls?</strong></summary>

A retpoline (return trampoline) is a compiler mitigation technique developed by Google to prevent Spectre Variant 2 attacks. Spectre v2 exploits the Branch Target Buffer (BTB) to poison indirect branch predictions and leak secret memory through speculative cache side channels. A retpoline replaces vulnerable indirect jumps (`call rax`) with an assembly sequence utilizing `call` and `ret` instructions that traps speculative execution in an infinite pause loop until the true address is resolved.
</details>

<details>
<summary><strong>7. How do designated initializers enhance the safety of array-indexed dispatch tables?</strong></summary>

Using C99 designated initializers (e.g. `[CMD_READ] = handle_read`) explicitly links each handler to its specific enum constant. In non-designated array declarations, if an engineer inserts an enum value into the middle of an enum definition, all subsequent array elements shift by one position silently, mapping opcodes to completely wrong handler functions. Designated initializers guarantee that array slots match their intended enum indices regardless of ordering.
</details>

<details>
<summary><strong>8. Why should you avoid allocating callback context on the stack?</strong></summary>

If an asynchronous operation (such as a timer or background network request) is registered with a pointer to a local stack variable (`&local_data`), and the calling function returns before the callback fires, the stack frame is destroyed. When the callback eventually executes, it dereferences invalid stack memory that may have been reused by other functions, causing memory corruption or crash. Asynchronous callback context must reside on the heap or in persistent static memory.
</details>

---

## 11. Practice Exercises

### Easy: Math Operation Dispatcher
Implement a calculator that maps operators `'+'`, `'-'`, `'*'`, `'/'` to function pointers using an array of structures.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>

typedef double (*MathOp)(double a, double b);

static double op_add(double a, double b) { return a + b; }
static double op_sub(double a, double b) { return a - b; }
static double op_mul(double a, double b) { return a * b; }
static double op_div(double a, double b) { return b != 0 ? a / b : 0; }

typedef struct {
    char symbol;
    MathOp func;
} OpEntry;

static const OpEntry OPERATIONS[] = {
    {'+', op_add},
    {'-', op_sub},
    {'*', op_mul},
    {'/', op_div}
};

double calculate(char op, double a, double b) {
    for (size_t i = 0; i < sizeof(OPERATIONS)/sizeof(OPERATIONS[0]); i++) {
        if (OPERATIONS[i].symbol == op) {
            return OPERATIONS[i].func(a, b);
        }
    }
    return 0.0;
}
```
</details>

### Medium: Generic Observable / Event Publisher
Implement an event listener registry allowing multiple callbacks to subscribe to an event and be notified sequentially with a context pointer.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdbool.h>

#define MAX_LISTENERS 8

typedef void (*EventListener)(int event_data, void *user_data);

typedef struct {
    EventListener listener;
    void *user_data;
} Subscriber;

typedef struct {
    Subscriber subscribers[MAX_LISTENERS];
    size_t count;
} EventBus;

void bus_init(EventBus *bus) {
    bus->count = 0;
}

bool bus_subscribe(EventBus *bus, EventListener listener, void *user_data) {
    if (bus->count >= MAX_LISTENERS || !listener) return false;
    bus->subscribers[bus->count++] = (Subscriber){listener, user_data};
    return true;
}

void bus_publish(const EventBus *bus, int event_data) {
    for (size_t i = 0; i < bus->count; i++) {
        bus->subscribers[i].listener(event_data, bus->subscribers[i].user_data);
    }
}
```
</details>

### Hard: Finite State Machine Turnstile
Implement a turnstile state machine with states `LOCKED` and `UNLOCKED`, and events `COIN` and `PUSH`, utilizing a 2D transition table.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>

typedef enum { STATE_LOCKED, STATE_UNLOCKED, NUM_STATES } State;
typedef enum { EVENT_COIN, EVENT_PUSH, NUM_EVENTS } Event;

typedef void (*Action)(void);

static void action_unlock(void) { printf("Action: Unlocking turnstile.\n"); }
static void action_lock(void)   { printf("Action: Locking turnstile.\n"); }
static void action_thank(void)  { printf("Action: Thanks for extra coin! Already unlocked.\n"); }
static void action_alarm(void)  { printf("Action: ALARM! Cannot pass locked turnstile.\n"); }

typedef struct {
    State next_state;
    Action action;
} Transition;

static const Transition FSM[NUM_STATES][NUM_EVENTS] = {
    [STATE_LOCKED] = {
        [EVENT_COIN] = {STATE_UNLOCKED, action_unlock},
        [EVENT_PUSH] = {STATE_LOCKED,   action_alarm}
    },
    [STATE_UNLOCKED] = {
        [EVENT_COIN] = {STATE_UNLOCKED, action_thank},
        [EVENT_PUSH] = {STATE_LOCKED,   action_lock}
    }
};

void turnstile_handle_event(State *current, Event ev) {
    Transition t = FSM[*current][ev];
    if (t.action) t.action();
    *current = t.next_state;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Feature | Idiom / Pattern | Best Practice |
| :--- | :--- | :--- |
| Callback Signature | `typedef void (*CB)(Data *, void *user_data);` | Always include context pointer |
| Dispatch Table | `static const TableEntry TABLE[] = {...};` | Store in `.rodata` for security |
| State Machine | `TransitionTable[state][event]` | $O(1)$ constant-time transitions |
| Jump Table | `[OPCODE_ADD] = handle_add` | Use designated initializers |
| Safety Check | `if (handler != NULL) handler(...)` | Guard against NULL calls |

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17) §6.3.2.3 (Pointers: function pointers).
- Miro Samek, *Practical UML Statecharts in C/C++: Event-Driven Programming for Embedded Systems* (Newnes).
- SEI CERT C Coding Standard: MEM51-C (Properly allocate and free dynamic resources in callbacks).

---

[← Previous](13-opaque-pointers-and-adts.md) | [Section Index](README.md) | [Next →](15-setjmp-longjmp.md)
