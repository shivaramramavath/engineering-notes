# Pass-by-Value

**Java is always pass-by-value.** When you call a method, every argument is **copied** into the parameter. For primitives the copy is the value. For objects the copy is the **reference** (the address), not the object.

This is one of the most asked Java interview questions, and the answer is one sentence: *"Java passes a copy of the value; for objects, that value is a copy of the reference."*

```
caller                         method
  x = 5      ──copy──►        n = 5          changing n does not change x
  p ──► [Person]                             both p and q point to the SAME object
  p = ref ──copy──►  q = ref ──► [Person]    changing the object is visible to both;
                                             reassigning q does not change p
```

## Primitives: the value is copied

```java
static void increment(int n) {
    n = n + 1;                    // changes only the local copy
}

int x = 5;
increment(x);
System.out.println(x);            // 5
```

```
before call        inside increment        after return
x: [5]             x: [5]   n: [5]         x: [5]
                   n changes → [6]         n is gone
```

## Objects: the reference is copied

```java
class Person {
    String name;
    Person(String name) { this.name = name; }
}

static void rename(Person p) {
    p.name = "Grace";             // follows the reference: modifies the shared object
}

static void replace(Person p) {
    p = new Person("Linus");      // changes only the local copy of the reference
}

Person a = new Person("Ada");
rename(a);
System.out.println(a.name);       // Grace   (the object was modified)

replace(a);
System.out.println(a.name);       // Grace   (the caller's reference is unchanged)
```

```
rename(a):                           replace(a):

a ──► [Person "Ada"]                 a ──► [Person "Grace"]
p ──┘  p.name = "Grace"  → visible   p ──► [Person "Linus"]   ← p re-pointed
                                     a still points at "Grace"
```

Two separate actions, with different results:

| Action inside the method | Visible to the caller? |
|--------------------------|------------------------|
| Mutate the object (`p.name = ...`, `list.add(...)`, `arr[0] = ...`) | **Yes**: same object |
| Reassign the parameter (`p = new Person()`) | **No**: only the local copy changes |

## Why "pass-by-reference" is wrong

In true pass-by-reference (C++ `int&`, C# `ref`), the parameter is an **alias for the caller's variable**, so reassigning it changes the caller's variable. Java cannot do that. The classic proof is that you cannot write a working `swap`:

```java
static void swap(int a, int b) {
    int t = a; a = b; b = t;
}
int x = 1, y = 2;
swap(x, y);
System.out.println(x + " " + y);       // 1 2  (not swapped)

static void swapPersons(Person p, Person q) {
    Person t = p; p = q; q = t;
}
// also does nothing to the caller's variables
```

Working alternatives:

```java
// 1. Swap array elements (mutate shared state)
static void swap(int[] a, int i, int j) {
    int t = a[i]; a[i] = a[j]; a[j] = t;
}

// 2. Return the new values
record Pair(int first, int second) {}
static Pair swapped(int a, int b) { return new Pair(b, a); }
```

## Arrays and collections

Arrays and collections are objects, so methods **can modify the caller's contents**.

```java
static void zeroFirst(int[] arr) {
    arr[0] = 0;                    // visible to the caller
}

static void reset(int[] arr) {
    arr = new int[]{0, 0, 0};      // NOT visible
}

static void addItem(List<String> list) {
    list.add("x");                 // visible
}
```

## `String` and wrappers: immutable, so changes look like "no effect"

```java
static void shout(String s) {
    s = s.toUpperCase();           // creates a new String; local s points at it
}
String name = "ada";
shout(name);
System.out.println(name);          // ada

static void inc(Integer n) {
    n++;                           // n = Integer.valueOf(n + 1): a NEW object
}
Integer count = 5;
inc(count);                        // count is still 5
```

Immutable objects cannot be modified through a shared reference, so they behave like values. By contrast, `StringBuilder` is mutable:

```java
static void exclaim(StringBuilder sb) {
    sb.append("!");                // visible to the caller
}
```

## `final` parameters

```java
static void f(final Person p) {
    // p = new Person("x");        // ERROR: cannot reassign
    p.name = "changed";            // still allowed: the object is not immutable
}
```

`final` prevents reassigning the **parameter**, not modifying the **object**. Real protection needs an immutable type or a defensive copy.

## Protecting your data: defensive copies

Because callers and callees share objects, mutable data crosses boundaries easily.

```java
class Schedule {
    private final List<String> days;

    Schedule(List<String> days) {
        this.days = new ArrayList<>(days);        // copy in: caller's later changes do not leak in
    }

    List<String> getDays() {
        return List.copyOf(days);                 // copy out: caller cannot mutate our list
    }
}
```

See [23-design-and-clean-code/04_immutability.md](../23-design-and-clean-code/04_immutability.md) and [08-collections/13_immutable-and-unmodifiable-collections.md](../08-collections/13_immutable-and-unmodifiable-collections.md).

## Predict the output

```java
static void m1(int[] a) { a[0] = 9; }
static void m2(int[] a) { a = new int[]{8}; a[0] = 7; }
static void m3(StringBuilder sb) { sb = new StringBuilder("new"); sb.append("!"); }
static void m4(StringBuilder sb) { sb.append("!"); }

int[] arr = {1};
m1(arr);  System.out.println(arr[0]);   // ?
m2(arr);  System.out.println(arr[0]);   // ?

StringBuilder s = new StringBuilder("old");
m3(s);    System.out.println(s);        // ?
m4(s);    System.out.println(s);        // ?
```

Answers: `9`, `9` (m2 only changed its local reference), `old`, `old!`.

## Terminology

Some authors call this **"pass-by-sharing"** or **"call by object sharing"**. The mechanics are identical to pass-by-value of a reference. Whatever the name, the model above predicts all behavior.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Writing `swap(a, b)` for two variables | Nothing changes | Swap array elements or return both values |
| Assigning to a parameter to "return" a result | Caller sees no change | Return the value |
| Expecting `Integer`/`String` parameters to be modifiable | No effect | They are immutable; return the new value |
| Modifying a collection parameter unintentionally | Callers see surprising changes | Copy first, or document the mutation |
| Storing a caller's mutable object directly in a field | Outside code can alter your internal state | Defensive copy |
| Returning an internal mutable collection | Same problem | Return an unmodifiable copy or view |
| Believing `final` makes the object immutable | Object still changes | Use immutable classes |
| Saying "objects are passed by reference" in an interview | Technically incorrect | "References are passed by value" |

## Key takeaways

- Every argument is copied into the parameter, always
- For objects the copy is the **reference**: both variables point at the same object
- **Mutating** the object is visible to the caller; **reassigning** the parameter is not
- A method cannot change the caller's variables, so return values or mutate shared objects instead
- Protect internal state with defensive copies and immutability

**Next:** [Overloading and Varargs](./02_overloading-and-varargs.md)
