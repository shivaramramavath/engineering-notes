# C Standards and History

C is defined by an ISO standard, and "C" means different things depending on the edition. Knowing which standard you target explains why some code compiles on one machine and not another, and why old tutorials show habits that modern C avoids.

## Timeline

```text
1972  C created at Bell Labs (Dennis Ritchie)
1978  K&R book: de facto "K&R C"
1989  ANSI C (C89)  → adopted by ISO as C90
1999  C99
2011  C11
2018  C17   (bug-fix release of C11, no new features)
2024  C23   (ISO/IEC 9899:2024)
```

| Standard | `__STDC_VERSION__` | GCC/Clang flag |
|---|---|---|
| C89 / C90 | not defined (`199409L` for the 1995 amendment) | `-std=c89` / `-std=c90` |
| C99 | `199901L` | `-std=c99` |
| C11 | `201112L` | `-std=c11` |
| C17 | `201710L` | `-std=c17` |
| C23 | `202311L` | `-std=c23` (older compilers: `-std=c2x`) |

## What each edition added

**C89/C90:** function prototypes, `void`, `const`, `volatile`, and the standard library. This is the baseline of "classic" C.

**C99:**
- `//` comments
- Declarations anywhere in a block, and `for (int i = 0; ...)`
- `<stdint.h>` fixed-width types (`int32_t`, `uint64_t`), `long long`, `<stdbool.h>`
- Designated initializers: `struct point p = { .x = 1, .y = 2 };`
- Compound literals, `inline`, flexible array members, variable length arrays (VLAs)
- `snprintf`

**C11:**
- `_Generic`, `_Static_assert`, `_Alignas` / `_Alignof`
- Anonymous structs and unions
- `<stdatomic.h>` and `<threads.h>`, both **optional** features
- `gets` removed
- VLAs became an **optional** feature (compilers may define `__STDC_NO_VLA__`)

**C17:** defect fixes and clarifications only. For practical purposes it is C11.

**C23:** among others,
- `bool`, `true`, `false`, `nullptr` as keywords
- `typeof`, `constexpr` for objects, and `auto` type inference
- `[[attributes]]` such as `[[nodiscard]]` and `[[maybe_unused]]`
- Binary literals (`0b1010`) and digit separators (`1'000'000`)
- `#embed`, `<stdbit.h>`, `<stdckdint.h>` (checked arithmetic)
- `int f()` now means "no parameters", same as `(void)`
- Old K&R-style function definitions removed

## Which one should you use?

A reasonable default is **C17** (`-std=c17`): universally supported, stable, and what most existing code assumes. Learn the C23 additions as you go, and use them where your compiler and your target platforms support them.

If you maintain older code, check which standard it targets. Embedded and legacy projects often pin C99 or even C89.

## `-std=c17` vs `-std=gnu17`

GCC and Clang default to a **GNU dialect** (for example `gnu17`), which is the standard plus compiler extensions. `-std=c17` turns the extensions off and enforces strict standard behavior. Combine it with `-Wpedantic` to be warned when you use something non-standard.

```bash
gcc -std=c17 -Wpedantic main.c      # portable, standard-only C
gcc -std=gnu17 main.c               # standard + GNU extensions
```

The default dialect depends on the compiler version (GCC 8–14 default to `gnu17`; newer releases move toward `gnu23`), so **always state `-std=` explicitly** in your build. Otherwise the same code can behave differently between machines.

## Detecting the standard in code

```c
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    /* C11 or later */
#endif
```

## Misconceptions

- **"C is a subset of C++."** Not true. Valid C can be invalid C++ (for example implicit `void *` conversion: `int *p = malloc(n);`) or mean something different. Do not compile C as C++.
- **"VLAs are standard, so use them."** They are optional since C11, risky (stack overflow with no error handling), and unsupported by some compilers. Prefer `malloc` for runtime-sized arrays.
- **"My compiler accepted it, so it's valid C."** Compilers accept many extensions and some undefined behavior. Use `-std=c17 -Wpedantic` to check. See [undefined behavior](../04-advanced/01-undefined-behavior.md).
- **"Old tutorial code is fine."** Patterns like `gets`, `void main`, implicit declarations, and K&R-style definitions are obsolete or removed.

## Summary

- C evolves through ISO editions: C89 → C99 → C11 → C17 → C23.
- Target **C17** by default; use C23 features only where supported.
- Always pass `-std=` explicitly; know the difference between `c17` and `gnu17`.
- C and C++ are separate languages.

**Next:** [Program structure](../01-fundamentals/01-program-structure.md)
