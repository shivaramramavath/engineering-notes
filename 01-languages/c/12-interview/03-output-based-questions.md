# Output-Based Questions and Code Puzzles (`03-output-based-questions`)

> Sharpen your mental compiler: analyze tricky C code snippets, promotion traps, order of evaluation puzzles, and operator precedence edge cases.

| Metadata | Details |
| :--- | :--- |
| **Guide** | `03-output-based-questions` |
| **Level** | Advanced |
| **Standard Baseline** | C11 / C17 / C23 |

---

## Puzzle 1: Unsigned Comparison Inversion

```c
#include <stdio.h>
int main(void) {
    int a = -10;
    unsigned int b = 5;
    if (a > b) {
        printf("Greater\n");
    } else {
        printf("Lesser\n");
    }
    return 0;
}
```

<details>
<summary><strong>Answer & Explanation</strong></summary>

**Output:** `Greater`

**Why:** Under the Usual Arithmetic Conversions, when a signed `int` (`a = -10`) is compared with an `unsigned int` (`b = 5`), `a` is implicitly converted to `unsigned int`. On 32-bit systems, `-10` becomes `4294967286U` (`UINT_MAX - 9`). Since `4294967286 > 5`, the condition evaluates to `true`!
</details>

---

## Puzzle 2: Pointer Arithmetic and Array Addresses

```c
#include <stdio.h>
int main(void) {
    int arr[] = {10, 20, 30, 40, 50};
    int *ptr1 = arr;
    int (*ptr2)[5] = &arr;

    printf("%d\n", *(ptr1 + 1));
    printf("%d\n", *(*ptr2 + 1));
    printf("%td\n", (char*)(ptr2 + 1) - (char*)ptr2);
    return 0;
}
```

<details>
<summary><strong>Answer & Explanation</strong></summary>

**Output:**
```
20
20
20
```

**Why:**
- `ptr1 + 1` increments by 1 `int` (4 bytes), pointing to element 1 (`20`).
- `*ptr2` dereferences the array pointer, yielding an array of 5 ints that decays to `&arr[0]`. `*ptr2 + 1` points to element 1 (`20`).
- `ptr2 + 1` advances by `sizeof(int[5])` = 20 bytes! Subtracting `(char*)ptr2` yields `20`.
</details>

---

## Puzzle 3: String Literal Modification

```c
#include <stdio.h>
int main(void) {
    char str1[] = "Hello";
    char *str2 = "Hello";

    str1[0] = 'M';
    printf("str1: %s\n", str1);

    str2[0] = 'M'; /* What happens here? */
    printf("str2: %s\n", str2);
    return 0;
}
```

<details>
<summary><strong>Answer & Explanation</strong></summary>

**Output:**
```
str1: Mello
Segmentation Fault (SIGSEGV) or Bus Error
```

**Why:** `str1[]` allocates an array on the stack and copies the characters `"Hello"` into mutable stack storage. `str2` points directly to string literal memory located in the read-only `.rodata` segment. Attempting to write to read-only virtual memory triggers a hardware page fault.
</details>

---

## Puzzle 4: Precedence of Dereference and Post-Increment

```c
#include <stdio.h>
int main(void) {
    int data[] = {100, 200, 300};
    int *p = data;

    int val1 = *p++;
    int val2 = (*p)++;
    int val3 = *++p;

    printf("%d, %d, %d\n", val1, val2, val3);
    printf("data[1] = %d\n", data[1]);
    return 0;
}
```

<details>
<summary><strong>Answer & Explanation</strong></summary>

**Output:**
```
100, 200, 300
data[1] = 201
```

**Why:**
1. `*p++`: Post-increment has higher precedence than `*`. It increments pointer `p` to `&data[1]`, but evaluates to the dereferenced old pointer value (`100`).
2. `(*p)++`: Parentheses enforce dereferencing `p` first (`data[1] = 200`). The value `200` is assigned to `val2`, and `data[1]` is post-incremented to `201`.
3. `*++p`: Pre-increment increments `p` to `&data[2]`, then dereferences, yielding `300`.
</details>

---

[← Pointers & Memory QA](./02-pointers-and-memory-qa.md) | [Interview Index](./README.md) | [Next: Bit Manipulation QA →](./04-bit-manipulation-qa.md)
