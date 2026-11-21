# C23 Standard Features Quick Reference (`08-c23-features`)

## 1. Keywords & Constants
- `nullptr`: True null pointer constant with type `nullptr_t`. Eliminates ambiguity of `NULL` macro.
- `bool`, `true`, `false`: Promoted to first-class core keywords without requiring `<stdbool.h>`.
- `constexpr`: Immutable compile-time evaluated constants (`constexpr int SIZE = 1024;`).
- `typeof` / `typeof_unqual`: Standard type inspection operator (e.g., `typeof(x) y = x;`).

## 2. Binary Literals and Digit Separators
```c
int mask = 0b1010_1100_0001; /* Binary literal with digit separators */
uint64_t large_num = 1_000_000_000ULL;
```

## 3. Binary Resource Embedding (`#embed`)
Directly embeds binary assets (images, certificates, shaders) into C byte arrays at compile time:
```c
const unsigned char shader_bin[] = {
    #embed "assets/shader.spv"
};
```

## 4. Standard Attribute Syntax `[[...]]`
- `[[nodiscard]]`: Warns if function return value is discarded.
- `[[maybe_unused]]`: Suppresses unused variable warnings.
- `[[deprecated("reason")]]`: Emits compiler deprecation diagnostic.
- `[[fallthrough]]`: Documents intentional switch-case fallthrough.
- `[[unsequenced]]` / `[[reproducible]]`: Functional purity attributes for compiler optimization.

## 5. Checked Arithmetic (`<stdckdint.h>`)
```c
#include <stdckdint.h>

int result;
if (ckd_add(&result, a, b)) {
    /* Overflow occurred safely without UB! */
}
```

## 6. Standard Bit Operations (`<stdbit.h>`)
- `stdc_count_ones(x)`: Population count (popcnt).
- `stdc_leading_zeros(x)`: Count leading zeros (clz).
- `stdc_trailing_zeros(x)`: Count trailing zeros (ctz).
- `stdc_has_single_bit(x)`: Power-of-two check.
