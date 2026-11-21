# Printf and Scanf Format Specifiers (`04-printf-scanf-formats`)

## 1. Type Conversion Specifiers
| Specifier | Argument Type | Output Format |
| :--- | :--- | :--- |
| `%d` / `%i` | `int` | Signed decimal integer |
| `%u` | `unsigned int` | Unsigned decimal integer |
| `%x` / `%X` | `unsigned int` | Hexadecimal lowercase / uppercase |
| `%o` | `unsigned int` | Octal integer |
| `%f` / `%F` | `double` | Decimal floating-point notation (`12.345`) |
| `%e` / `%E` | `double` | Scientific exponent notation (`1.2345e+01`) |
| `%g` / `%G` | `double` | Compact format (chooses `%f` or `%e`) |
| `%c` | `int` (promoted `char`) | Single character |
| `%s` | `char*` | Null-terminated string |
| `%p` | `void*` | Implementation-defined pointer address |
| `%%` | None | Literal `%` character |

## 2. Length Modifiers
- `hh`: Signed/unsigned `char` (`%hhd`).
- `h`: `short int` (`%hd`).
- `l`: `long int` (`%ld`) or `wchar_t` (`%lc`).
- `ll`: `long long int` (`%lld`, `%llu`).
- `z`: `size_t` (`%zu`).
- `t`: `ptrdiff_t` (`%td`).
- `j`: `intmax_t` (`%jd`).

## 3. Safe `scanf` Rules
- **Always specify field width on strings:** `scanf("%255s", buffer)` prevents buffer overflow into `char buffer[256]`.
- **Check return value:** `scanf` returns the number of successfully assigned items.
