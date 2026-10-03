# Loops

**Loops** repeat code. Java has four forms: `for`, enhanced `for` (for-each), `while` and `do-while`. Choose by how you know when to stop.

```
        ┌──────────────┐
  ──►   │  condition?  │──false──► exit
        └──────┬───────┘
             true
               ▼
          loop body ──► update ──┐
               ▲                 │
               └─────────────────┘
```

## `for`: a known number of iterations

```java
for (int i = 0; i < 5; i++) {
    System.out.println(i);          // 0 1 2 3 4
}
```

```
for ( init ; condition ; update ) { body }
       1        2          4         3
```

1. `init` runs once; 2. `condition` is checked before each iteration; 3. the body runs; 4. `update` runs; back to 2.

```java
for (int i = 10; i > 0; i -= 2) { ... }           // count down by 2
for (int i = 0, j = 9; i < j; i++, j--) { ... }   // two variables (two-pointer style)
for (;;) { ... }                                  // infinite loop; exit with break/return
```

## Enhanced `for` (for-each): every element

```java
int[] nums = {3, 1, 4};
for (int n : nums) {
    System.out.println(n);
}

List<String> names = List.of("Ada", "Linus");
for (String name : names) { ... }
```

Works on arrays and anything `Iterable`. Cleaner and safer, but:

| Limitation | Use instead |
|------------|-------------|
| No index | A classic `for` |
| Cannot assign back to the array element (`n = 0` changes only the copy) | `nums[i] = 0` in a classic `for` |
| Cannot add or remove from the collection being iterated | `Iterator.remove()`, `removeIf` ([iterators](../08-collections/12_iterators-and-fail-fast-behavior.md)) |
| Only forward, one element at a time | A classic `for` |

## `while`: until a condition changes

```java
int n = 12345, digits = 0;
while (n > 0) {
    n /= 10;
    digits++;
}
System.out.println(digits);          // 5
```

The condition is checked **first**, so the body may never run. Use `while` when the number of iterations is not known in advance: reading input, searching, retrying.

## `do-while`: run at least once

```java
int choice;
do {
    System.out.print("Enter 1-3: ");
    choice = readInt();
} while (choice < 1 || choice > 3);
```

The condition is checked **after** the body, and the final `;` is required. Typical for menus and input validation.

## `break` and `continue`

```java
for (int i = 1; i <= 10; i++) {
    if (i % 2 == 0) continue;        // skip to the next iteration
    if (i > 7) break;                // leave the loop entirely
    System.out.println(i);           // 1 3 5 7
}
```

### Labeled `break` / `continue`

Leave or continue an **outer** loop from an inner one.

```java
outer:
for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
        if (i * j == 2) break outer;       // exits both loops
        if (j == 1) continue outer;        // next i
        System.out.println(i + "," + j);
    }
}
```

Use sparingly. Often a method with an early `return` is clearer.

## Choosing a loop

| Situation | Use |
|-----------|-----|
| Count from a to b | `for` |
| Visit every element, no index needed | for-each |
| Repeat until something happens | `while` |
| Must run once before checking | `do-while` |
| Need to modify while iterating | Classic `for` with index, or `Iterator` |

## Common patterns

```java
// Sum
int sum = 0;
for (int n : nums) sum += n;

// Max
int max = nums[0];
for (int i = 1; i < nums.length; i++) if (nums[i] > max) max = nums[i];

// Search with early exit
boolean found = false;
for (int n : nums) {
    if (n == target) { found = true; break; }
}

// Reverse a string
String s = "hello";
StringBuilder sb = new StringBuilder();
for (int i = s.length() - 1; i >= 0; i--) sb.append(s.charAt(i));

// Nested loops: multiplication table
for (int r = 1; r <= 3; r++) {
    for (int c = 1; c <= 3; c++) System.out.print(r * c + "\t");
    System.out.println();
}
```

Nested loops multiply the work: two nested loops over `n` items are `n²` iterations ([27-dsa/00_complexity-analysis.md](../27-dsa/00_complexity-analysis.md)).

## Loop variable scope

```java
for (int i = 0; i < 3; i++) { }
// i is not visible here

int j;
for (j = 0; j < 3; j++) { }
System.out.println(j);               // 3: declared outside, still in scope
```

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Off-by-one: `i <= arr.length` | `ArrayIndexOutOfBoundsException` | `i < arr.length` |
| Semicolon after the header: `for (...);` or `while (x > 0);` | Body never runs / infinite loop | Remove the `;` |
| Forgetting the update (`n--`) | Infinite loop | Make sure the condition can become false |
| `for (byte b = 0; b < 200; b++)` | Infinite: `byte` wraps at 127 | Use `int` |
| Floating-point counter: `for (double d = 0; d != 1.0; d += 0.1)` | Never exactly 1.0, runs forever | Count with an `int` |
| Modifying a collection in for-each | `ConcurrentModificationException` | `Iterator.remove()` or `removeIf` |
| `String +=` in a loop | Slow for large loops | `StringBuilder` ([details](../03-strings-and-text/03_stringbuilder-and-stringbuffer.md)) |
| Recomputing `list.size()` or an expensive call every iteration | Slow | Store it in a variable |
| Assigning to the for-each variable | Array unchanged | Use an index |
| Using `break` in a `switch` inside a loop and expecting to exit the loop | Only the `switch` ends | Use a label or `return` |

## Key takeaways

- `for` when you know the count, for-each to visit every element, `while` when you do not, `do-while` for at-least-once
- `break` leaves a loop, `continue` skips to the next iteration; labels target outer loops
- Watch loop bounds (`<` vs `<=`), stray semicolons, and missing updates
- Never modify a collection while iterating with for-each

**Next:** [Arrays](./07_arrays.md)
