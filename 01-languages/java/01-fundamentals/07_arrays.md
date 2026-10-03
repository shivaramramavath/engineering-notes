# Arrays

An **array** is a fixed-size, ordered sequence of elements of one type, accessed by a zero-based index. Arrays are **objects**: the variable holds a reference to the array on the heap.

```
int[] a = {10, 20, 30};

 a ───► ┌────┬────┬────┐
        │ 10 │ 20 │ 30 │      a.length = 3
        └────┴────┴────┘
          [0]  [1]  [2]
```

## Declaring and creating

```java
int[] nums = new int[5];                 // five zeros
String[] names = new String[3];          // three nulls
int[] primes = {2, 3, 5, 7};             // initializer
int[] copy = new int[]{1, 2, 3};         // explicit form (needed in assignments/arguments)
```

Prefer `int[] a` over the C-style `int a[]`. Beware: `int[] a, b;` makes both arrays, while `int a[], b;` makes only `a` an array.

### Default values

| Element type | Default |
|--------------|---------|
| Numeric primitives | `0` / `0.0` |
| `boolean` | `false` |
| `char` | `'\u0000'` |
| Reference types | `null` |

## Access and length

```java
nums[0] = 42;
int first = nums[0];
int n = nums.length;           // a field, NOT a method (compare String.length())
int last = nums[nums.length - 1];

nums[5];                       // ArrayIndexOutOfBoundsException: Index 5 out of bounds for length 5
new int[-1];                   // NegativeArraySizeException
```

The size is fixed at creation. To "grow", create a bigger array and copy, or use `ArrayList` ([08-collections/06_arraylist.md](../08-collections/06_arraylist.md)).

## Iterating

```java
for (int i = 0; i < nums.length; i++) {
    System.out.println(i + ": " + nums[i]);
}

for (int n : nums) {              // when no index is needed
    System.out.println(n);
}
```

## Arrays are references

```java
int[] a = {1, 2, 3};
int[] b = a;                   // same array, not a copy
b[0] = 99;
System.out.println(a[0]);      // 99
```

Passing an array to a method also passes the reference, so the method **can modify the caller's array** ([02-methods/01_pass-by-value.md](../02-methods/01_pass-by-value.md)).

### `==` and `equals` do not compare contents

```java
int[] x = {1, 2}, y = {1, 2};
x == y;                        // false (different objects)
x.equals(y);                   // false (arrays inherit Object.equals)
Arrays.equals(x, y);           // true
```

## Copying

```java
int[] src = {1, 2, 3, 4, 5};

int[] c1 = src.clone();                          // shallow copy
int[] c2 = Arrays.copyOf(src, src.length);       // copy (pad or truncate to new length)
int[] c3 = Arrays.copyOf(src, 8);                // [1,2,3,4,5,0,0,0]
int[] c4 = Arrays.copyOfRange(src, 1, 4);        // [2,3,4]  (end is exclusive)
System.arraycopy(src, 0, dest, 0, src.length);   // fastest, into an existing array
```

All of these are **shallow**: for arrays of objects (or arrays of arrays) the elements are shared. See [04-oop/15_clone-and-copying.md](../04-oop/15_clone-and-copying.md).

## Two-dimensional arrays

An array of arrays.

```java
int[][] grid = new int[3][4];            // 3 rows, 4 columns, all 0
int[][] m = {
    {1, 2, 3},
    {4, 5, 6}
};

m[1][2];                                 // 6 (row 1, column 2)
m.length;                                // 2 (rows)
m[0].length;                             // 3 (columns in row 0)

for (int r = 0; r < m.length; r++) {
    for (int c = 0; c < m[r].length; c++) {
        System.out.print(m[r][c] + " ");
    }
    System.out.println();
}
```

### Jagged arrays

Rows may have different lengths.

```java
int[][] tri = new int[3][];              // rows not yet created
for (int i = 0; i < tri.length; i++) {
    tri[i] = new int[i + 1];             // lengths 1, 2, 3
}
```

## The `Arrays` utility class (`java.util.Arrays`)

| Method | Purpose | Example |
|--------|---------|---------|
| `toString(a)` | Readable 1D output | `Arrays.toString(a)` → `[1, 2, 3]` |
| `deepToString(a)` | Readable nested output | `[[1, 2], [3, 4]]` |
| `sort(a)` | Sort ascending in place | `Arrays.sort(a)` |
| `sort(a, from, to)` | Sort a range | |
| `fill(a, v)` | Set every element | `Arrays.fill(a, -1)` |
| `equals(a, b)` | Compare contents | |
| `deepEquals(a, b)` | Compare nested arrays | |
| `binarySearch(a, key)` | Search a **sorted** array | Index, or `-(insertion point) - 1` if absent |
| `copyOf`, `copyOfRange` | Copy | |
| `asList(...)` | Fixed-size `List` view | see below |
| `stream(a)` | Stream over elements | `Arrays.stream(a).sum()` |
| `setAll(a, i -> i * i)` | Fill by index function | |

```java
System.out.println(nums);                  // [I@1b6d3586   (useless: type and hash)
System.out.println(Arrays.toString(nums)); // [42, 0, 0, 0, 0]
```

### Sorting

```java
int[] a = {5, 2, 9};
Arrays.sort(a);                            // [2, 5, 9]

Integer[] boxed = {5, 2, 9};
Arrays.sort(boxed, Collections.reverseOrder());   // comparators need object arrays
Arrays.sort(names, String.CASE_INSENSITIVE_ORDER);
```

Primitive arrays use a dual-pivot quicksort; object arrays use a stable merge sort (TimSort). See [08-collections/05_comparable-and-comparator.md](../08-collections/05_comparable-and-comparator.md).

### `Arrays.asList`

```java
Integer[] arr = {1, 2, 3};
List<Integer> view = Arrays.asList(arr);
view.set(0, 99);          // writes through: arr[0] is now 99
view.add(4);              // UnsupportedOperationException: fixed size

int[] prims = {1, 2, 3};
Arrays.asList(prims).size();   // 1 (a List<int[]> with one element)
```

For an independent modifiable list: `new ArrayList<>(Arrays.asList(arr))`. For an immutable one: `List.of(arr)`.

## Arrays of objects and covariance

```java
Person[] people = new Person[3];      // three null references, no Person objects yet
people[0].getName();                  // NullPointerException
people[0] = new Person("Ada");

Object[] objs = new String[1];
objs[0] = 42;                         // compiles; ArrayStoreException at runtime
```

Arrays are covariant and checked at runtime; generics are not ([07-generics/03_type-erasure.md](../07-generics/03_type-erasure.md)).

## Arrays or `ArrayList`?

| Arrays | `ArrayList` |
|--------|-------------|
| Fixed size | Grows automatically |
| Can hold primitives (no boxing) | Objects only (boxing) |
| Fast and compact | Convenient methods (`add`, `remove`, `contains`) |
| Needed for `main(String[] args)`, varargs, low-level code, performance-critical code | Default for general application code |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `i <= a.length` | `ArrayIndexOutOfBoundsException` | `i < a.length` |
| `a.length()` or `s.length` | Compile error | `a.length` for arrays, `s.length()` for strings |
| Printing an array directly | `[I@...` | `Arrays.toString` |
| `b = a` expecting a copy | Both change | `Arrays.copyOf` / `clone` |
| `a == b` / `a.equals(b)` | `false` for equal contents | `Arrays.equals` |
| Forgetting to create each object in an object array | `NullPointerException` | Assign every element |
| `Arrays.binarySearch` on unsorted data | Wrong result | Sort first |
| Shallow copy of a 2D array | Rows are shared | Copy each row |
| `Arrays.asList(int[])` | List of size 1 | Use `Integer[]` or streams |
| Needing a resizable collection | Awkward manual growth | `ArrayList` |

## Key takeaways

- Arrays are fixed-size, zero-indexed objects; `length` is a field
- Elements get default values (`0`, `false`, `null`)
- Variables hold references: assignment shares, `copyOf`/`clone` copies (shallowly)
- Use `Arrays.toString`, `equals`, `sort`, `fill`, `copyOf`; use `ArrayList` when you need resizing

**Next:** [Numeric Precision and Math](./08_numeric-precision-and-math.md)
