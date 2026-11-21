# GCC, GDB, and Make Tooling Cheatsheet (`05-gcc-gdb-make`)

## 1. Essential GCC Compiler Flags
```bash
# Maximum Warnings & Strict Standards
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror

# Debug Build with Sanitizers
gcc -Og -g3 -fsanitize=address,undefined -fno-omit-frame-pointer

# Production Release Build
gcc -O3 -flto -DNDEBUG -march=native

# Shared Library Build
gcc -fPIC -shared -fvisibility=hidden -Wl,-soname,libfoo.so.1 -o libfoo.so.1.0.0 foo.o
```

## 2. Essential GDB Commands
- `gdb ./program`: Start debugging session.
- `r [args]`: Run program with arguments.
- `b main` / `b file.c:42`: Set breakpoint.
- `n`: Step over (next line).
- `s`: Step into function.
- `c`: Continue execution until next breakpoint.
- `bt`: Print call stack backtrace.
- `p variable`: Print variable value.
- `p/x variable`: Print in hexadecimal.
- `x/4xg 0x7fffffffe000`: Examine 4 giant words (8 bytes each) in hex at address.
- `watch var`: Break when variable value changes.

## 3. Makefile Automatic Variables
- `$@`: The file name of the target of the rule.
- `$<`: The name of the first prerequisite.
- `$^`: The names of all prerequisites, with spaces between them.
- `$?`: The names of all prerequisites that are newer than the target.
