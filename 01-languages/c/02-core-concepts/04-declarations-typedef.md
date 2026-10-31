# Declarations and typedef

C declarations mirror how the thing is **used**, which makes them powerful and hard to read. A few rules let you decode any declaration, and `typedef` lets you tame the ugly ones.

**Prerequisites:** [Pointers](./03-pointers.md), [arrays](./01-arrays.md).

## Reading a declaration

A declaration has a base type followed by a **declarator** containing the name:

```c
int *p;          /* base type: int; declarator: *p  → "*p is an int" */
```

Operator precedence in declarators matches expressions: `[]` and `()` bind **tighter** than `*`.

### Method: start at the name, go right, then left

1. Find the identifier.
2. Read to the right until a `)` or the end (`[]` = array of, `()` = function returning).
3. Then read to the left (`*` = pointer to).
4. Parentheses flip the direction; resolve the innermost first.

| Declaration | Reads as |
|---|---|
| `int *p` | p is a pointer to int |
| `int a[5]` | a is an array of 5 int |
| `int *a[5]` | a is an array of 5 pointers to int |
| `int (*p)[5]` | p is a pointer to an array of 5 int |
| `int f(int)` | f is a function taking int returning int |
| `int *f(int)` | f is a function returning pointer to int |
| `int (*fp)(int)` | fp is a pointer to a function taking int returning int |
| `int (*fa[3])(int)` | fa is an array of 3 pointers to functions taking int returning int |
| `const char *p` | p is a pointer to const char |
| `char *const p` | p is a const pointer to char |

A harder one, step by step:

```c
int (*(*f)(int))[5];
```

- `f` is a pointer (`*f`) to a function taking `int` …
- … returning a pointer (`*(...)`) to an array of 5 `int`.

If you ever need to decode one quickly, use the `cdecl` tool or `https://cdecl.org`, or better: introduce typedefs.

## `typedef`

`typedef` creates an **alias** for a type; it doesn't create a new type. The syntax is a declaration with `typedef` in front, where the declared name becomes the type name:

```c
typedef unsigned long ulong_t;
typedef int (*binary_op)(int, int);        /* binary_op is a pointer-to-function type */

int add(int a, int b) { return a + b; }
binary_op op = add;                        /* far more readable than int (*op)(int, int) */
```

Typedefs make complex declarations readable:

```c
typedef int row_t[5];
typedef row_t *row_ptr;                    /* pointer to array of 5 ints */
```

### Typedef and structs

```c
struct node {
    int value;
    struct node *next;       /* must use the struct tag here: typedef isn't complete yet */
};
typedef struct node node_t;

/* or both at once */
typedef struct list {
    node_t *head;
    size_t  len;
} list_t;
```

Both `struct list` and `list_t` name the same type. Self-referencing structs need the tag.

### Opaque types

Typedef plus an incomplete struct hides implementation details in a library header:

```c
/* stack.h */
typedef struct stack stack_t;           /* incomplete: layout is private */
stack_t *stack_new(void);
void     stack_free(stack_t *s);
```

See [opaque types](../04-advanced/07-opaque-types-oop-in-c.md).

## Guidelines and pitfalls

- **Use typedef for:** function-pointer types, platform-dependent integer types (`size_t`, `uint32_t`), opaque handles, and tidy aliases for complex declarators.
- **Don't hide pointers:** `typedef struct foo *foo_ptr;` makes `const foo_ptr` mean a const *pointer*, not a pointer to const. Readers also lose the cue that `*` is involved.
- **Don't typedef everything:** Linux kernel style, for example, prefers writing `struct foo` so the kind of type is visible. Follow your project's convention.
- **Naming:** suffix `_t` is common but names ending in `_t` are reserved by POSIX; for your own library choose a project prefix (`mylib_buf`) to avoid clashes.
- **Typedefs are aliases:** `typedef int meters; typedef int feet;` are the *same* type. The compiler won't catch mixing them.

```c
typedef char *str_t;
const str_t a;          /* char *const a, NOT const char *a */
```

## Summary

- `[]` and `()` bind tighter than `*`: read from the name outward, right then left.
- `int *a[5]` is an array of pointers; `int (*a)[5]` is a pointer to an array.
- `typedef` names a type; use it for function pointers and opaque handles, not to hide `*`.

**Next:** [Type qualifiers](./05-type-qualifiers.md)
