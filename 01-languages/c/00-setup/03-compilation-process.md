# The Compilation Process

`gcc hello.c -o hello` looks like one step, but it runs four. Knowing which stage produced an error tells you where to look, and it explains headers, object files, and linking later in the course.

## The four stages

```text
hello.c
   │  1. Preprocess   (gcc -E)   expand #include, #define, #if
   ▼
hello.i              pure C, no directives
   │  2. Compile      (gcc -S)   C → assembly
   ▼
hello.s
   │  3. Assemble     (gcc -c)   assembly → machine code
   ▼
hello.o              object file (not yet runnable)
   │  4. Link         (ld)       combine objects + libraries
   ▼
hello                executable
```

You can stop at any stage and inspect the result:

```bash
gcc -E hello.c -o hello.i     # preprocessed source
gcc -S hello.c -o hello.s     # assembly
gcc -c hello.c -o hello.o     # object file
gcc hello.o -o hello          # link
```

## What each stage does

**1. Preprocessor.** Handles lines starting with `#`. `#include <stdio.h>` is replaced by the literal text of that header. `#define` macros are expanded. `#if` / `#ifdef` blocks are kept or dropped. It does text substitution only and knows nothing about C syntax.

**2. Compiler.** Parses the C, checks types, optimizes, and emits assembly. Syntax errors, type errors and most warnings come from here.

**3. Assembler.** Turns assembly into machine code in an object file. The object file contains your function bodies and a symbol table, but calls to functions defined elsewhere (like `printf`) are left as unresolved placeholders.

**4. Linker.** Joins object files and libraries, resolves those placeholders, and produces the executable. See [linking and loading](../05-internals/04-linking-loading.md).

## Inspecting the stages

```bash
nm hello.o            # symbols; 'U printf' means undefined, to be resolved by the linker
objdump -d hello.o    # disassembly
gcc -v hello.c        # show the exact sub-commands gcc runs
wc -l hello.i         # a 5-line program becomes hundreds of lines after #include
```

## Multiple files

Each `.c` file is compiled to its own object file independently. The linker joins them.

```bash
gcc -c main.c -o main.o
gcc -c math_utils.c -o math_utils.o
gcc main.o math_utils.o -o app
```

This is why build tools exist: if only `math_utils.c` changed, only it needs recompiling. See [multi-file programs](../03-intermediate/02-multi-file-programs.md).

## Libraries and search paths

| Flag | Meaning | Example |
|---|---|---|
| `-I<dir>` | Where to look for headers | `-Iinclude` |
| `-L<dir>` | Where to look for libraries | `-L/opt/lib` |
| `-l<name>` | Link `lib<name>.so` / `.a` | `-lm` (math library) |
| `-D<name>[=val]` | Define a macro | `-DDEBUG` |

Order matters for `-l`: put libraries **after** the files that use them.

```bash
gcc main.c -lm -o app   # correct: sqrt() etc. resolved from libm
gcc -lm main.c -o app   # may fail on linkers that process arguments left to right
```

## Which stage produced the error?

| Message | Stage | Meaning |
|---|---|---|
| `fatal error: foo.h: No such file or directory` | Preprocessor | Header not found; check `-I` |
| `error: expected ';' before ...` | Compiler | Syntax error |
| `warning/error: implicit declaration of function 'x'` | Compiler | Function used before declaration; usually a missing `#include` |
| `undefined reference to 'x'` | Linker | Declared but never defined, or the library/object wasn't linked |
| `multiple definition of 'x'` | Linker | Same symbol defined in two files, often a function body in a header |

> **Version note:** implicit function declarations were removed from the language in C99, but compilers kept accepting them with a warning for decades. Recent compilers (GCC 14, Clang 16) reject them by default. Don't rely on them.

## Misconception

"The compiler produces the executable." Strictly, the **linker** does. This matters when debugging: "undefined reference" means your code compiled fine but the link step couldn't find a definition, so adding more `#include`s won't fix it.

## Summary

- Four stages: preprocess → compile → assemble → link.
- `-E`, `-S` and `-c` stop early so you can inspect each result.
- Compile errors and linker errors have different causes. Identify the stage first.
- Libraries go after the files that need them.

**Next:** [First program](./04-first-program.md)
