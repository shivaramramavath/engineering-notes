# Structs, Unions, and Enums

These are C's tools for defining your own types: `struct` groups values, `union` overlays them in the same storage, and `enum` names integer constants. Together with pointers they are how every non-trivial C program models data.

**Prerequisites:** [Pointers](./03-pointers.md), [declarations and typedef](./04-declarations-typedef.md).

## Structs

```c
struct point {
    int x;
    int y;
};

struct point a = { 1, 2 };                 /* positional */
struct point b = { .y = 5, .x = 3 };       /* designated initializers (C99), preferred */
struct point c = { 0 };                    /* all members zeroed */

a.x = 10;                                  /* member access */
struct point *p = &a;
p->y = 20;                                 /* same as (*p).y */
```

### Structs are values

Assignment copies every member, including embedded arrays. Passing or returning a struct by value copies it too.

```c
struct point d = a;         /* copy */
```

Pass large structs by pointer (`const struct big *`) to avoid copying. Structs cannot be compared with `==`.

### Nesting, arrays, and self-reference

```c
struct rect { struct point min, max; };
struct node { int value; struct node *next; };      /* pointer to itself: allowed */
```

A struct cannot contain itself directly, only a pointer to itself.

### Padding and `sizeof`

The compiler inserts padding so members are properly aligned:

```c
struct a { char c; int i; char d; };    /* typically 12 bytes on 32/64-bit ABIs */
struct b { int i; char c; char d; };    /* typically 8 bytes */
```

```text
struct a:  c . . . | i i i i | d . . .      (. = padding)
struct b:  i i i i | c d . .
```

Ordering members from largest to smallest alignment usually reduces padding. Always use `sizeof(struct x)`, never a computed sum. Padding bytes have unspecified values, so **comparing structs with `memcmp` is unreliable**; compare member by member. Details in [memory layout and alignment](../04-advanced/02-memory-layout-alignment.md).

### Flexible array member (C99)

A struct may end with an array of unspecified size, for variable-length data in one allocation:

```c
struct buf { size_t len; char data[]; };

struct buf *b = malloc(sizeof *b + n);
b->len = n;
```

### Bit-fields

```c
struct flags { unsigned ready : 1; unsigned mode : 3; };
```

The packing and ordering of bit-fields is implementation-defined, so don't use them for file formats or network protocols. Use shifts and masks instead ([bit manipulation](../03-intermediate/05-bit-manipulation.md)).

### Typedef idiom

```c
typedef struct {
    char name[32];
    int  age;
} person_t;
```

Use the tagged form (`struct node`) when the struct refers to itself.

## Unions

All members share the same memory; the size is that of the largest member.

```c
union value {
    int    i;
    float  f;
    char   bytes[4];
};

union value v;
v.f = 1.0f;
/* reading v.i now reinterprets the bytes of the float */
```

Only the last-written member holds a meaningful value, though C (unlike C++) permits reading another member to reinterpret the object representation. The result is implementation-dependent and may be a trap representation for some types.

### The tagged union pattern

The main practical use: a value that is one of several types, plus a tag recording which.

```c
typedef enum { VAL_INT, VAL_DOUBLE, VAL_STRING } val_kind;

typedef struct {
    val_kind kind;
    union {
        long        i;
        double      d;
        const char *s;
    } as;
} value_t;

void print_value(const value_t *v) {
    switch (v->kind) {
    case VAL_INT:    printf("%ld\n", v->as.i); break;
    case VAL_DOUBLE: printf("%g\n",  v->as.d); break;
    case VAL_STRING: printf("%s\n",  v->as.s); break;
    }
}
```

Use it for interpreters, ASTs, and protocol messages. C11 also allows **anonymous** structs/unions inside structs (members accessed directly).

## Enums

```c
enum color { RED, GREEN, BLUE };            /* 0, 1, 2 */
enum http { OK = 200, NOT_FOUND = 404 };

enum color c = GREEN;
```

Key facts:

- Enumeration **constants** have type `int` and are true compile-time constants, usable in `case` labels and array sizes. See [variables and constants](../01-fundamentals/04-variables-constants.md).
- The enum **type** is compatible with some implementation-chosen integer type (C23 allows you to fix it: `enum color : unsigned char { ... }`).
- C does not enforce that a variable holds a valid enumerator: `enum color c = 42;` compiles. Handle unexpected values with `default:` in `switch`.
- Enumerators share the surrounding scope, so `RED` can't be reused elsewhere. Prefix them (`COLOR_RED`).
- Enums don't give you string names. Provide a lookup table if you need printing.

### Enums as bit flags

```c
enum perm { PERM_READ = 1u << 0, PERM_WRITE = 1u << 1, PERM_EXEC = 1u << 2 };

unsigned p = PERM_READ | PERM_WRITE;
if (p & PERM_WRITE) { /* writable */ }
p &= ~PERM_WRITE;                      /* clear a flag */
```

Store flag combinations in an `unsigned`, not in the enum type.

## Common mistakes

| Mistake | Effect |
|---|---|
| Comparing structs with `==` | Compile error |
| `memcmp` on structs | Padding makes it unreliable |
| Using `sizeof` of a struct pointer instead of the struct | Allocating too little: write `malloc(sizeof *p)` |
| Reading a union member that wasn't written last, without a tag | Garbage or misinterpretation |
| Forgetting the `;` after `struct ... { }` | Confusing error on the next line |
| Bit-fields in wire formats | Layout differs across compilers |
| No `default` when switching on an enum | Missing cases after the enum grows (`-Wswitch` helps) |

## Summary

- Struct: grouped fields, copied by value, padded for alignment; initialize with designated initializers.
- Union: overlapped storage; pair it with an enum tag.
- Enum: named `int` constants; not type-safe; prefix names and use `-Wswitch`.

**Next:** [Scope and storage classes](./07-scope-storage-classes.md)
