# Insecure Deserialization in Java

> Comprehensive analysis of native Java serialization vulnerabilities, gadget chains, Remote Code Execution (RCE), JEP 290 serialization filters, JEP 415 context-specific filter factories, and secure data exchange architectures.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Enterprise Security |
| **Prerequisites** | [01_cryptography](01_cryptography.md), [11_io](../11-io-and-networking/README.md), [17_json](../17-json-and-data-formats/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

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
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is Serialization and Deserialization?
- **Serialization**: The process of translating a live Java in-memory object graph into a serialized binary byte stream (`java.io.ObjectOutputStream`).
- **Deserialization**: Reconstructing the complete in-memory object graph from the binary stream (`java.io.ObjectInputStream`).

### The Danger: Untrusted Object Deserialization
When an application invokes `ObjectInputStream.readObject()` on data received from an untrusted source (HTTP request, network socket, message queue, untrusted database), it creates objects and executes code **before** application logic ever inspects the object.

Brian Goetz (Java Language Architect) famously called Java serialization a "horrible mistake", and former Sun engineers called it "the gift that keeps on giving" for security researchers.

Untrusted deserialization enables:
1. **Remote Code Execution (RCE)**: By linking together "gadget chains" (existing classes on the application classpath), attackers construct payloads that execute arbitrary shell commands.
2. **Denial of Service (DoS)**: "Deserialization bombs" consume gigabytes of heap memory or lock CPU threads indefinitely using tiny payloads (Billion Laughs equivalent in Java binary streams).
3. **Privilege Escalation**: Mutating internal object state by manipulating private fields within the serialized stream.

---

## 2. Core Concepts

### What is a Gadget Chain?
A "gadget" is an existing class on the application\'s classpath (or within standard JDK libraries and third-party dependencies like Commons-Collections, Spring, or Groovy) that contains executable methods called during deserialization (e.g., `readObject()`, `finalize()`, `hashCode()`, `equals()`, `compareTo()`).

A **gadget chain** links multiple gadgets together:
```
Untrusted Byte Stream
         |
         v
ObjectInputStream.readObject()
         |
         v
Gadget 1: HashMap.readObject()
         | calls key.hashCode()
         v
Gadget 2: InvocationHandler (Dynamic Proxy)
         | dispatches method call
         v
Gadget 3: Transformer (e.g., InvokerTransformer)
         | executes Runtime.getRuntime().exec("malicious_command")
         v
Arbitrary Remote Code Execution (RCE) on Server
```
The attacker does **not** need to upload malicious code to the server; they only manipulate existing legitimate classes already resident on the server\'s classpath.

### JEP 290: Filter Incoming Serialization Data (Java 9+)
Introduced in Java 9 (and backported to Java 8), JEP 290 provides an `ObjectInputFilter` mechanism that inspects:
- The class name of every object before instantiation.
- Array sizes and total object references.
- Stream depth and total bytes read.
If an incoming class is not explicitly permitted by an allow-list, deserialization is rejected immediately.

### JEP 415: Context-Specific Deserialization Filters (Java 17+)
JEP 415 introduces a JVM-wide filter factory (`ObjectInputFilter.Config.setSerialFilterFactory`). It allows applications to dynamically configure custom, context-specific filters for every individual `ObjectInputStream` based on the calling context, thread, or library.

---

## 3. How It Works Internally

### Java Native Serialization Stream Protocol (Object Serialization Stream Protocol)
A Java serialized stream begins with a 4-byte header:
```
0xAC 0xED (STREAM_MAGIC)
0x00 0x05 (STREAM_VERSION)
```
When `ObjectInputStream.readObject()` runs:
1. It reads type codes: `TC_OBJECT (0x73)`, `TC_CLASSDESC (0x72)`, class names, field descriptors.
2. It allocates memory for the target class directly via native reflection (bypassing normal constructors!).
3. If the class defines a custom `readObject(ObjectInputStream in)` method, HotSpot invokes it immediately via reflection.
4. Any logic inside `readObject()` executes with the full privileges of the JVM process.

```
+--------------------------------------------------------------------------+
|                  DESERIALIZATION EXECUTION FLOW                          |
|                                                                          |
|  Byte Stream [AC ED 00 05...]                                           |
|       |                                                                  |
|       v                                                                  |
|  ObjectInputStream.readObject()                                          |
|       |                                                                  |
|       +--> [JEP 290 Filter Check]                                        |
|       |         |                                                        |
|       |         +-- Rejected? --> Throw InvalidClassException (SAFE)    |
|       |                                                                  |
|       v (If Accepted)                                                    |
|  Allocate instance without calling constructor                           |
|       |                                                                  |
|       v                                                                  |
|  Invoke class.readObject() / comparator / hashCode                       |
|       |                                                                  |
|       v                                                                  |
|  Gadget execution begins! (VULNERABLE IF UNFILTERED)                     |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Declaring an Allow-List Serialization Filter (JEP 290)
Using pattern strings:
```java
import java.io.ObjectInputFilter;
import java.io.ObjectInputStream;

// Allow java.base classes and our specific DTO, reject everything else
ObjectInputFilter filter = ObjectInputFilter.Config.createFilter(
    "com.mycompany.dto.SafePayload;java.lang.*;java.util.*;!*"
);

ObjectInputStream ois = new ObjectInputStream(inputStream);
ois.setObjectInputFilter(filter);
Object obj = ois.readObject();
```

### 2. Setting a Process-Wide Filter via System Property
You can protect legacy Java services globally without modifying source code by setting the JVM system property:
```bash
java -Djdk.serialFilter="com.mycompany.dto.*;java.lang.*;java.util.*;maxdepth=5;maxrefs=500;!*" -jar app.jar
```

---

## 5. Practical Examples

### Example 1: Secure ObjectInputStream with Programmatic JEP 290 Filter
**Goal:** Deserialize a known, trusted DTO while strictly blocking all unknown classes, gadget libraries, and deep object graphs.

```java
import java.io.*;

public class SecureDeserializer {
    public static <T> T deserializeSafe(byte[] data, Class<T> expectedType) 
            throws IOException, ClassNotFoundException {
        
        // Step 1: Create a strict allow-list filter
        ObjectInputFilter filter = filterInfo -> {
            Class<?> serialClass = filterInfo.serialClass();
            if (serialClass != null) {
                // Explicit allow-list check
                if (serialClass.equals(expectedType) ||
                    serialClass.getName().startsWith("java.lang.") ||
                    serialClass.getName().startsWith("java.util.")) {
                    return ObjectInputFilter.Status.ALLOWED;
                }
                // Reject all other classes
                return ObjectInputFilter.Status.REJECTED;
            }

            // Stream size limits to defend against resource exhaustion bombs
            if (filterInfo.depth() > 5) {
                return ObjectInputFilter.Status.REJECTED; // Max nesting depth 5
            }
            if (filterInfo.references() > 1000) {
                return ObjectInputFilter.Status.REJECTED; // Max 1000 object references
            }
            if (filterInfo.streamBytes() > 1_000_000) {
                return ObjectInputFilter.Status.REJECTED; // Max 1 MB payload
            }

            return ObjectInputFilter.Status.UNDECIDED;
        };

        // Step 2: Bind filter to ObjectInputStream
        try (ByteArrayInputStream bais = new ByteArrayInputStream(data);
             ObjectInputStream ois = new ObjectInputStream(bais)) {
            
            ois.setObjectInputFilter(filter);
            Object obj = ois.readObject();

            if (!expectedType.isInstance(obj)) {
                throw new InvalidClassException("Unexpected object type: " + obj.getClass().getName());
            }

            return expectedType.cast(obj);
        }
    }
}
```

### Example 2: Migrating from Java Native Serialization to Safe JSON / Records
**Goal:** Completely eliminate `Serializable` and `ObjectInputStream` by adopting immutable Java records with Jackson or JSON-B.

```java
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;

// Safe immutable data structure
public record OrderTransferPayload(
    long orderId,
    String customerEmail,
    java.math.BigDecimal totalAmount
) {}

public class ModernDataSerializer {
    private static final ObjectMapper MAPPER = new ObjectMapper();

    public static byte[] serialize(OrderTransferPayload payload) throws IOException {
        return MAPPER.writeValueAsBytes(payload);
    }

    public static OrderTransferPayload deserialize(byte[] jsonBytes) throws IOException {
        // Jackson uses standard constructors and explicit fields;
        // No arbitrary byte-stream bytecode execution or gadget invocation!
        return MAPPER.readValue(jsonBytes, OrderTransferPayload.class);
    }
}
```

### Example 3: Edge Case — Deserialization Bomb Defense
**Goal:** Block an attacker sending a tiny payload that expands into millions of nested objects upon deserialization.

```java
import java.io.*;

public class DeserializationBombGuard {
    public static ObjectInputStream createProtectedStream(InputStream rawIn) throws IOException {
        ObjectInputStream ois = new ObjectInputStream(rawIn);
        // Enforce strict graph depth and reference limits
        ObjectInputFilter bombFilter = ObjectInputFilter.Config.createFilter(
            "maxdepth=4;maxrefs=500;maxbytes=65536;!*"
        );
        ois.setObjectInputFilter(bombFilter);
        return ois;
    }
}
```

### Example 4: Failure Case — Vulnerable Native Deserialization Endpoint
```java
// CRITICAL VULNERABILITY - NEVER DO THIS:
@PostMapping("/api/restore")
public void handleUntrustedData(@RequestBody byte[] rawPayload) throws Exception {
    ObjectInputStream ois = new ObjectInputStream(new ByteArrayInputStream(rawPayload));
    Object userObj = ois.readObject(); // VULNERABLE TO REMOTE CODE EXECUTION (RCE)!
}
```

---

## 6. Best Practices

1. **Avoid Java Native Serialization Entirely**: For any new design, use structured data formats: JSON, Protocol Buffers, FlatBuffers, or Avro.
2. **If Java Serialization is Unavoidable, Enforce JEP 290 Filters**: Apply strict class allow-lists with `ObjectInputFilter` and reject everything else (`!*`).
3. **Limit Stream Limits**: Constrain maximum depth (`maxdepth`), total references (`maxrefs`), and maximum bytes (`maxbytes`).
4. **Use JEP 415 Global Filter Factory in Java 17+**: Ensure every `ObjectInputStream` created by third-party libraries has a default protective filter applied.
5. **Never Deserialize Untrusted Data with Jackson Polymorphic Typing (`enableDefaultTyping()`)**: Deserializing generic `Object` with Jackson or Fastjson introduces polymorphic gadget chain vulnerabilities equivalent to native serialization.
6. **Mark Sensitive Fields as `transient`**: Prevent accidental leakage of passwords, keys, or internal tokens during serialization.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Deserializing before type checking (`(User) ois.readObject()`)** | Casting happens *after* `readObject()` completes; gadgets execute inside `readObject()`. | Apply `ObjectInputFilter` before invoking `readObject()`. |
| **Using Deny-List filters (blocking known bad classes)** | Attackers find new gadgets in dependencies or JDK classes not on the deny-list. | Always use an explicit allow-list ending with `!*`. |
| **Jackson `enableDefaultTyping()`** | Allows JSON payload to specify arbitrary class names, enabling gadget RCE. | Use explicit `@JsonTypeInfo` with an allow-list of known subclasses. |
| **Relying on signature checks over serialized objects** | If the verification key is compromised or verification code has flaws, RCE occurs. | Migrate away from native serialization to JSON/Protobuf. |
| **Ignoring Deserialization Bombs** | Small nested collections cause $O(2^n)$ expansions, freezing JVM memory and CPU. | Set `maxdepth` and `maxrefs` limits in `ObjectInputFilter`. |
| **Storing serialized Java objects in relational databases** | If database is updated via SQLi or untrusted DB admins, attacker achieves RCE. | Store JSON or structured columns instead of binary blobs. |

---

## 8. Performance & Security Notes

### Native Serialization Overhead
Java native serialization is notoriously slow and bloated:
- Includes extensive class metadata, serialVersionUIDs, and type descriptors in every byte stream.
- Consumes significant CPU reflecting on private fields.
- Protocol Buffers and compact JSON parsers (Jackson Afterburner / Blackbird) are typically **3x to 10x faster** and produce payloads **50% to 80% smaller** than native Java serialization.

---

## 9. Comparison / When to Use What

| Serialization Mechanism | Security Profile | Performance | Interoperability | Recommendation |
| :--- | :--- | :--- | :--- | :--- |
| **Java Native Serialization** | Critical Security Risk | Slow & Bloated | Java Only | Deprecated; migrate away |
| **Protocol Buffers (Protobuf)**| Exceptional (Schema-first) | Ultra Fast | Multi-language | Recommended for microservices & RPC |
| **JSON (Jackson / JSON-B)** | High (When untyped) | Fast | Universal | Recommended for REST APIs & web |
| **Avro** | Exceptional (Schema-first) | High Throughput | Multi-language | Recommended for Kafka & big data |

---

## 10. Interview Questions & Answers

### Q1: What is a gadget chain in the context of Java deserialization vulnerabilities?
<details>
<summary>View Answer</summary>
A gadget chain is a sequence of pre-existing, legitimate classes resident on the application\'s classpath that can be linked together to achieve an unintended malicious outcome (such as arbitrary command execution). When `ObjectInputStream.readObject()` is called on a malicious payload, it triggers common methods (like `readObject()`, `hashCode()`, or `InvocationHandler.invoke()`) in the first "gadget" class. This class delegates to another class, chaining execution until a "sink" gadget (such as `Runtime.getRuntime().exec()` or an arbitrary class loader) is invoked.
</details>

### Q2: Why is casting after `readObject()` useless as a security defense?
<details>
<summary>View Answer</summary>
In Java code like `MySafeClass obj = (MySafeClass) ois.readObject();`, the cast `(MySafeClass)` is evaluated only *after* the `readObject()` method has completely parsed the stream, instantiated objects, and executed their internal deserialization hooks. If the incoming stream contains a malicious gadget payload, the arbitrary code execution occurs inside `readObject()` before the type-cast instruction is ever reached.
</details>

### Q3: How does JEP 290 protect Java applications against deserialization attacks?
<details>
<summary>View Answer</summary>
JEP 290 introduces the `ObjectInputFilter` interface. Before an `ObjectInputStream` instantiates a class or allocates array memory, it passes the class descriptor, graph depth, number of references, and total bytes to the registered filter. If the filter returns `REJECTED`, the stream immediately throws an `InvalidClassException` and halts processing before any constructor, memory allocation, or gadget method can run.
</details>

### Q4: What is the difference between JEP 290 and JEP 415?
<details>
<summary>View Answer</summary>
JEP 290 provided process-wide and stream-specific serialization filters, but third-party libraries creating their own `ObjectInputStream` instances often failed to apply filters. JEP 415 (Context-Specific Deserialization Filters, Java 17) introduces a dynamic filter factory (`ObjectInputFilter.Config.setSerialFilterFactory`). It intercepts every creation of an `ObjectInputStream` across the JVM, allowing developers to configure context-dependent, library-specific filters automatically.
</details>

### Q5: What is a "Deserialization Bomb" and how do you mitigate it?
<details>
<summary>View Answer</summary>
A deserialization bomb is a compact serialized payload designed to exhaust CPU or memory when unpacked. For example, a deeply nested tree of `HashSet` objects containing identical elements can require billions of hash comparisons during deserialization, freezing the JVM thread, or allocating enormous object graphs from a few kilobytes of input. Mitigation involves setting `maxdepth` and `maxrefs` constraints in the JEP 290 `ObjectInputFilter`.
</details>

### Q6: Why is Jackson\'s `enableDefaultTyping()` considered a security antipattern?
<details>
<summary>View Answer</summary>
When `enableDefaultTyping()` is enabled on Jackson\'s `ObjectMapper`, JSON payloads can include arbitrary class names (e.g. `["org.apache.commons.collections.functors.InvokerTransformer", {...}]`). When Jackson deserializes the JSON, it instantiates and populates the specified class via reflection, reproducing the exact same polymorphic gadget chain vulnerabilities found in native Java serialization.
</details>

### Q7: What is the purpose of the `transient` keyword in Java serialization?
<details>
<summary>View Answer</summary>
The `transient` keyword marks a field so that it is excluded from native Java serialization. When an object is serialized, transient fields are omitted from the byte stream; when deserialized, transient fields are initialized to their default values (null, 0, or false). It is critical for preventing sensitive data (like passwords, encryption keys, or database connection handles) from leaking into persistent streams.
</details>

### Q8: What is `serialVersionUID` and how does it relate to security and compatibility?
<details>
<summary>View Answer</summary>
`serialVersionUID` is a 64-bit hash that identifies the version of a `Serializable` class. If an incoming serialized object\'s ID does not match the local class\'s ID, deserialization throws an `InvalidClassException`. While primarily designed to prevent structural incompatibilities across code revisions, it does not provide security against malicious gadget attacks because attackers can easily forge matching `serialVersionUID` values in the binary stream.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Custom JEP 290 Filter Predicate
**Task:** Implement a custom `ObjectInputFilter` that allows only classes from package `com.enterprise.model.*` and primitive Java wrappers (`java.lang.Integer`, `java.lang.String`, `java.lang.Long`), while enforcing a max stream depth of 3.
<details>
<summary>View Solution</summary>

```java
import java.io.ObjectInputFilter;
import java.util.Set;

public class CustomSecurityFilter implements ObjectInputFilter {
    private static final Set<String> ALLOWED_JAVA_CLASSES = Set.of(
        "java.lang.String",
        "java.lang.Integer",
        "java.lang.Long",
        "java.lang.Boolean"
    );

    @Override
    public Status checkInput(FilterInfo info) {
        if (info.depth() > 3) {
            return Status.REJECTED;
        }

        Class<?> clazz = info.serialClass();
        if (clazz == null) {
            return Status.UNDECIDED;
        }

        String name = clazz.getName();
        if (name.startsWith("com.enterprise.model.") || ALLOWED_JAVA_CLASSES.contains(name)) {
            return Status.ALLOWED;
        }

        return Status.REJECTED;
    }
}
```
</details>

### Exercise 2: Safe Object Cloning without Serialization
**Task:** Historically developers used `ByteArrayOutputStream` + `ObjectInputStream` for deep cloning. Implement a deep cloner using a modern, safe approach (copy constructors or Jackson/Protobuf).
<details>
<summary>View Solution</summary>

```java
import com.fasterxml.jackson.databind.ObjectMapper;

public class SafeCloner {
    private static final ObjectMapper MAPPER = new ObjectMapper();

    public static <T> T deepClone(T object, Class<T> clazz) {
        try {
            byte[] bytes = MAPPER.writeValueAsBytes(object);
            return MAPPER.readValue(bytes, clazz);
        } catch (Exception e) {
            throw new RuntimeException("Safe cloning failed", e);
        }
    }
}
```
</details>

### Exercise 3: Setting a JVM-Wide Filter Factory (JEP 415)
**Task:** Write a bootstrapping class that registers a global `SerialFilterFactory` enforcing a default baseline filter on all threads.
<details>
<summary>View Solution</summary>

```java
import java.io.ObjectInputFilter;
import java.io.ObjectInputFilter.Config;

public class SecurityBootstrapper {
    public static void initializeGlobalFilter() {
        // Enforce baseline filter factory
        Config.setSerialFilterFactory((currentFilter, nextFilter) -> {
            ObjectInputFilter baseline = Config.createFilter("maxdepth=5;maxrefs=500;!*");
            return ObjectInputFilter.merge(nextFilter != null ? nextFilter : currentFilter, baseline);
        });
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Golden Rule**: Never deserialize untrusted binary data with `ObjectInputStream`.
- **Modern Alternatives**: Migrate to JSON (Jackson), Protocol Buffers, or Avro.
- **JEP 290**: If native serialization is unavoidable, always apply an allow-list `ObjectInputFilter` ending with `!*`.
- **Enforce Constraints**: Limit `maxdepth` ($\le 5$), `maxrefs` ($\le 1000$), and `maxbytes`.
- **Jackson Security**: Never enable `enableDefaultTyping()` or polymorphic deserialization without strict subtype allow-lists.
- **Data Protection**: Mark all sensitive fields as `transient`.

---

## 13. Further Reading
- OpenJDK JEP 290: [Filter Incoming Serialization Data](https://openjdk.org/jeps/290)
- OpenJDK JEP 415: [Context-Specific Deserialization Filters](https://openjdk.org/jeps/415)
- OWASP Top 10: [A08:2021 - Software and Data Integrity Failures](https://owasp.org/Top10/A08_2021-Software_and_Data_Integrity_Failures/)
- YsoSerial Research Project: [Java Deserialization Exploit Payloads](https://github.com/frohoff/ysoserial)
