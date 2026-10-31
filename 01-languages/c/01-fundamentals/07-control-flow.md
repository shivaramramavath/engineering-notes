# Control Flow

Control flow statements decide which code runs and how many times. C's versions are simple, but they have a few sharp edges: `switch` fall-through, the dangling `else`, stray semicolons, and `goto`.

## Conditions

There is no boolean requirement in conditions: **zero is false, anything else is true**. Comparison and logical operators produce `int` `0` or `1`.

```c
if (n)        { /* n != 0 */ }
if (!ptr)     { /* ptr == NULL */ }
```

## `if` / `else`

```c
if (score >= 90) {
    grade = 'A';
} else if (score >= 80) {
    grade = 'B';
} else {
    grade = 'F';
}
```

**Always use braces**, even for one statement:

```c
if (ok)
    log("ok");
    cleanup();          /* runs unconditionally; the indentation lies */
```

**Dangling else:** an `else` attaches to the nearest `if`.

```c
if (a)
    if (b) x();
else            /* belongs to "if (b)", not "if (a)" */
    y();
```

**Stray semicolon:** `if (x > 0);` is an empty statement; the following block always runs. `-Wall` warns (`-Wempty-body`).

## `switch`

```c
switch (cmd) {
case 'a':
case 'A':
    add();
    break;
case 'q':
    quit();
    break;
default:
    unknown();
    break;
}
```

Rules:

- The controlling expression and each `case` label must be **integer** constants (no strings, no floats).
- Execution **falls through** to the next label unless you `break`, `return`, or `goto`. Falling through is occasionally intentional, but it is the cause of countless bugs.
- Always include a `default`, even if it only asserts.
- A declaration directly after a label is not allowed before C23, and jumping past an initialization is risky. Wrap case bodies in braces if they declare variables.

```c
case 1: {
    int tmp = compute();
    use(tmp);
    break;
}
```

When fall-through is intended, say so: C23 provides `[[fallthrough]];`, and GCC/Clang provide `__attribute__((fallthrough));`. Enable `-Wimplicit-fallthrough` (part of `-Wextra`) to find the accidental ones.

## Loops

```c
for (int i = 0; i < n; i++) { ... }    /* init; condition; step */
while (cond)  { ... }                  /* test first */
do { ... } while (cond);               /* body runs at least once; note the final ; */
for (;;)      { ... }                  /* infinite loop; exit with break/return */
```

- `int` declared in `for (int i = ...)` is scoped to the loop (C99).
- `break` leaves the **innermost** loop or `switch`. `continue` skips to the next iteration (for a `for` loop, it still runs the step expression).
- Off-by-one errors: with arrays of `n` elements, valid indexes are `0 .. n-1`, so use `i < n`, not `i <= n`.
- For unsigned counters going down, see [type conversions](./06-type-conversions.md).
- Do not modify the loop counter inside the body unless you intend to.

### Reading until a condition

```c
int c;
while ((c = getchar()) != EOF) {
    putchar(c);
}
```

## `goto`

`goto` jumps to a label within the same function. It is usually a bad idea, with one widely accepted use: **centralized cleanup on error**, where C has no exceptions or destructors.

```c
int process(const char *path) {
    int rc = -1;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return -1;

    char *buf = malloc(4096);
    if (buf == NULL) goto close_file;

    if (fread(buf, 1, 4096, f) == 0) goto free_buf;

    rc = 0;                   /* success */

free_buf:
    free(buf);
close_file:
    fclose(f);
    return rc;
}
```

Keep jumps forward and downward, never into the scope of a variable-length array, and release resources in reverse order of acquisition. See [error handling](../03-intermediate/03-error-handling.md).

## Common mistakes

| Mistake | Fix |
|---|---|
| Missing `break` in `switch` | Add `break`, or mark intentional fall-through |
| `if (x = 0)` | Use `==`; compile with `-Wall` |
| `;` after `if (...)`, `for (...)`, or `while (...)` | Remove it |
| Unbraced multi-statement `if` / `for` | Always use braces |
| `i <= n` loops over arrays | Use `i < n` |
| Comparing strings in `switch` or with `==` | Use `strcmp` in an `if`/`else` chain |

## Summary

- Zero is false; non-zero is true. Always use braces.
- `switch` works on integer constants and falls through by default.
- Use `goto` only for forward-jumping cleanup.

**Next:** [Functions](./08-functions.md)
