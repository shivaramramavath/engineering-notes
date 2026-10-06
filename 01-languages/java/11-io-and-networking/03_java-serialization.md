# Java Serialization

Java serialization is the built-in mechanism for turning an **object graph into bytes** (`ObjectOutputStream`) and rebuilding it later (`ObjectInputStream`). It's easy to start with, since you add `implements Serializable` and you're done, and that ease is exactly why it's dangerous: it is fragile across versions, slow, bypasses constructors, and is a well-known **remote code execution** vector when used on untrusted data.

Learn it because you will meet it in legacy code, caches, session stores, RMI, and interview questions. For new designs, prefer a data format such as JSON or Protobuf (section 8).

**Prerequisites:** [I/O Streams, Readers and Writers](00_io-streams-readers-writers.md), [Initialization Order](../04-oop/05_initialization-order.md).

---

## 1. Basic usage

```java
public class User implements Serializable {
    @Serial private static final long serialVersionUID = 1L;

    private final String name;
    private int loginCount;
    private transient String sessionToken;     // never written

    public User(String name) { this.name = name; }
    // getters...
}
```

```java
// Write
try (ObjectOutputStream out = new ObjectOutputStream(new BufferedOutputStream(Files.newOutputStream(file)))) {
    out.writeObject(new User("asha"));
}

// Read
try (ObjectInputStream in = new ObjectInputStream(new BufferedInputStream(Files.newInputStream(file)))) {
    User u = (User) in.readObject();          // throws ClassNotFoundException (checked)
}
```

`Serializable` is a **marker interface** (no methods). `@Serial` (Java 14+) is an optional annotation that lets the compiler check the signatures of the special serialization members.

---

## 2. What gets serialized

```text
User object ──► class descriptor (name, serialVersionUID, field list)
            ──► non-static, non-transient field values
            ──► referenced objects, recursively (the whole object graph)
```

| Member | Serialized? |
|---|---|
| Instance fields | Yes |
| `transient` fields | No: come back as `null` / `0` / `false` |
| `static` fields | No (they belong to the class, not the object) |
| Fields of a non-serializable **superclass** | No (see section 3) |
| Methods, constructors | No |

Rules that bite:

- Every object reachable through a non-transient field must be `Serializable`, or writing fails with `NotSerializableException` naming the class.
- **Shared references and cycles are preserved.** If two fields point to the same object, they still point to one object after reading. Within a single stream, writing the same object twice writes a back-reference the second time, so changes made between writes are not re-sent (call `out.reset()` to clear that cache).
- A non-static **inner class** has a hidden reference to its outer instance. Serializing an inner object drags the outer object along (and fails if it isn't serializable). Anonymous classes and lambdas have the same problem. Use `static` nested classes.
- Collections (`ArrayList`, `HashMap`, ...) are serializable if their contents are.

---

## 3. Constructors are bypassed

Deserialization **does not call the constructor of the class being restored**, or of any serializable ancestor. The object is created by the runtime and its fields are filled from the stream.

- Field initializers and constructor validation of serializable classes do **not** run.
- The no-arg constructor of the **first non-serializable superclass** (usually `Object`) *is* called; it must be accessible, otherwise you get `InvalidClassException: no valid constructor`.
- A `transient` field has its default value (`null`, `0`), **not** the value your constructor or initializer would have set.

```java
class Base { int x = 5; }                           // not Serializable
class Child extends Base implements Serializable { int y; }

// After deserializing a Child: y comes from the stream; x is re-initialized by Base's constructor, not from the stream.
```

This means serialization can create objects that **violate your class invariants**. Treat deserialization as a second, hidden constructor (section 5).

---

## 4. `serialVersionUID`

Every serializable class has a version identifier. When reading, the stream's value must match the loaded class's value, or you get:

```text
java.io.InvalidClassException: com.example.User; local class incompatible:
stream classdesc serialVersionUID = 1, local class serialVersionUID = 2
```

- If you **don't declare it**, the JVM computes one from the class's structure (name, fields, methods, modifiers...). Almost any edit, even adding a method, changes it, and compilers can compute it differently. Old data then becomes unreadable for no good reason.
- **Always declare it**: `private static final long serialVersionUID = 1L;`
- The JDK tool `serialver` prints the computed value for an existing class, which is useful when you must read data written before you added an explicit one.
- Bump the number deliberately to say "old data is no longer compatible".

### Evolving a class

| Change | Effect on old data |
|---|---|
| Add a field (same UID) | Generally compatible: the new field gets its default value |
| Remove a field, change a field's type, change the class hierarchy | Unsafe/incompatible, so plan a migration |
| Change the UID | Old data rejected |

Serialization gives no schema tooling, which is why long-lived data belongs in a format designed for evolution (section 8).

---

## 5. Customizing: hooks you can write

### `writeObject` / `readObject`

Private methods with these exact signatures are called by the runtime:

```java
public final class Range implements Serializable {
    @Serial private static final long serialVersionUID = 1L;
    private final int lo, hi;

    public Range(int lo, int hi) {
        if (lo > hi) throw new IllegalArgumentException("lo > hi");
        this.lo = lo; this.hi = hi;
    }

    @Serial
    private void readObject(ObjectInputStream in) throws IOException, ClassNotFoundException {
        in.defaultReadObject();                       // restores the normal fields
        if (lo > hi) throw new InvalidObjectException("lo > hi");   // re-check the invariant
    }
}
```

`writeObject` is the mirror (call `out.defaultWriteObject()` first, then write extra data). Use these hooks for:

- **validating** restored state (the constructor never ran),
- **defensive-copying** mutable fields (an attacker's stream may share references),
- storing a derived/compact form,
- handling `transient` data that must be rebuilt.

### `readResolve` / `writeReplace`

```java
@Serial private Object readResolve() { return INSTANCE; }   // replace the deserialized object
```

A classic singleton that implements `Serializable` is **broken** without `readResolve`: each deserialization creates a second instance. An `enum` singleton avoids the problem entirely, because enums serialize by name and always resolve to the existing constant ([Enums](../04-oop/11_enums.md), [Singleton](../24-design-patterns/01-creational/00_singleton.md)).

### Serialization proxy

The sturdiest pattern for classes with invariants: serialize a small, simple stand-in and rebuild the real object through the **public constructor**.

```java
public final class Range implements Serializable {
    private final int lo, hi;
    public Range(int lo, int hi) { /* validates */ this.lo = lo; this.hi = hi; }

    @Serial private Object writeReplace() { return new Proxy(lo, hi); }

    @Serial private void readObject(ObjectInputStream in) throws InvalidObjectException {
        throw new InvalidObjectException("Use the proxy");     // blocks forged streams of Range itself
    }

    private record Proxy(int lo, int hi) implements Serializable {
        @Serial private Object readResolve() { return new Range(lo, hi); }   // goes through validation
    }
}
```

### `Externalizable`

`Externalizable` hands you full control (`writeExternal` / `readExternal`) but requires a **public no-arg constructor** and you write every field yourself. It's rarely worth it.

### Records

`record`s can implement `Serializable` ([Records](../12-modern-java/01_records.md)), and they behave better than ordinary classes:

- Only the **components** are serialized.
- Deserialization goes through the **canonical constructor**, so your validation runs.
- `serialVersionUID` defaults to `0L` and isn't required to match between versions.
- Custom `writeObject`/`readObject` are ignored (`writeReplace`/`readResolve` still apply).

---

## 6. Security: never deserialize untrusted data

`ObjectInputStream.readObject()` instantiates classes **chosen by the byte stream**, and runs their `readObject` logic, *before* your cast `(User)` executes. An attacker who controls the bytes can instantiate "gadget" classes already present on your classpath (JDK or third-party libraries) and chain their behavior into arbitrary code execution, file access, or denial of service. This has been behind many real-world critical vulnerabilities. Full treatment: [Insecure Deserialization](../21-security/04_insecure-deserialization.md).

Rules:

1. **Don't use Java deserialization for data from outside your trust boundary** (network, user uploads, cookies, message queues you don't fully control).
2. If you must read such data, use a **filter** (`ObjectInputFilter`, Java 9+), as an allow-list with limits:

```java
ObjectInputFilter filter = ObjectInputFilter.Config.createFilter(
    "maxbytes=1048576;maxdepth=20;com.example.model.*;java.lang.*;java.util.*;!*");   // allow listed, reject the rest

try (ObjectInputStream in = new ObjectInputStream(source)) {
    in.setObjectInputFilter(filter);
    Object o = in.readObject();
}
```

The patterns are evaluated in order, and the final `!*` rejects everything not listed. You can also set a JVM-wide filter with `-Djdk.serialFilter=...`. Java 17 added filter factories for per-stream policies.

3. A filter reduces risk but is not a substitute for choosing a data-only format.
4. Never trust a `readObject` on a class just because it's yours: validate its fields.

---

## 7. Practical usage today

Where Java serialization still appears:

- legacy **RMI** and some **session replication** / distributed-cache setups,
- old persistence layers and "save game"-style files,
- interview questions.

When it's used, keep it inside a trust boundary, declare `serialVersionUID`, keep serialized classes small and stable, and mark secrets `transient`.

Don't use it for **deep copying** (slow and brittle, so write a copy constructor or use records), for **long-term storage**, or for **inter-service communication**.

---

## 8. Alternatives

| Need | Prefer |
|---|---|
| Human-readable, web APIs, config | JSON ([Jackson](../17-json-and-data-formats/01_jackson.md), [Gson](../17-json-and-data-formats/02_gson.md)) |
| Compact, schema-evolution-friendly, cross-language | Protobuf, Avro |
| Document-oriented | XML (JAXB-style binding libraries) |
| Copying objects | Copy constructors, `record` + `with`-style methods ([clone and copying](../04-oop/15_clone-and-copying.md)) |

Compare them in [Data Formats Comparison](../17-json-and-data-formats/04_data-formats-comparison.md). Data-only formats don't let the input choose which classes to run, and they handle schema changes explicitly, which are two big reasons to move away from Java serialization.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| No explicit `serialVersionUID` | Declare it |
| Assuming the constructor runs on deserialization | Validate in `readObject` or use the proxy pattern |
| Serializing a class holding non-serializable fields | Make the field `transient`/serializable, or redesign |
| Serializing an inner/anonymous class (drags the outer object) | Use a `static` nested class |
| Serializable singleton without `readResolve` | Use an enum singleton |
| Storing passwords/tokens in serialized fields | `transient` and re-obtain after restore |
| `readObject` from network/user data | Don't, or apply `ObjectInputFilter` with an allow-list |
| Expecting `transient` fields to keep initializer values | They're default values after reading; restore in `readObject` |
| Using serialization for deep copy / long-term storage | Copy constructor; a schema'd format |

### Debugging

- `NotSerializableException: com.x.Y` → that class (or something it references) isn't `Serializable`. For the full object path, run with `-Dsun.io.serialization.extendedDebugInfo=true`.
- `InvalidClassException ... serialVersionUID` → class changed since the data was written. Check the UIDs (`serialver`) and your compatibility plan.
- `InvalidClassException: no valid constructor` → a non-serializable superclass lacks an accessible no-arg constructor.
- `StreamCorruptedException: invalid stream header` → the bytes weren't produced by `ObjectOutputStream` (or are encoded/compressed differently, or are being read from the wrong place).
- `ClassNotFoundException` → the class isn't on the reader's classpath.
- `ClassCastException` after a successful read → a different type was written than expected.

---

## Quick Summary

- `Serializable` + `ObjectOutputStream`/`ObjectInputStream` converts an object graph to bytes and back. `transient` and `static` fields are skipped.
- **Constructors don't run** on deserialization: add validation in `readObject`, or use `writeReplace` + a serialization proxy. Records re-run their canonical constructor.
- Always declare `serialVersionUID`. Evolving classes safely is hard, which is a reason to avoid long-lived serialized data.
- Serializable singletons need `readResolve` (or use an enum).
- **Never deserialize untrusted data.** If unavoidable, use an `ObjectInputFilter` allow-list.
- For new work, use JSON, Protobuf, or Avro instead.

**Next:** [HTTP Client and Networking](04_http-client-and-networking.md)
