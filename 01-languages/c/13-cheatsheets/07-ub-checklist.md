# The 25 Most Critical Undefined Behaviors in C (`07-ub-checklist`)

| # | Undefined Behavior Trap | Dangerous Example | Safe Standard Idiom |
| :- | :--- | :--- | :--- |
| **1** | Signed Integer Overflow | `int z = x + y;` | `__builtin_add_overflow` or `<stdckdint.h>` |
| **2** | Dereferencing NULL Pointer | `*ptr = 10;` | Check `if (ptr != NULL)` before dereference |
| **3** | Out-of-bounds Array Index | `arr[10] = 5;` | Explicit bound check `if (idx < SIZE)` |
| **4** | Use-After-Free | `free(p); *p = 1;` | Assign `p = NULL;` immediately after free |
| **5** | Double Free | `free(p); free(p);` | Double pointer destructor setting `*ptr = NULL;` |
| **6** | Reading Uninitialized Variable | `int x; printf("%d", x);` | Always initialize: `int x = 0;` |
| **7** | Strict Aliasing Violation | `float f = *(float*)&int_val;` | Use `memcpy(&f, &int_val, sizeof(f));` |
| **8** | Shifting by $\ge$ Bit Width | `uint32_t x = 1U << 32;` | Check `if (shift < 32)` |
| **9** | Negative Bit Shift Count | `x << -1;` | Ensure unsigned shift count |
| **10**| Modifying String Literal | `char *s = "hi"; s[0]='H';` | Use mutable array: `char s[] = "hi";` |
| **11**| Division or Modulo by Zero | `int z = x / 0;` | Check `if (denom != 0)` |
| **12**| `INT_MIN / -1` Overflow | `INT_MIN / -1;` | Check before dividing signed minimum |
| **13**| Unsequenced Side Effects | `i = i++;` | Separate statements: `i++;` |
| **14**| Passing NULL to Non-Null API | `memcpy(NULL, src, n);` | Guard `if (src && dst && n > 0)` |
| **15**| Pointer Arithmetic Past End+1 | `int *p = &arr[N + 2];` | Never advance pointer past `&arr[N]` |
| **16**| Subtraction of Unrelated Ptrs | `ptrdiff_t d = p1 - p2;` | Only subtract pointers within same array |
| **17**| Accessing Misaligned Pointer | `*(int*)odd_byte_address` | Use `memcpy` or `_Alignas` |
| **18**| Returning Pointer to Local Var | `return &local_var;` | Return heap memory or caller buffer |
| **19**| Overlapping Memory in `memcpy` | `memcpy(buf, buf+2, 5);` | Use `memmove(buf, buf+2, 5);` |
| **20**| Unhandled `realloc` Failure | `p = realloc(p, sz);` | Assign to `tmp = realloc(...)` to avoid leak |
| **21**| Data Race in Multithreading | Concurrent unsync write | Use `_Atomic` or `pthread_mutex_t` |
| **22**| Calling Non-Signal-Safe in ISR| `printf` inside handler | Use `write()` or `volatile sig_atomic_t` |
| **23**| Freeing Non-Heap Memory | `free(&local_buf);` | Only call `free()` on `malloc` return |
| **24**| Format String Attack | `printf(user_input);` | Always use format string: `printf("%s", str);` |
| **25**| Missing Return in Non-Void Func| `int f() { /* no ret */ }` | Enable `-Werror=return-type` |
