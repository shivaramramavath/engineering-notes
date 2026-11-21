# Pointers and Memory Management Interview Guide (`02-pointers-and-memory-qa`)

> Master complex pointer mechanics: multidimensional pointer decay, function pointers, strict aliasing, heap allocators, and memory alignment.

| Metadata | Details |
| :--- | :--- |
| **Guide** | `02-pointers-and-memory-qa` |
| **Level** | Advanced |
| **Standard Baseline** | C11 / C17 / C23 |

---

## 1. Pointers, Arrays, and Decay

<details>
<summary><strong>Q1: What is array decay in C, and in which three situations does an array NOT decay into a pointer?</strong></summary>

Array decay is the automatic conversion of an array lvalue expression into a pointer to its first element (`&arr[0]`).
An array does **NOT** decay in exactly three situations:
1. When it is the operand of the `sizeof` operator (`sizeof(arr)` yields total array bytes, not pointer size).
2. When it is the operand of the unary `&` (address-of) operator (`&arr` yields a pointer to the entire array `T (*)[N]`).
3. When it is a string literal used to initialize a character array (`char str[] = "hello";`).
</details>

<details>
<summary><strong>Q2: What is the difference between `int *arr[10]` and `int (*arr)[10]`?</strong></summary>

- `int *arr[10]`: An array of 10 pointers to `int`. (Subscript `[]` has higher precedence than `*`).
- `int (*arr)[10]`: A single pointer to an array of 10 integers. Incrementing `arr` (`arr + 1`) jumps forward by `10 * sizeof(int)` bytes (40 bytes on 32-bit `int`).
</details>

<details>
<summary><strong>Q3: How do you declare a pointer to a function returning a pointer to an array of 5 integers?</strong></summary>

```c
int (*(*func_ptr)(void))[5];
```
Breakdown using the spiral rule:
1. `func_ptr`: Identifier.
2. `*func_ptr`: Pointer to...
3. `(*func_ptr)(void)`: Function taking `void` returning...
4. `*(*func_ptr)(void)`: Pointer to...
5. `int (*(*func_ptr)(void))[5]`: Array of 5 integers.
</details>

---

## 2. Heap Allocators and Strict Aliasing

<details>
<summary><strong>Q4: What is the Strict Aliasing Rule in C99/C11, and how does `char*` behave as an exception?</strong></summary>

The Strict Aliasing Rule (Section 6.5#7) states that two pointers of incompatible types cannot point to the same memory location, allowing the compiler to assume that writes through pointer A do not alter data read through pointer B.
Dereferencing an aliased pointer of incompatible type (e.g., casting `int*` to `float*` and dereferencing) invokes Undefined Behavior.

**Exceptions:**
1. Character types (`char*`, `unsigned char*`, `signed char*`): Allowed to alias and inspect any memory object byte-by-byte.
2. Signed/unsigned variants of the same type.
3. Compatible structures or unions containing common initial sequences.
</details>

<details>
<summary><strong>Q5: What is the difference between `malloc`, `calloc`, `realloc`, and `aligned_alloc`?</strong></summary>

- `malloc(size)`: Allocates uninitialized heap memory. Contains indeterminate values.
- `calloc(num, size)`: Allocates zero-initialized heap memory. Checks for integer multiplication overflow internally.
- `realloc(ptr, new_size)`: Resizes existing block. If expanded in-place, preserves pointer; otherwise allocates new block, copies bytes, frees old block, and returns new pointer.
- `aligned_alloc(alignment, size)` (C11): Allocates memory aligned to a specified boundary (e.g., 64 bytes for AVX-512 or cache line isolation). `size` must be a multiple of `alignment`.
</details>

<details>
<summary><strong>Q6: Why is `void*` pointer arithmetic illegal in standard C, yet supported in GCC?</strong></summary>

In standard ISO C, `sizeof(void)` is undefined because `void` represents an incomplete, non-object type; pointer arithmetic requires multiplying the step count by `sizeof(*ptr)`.
GCC provides a language extension treating `sizeof(void) == 1`, allowing byte-wise pointer arithmetic directly on `void*`. In portable code, always cast to `uint8_t*` or `char*` before adding offsets.
</details>

---

## 3. Alignment and Cache Behavior

<details>
<summary><strong>Q7: What is struct padding, and how do you minimize struct size without packing?</strong></summary>

CPUs enforce memory alignment constraints (e.g., 4-byte integers must align on 4-byte addresses; 8-byte pointers align on 8-byte addresses). Compilers insert invisible padding bytes between struct members to maintain alignment.
To minimize struct size without `#pragma pack` (which degrades memory access speed):
**Order struct members by descending size:** Place 8-byte types first (`uint64_t`, pointers, `double`), followed by 4-byte types (`int`, `float`), 2-byte types (`short`), and 1-byte types (`char`, `bool`).
</details>

<details>
<summary><strong>Q8: What is the difference between memory leak, double free, and use-after-free?</strong></summary>

- **Memory Leak:** Memory allocated on the heap is never freed and all references to it are lost.
- **Double Free:** Calling `free(ptr)` twice on the same memory block, corrupting allocator metadata and enabling heap-exploitation vulnerabilities.
- **Use-After-Free (UAF):** Accessing memory through a pointer after `free(ptr)` was called. The memory may have been reassigned to another object or returned to the OS.
</details>

---

[← C Basics QA](./01-c-basics-qa.md) | [Interview Index](./README.md) | [Next: Output-Based Questions →](./03-output-based-questions.md)
