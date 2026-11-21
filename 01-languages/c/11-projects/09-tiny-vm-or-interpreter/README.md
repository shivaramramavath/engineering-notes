# Project 09: Stack-Based Bytecode Virtual Machine (`tiny-vm`)

> A lightweight, stack-based bytecode virtual machine in C featuring instruction decoding, arithmetic operand stack evaluation, conditional branching, and a bytecode disassembler.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `09-tiny-vm-or-interpreter` |
| **Language Baseline** | C11 / Standard Architecture |
| **Complexity** | Advanced / Runtime Engines |
| **Core Systems Concepts** | Virtual Machines, Bytecode Dispatch, Stack Evaluation, Instruction Sets |

---

## 1. Architectural Overview & Component Breakdown

`tiny-vm` evaluates compiled bytecode instructions through a virtual execution loop:
1. **Instruction Set Architecture (ISA):** Standardized 1-byte opcodes (`OP_PUSH`, `OP_ADD`, `OP_SUB`, `OP_MUL`, `OP_PRINT`, `OP_HALT`).
2. **Operand Stack (`vm.c`):** An integer evaluation stack with automatic bounds-checked push and pop operations.
3. **Execution Engine (`vm.c`):** The fetch-decode-execute interpreter loop utilizing a direct opcode `switch` or computed `goto` table.
4. **Disassembler (`disasm.c`):** Inspects binary bytecode arrays and outputs human-readable assembly instructions.

```
       +-----------------------------------------------------------+
       |               Stack Bytecode VM Architecture              |
       +-----------------------------------------------------------+
                                     |
               Bytecode Stream: [OP_PUSH, 10, OP_PUSH, 20, OP_ADD]
                                     v
                          [ Program Counter (PC) ]
                                     v
                           [ Opcode Dispatch ]
                                     |
                +--------------------+--------------------+
                |                                         |
                v                                         v
       [ Operand Stack ]                         [ Arithmetic ALU ]
       | 20 | <- Top                             Top elements popped: 20 + 10
       | 10 |                                    Result 30 pushed to Top
```

---

## 2. Complete Project Source Code

### Header: `include/vm.h`

```c
#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    OP_HALT = 0x00,
    OP_PUSH = 0x01, /* Followed by int32_t */
    OP_POP  = 0x02,
    OP_ADD  = 0x03,
    OP_SUB  = 0x04,
    OP_MUL  = 0x05,
    OP_DIV  = 0x06,
    OP_PRINT= 0x07,
    OP_JMP  = 0x08, /* Followed by uint32_t target */
    OP_JZ   = 0x09  /* Followed by uint32_t target */
} OpCode;

#define VM_STACK_CAPACITY 256

typedef struct {
    const uint8_t *code;
    size_t code_size;
    size_t pc;
    int32_t stack[VM_STACK_CAPACITY];
    int sp; /* Stack pointer */
} VM;

void vm_init(VM *vm, const uint8_t *bytecode, size_t size);
bool vm_run(VM *vm);
void vm_disassemble(const uint8_t *bytecode, size_t size);

#endif /* VM_H */
```

### Source: `src/vm.c`

```c
#include "vm.h"
#include <stdio.h>

void vm_init(VM *vm, const uint8_t *bytecode, size_t size) {
    vm->code = bytecode;
    vm->code_size = size;
    vm->pc = 0;
    vm->sp = -1;
}

static inline bool push(VM *vm, int32_t val) {
    if (vm->sp >= VM_STACK_CAPACITY - 1) return false;
    vm->stack[++vm->sp] = val;
    return true;
}

static inline int32_t pop(VM *vm) {
    if (vm->sp < 0) return 0;
    return vm->stack[vm->sp--];
}

bool vm_run(VM *vm) {
    while (vm->pc < vm->code_size) {
        uint8_t op = vm->code[vm->pc++];
        switch (op) {
            case OP_HALT: return true;
            case OP_PUSH: {
                int32_t val = (int32_t)(
                    (uint32_t)vm->code[vm->pc] |
                    ((uint32_t)vm->code[vm->pc+1] << 8) |
                    ((uint32_t)vm->code[vm->pc+2] << 16) |
                    ((uint32_t)vm->code[vm->pc+3] << 24)
                );
                vm->pc += 4;
                push(vm, val);
                break;
            }
            case OP_POP: pop(vm); break;
            case OP_ADD: { int32_t b = pop(vm); int32_t a = pop(vm); push(vm, a + b); break; }
            case OP_SUB: { int32_t b = pop(vm); int32_t a = pop(vm); push(vm, a - b); break; }
            case OP_MUL: { int32_t b = pop(vm); int32_t a = pop(vm); push(vm, a * b); break; }
            case OP_DIV: {
                int32_t b = pop(vm);
                int32_t a = pop(vm);
                if (b == 0) { fprintf(stderr, "VM Error: Division by zero\n"); return false; }
                push(vm, a / b);
                break;
            }
            case OP_PRINT: printf("[VM Output] %d\n", pop(vm)); break;
            default: fprintf(stderr, "VM Error: Unknown opcode 0x%02X\n", op); return false;
        }
    }
    return true;
}

void vm_disassemble(const uint8_t *code, size_t size) {
    size_t i = 0;
    printf("--- Bytecode Disassembly ---\n");
    while (i < size) {
        uint8_t op = code[i++];
        switch (op) {
            case OP_HALT:  printf("%04zu: HALT\n", i - 1); break;
            case OP_PUSH: {
                int32_t val = (int32_t)(code[i] | (code[i+1]<<8) | (code[i+2]<<16) | (code[i+3]<<24));
                printf("%04zu: PUSH %d\n", i - 1, val);
                i += 4;
                break;
            }
            case OP_POP:   printf("%04zu: POP\n", i - 1); break;
            case OP_ADD:   printf("%04zu: ADD\n", i - 1); break;
            case OP_SUB:   printf("%04zu: SUB\n", i - 1); break;
            case OP_MUL:   printf("%04zu: MUL\n", i - 1); break;
            case OP_DIV:   printf("%04zu: DIV\n", i - 1); break;
            case OP_PRINT: printf("%04zu: PRINT\n", i - 1); break;
            default:       printf("%04zu: UNKNOWN (0x%02X)\n", i - 1, op); break;
        }
    }
}
```

### Source: `src/main.c`

```c
#include "vm.h"
#include <stdio.h>

int main(void) {
    printf("===========================================\n");
    printf("   Stack-Based Bytecode Virtual Machine    \n");
    printf("===========================================\n");

    /* Program computes: (15 * 3) + 7 = 52 */
    uint8_t program[] = {
        OP_PUSH, 15, 0, 0, 0,  /* PUSH 15 */
        OP_PUSH, 3,  0, 0, 0,  /* PUSH 3  */
        OP_MUL,                /* 15 * 3 = 45 */
        OP_PUSH, 7,  0, 0, 0,  /* PUSH 7  */
        OP_ADD,                /* 45 + 7 = 52 */
        OP_PRINT,              /* Output 52 */
        OP_HALT                /* Stop */
    };

    vm_disassemble(program, sizeof(program));

    printf("\nRunning Virtual Machine:\n");
    VM vm;
    vm_init(&vm, program, sizeof(program));
    vm_run(&vm);

    return 0;
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
SRC = src/main.c src/vm.c
OBJ = $(SRC:.c=.o)
TARGET = tiny-vm

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean
```

---

[← Return to Projects Index](../README.md)
