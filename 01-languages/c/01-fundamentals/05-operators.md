# Operators

C has a rich set of operators, and several have surprising precedence or evaluation rules. Most operator bugs come from three places: integer division, precedence of bitwise operators, and expressions whose evaluation order is not defined.

## Arithmetic

| Operator | Meaning | Note |
|---|---|---|
| `+ - * /` | basic arithmetic | `/` on integers truncates toward zero (C99 and later) |
| `%` | remainder (integers only) | result has the sign of the left operand |
| `+x -x` | unary plus / minus | |

```c
7 / 2        /* 3   */
-7 / 2       /* -3  (truncates toward zero, not floor) */
-7 % 2       /* -1  */
7.0 / 2      /* 3.5 (one operand is double) */
```

Dividing or taking `%` by zero is undefined behavior for integers. Check the divisor first.

## Comparison and logical

| Operator | Meaning |
|---|---|
| `== != < > <= >=` | compare; result is `int` `1` or `0` |
| `&&` `\|\|` `!` | logical AND, OR, NOT |

C treats **any non-zero value as true**. `&&` and `||` **short-circuit**: the right operand is evaluated only if needed.

```c
if (p != NULL && p->value > 0) { ... }   /* safe: p->value not evaluated if p is NULL */
```

Don't confuse `=` (assignment) with `==` (comparison): `if (x = 5)` assigns and is always true. Compile with `-Wall`, which warns about it.

## Bitwise

| Operator | Meaning |
|---|---|
| `&` `\|` `^` | AND, OR, XOR |
| `~` | NOT (invert bits) |
| `<<` `>>` | shift left / right |

Use these on unsigned types and mind shift limits; see [integer representation](./03-integer-representation.md). Don't mix up `&` with `&&`: `1 & 2` is `0` but `1 && 2` is `1`.

## Assignment, increment, compound

```c
x += 5;        /* x = x + 5 */
x <<= 2;       /* x = x << 2 */
i++; ++i;      /* post- and pre-increment */
```

`i++` yields the old value, `++i` the new one. On a standalone line they're equivalent. Don't use both on the same variable inside one expression.

## Other operators

```c
size_t s = sizeof(int);          /* size in bytes, compile time */
int m = (a > b) ? a : b;         /* ternary: condition ? if_true : if_false */
int r = (f(), g());              /* comma: evaluate f, then g; result is g() */
```

`&x` takes an address, `*p` dereferences, `.` and `->` access struct members. See [pointers](../02-core-concepts/03-pointers.md).

## Precedence you must remember

Highest to lowest (simplified):

```text
() [] -> .   postfix ++ --
! ~ unary+ unary- ++ -- * & sizeof (type)    (unary, right-to-left)
* / %
+ -
<< >>
< <= > >=
== !=
&
^
|
&&
||
?:
= += -= ...      (right-to-left)
,
```

The classic trap: `&`, `^`, `|` bind **looser** than `==`.

```c
if (flags & MASK == 0)       /* parsed as flags & (MASK == 0) */
if ((flags & MASK) == 0)     /* what you meant */
```

When unsure, add parentheses. See the full table in [operator precedence](../12-reference/03-operator-precedence.md).

## Order of evaluation

C does not specify the order in which operands or function arguments are evaluated, and modifying a variable twice between sequence points is undefined.

```c
int i = 1;
i = i++ + 1;            /* undefined behavior */
a[i] = i++;             /* undefined behavior */
f(i++, i++);            /* unspecified order; also undefined */
printf("%d %d\n", g(), h());   /* g and h may run in either order */
```

Rules that *are* guaranteed: `&&`, `||`, `?:` and `,` evaluate left to right with a sequence point. Otherwise, write one side effect per statement.

## Common mistakes

- Integer division where a fractional result was expected: `double avg = sum / count;` still divides as integers if both are `int`. Cast one operand: `(double)sum / count`.
- `=` instead of `==` in conditions.
- Missing parentheses around bitwise expressions in comparisons.
- Side effects in expressions that use the same variable more than once.
- Forgetting that `%` with negative operands can return negative values.

## Summary

- Integer `/` truncates toward zero; `%` takes the sign of the dividend; dividing by zero is undefined.
- `&&` and `||` short-circuit; any non-zero is true.
- Parenthesize bitwise operations; avoid multiple side effects in one expression.

**Next:** [Type conversions](./06-type-conversions.md)
