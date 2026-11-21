# Type Erasure and Bytecode Mechanics

> Uncovering the compile-time translation of Java generics: type parameter erasure, synthetic bridge methods, runtime reification boundaries, and bytecode verification.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_generic-classes-and-methods.md](00_generic-classes-and-methods.md), [01_bounded-types.md](01_bounded-types.md), [04-oop/07_polymorphism.md](../04-oop/07_polymorphism.md) |
| **Time to Read** | 24 minutes |
| **Status** | Active |
| **Java Version** | Java 5+ |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

When generics were introduced in Java 5, the Java language designers faced a fundamental architectural decision:
1. **Reified Generics** (like C# or C++ templates): Modify the JVM bytecode and class-file format so the runtime preserves type arguments, generating specialized machine code or classes for each type instantiation (`List<int>`, `List<String>`).
2. **Type Erasure**: Retain the existing JVM architecture and bytecode format, enforcing type safety entirely during compilation by checking constraints and erasing type parameters from class files.

To ensure **100% backward compatibility** with billions of lines of legacy pre-Java 5 code and allow legacy libraries to interoperate seamlessly with new generic code, Java chose **Type Erasure**.

Understanding type erasure is essential because almost every limitation and syntactic peculiarity of Java generics—why you cannot create generic arrays, why `instanceof` fails on parameterized types, and why synthetic bridge methods exist—stems directly from erasure.

---

## 2. Core Concepts

### What Gets Erased?
During compilation by `javac`:
1. All type parameters in generic types are replaced with their **upper bounds** (or `java.lang.Object` if unbounded).
2. Type casts (`checkcast`) are inserted into the bytecode where necessary to preserve type safety.
3. **Bridge methods** (`ACC_BRIDGE`) are generated to preserve polymorphism in extended generic classes.
4. Generic signatures are retained only as class-file metadata attributes (`Signature` attribute) for reflection and IDE tooling, but are ignored during normal JVM execution.

### Reifiable vs Non-Reifiable Types
A **reifiable type** is a type whose complete type information is available at runtime:
- Primitives (`int`, `double`)
- Non-generic reference types (`String`, `Thread`)
- Raw types (`List`, `Map`)
- Unbounded wildcards (`List<?>`, `Map<?, ?>`)
- Arrays of reifiable types (`String[]`, `int[]`, `List<?>[]`)

A **non-reifiable type** is a type whose runtime representation has been stripped of information:
- Parameterized types with concrete parameters (`List<String>`, `Map<Integer, User>`)
- Parameterized types with bounds (`List<? extends Number>`)
- Type variables (`T`, `E`)

---

## 3. How It Works Internally

### 1. Bytecode Erasure to Object
Consider a simple generic class:
```java
public class Holder<T> {
    private T value;
    public void set(T value) { this.value = value; }
    public T get() { return value; }
}
```
When compiled and disassembled with `javap -c Holder`:
```
public class Holder {
  public void set(java.lang.Object);
    Code:
       0: aload_0
       1: aload_1
       2: putfield      #7 // Field value:Ljava/lang/Object;
       5: return

  public java.lang.Object get();
    Code:
       0: aload_0
       1: getfield       #7 // Field value:Ljava/lang/Object;
       4: areturn
}
```
`T` is completely replaced by `Ljava/lang/Object;`.

At the call site:
```java
Holder<String> holder = new Holder<>();
holder.set("Java");
String s = holder.get();
```
The call site decompiles to:
```
invokevirtual #11 // Method Holder.get:()Ljava/lang/Object;
checkcast     #12 // class java/lang/String
astore_2
```
The compiler automatically injects the `checkcast` instruction.

### 2. Synthetic Bridge Methods
Consider generic inheritance:
```java
public class Node<T> {
    public void setData(T data) {}
}

public class StringNode extends Node<String> {
    @Override
    public void setData(String data) {
        System.out.println(data);
    }
}
```
In `Node`, `setData` erases to `setData(Object)`. But in `StringNode`, the method is `setData(String)`. In standard bytecode, these two methods have different parameter signatures and do not override each other!

To preserve polymorphic dispatch, the compiler synthesizes a **Bridge Method** in `StringNode`:
```
public void setData(java.lang.Object);
  flags: (0x1041) ACC_PUBLIC, ACC_BRIDGE, ACC_SYNTHETIC
  Code:
     0: aload_0
     1: aload_1
     2: checkcast     #7 // class java/lang/String
     5: invokevirtual #8 // Method setData:(Ljava/lang/String;)V
     8: return
```
The synthetic bridge method accepts `Object`, casts it to `String`, and delegates to `setData(String)`.

```
Caller -> invokes Node.setData(Object)
            |
            v
       StringNode.setData(Object)  [ACC_BRIDGE]
            |
            | checkcast java/lang/String
            v
       StringNode.setData(String)  [Target implementation]
```

---

## 4. Syntax & Usage

### 1. Accessing Type Metadata via Class Token
Because `new T()` is impossible due to erasure, pass a `Class<T>` token:
```java
public class EntityFactory<T> {
    private final Class<T> clazz;

    public EntityFactory(Class<T> clazz) {
        this.clazz = clazz;
    }

    public T newInstance() throws ReflectiveOperationException {
        return clazz.getDeclaredConstructor().newInstance();
    }
}
```

### 2. Safe Generic Array Construction via Reflection
```java
import java.lang.reflect.Array;

public class GenericArrayHelper {
    @SuppressWarnings("unchecked")
    public static <T> T[] createArray(Class<T> componentType, int length) {
        return (T[]) Array.newInstance(componentType, length);
    }
}
```

### 3. Reading Class-File Signature Attribute via Reflection
While local variables lose type arguments, class declarations and fields retain generic metadata in the `Signature` attribute:
```java
import java.lang.reflect.Field;
import java.lang.reflect.ParameterizedType;
import java.lang.reflect.Type;
import java.util.List;

public class SignatureDemo {
    private List<String> stringList;

    public static void main(String[] args) throws Exception {
        Field field = SignatureDemo.class.getDeclaredField("stringList");
        Type genericType = field.getGenericType();
        if (genericType instanceof ParameterizedType pt) {
            System.out.println("Raw type: " + pt.getRawType());
            System.out.println("Actual argument: " + pt.getActualTypeArguments()[0]);
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Super Type Token (GSON / Jackson Pattern) (Real-World)
```java
import java.lang.reflect.ParameterizedType;
import java.lang.reflect.Type;

// Anonymous subclass captures generic type in class signature attribute!
public abstract class TypeReference<T> {
    private final Type type;

    protected TypeReference() {
        Type superClass = getClass().getGenericSuperclass();
        if (superClass instanceof ParameterizedType pt) {
            this.type = pt.getActualTypeArguments()[0];
        } else {
            throw new IllegalArgumentException("Must declare concrete generic parameter");
        }
    }

    public Type getType() { return type; }

    public static void main(String[] args) {
        TypeReference<java.util.List<String>> token = new TypeReference<>() {};
        System.out.println("Captured generic type at runtime: " + token.getType());
    }
}
```

### Example 2: Inspecting Synthetic Bridge Methods (Advanced)
```java
import java.lang.reflect.Method;

public class BridgeMethodInspector {
    public static class Base<T> {
        public T process(T input) { return input; }
    }

    public static class Derived extends Base<String> {
        @Override
        public String process(String input) { return input.toUpperCase(); }
    }

    public static void main(String[] args) {
        for (Method m : Derived.class.getDeclaredMethods()) {
            System.out.printf("Method: %-20s Return: %-15s isBridge: %b\n",
                m.getName(), m.getReturnType().getSimpleName(), m.isBridge());
        }
    }
}
```

### Example 3: Edge Case - Overloading Collision After Erasure
```java
public class ErasureCollision {
    // COMPILE ERROR: Name clash: both methods have same erasure process(List)
    // public void process(List<String> list) {}
    // public void process(List<Integer> list) {}

    // Workaround: Distinct method names
    public void processStrings(java.util.List<String> list) {}
    public void processIntegers(java.util.List<Integer> list) {}
}
```

### Example 4: Failure Case - Illegal `instanceof` with Non-Reifiable Types
```java
import java.util.List;

public class InstanceOfFailure {
    public static void test(Object obj) {
        // COMPILE ERROR: Cannot perform instanceof check against parameterized type List<String>
        // if (obj instanceof List<String>) {}

        // VALID: Check against unbounded wildcard (reifiable)
        if (obj instanceof List<?> list) {
            System.out.println("List of size: " + list.size());
        }
    }
}
```

---

## 6. Best Practices

1. **Rely on Class Tokens for Dynamic Instantiation**: Pass `Class<T>` or `Supplier<T>` when runtime reflection or creation is required.
2. **Use Super Type Tokens for Serialization**: When parsing generic types with Jackson (`new TypeReference<List<T>>() {}`) or Gson (`new TypeToken<List<T>>() {}`), capture the `ParameterizedType`.
3. **Never Suppress Bridge Warnings Unintentionally**: Ensure overridden methods use exact matching return types to allow `javac` to wire bridges cleanly.
4. **Avoid Method Overloads that Differ Only by Parameterized Type Arguments**: Erasure causes signature clashes and bytecode collisions.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| `new T()` | `T` is erased to `Object`; constructor metadata is absent at runtime. | Pass `Supplier<T>` or `Class<T>`. |
| `new T[10]` | Generic array creation is prohibited to prevent hidden `ArrayStoreException`. | Use `(T[]) Array.newInstance(clazz, 10)` or `List<T>`. |
| `if (x instanceof List<String>)` | Non-reifiable; JVM cannot inspect element types at runtime. | Check `x instanceof List<?>`. |
| `catch (T e)` | Cannot catch generic type variables; exception handling is strictly runtime-typed. | Catch concrete exceptions or `Exception`. |
| Defining static field of type `T` in generic class | Static fields belong to the raw class, shared across all type instances. | Declare separate non-generic or instance fields. |
| Expecting method overloads with `List<String>` and `List<Integer>` | Both erase to `process(List)`, causing method signature clash. | Use different method names. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Runtime Abstraction Penalty**: There is no dynamic dispatch overhead or table lookup for type parameter resolution. Method invocations execute as standard `invokevirtual` or `invokeinterface`.
- **Micro-Benchmark Overhead of Bridge Methods**: Bridge methods add a single extra jump and cast (`checkcast`), which the HotSpot JIT compiler routinely inlines completely.

### Security
- **Type Erasure Means No Runtime Heap Enforcement**: Malicious code or uninspected bytecode can place invalid objects into generic collections via reflection. Always perform runtime defense-in-depth validation at security-critical boundaries.

---

## 9. Comparison / When to Use What

| Technique | When to Use | Mechanism |
|---|---|---|
| **`Class<T>` Token** | Instantiating single types, simple reflection | Reifiable `Class` object representing raw type |
| **Super Type Token** | Deserializing complex generic collections (`List<Map<String, User>>`) | Anonymous subclass capturing `ParameterizedType` metadata |
| **Unbounded Wildcard (`?`)** | Safe runtime type inspection (`instanceof List<?>`) | Reifiable type representation |
| **`Supplier<T>` Factory** | Clean instantiation without reflection exceptions | Functional lambda instantiation (`User::new`) |

---

## 10. Interview Questions

<details>
<summary>1. What is type erasure in Java and what are its primary goals?</summary>
Type erasure is the compile-time process where the Java compiler translates parameterized types to their raw bounds, inserts synthetic casts, and produces bytecode that contains no generic type variables. Its primary goal was full backward compatibility with pre-Java 5 JVM bytecode and existing libraries.
</details>

<details>
<summary>2. What is a synthetic bridge method and why does the compiler generate it?</summary>
A bridge method is a synthetic method generated by `javac` when a class extends or implements a generic class or interface with a specific type argument. Because erasure changes the superclass method signature to `Object`, the bridge method overrides `method(Object)`, casts the argument, and invokes the specialized `method(TargetType)` to maintain polymorphic method dispatch.
</details>

<details>
<summary>3. Why is `new T()` forbidden in Java?</summary>
Because `T` is erased to `Object` (or its bound). The JVM does not know which constructor to invoke or whether `T` is an interface or abstract class.
</details>

<details>
<summary>4. Why can you not create a generic array `new List<String>[10]`?</summary>
Arrays in Java are covariant and reified (they enforce component type checks at runtime). Generics are invariant and erased. Mixing reified arrays with erased generics would cause silent type safety violations without runtime detection.
</details>

<details>
<summary>5. Is all generic information erased at runtime?</summary>
No. Generic information for classes, interfaces, method signatures, and field declarations is preserved in the class file's `Signature` attribute. This metadata can be read reflectively via `Class.getGenericSuperclass()`, `Method.getGenericParameterTypes()`, etc. Only local variable generic types are completely erased.
</details>

<details>
<summary>6. How does a Super Type Token work?</summary>
By creating an anonymous inner subclass (`new TypeReference<List<String>>() {}`), the class definition itself carries the parameterized supertype in its class metadata. The constructor can then inspect `getClass().getGenericSuperclass()` to retrieve the exact runtime type argument.
</details>

<details>
<summary>7. What happens if you decompile a generic class with `javap -c`?</summary>
You observe that all method parameters and return types of type `T` are replaced by `java.lang.Object` (or the bound), and call sites contain explicit `checkcast` instructions.
</details>

<details>
<summary>8. Why can't a static field use the class's type parameter `private static T instance;`?</summary>
Static members belong to the class itself, not to any individual instance. Since there is only one copy of the class loaded in memory regardless of how many parameterized instances exist (`Box<String>`, `Box<Integer>`), a static type parameter cannot be resolved.
</details>

---

## 11. Practice Exercises

### Easy
1. Write a generic class and inspect its compiled bytecode using `javap -c` to locate inserted `checkcast` instructions.
2. Implement a method that accepts `Object` and uses `instanceof List<?>` to safely print its size.
3. Create a class with a bounded parameter `<T extends Number>` and confirm via reflection that its erased type is `Number`.

### Medium
4. Implement a `TypeToken<T>` abstract class that accurately extracts the `Class<?>` of `T` using reflection.
5. Write a program that uses reflection to discover all synthetic bridge methods on a given compiled class.
6. Build a safe generic array wrapper `DynamicArray<T>` backed by an `Object[]` array that provides type-safe `get` and `set`.

### Hard
7. Implement a complete JSON deserializer stub that deserializes nested generic collections (`Map<String, List<Integer>>`) using Super Type Tokens.
8. Create a bytecode verification test using `java.lang.invoke.MethodHandles` that dynamically validates bridge method invocation behavior.
9. Construct a dependency injection container that resolves dependencies parameterized by complex generic types.

<details>
<summary>Solutions</summary>

```java
// Easy 3: Reflection inspection
public class ErasureVerifier<T extends Number> {
    public void inspect() {
        java.lang.reflect.TypeVariable<?> tv = ErasureVerifier.class.getTypeParameters()[0];
        System.out.println("Bound: " + tv.getBounds()[0].getTypeName()); // java.lang.Number
    }
}

// Medium 5: Bridge method discovery
public static void printBridgeMethods(Class<?> clazz) {
    for (java.lang.reflect.Method m : clazz.getDeclaredMethods()) {
        if (m.isBridge()) {
            System.out.println("Found bridge method: " + m);
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Type Erasure**: Replaces type parameters with bounds (`Object` or first bound).
- **Synthetic Casts**: Compiler automatically inserts `checkcast` at reading call sites.
- **Bridge Methods**: Synthetic methods generated to maintain polymorphism when overriding generic methods.
- **Class Metadata**: Preserved in `Signature` attributes for class, field, and method declarations; inspectable via reflection.
- **Reification Limits**: Cannot do `new T()`, `new T[]`, `instanceof T`, or `catch (T e)`.
- **Super Type Token**: Captures generic metadata at runtime via anonymous subclassing (`new TypeRef<List<String>>() {}`).

---

## 13. Further Reading
- Goetz, Brian. *Java Generics: Type Erasure and its Consequences*.
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 28 - Prefer lists to arrays.
- Java Language Specification: *Section 4.6 - Type Erasure*.
