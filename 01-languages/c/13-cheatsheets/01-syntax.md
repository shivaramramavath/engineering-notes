# C Syntax and Grammar Cheatsheet (`01-syntax`)

## 1. Storage Classes & Scope Qualifiers
| Keyword | Storage Duration | Scope | Linkage | Common Use Case |
| :--- | :--- | :--- | :--- | :--- |
| `auto` | Automatic (Stack) | Block | None | Default for local variables |
| `register`| Automatic (CPU) | Block | None | Hint to place in register (cannot take `&`) |
| `static` (local)| Static (Data/BSS) | Block | None | Persistent across function calls |
| `static` (global)| Static (Data/BSS) | File | Internal | Private to translation unit |
| `extern` | Static (Data/BSS) | File/Block | External | Reference global symbol in other TU |
| `_Thread_local`| Thread | Block/File | None/External | Thread-specific storage (C11) |

## 2. Type Qualifiers
- `const`: Object value cannot be modified through this lvalue.
- `volatile`: Value can change unexpectedly; prevents compiler from caching in register or eliding reads/writes.
- `restrict`: Hint that for the lifetime of the pointer, only it (or a pointer derived from it) will access the object. Enables SIMD vectorization.
- `_Atomic` (C11): Enforces atomic load, store, and read-modify-write operations.

## 3. Operator Precedence (Highest to Lowest)
1. `()` `[]` `->` `.` `++` (post) `--` (post)
2. `++` (pre) `--` (pre) `+` (unary) `-` (unary) `!` `~` `*` (deref) `&` (addr) `sizeof` `_Alignof`
3. `(type)` cast
4. `*` `/` `%`
5. `+` `-`
6. `<<` `>>`
7. `<` `<=` `>` `>=`
8. `==` `!=`
9. `&` (bitwise AND)
10. `^` (bitwise XOR)
11. `|` (bitwise OR)
12. `&&` (logical AND - sequence point)
13. `||` (logical OR - sequence point)
14. `?:` (ternary conditional)
15. `=` `+=` `-=` `*=` `/=` `%=` `<<=` `>>=` `&=` `^=` `|=`
16. `,` (comma operator - sequence point)
