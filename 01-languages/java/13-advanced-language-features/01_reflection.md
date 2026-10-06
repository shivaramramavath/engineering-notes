# Reflection

**Reflection** is the ability of a running program to inspect and manipulate its own structure: discover a class's fields, methods, and constructors; read annotations; create objects and call methods **by name**, without knowing the types at compile time.

Normal code decides *what to call* when you write it. Reflection decides *at runtime*. That is how a JSON library maps `{"name": "Asha"}` onto your `User` class, how JUnit finds methods annotated `@Test`, and how a dependency-injection container builds your objects.

**Prerequisites:** [Annotations](00_annotations.md), [Class Loading](../15-jvm-internals/01_class-loading.md), [Type Erasure](../07-generics/03_type-erasure.md), [Java Modules](../05-packages-and-modules/02_java-modules.md).

---

## 1. The entry point: `Class`

Every type has a `java.lang.Class` object describing it. There are three ways to get one:

```java
Class<String> a = String.class;                       // class literal: compile-time, safest
Class<?> b = "hello".getClass();                      // from an instance: the runtime type
Class<?> c = Class.forName("com.example.User");       // from a name: dynamic; may throw ClassNotFoundException
```

`Class.forName` loads **and initializes** the class (runs static initializers) using the caller's class loader. When the name comes from configuration, that is the "plugin" case ([Class Loading](../15-jvm-internals/01_class-loading.md)).

### Inspecting a class

```java
Class<?> c = ArrayList.class;

c.getName();               // java.util.ArrayList
c.getSimpleName();         // ArrayList
c.getSuperclass();         // class java.util.AbstractList
c.getInterfaces();         // [List, RandomAccess, Cloneable, Serializable]
Modifier.toString(c.getModifiers());   // "public"
c.isInterface(); c.isEnum(); c.isRecord(); c.isSealed(); c.isArray();
c.getPackageName();
```

---

## 2. Fields, methods, constructors

Each kind of member has two lookups, and the difference matters:

| Lookup | Returns |
|---|---|
| `getFields()` / `getMethods()` / `getConstructors()` | **Public** members, **including inherited** ones |
| `getDeclaredFields()` / `getDeclaredMethods()` / `getDeclaredConstructors()` | **All** members (any access level) declared **in this class only**, no inherited members |

```java
public class User {
    private String name;
    public int age;
    public User() {}
    private User(String name) { this.name = name; }
    public String greet(String other) { return "Hi " + other + ", I'm " + name; }
}

Class<User> cls = User.class;

Field nameField = cls.getDeclaredField("name");                  // private: use getDeclared...
Method greet    = cls.getMethod("greet", String.class);          // name + parameter TYPES
Constructor<User> ctor = cls.getDeclaredConstructor(String.class);
```

Lookup methods throw `NoSuchFieldException` / `NoSuchMethodException` when nothing matches. Note that **parameter types must match exactly**: use `int.class` for a primitive `int` parameter, not `Integer.class`.

To walk an inheritance chain, loop with `getSuperclass()`.

---

## 3. Creating objects, getting/setting fields, calling methods

```java
// Create
User u1 = cls.getDeclaredConstructor().newInstance();            // no-arg constructor
ctor.setAccessible(true);                                         // the constructor is private
User u2 = ctor.newInstance("Asha");

// Read / write fields
nameField.setAccessible(true);
String n = (String) nameField.get(u2);                            // "Asha"
nameField.set(u2, "Ravi");
cls.getField("age").setInt(u2, 30);                               // typed variants for primitives

// Call methods
Object result = greet.invoke(u2, "Meera");                        // "Hi Meera, I'm Ravi"
Method staticM = Math.class.getMethod("max", int.class, int.class);
Object max = staticM.invoke(null, 3, 9);                          // static: target is null → 9
```

Important details:

- `invoke` and `get` return `Object`, so primitives are **boxed** and you cast the result.
- **`InvocationTargetException`**: if the invoked method throws, reflection wraps it. The *real* exception is `e.getCause()`. Unwrap it:

```java
try {
    method.invoke(target, args);
} catch (InvocationTargetException e) {
    throw e.getCause();      // rethrow what the method actually threw
}
```

- `Class.newInstance()` is deprecated, since it hid checked exceptions. Use `getDeclaredConstructor().newInstance()`.
- Wrong argument types or counts throw `IllegalArgumentException`. Inaccessible members throw `IllegalAccessException`.

---

## 4. Access control and modules

`private` normally means "only this class". Reflection can ask to bypass that:

```java
field.setAccessible(true);                 // suppress access checks (if permitted)
if (field.trySetAccessible()) { ... }      // returns false instead of throwing
```

**Whether that succeeds depends on the module system** ([Java Modules](../05-packages-and-modules/02_java-modules.md)):

- Your own code on the classpath, or in the same module: fine.
- Another **named module** must *open* the package to you (`opens com.example to your.module;`), otherwise `setAccessible` throws `InaccessibleObjectException`.
- **JDK internals** (`java.*`, `sun.*`): since Java 17 deep reflection into them is blocked. Libraries that relied on it fail with `InaccessibleObjectException: ... module java.base does not "opens java.lang" to unnamed module`. The escape hatch is a command-line flag (`--add-opens java.base/java.lang=ALL-UNNAMED`), and the real fix is upgrading the library.

### `final` fields

Reflection can technically modify some `final` instance fields after `setAccessible(true)`, and `static final` fields can't be changed reliably. It's discouraged because the JIT and other code assume finals never change. Starting with Java 26 the JDK warns by default when it happens (JEP 500), as a step toward disallowing it. Don't depend on it.

Records are special: their fields cannot be modified through reflection at all.

---

## 5. Generics, annotations, parameters, records, enums

```java
// Annotations (RUNTIME retention only): see [Annotations](00_annotations.md)
Test t = method.getAnnotation(Test.class);

// Generic information that survives erasure in declarations
Field f = Holder.class.getDeclaredField("items");                 // List<String> items;
Type type = f.getGenericType();                                    // java.util.List<java.lang.String>
if (type instanceof ParameterizedType pt) {
    Type arg = pt.getActualTypeArguments()[0];                    // class java.lang.String
}

// Records
for (RecordComponent rc : MyRecord.class.getRecordComponents()) {
    rc.getName(); rc.getType(); rc.getAccessor();
}

// Enums
Color[] values = Color.class.getEnumConstants();

// Parameter names are NOT available unless compiled with `javac -parameters`
for (Parameter p : method.getParameters()) { p.isNamePresent(); p.getName(); }
```

Type arguments of *declarations* (fields, method signatures, superclasses) are kept in class files and readable. Type arguments of *runtime objects* are erased: you can't ask a `new ArrayList<String>()` what it contains ([Type Erasure](../07-generics/03_type-erasure.md)). This is the basis of "type token" tricks such as Jackson's `TypeReference`.

---

## 6. A realistic mini-example: object → map

```java
public static Map<String, Object> toMap(Object obj) throws IllegalAccessException {
    Map<String, Object> map = new LinkedHashMap<>();
    for (Field f : obj.getClass().getDeclaredFields()) {
        if (Modifier.isStatic(f.getModifiers()) || f.isSynthetic()) continue;
        f.setAccessible(true);
        map.put(f.getName(), f.get(obj));
    }
    return map;
}
```

This is the heart of many serialization, mapping, and logging utilities. Real libraries add caching, type conversion, naming rules, annotations, and handling of inheritance, which is why you should usually use an existing library instead of hand-rolling.

---

## 7. Cost, alternatives, and when to use it

### Performance

- Reflective calls are slower than direct calls (argument boxing, access checks, no inlining in many cases). Since Java 18, core reflection is implemented on top of method handles, which narrowed the gap, but it still isn't free.
- **Looking things up is the expensive part** (`getDeclaredMethods`, `getMethod`). Do it once and **cache** the `Method`/`Field`/`Constructor` objects. Never look up members inside a hot loop.
- Measure before worrying: benchmark with JMH ([Benchmarking with JMH](../20-performance/01_benchmarking-with-jmh.md)).

### Alternatives

| Alternative | When |
|---|---|
| Plain code, interfaces, generics | **Almost always**: type-safe and fast |
| `MethodHandle` / `VarHandle` (`java.lang.invoke`) | Reflection-like access that can be fully optimized by the JIT when handles are constants |
| Annotation processors / code generation | Avoid runtime reflection altogether (faster startup, native-image friendly) |
| Dynamic proxies | Interface-based interception: [Dynamic Proxies](02_dynamic-proxies.md) |

```java
MethodHandle length = MethodHandles.lookup()
        .findVirtual(String.class, "length", MethodType.methodType(int.class));
int n = (int) length.invokeExact("hello");       // 5: the call's static types must match exactly
```

### When it's the right tool

Frameworks (DI, ORM, serialization, test runners), plugin systems loading classes by name, generic tools (debuggers, object inspectors, mappers). When you write application features, it's usually the wrong tool.

---

## 8. Security and robustness

- Reflection **bypasses compile-time checks and access modifiers**, so errors move from compile time to runtime.
- **Never pass untrusted input to `Class.forName`, `newInstance`, or `Method.invoke`** (class names, method names from a request). An attacker choosing which class to instantiate or method to call is a classic vulnerability class ([Insecure Deserialization](../21-security/04_insecure-deserialization.md) is one instance). Use an allow-list.
- The Security Manager no longer exists (permanently disabled in Java 24), so there is no JVM-level sandbox for reflective calls. Enforce restrictions in your own code.
- Reflective code breaks easily under **refactoring**: renaming a method makes the string `"calculateTotal"` stale without any compiler error.
- Tools that analyze or compile ahead of time (for example GraalVM native image) can't see reflective access unless you provide configuration. This is another reason to prefer code generation in new libraries.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `getMethod` for a private method | `getDeclaredMethod` + `setAccessible(true)` |
| `getDeclaredFields()` expecting inherited fields | Loop through `getSuperclass()` |
| `getMethod("m", Integer.class)` for an `int` parameter | `int.class` |
| Catching the wrong exception from `invoke` | Unwrap `InvocationTargetException.getCause()` |
| Looking up `Method`/`Field` on every call | Cache them |
| Reading annotations that have `CLASS`/`SOURCE` retention | Use `RUNTIME` retention |
| Using reflection where an interface or generics would do | Design the types so you don't need it |
| Hard-coded member names in strings | Constants/tests, or avoid reflection |
| Changing `final` fields | Don't. Use a mutable design or rebuild the object |
| Passing user input to `Class.forName` | Allow-list class names |

### Debugging

- `NoSuchMethodException: com.x.Foo.bar(java.lang.Integer)` → print what you have: loop over `getDeclaredMethods()` and compare names and parameter types (primitive vs boxed is the usual culprit).
- `InaccessibleObjectException` → a module boundary: check `opens`, library versions, or `--add-opens` as a last resort.
- `IllegalAccessException` → you didn't call `setAccessible(true)` (or can't, per the module rules).
- `InvocationTargetException` with no useful message → print `e.getCause()` and its stack trace.
- `ClassNotFoundException` for a class you can see → wrong class loader, or the class isn't on the classpath at runtime. In containers/app servers, check which loader `Class.forName` uses (use the context class loader where appropriate).
- `NoClassDefFoundError` is a different problem: the class was present at compile time and missing (or failed to initialize) at runtime.

---

## Quick Summary

- Reflection lets code inspect and use classes **by name at runtime**: start from a `Class`, then get `Field`, `Method`, `Constructor`, annotations, and generic signatures.
- `getX()` = public (including inherited). `getDeclaredX()` = everything declared in that class. `setAccessible(true)` may be blocked by modules.
- `invoke` boxes values, and **wraps exceptions in `InvocationTargetException`**. Unwrap `getCause()`.
- Cache lookups. Reflection is slower than direct calls, but lookups are the costly part.
- Don't pass untrusted strings into reflective calls, and avoid reflection in ordinary application code.
- Alternatives: plain types, `MethodHandle`, annotation processors, [dynamic proxies](02_dynamic-proxies.md).

**Next:** [Dynamic Proxies](02_dynamic-proxies.md)