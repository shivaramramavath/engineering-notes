# Variables and Data Types

A **variable** is a named storage location with a **type**. The type decides what values it can hold and which operations are allowed. Java is **statically typed**: the type is fixed at compile time.

```java
int age = 30;              // type  name  =  value
String name = "Ada";
double price = 9.99;
boolean active = true;
```

## Two kinds of types

```
Primitive types                         Reference types
────────────────                        ───────────────────────────
int age = 5;                            String s = "hi";
age: [ 5 ]                              s: [ ref ] ───► heap: "hi"
holds the VALUE itself                  holds an ADDRESS of an object (or null)
```

| | Primitive | Reference |
|---|-----------|-----------|
| Examples | `int`, `double`, `boolean`, `char` | `String`, arrays, `List`, your own classes |
| Stores | The value | A reference to an object |
| Can be `null` | No | Yes |
| Has methods | No | Yes |
| Default (as a field) | `0`, `false` | `null` |

## The 8 primitive types

| Type | Size | Range / values | Default | Example |
|------|------|----------------|---------|---------|
| `byte` | 8 bit | -128 to 127 | `0` | `byte b = 100;` |
| `short` | 16 bit | -32,768 to 32,767 | `0` | `short s = 30000;` |
| `int` | 32 bit | about ±2.1 billion (-2³¹ to 2³¹-1) | `0` | `int i = 42;` |
| `long` | 64 bit | about ±9.2 × 10¹⁸ | `0L` | `long l = 5_000_000_000L;` |
| `float` | 32 bit | about 7 decimal digits of precision | `0.0f` | `float f = 1.5f;` |
| `double` | 64 bit | about 15-16 decimal digits | `0.0` | `double d = 1.5;` |
| `char` | 16 bit | `0` to `65535` (a UTF-16 code unit) | `'\u0000'` | `char c = 'A';` |
| `boolean` | JVM-defined | `true` / `false` | `false` | `boolean ok = true;` |

**Defaults:** `int` for whole numbers and `double` for decimals, unless you need something else. `byte` and `short` are mainly for large arrays and binary data.

```java
System.out.println(Integer.MAX_VALUE);   // 2147483647
System.out.println(Integer.MIN_VALUE);   // -2147483648
System.out.println(Long.MAX_VALUE);      // 9223372036854775807
System.out.println(Double.MAX_VALUE);    // 1.7976931348623157E308
```

## Literals

A **literal** is a value written directly in code.

| Kind | Examples | Note |
|------|----------|------|
| Integer (`int`) | `42`, `-7` | Default type for whole numbers |
| Long | `42L` | Needed above `int` range: `3000000000L` |
| Hex / binary / octal | `0xFF`, `0b1010`, `017` | `017` is **octal** (15), a classic trap |
| Underscores | `1_000_000`, `0xFF_EC` | Readability only |
| Double | `3.14`, `1e3`, `2.5E-4` | Default type for decimals |
| Float | `3.14f` | The `f` suffix is required |
| Char | `'A'`, `'\n'`, `'\u0041'` | Single quotes, one UTF-16 unit |
| String | `"hello"` | Double quotes; see [03-strings-and-text](../03-strings-and-text/README.md) |
| Boolean | `true`, `false` | Lowercase |
| Null | `null` | Reference types only |

Common escapes: `\n` newline, `\t` tab, `\\` backslash, `\"` quote, `\'` apostrophe.

```java
long big = 3000000000;     // ERROR: integer number too large
long ok  = 3000000000L;    // fine
float f  = 3.14;           // ERROR: double to float
float g  = 3.14f;          // fine
int mask = 0b1010_1010;
```

## Declaring and initializing

```java
int a;                  // declaration
a = 5;                  // assignment
int b = 7;              // declaration + initialization
int x = 1, y = 2, z;    // several in one line (avoid mixing styles)

final double TAX_RATE = 0.18;   // constant: assigned once
// TAX_RATE = 0.2;              // ERROR
```

### Local variables must be initialized before use

```java
int count;
System.out.println(count);   // ERROR: variable count might not have been initialized

int total;
if (flag) total = 10;
System.out.println(total);   // ERROR: not assigned on every path
```

**Fields** (declared in a class, outside methods) receive default values automatically. **Local variables** do not.

## `var` (type inference, Java 10+)

```java
var name = "Ada";                    // inferred as String
var list = new ArrayList<String>();  // inferred as ArrayList<String>
```

The type is still fixed at compile time. `var` works only for **local variables with an initializer**, not for fields, parameters or `var x;`. See [12-modern-java/05_var-and-type-inference.md](../12-modern-java/05_var-and-type-inference.md).

## Scope and lifetime

A variable exists from its declaration to the end of its **block**.

```java
void demo() {
    int a = 1;                     // visible in the whole method after this line
    for (int i = 0; i < 3; i++) {  // i only exists inside the loop
        int sq = i * i;            // sq only exists inside the loop body
    }
    // System.out.println(i);      // ERROR: cannot find symbol
    // int a = 2;                  // ERROR: already defined in this scope
}
```

| Kind | Declared | Lifetime | Default value |
|------|----------|----------|---------------|
| Local | In a method or block | Until the block ends | None (must assign) |
| Parameter | In a method signature | Until the method ends | Caller's value |
| Instance field | In a class, non-`static` | As long as the object | Yes |
| Static field | In a class, `static` | As long as the class is loaded | Yes |

## Strings are references, not primitives

```java
String s = "Java";
int n = s.length();      // methods on references
String t = null;
t.length();              // NullPointerException
```

`String` has special language support (literals, `+`) but is an ordinary class. Details in [03-strings-and-text](../03-strings-and-text/README.md).

## Constants

```java
public static final int MAX_USERS = 100;   // class-level constant, UPPER_SNAKE_CASE
```

`final` on a **reference** means the reference cannot change, not that the object is immutable ([04-oop/03_final.md](../04-oop/03_final.md)).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `long x = 3000000000;` | `integer number too large` | Add `L` |
| `float f = 1.5;` | `possible lossy conversion` | Use `1.5f` or `double` |
| `int x = 010;` | Value is 8 | A leading `0` means octal; drop it |
| Using a local before assigning | `might not have been initialized` | Initialize it |
| Redeclaring a name in the same scope | `already defined` | Rename or reuse |
| `char c = "A";` | `incompatible types` | `'A'` for char, `"A"` for String |
| Using `double` for money | `0.1 + 0.2` surprises | `BigDecimal` ([precision](./08_numeric-precision-and-math.md)) |
| Overflowing `int` silently | Negative results | Use `long` or `Math.addExact` |
| Calling a method on `null` | `NullPointerException` | Check for `null` or avoid it |

## Key takeaways

- 8 primitives hold values; everything else is a reference (or `null`)
- Use `int`, `long`, `double`, `boolean`, `char` by default; suffixes `L` and `f` are required for `long` and `float` literals
- Local variables must be initialized; fields get defaults
- Scope is the enclosing block; `final` makes a variable assign-once
- `var` infers a local type; it does not make Java dynamically typed

**Next:** [Operators](./02_operators.md)
