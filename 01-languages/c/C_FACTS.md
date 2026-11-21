# C Language Standards, Versions & Verified Facts (C_FACTS.md)

> **Verified Date:** November 21, 2026  
> **Authority:** ISO/IEC JTC1/SC22/WG14 C Standards Committee, GCC Releases, LLVM/Clang Releases

---

## 1. ISO C Standards Timeline & Status

| Standard | ISO Identifier | Publication Date | Status & Key Highlights |
|---|---|---|---|
| **C89 / C90** | ANSI X3.159-1989 / ISO/IEC 9899:1990 | Dec 1989 / Dec 1990 | First formal standardization; function prototypes, `void*`, `const`, `volatile`. |
| **C99** | ISO/IEC 9899:1999 | Dec 1999 | `inline`, `<stdint.h>`, `long long`, variable-length arrays (VLAs), designated initializers, `//` comments, `<stdbool.h>`. |
| **C11** | ISO/IEC 9899:2011 | Dec 2011 | Anonymous structs/unions, `<threads.h>`, `<stdatomic.h>`, `_Generic`, `_Static_assert`, Annex K bounds-checking, removal of `gets()`, VLAs made optional. |
| **C17 / C18** | ISO/IEC 9899:2018 | June 2018 | Bugfix and defect report release; no new language features; `__STDC_VERSION__` defined as `201710L`. |
| **C23** | ISO/IEC 9899:2024 | October 30, 2024 | Major modernization; `__STDC_VERSION__` defined as `202311L`; native `nullptr`, `constexpr`, `typeof`, `#embed`, `<stdckdint.h>`, `<stdbit.h>`. |
| **C2y** | In progress | Expected ~2027+ | Next standardization cycle; active working drafts in WG14. |

*Sources:*
- ISO Standards Catalogue: [ISO/IEC 9899:2024](https://www.iso.org/standard/82075.html)
- WG14 Working Group Page: [open-std.org/jtc1/sc22/wg14](https://www.open-std.org/jtc1/sc22/wg14/)

---

## 2. Compiler Implementation Status & Defaults

### GNU GCC
- **GCC 14 (Released May 2024):** Introduced official `-std=c23` and `-std=gnu23` flags (superseding experimental `-std=c2x`). Default remained `-std=gnu17`.
- **GCC 15 (Released mid-2025):** **C23 (`-std=gnu23`) became the default language standard** when no `-std` flag is specified. Complete implementations of `#embed`, `<stdbit.h>`, `nullptr`, and standard attributes.
- **Breaking changes in GCC 15 default:** Unprototyped functions `int f()` now mean `int f(void)`. `bool`, `true`, and `false` are keywords, breaking code that defined custom identifiers with these names without `<stdbool.h>`.

### LLVM Clang
- **Clang 18 (Released early 2024):** Introduced official `-std=c23` flag.
- **Clang 19 / 20 (2024–2025):** Full support for `#embed`, `_BitInt(N)`, binary literals, digit separators, `[[nodiscard]]`, `[[maybe_unused]]`, `[[fallthrough]]`, `[[deprecated]]`, and C23 attribute syntax. C2y exploration flag `-std=c2y` enabled.

---

## 3. Critical Behavioral & Semantic Changes in C23

1. **`realloc(ptr, 0)` is Undefined Behavior (UB):**
   - *C89–C17:* Implementation-defined whether it freed the pointer and returned null or returned a non-null pointer.
   - *C23:* Officially classified as **Undefined Behavior** (WG14 paper N2396/N2464) to eliminate security vulnerabilities and error-handling ambiguities. Code must explicitly call `free(ptr)` when the target size is 0.
2. **Removal of K&R Function Declarations:**
   - In C89–C17, `int f()` meant "accepts an unspecified number of arguments of unspecified types".
   - In C23, `int f()` is strictly identical to `int f(void)` (accepts zero arguments).
3. **Mandatory Two's Complement:**
   - C23 officially eliminated one's complement and sign-magnitude integer representations. All conforming C23 hardware must use two's complement for signed integers. Signed overflow remains Undefined Behavior.
4. **Native `nullptr` and `nullptr_t`:**
   - Replaces `NULL` (which was typed as `0` or `(void*)0`). `nullptr` has type `nullptr_t` and cannot be implicitly converted to integer types, preventing function overload ambiguities and macro bugs.
5. **C23 `constexpr` Specifier:**
   - Creates true constant-expression objects at compile time. Can be used in case labels, enum values, and static array bounds without triggering VLA behavior.
6. **Binary Literals and Digit Separators:**
   - `0b10101010` and `0B1100` are standard in C23.
   - Single quote `'` digit separators: `1'000'000` or `0xFF'EE'DD'CC`.
7. **Embed Directive (`#embed`):**
   - Direct preprocessor binary asset embedding: `const uint8_t font[] = { #embed "font.bin" };`.
8. **Checked Integer Arithmetic (`<stdckdint.h>`):**
   - Safe arithmetic operations that return a boolean overflow indicator: `ckd_add(&result, a, b)`, `ckd_sub(&result, a, b)`, `ckd_mul(&result, a, b)`.

---

## 4. Local Host Verification Environment

- **OS:** Windows (Host session)
- **Local Compiler:** `MinGW.org GCC 6.3.0 (i586-mingw32)`
  - Confirmed capabilities: Compiles `-std=c11`, `-std=c99`, `-Wall -Wextra -Wpedantic -Werror`.
  - Limitations: GCC 6.3.0 predates C17 and C23 flags; 32-bit MinGW lacks `-lasan`/`-lubsan` runtime binaries.
  - Verification strategy: Local compilation tests run under `-std=c11 -Wall -Wextra -Wpedantic -Werror`; notes provide verified code portable to C11/C17/C23 and annotate how to run with ASan/UBSan on modern Linux/WSL2/macOS.
