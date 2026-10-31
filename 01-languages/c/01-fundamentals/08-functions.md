# Functions

Functions are the only way to structure behavior in C. Two rules explain most of their behavior: arguments are always **passed by value**, and a function must be **declared before it is called**.

**Prerequisites:** [Program structure](./01-program-structure.md).

## Declaring and defining

```c
int add(int a, int b);        /* declaration (prototype): usually in a header */

int add(int a, int b) {       /* definition */
    return a + b;
}
```

A prototype tells the compiler the return type and parameter types so it can check calls and convert arguments. Always provide prototypes (in a header, or at the top of the file). Since C99, calling an undeclared function is invalid; modern compilers reject it.

`()` versus `(void)`: in C before C23, `int f()` means "unspecified parameters" and disables argument checking. Write `int f(void)`. In C23, `int f()` means no parameters.

## Pass by value

The function receives **copies** of its arguments:

```c
void bump(int x) { x++; }

int n = 5;
bump(n);
/* n is still 5 */
```

To let a function modify the caller's variable, pass its address:

```c
void bump(int *x) { (*x)++; }

bump(&n);       /* n is now 6 */
```

C has no references; this pointer technique is how "output parameters" work.

```c
#include <stdbool.h>

bool divmod(int a, int b, int *quot, int *rem) {
    if (b == 0) return false;
    *quot = a / b;
    *rem  = a % b;
    return true;
}
```

## Return values

A function returns at most one value. To return several, use an output parameter or return a `struct` (structs can be returned and passed by value; small ones are cheap).

```c
struct divres { int quot; int rem; };

struct divres divide(int a, int b) {
    return (struct divres){ a / b, a % b };    /* compound literal (C99) */
}
```

Ignoring a return value is allowed but often a bug (`fclose`, `malloc`, `printf`, `scanf`). Check what matters. GCC/Clang offer `__attribute__((warn_unused_result))`, and C23 offers `[[nodiscard]]`.

### Never return a pointer to a local variable

```c
int *bad(void) {
    int x = 42;
    return &x;          /* x is destroyed when the function returns */
}
```

Locals live on the stack and disappear on return; the pointer dangles. Either return a value, have the caller supply the storage, or allocate with `malloc` (and document who frees it). See [memory management](../02-core-concepts/08-memory-management.md).

## Arrays as parameters

Arrays **decay to pointers** when passed to functions, so the function doesn't know the length:

```c
void print_all(const int *a, size_t n) {    /* identical to int a[] or int a[10] */
    for (size_t i = 0; i < n; i++) printf("%d\n", a[i]);
}

int data[] = {1, 2, 3};
print_all(data, sizeof data / sizeof data[0]);
```

Inside the function, `sizeof a` is the size of a pointer, not of the array. **Always pass the length separately.** More in [arrays](../02-core-concepts/01-arrays.md).

## `static` functions

A function marked `static` is visible only inside its own `.c` file. Use it for helpers that are not part of the module's public interface; this avoids name clashes and lets the compiler optimize more freely.

```c
static int clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
```

## `inline` (brief)

`inline` is a hint that the compiler may substitute the body at the call site. Its linkage rules in C are subtle: the usual safe pattern for header-defined helpers is `static inline`. Modern optimizers inline on their own, so you rarely need it.

## Recursion

A function can call itself. Each call uses stack space for its parameters and locals, so deep recursion can overflow the stack, with no error handling available; the program crashes.

```c
unsigned long factorial(unsigned n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}
```

Always have a base case, and prefer a loop when the depth depends on untrusted input. C does not guarantee tail-call optimization.

```text
Call stack for factorial(3):

 factorial(1)   ← top, returns 1
 factorial(2)   waiting: 2 * ?
 factorial(3)   waiting: 3 * ?
 main()
```

## Argument evaluation order

The order in which arguments are evaluated is unspecified: `f(g(), h())` may call `h` first. Don't depend on it, and avoid side effects on the same variable in several arguments.

## Common mistakes

- Expecting a function to modify its argument without passing a pointer.
- Returning the address of a local variable or local array.
- Using `sizeof` on an array parameter.
- Missing prototype, or `()` instead of `(void)`.
- Ignoring return values of functions that report errors.
- Unbounded recursion.

## Debugging

- Compile with `-Wall -Wextra`: it catches missing returns (`-Wreturn-type`), unused values, and dangling-pointer returns (`-Wreturn-local-addr` in GCC).
- Falling off the end of a non-`void` function and using its result is undefined. `main` is the one exception.
- AddressSanitizer reports use of stack memory after return (`-fsanitize=address`, plus `-fsanitize-address-use-after-return` where available).

## Summary

- Arguments are copied; pass pointers to modify caller data.
- Declare before use with a full prototype; use `(void)` for no parameters.
- Arrays decay to pointers, so pass their length too.
- Mark file-private helpers `static`; never return pointers to locals.

**Next:** [Input and output](./09-input-output.md)
