# Pointers and Declarations Cheatsheet (`02-pointers-and-declarations`)

## 1. Spiral / Right-Left Rule
To read any complex C declaration:
1. Start at the identifier.
2. Move right if a `(` or `[` is present.
3. Move left when you hit a `)` or end of token.
4. Continue spiraling outward.

```
+-----------------------------------------------------------+
| Example: char *(*fp)(int, void *)                         |
+-----------------------------------------------------------+
fp                     -> fp is a
*fp                    -> pointer to
(*fp)(...)             -> function taking (int, void *) returning
*(*fp)(...)            -> pointer to
char *(*fp)(...)       -> char
```

## 2. Pointer & Const Combinations
| Declaration | Meaning | Can change pointer? | Can change data? |
| :--- | :--- | :--- | :--- |
| `int *p` | Non-const pointer to non-const int | YES | YES |
| `const int *p` | Pointer to const int | YES | NO |
| `int const *p` | Identical to `const int *p` | YES | NO |
| `int * const p`| Const pointer to non-const int | NO | YES |
| `const int * const p`| Const pointer to const int | NO | NO |

## 3. Function Pointer Types
```c
/* Simple function pointer */
typedef int (*BinaryOp)(int a, int b);

/* Array of function pointers (Jump table) */
BinaryOp operations[4] = { add, sub, mul, div };

/* Function returning a function pointer */
BinaryOp get_operation(char op_char);
```

## 4. Array vs Pointer Decay Summary
- `arr`: Decays to `&arr[0]` of type `T*`.
- `&arr`: Does NOT decay; type is `T (*)[N]` (pointer to whole array).
- `sizeof(arr)`: Total array byte size ($N \times \text{sizeof}(T)$).
- `sizeof(&arr)`: Pointer size (8 bytes on 64-bit systems).
