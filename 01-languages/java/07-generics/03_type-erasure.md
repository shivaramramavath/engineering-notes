# Type Erasure

Generics exist **only at compile time**. After the compiler has checked the types, it **erases** the type arguments and emits ordinary bytecode that uses `Object` (or the type's bound) plus inserted casts. This was done for **backward compatibility**: generic code (Java 5) had to interoperate with the billions of lines of pre-generics code and the same JVM.

```java
List<String> names = new ArrayList<>();
names.add("Ada");
String s = names.get(0);
```

After compilation (conceptually):

```java
List names = new ArrayList();
names.add("Ada");
String s = (String) names.get(0);      // the compiler inserted this cast
```

At runtime there is **no** `List<String>`, only `List`.

## What the compiler does

| Step | Effect |
|------|--------|
| 1. Type-check using the generic signatures | Reports errors such as `incompatible types: Integer cannot be converted to String` |
| 2. **Erase** type parameters | `T` becomes its **bound** (first bound, or `Object` if unbounded); type arguments disappear |
| 3. Insert **casts** where needed | Where generic values are read |
| 4. Generate **bridge methods** | To keep overriding and polymorphism working (below) |

```java
class Box<T> { T value; T get() { return value; } }                 // erased: Object value; Object get()
class NumBox<T extends Number> { T value; }                         // erased: Number value;
static <T extends Comparable<T>> T max(T a, T b) { ... }           // erased: Comparable max(Comparable, Comparable)
```

## Consequences: what you cannot do

### 1. No runtime type checks on parameterized types

```java
List<String> a = new ArrayList<>();
List<Integer> b = new ArrayList<>();
a.getClass() == b.getClass();               // true: both are just ArrayList

if (obj instanceof List<String>) { }        // ERROR: illegal generic type for instanceof
if (obj instanceof List<?>) { }             // OK: only the raw/wildcard type can be checked
if (obj instanceof List<String> list) { }   // ERROR (for an unrelated Object); OK only if provably safe, e.g. obj is a Collection<String>
```

A cast to a parameterized type is **unchecked**:

```java
List<String> list = (List<String>) object;  // warning: unchecked cast; no runtime check of the element type
```

### 2. No `new T()`, no `new T[n]`

```java
class Factory<T> {
    T create() { return new T(); }                    // ERROR: T is erased: the JVM does not know which class
    T[] makeArray(int n) { return new T[n]; }         // ERROR: generic array creation
}
```

Workarounds: [below](#workarounds).

### 3. No generic array creation

```java
List<String>[] arr = new List<String>[10];            // ERROR: generic array creation
List<?>[] ok = new List<?>[10];                       // OK (unbounded wildcard is reifiable)
List<String>[] unsafe = (List<String>[]) new List[10];   // compiles with an unchecked warning
```

Arrays check their element type at runtime, generics do not; mixing them could break type safety. Prefer `List<List<String>>`.

### 4. Overloads that differ only by type arguments clash

```java
void process(List<String> list) { }
void process(List<Integer> list) { }      // ERROR: name clash: same erasure (process(List))
```

Use different method names, or different parameter shapes.

### 5. No generic exceptions, no `T` in `catch`

```java
class MyEx<T> extends Exception { }       // ERROR: a generic class may not extend Throwable
```

### 6. No static fields of type `T`

```java
class Box<T> { static T shared; }         // ERROR: all Box<...> share one class, so what would T be?
```

### 7. Primitives are not allowed as type arguments

`List<int>` is illegal because erasure leaves `Object`, and a primitive is not an `Object`. Use `List<Integer>` (boxing: [wrappers](../01-fundamentals/04_wrapper-classes-and-autoboxing.md)).

## Reifiable vs non-reifiable types

A type is **reifiable** if its full type information exists at runtime.

| Reifiable (available at runtime) | Not reifiable (erased) |
|----------------------------------|------------------------|
| Primitives (`int`) | `List<String>` |
| Non-generic types (`String`) | `Map<String, Integer>` |
| Raw types (`List`) | Type variables (`T`) |
| Unbounded wildcards (`List<?>`) | `List<? extends Number>` |
| Arrays of reifiable types (`int[]`, `List<?>[]`) | `List<String>[]` |

Only reifiable types can be used with `instanceof`, array creation (`new X[n]`) and exact runtime checks.

## Heap pollution

**Heap pollution** happens when a variable of a parameterized type refers to an object that is **not** of that type. Because generics are erased, the JVM cannot catch it where it happens: the failure appears later, **at a cast the compiler inserted**.

```java
List<String> strings = new ArrayList<>();
List raw = strings;                      // raw type: warning
raw.add(42);                             // unchecked call: allowed at runtime! (it is just an ArrayList)

String s = strings.get(0);               // ClassCastException: Integer cannot be cast to String
                                         // ...thrown HERE, far from the actual mistake
```

```java
// Another source: unsafe varargs or unchecked casts
static <T> T[] toArray(List<T> list) {
    return (T[]) list.toArray();         // unchecked: actually an Object[]; fails when the caller assigns it to String[]
}
```

The compiler warns you with **unchecked warnings** (`unchecked cast`, `unchecked call`, `unchecked generic array creation for varargs`). Treat each as a potential `ClassCastException` waiting for a different line of code.

### `@SuppressWarnings("unchecked")`

Use it **only** when you can prove the operation is safe, on the **smallest scope**, with a comment:

```java
@SuppressWarnings("unchecked")
public E get(int index) {
    return (E) elements[index];          // elements is Object[]; we only store E values, so the cast is safe
}
```

Typical safe uses: internal arrays inside a generic collection (`ArrayList` does exactly this), and type-token casts after a runtime check.

### `@SafeVarargs`

Generic varargs create an array of a non-reifiable type (`T...`), which can pollute the heap:

```java
@SafeVarargs
static <T> List<T> listOf(T... items) {      // safe: we do not store or expose the array
    return new ArrayList<>(Arrays.asList(items));
}
```

Allowed on `static`, `final` and `private` methods and constructors. Use it only when the method neither stores into the array nor lets it escape ([02-methods/02_overloading-and-varargs.md](../02-methods/02_overloading-and-varargs.md)).

## Bridge methods

Erasure can break overriding. The compiler generates **synthetic bridge methods** to preserve polymorphism:

```java
class Name implements Comparable<Name> {
    public int compareTo(Name other) { ... }          // what you wrote
}
```

Erased interface: `int compareTo(Object o)`. The compiler adds:

```java
public int compareTo(Object o) {                      // bridge (synthetic)
    return compareTo((Name) o);                       // casts, then calls yours
}
```

You normally never see this, but it explains odd stack traces (`compareTo(Object)` frames) and why `ClassCastException`s can appear in generated code. Inspect with:

```bash
javap -p -c Name.class
```

## What is **not** erased

Generic information is kept in the class file's **signature metadata** and available through **reflection** for *declarations* (fields, method signatures, superclasses):

```java
class StringList extends ArrayList<String> { }
Type superType = StringList.class.getGenericSuperclass();     // ArrayList<java.lang.String>
ParameterizedType p = (ParameterizedType) superType;
p.getActualTypeArguments()[0];                                 // class java.lang.String
```

Only the types of **values** are erased: you cannot ask an `ArrayList<String>` instance what its `E` is, but you can ask a subclass declaration or a field declaration (`Field.getGenericType()`). Libraries exploit this with **type tokens** ([below](#workarounds), [04](./04_generic-patterns.md)).

## Workarounds

### Need a `Class` at runtime: pass a `Class<T>`

```java
public static <T> T parse(String json, Class<T> type) { ... }       // Jackson / Gson style
User u = parse(text, User.class);

Object o = type.cast(value);                  // checked cast at runtime
boolean ok = type.isInstance(value);
```

### Need to create `T`: pass a `Supplier<T>` or `Class<T>`

```java
static <T> List<T> create(int n, Supplier<T> factory) {
    List<T> list = new ArrayList<>();
    for (int i = 0; i < n; i++) list.add(factory.get());
    return list;
}
create(3, StringBuilder::new);

T instance = type.getDeclaredConstructor().newInstance();    // via reflection (checked exceptions)
```

### Need an array of `T`: use `Array.newInstance`, a generator function, or avoid arrays

```java
@SuppressWarnings("unchecked")
T[] array = (T[]) Array.newInstance(type, n);            // type is a Class<T>

String[] names = list.toArray(new String[0]);            // classic
String[] names2 = list.toArray(String[]::new);           // IntFunction<T[]> generator (Java 11+ on collections, 8+ on streams)
```

Inside a generic collection, an `Object[]` with a cast on read is the standard approach (see `@SuppressWarnings` above).

### Need the full generic type: super type tokens

```java
Type listOfUsers = new TypeReference<List<User>>() { }.getType();     // Jackson's TypeReference; Gson has TypeToken
List<User> users = mapper.readValue(json, new TypeReference<List<User>>() { });
```

The anonymous subclass **records** `List<User>` in its superclass signature, which reflection can read. See [04_generic-patterns.md](./04_generic-patterns.md) and [17-json-and-data-formats/01_jackson.md](../17-json-and-data-formats/01_jackson.md).

## Why Java chose erasure

| Reason | Detail |
|--------|--------|
| **Migration compatibility** | Old non-generic libraries and new generic code interoperate; `List` and `List<String>` are the same class at runtime |
| **No JVM change** | The JVM and class file format needed no support for parameterized types |
| **Smaller footprint** | One class `ArrayList`, not one per type (unlike C++ templates) |

The cost: no runtime type arguments, no generics over primitives, and the restrictions above. Project Valhalla (value classes and specialization) aims to improve primitives with generics in the future.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `obj instanceof List<String>` | Compile error | `instanceof List<?>`, then check elements |
| `new T()` / `new T[n]` | Compile error | `Supplier<T>`, `Class<T>`, `Array.newInstance`, `List<T>` |
| Overloads `m(List<String>)` and `m(List<Integer>)` | `name clash: same erasure` | Rename |
| Ignoring unchecked warnings | Surprise `ClassCastException` later | Fix the cause, or suppress with justification |
| `@SuppressWarnings("unchecked")` on a whole class | Hides real problems | Narrowest possible scope |
| Passing a raw `List` to a generic method | Heap pollution | Use parameterized types |
| Expecting `list.getClass()` to reveal `<String>` | Just `ArrayList` | Pass a `Class<T>` or use a type token |
| Returning `(T[]) new Object[n]` from a public method | `ClassCastException` at the caller | Return `List<T>`, or create via `Array.newInstance` |
| `@SafeVarargs` on a method that stores the varargs array | Heap pollution | Remove it |
| Assuming erasure means "no runtime cost" | Casts and boxing still happen | Know that primitives are boxed |

## Key takeaways

- Generics are a **compile-time** feature; at runtime type arguments are erased, leaving bounds or `Object` plus compiler-inserted casts
- That is why `instanceof List<String>`, `new T()`, `new T[n]`, generic exceptions and same-erasure overloads are illegal
- Unchecked warnings signal possible **heap pollution**; the failure shows up later as a `ClassCastException`
- Pass `Class<T>`, `Supplier<T>` or a type token when you need type information at runtime
- Generic information on **declarations** survives in metadata and is what libraries like Jackson read through reflection

**Next:** [Generic Patterns](./04_generic-patterns.md)
