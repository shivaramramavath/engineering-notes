# JSON Basics and the Java JSON-P Specification

> Core JSON data modeling standards, RFC 8259 syntax specifications, IEEE-754 precision traps, and the official Jakarta JSON Processing (JSON-P / JSR 374) API.

| Metadata | Details |
|---|---|
| **Level** | Core |
| **Prerequisites** | Java Collections, Streams, String Processing |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: RFC 8259 & Memory Representation](#3-how-it-works-internally-rfc-8259--memory-representation)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why
JavaScript Object Notation (JSON) is the universal lingua franca of modern distributed systems, RESTful microservices, and web APIs. Governed by IETF RFC 8259 and ECMA-404, JSON is a language-independent, human-readable text format representing structured data using only key-value objects and ordered lists.

While third-party libraries like Jackson and Gson dominate application development, the Java Community Process defined the official standard **Jakarta JSON Processing (JSON-P / JSR 374)** specification. Understanding foundational JSON grammar, streaming parsers, JSON Pointer (RFC 6901), JSON Patch (RFC 6902), and number precision boundaries is indispensable for building robust enterprise data pipelines.

---

## 2. Core Concepts
- **RFC 8259 Syntax**: JSON specifies exactly six value types: Object (`{...}`), Array (`[...]`), String (`"..."`), Number (arbitrary precision integer or floating-point), Boolean (`true`/`false`), and `null`.
- **IEEE-754 Numeric Trap**: JSON numbers have no theoretical length limit in the grammar, but most JavaScript and JSON parsers convert numbers into 64-bit IEEE-754 floats. Large 64-bit Java `Long` integers (exceeding $2^{53} - 1 = 9,007,199,254,740,991$) lose precision unless transferred as strings.
- **Jakarta JSON-P (JSR 374)**: The official standardized API providing two parsing models:
  1. **Object Model API**: In-memory DOM tree representation (`JsonObject`, `JsonArray`, `JsonValue`).
  2. **Streaming API**: Low-level event-driven cursor (`JsonParser`, `JsonGenerator`) operating with zero in-memory DOM overhead.
- **JSON Pointer (RFC 6901)**: A string syntax for identifying a specific value within a JSON document (e.g., `/users/0/email`).
- **JSON Patch (RFC 6902)**: A declarative format for describing a sequence of operations (`add`, `remove`, `replace`, `move`, `copy`, `test`) to apply to a JSON document.

---

## 3. How It Works Internally: RFC 8259 & Memory Representation

JSON-P implements parser models designed for memory efficiency:

```
Raw JSON Byte Stream: {"orderId": 101, "items": ["sku-1"]}
             |
             v
+-------------------------------------------------------+
|             Low-Level Lexer / Tokenizer               |
|      Identifies: START_OBJECT, KEY_NAME, VALUE_NUMBER |
+-------------------------------------------------------+
            /                                            /                                             v                                     v
+--------------------+                 +--------------------+
|  Streaming Model   |                 | Object Model (DOM) |
| (Event-driven, 0   |                 | In-memory AST tree |
| object allocations)|                 | of JsonObject nodes|
+--------------------+                 +--------------------+
```

### Streaming vs DOM Parsing Mechanics:
- **Streaming Parser (`JsonParser`)**: Acts like an iterator advancing over token events (`START_OBJECT`, `KEY_NAME`, `VALUE_STRING`, `END_OBJECT`). It never allocates an entire document tree in memory, making it ideal for processing gigabyte-scale JSON log streams.
- **Object Model Tree (`JsonObject`)**: Parses the entire input into an immutable tree of `JsonValue` objects in heap memory. Enables random access, JSON Pointer evaluation, and tree transformations at the expense of memory allocations.

---

## 4. Syntax & Usage

### 4.1 Jakarta JSON-P Object Model Construction
```java
package com.example.json;

import jakarta.json.*;
import java.io.StringReader;
import java.io.StringWriter;
import java.math.BigDecimal;

public final class JsonPObjectDemo {

    public static String buildOrderJson() {
        // Constructing an immutable JSON Object DOM using Builder
        JsonObject orderJson = Json.createObjectBuilder()
                .add("orderId", 98234)
                .add("customerEmail", "engineer@example.com")
                .add("totalAmount", new BigDecimal("1249.50"))
                .add("active", true)
                .add("tags", Json.createArrayBuilder()
                        .add("PRIORITY")
                        .add("ELECTRONICS")
                        .build())
                .add("shippingAddress", Json.createObjectBuilder()
                        .add("city", "Seattle")
                        .add("zip", "98101")
                        .build())
                .build();

        StringWriter sw = new StringWriter();
        try (JsonWriter jw = Json.createWriter(sw)) {
            jw.writeObject(orderJson);
        }
        return sw.toString();
    }

    public static void readOrderJson(String jsonString) {
        try (JsonReader reader = Json.createReader(new StringReader(jsonString))) {
            JsonObject root = reader.readObject();
            long orderId = root.getJsonNumber("orderId").longValue();
            String email = root.getString("customerEmail");
            BigDecimal total = root.getJsonNumber("totalAmount").bigDecimalValue();
            JsonArray tags = root.getJsonArray("tags");

            System.out.printf("Order #%d for %s (Total: $%s)%n", orderId, email, total);
            for (JsonValue tag : tags) {
                System.out.println(" - Tag: " + ((JsonString) tag).getString());
            }
        }
    }
}
```

### 4.2 High-Throughput Streaming Parser (`JsonParser`)
```java
package com.example.json;

import jakarta.json.Json;
import jakarta.json.stream.JsonParser;
import jakarta.json.stream.JsonParser.Event;
import java.io.InputStream;

public final class JsonPStreamDemo {
    public static void scanHighVolumeStream(InputStream is) {
        try (JsonParser parser = Json.createParser(is)) {
            while (parser.hasNext()) {
                Event event = parser.next();
                if (event == Event.KEY_NAME && "targetField".equals(parser.getString())) {
                    parser.next(); // Advance to value
                    System.out.println("Found targetField value: " + parser.getString());
                }
            }
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: JSON Pointer (RFC 6901) Evaluation
```java
package com.example.json;

import jakarta.json.*;
import java.io.StringReader;

public final class JsonPointerDemo {
    public static void evaluatePointer(String jsonText) {
        try (JsonReader reader = Json.createReader(new StringReader(jsonText))) {
            JsonObject doc = reader.readObject();

            // JSON Pointer navigating into nested array index 1
            JsonPointer pointer = Json.createPointer("/items/1/price");
            JsonValue priceVal = pointer.getValue(doc);

            System.out.println("Second item price: " + priceVal);
        }
    }
}
```

### Example 2: Real-World JSON Patch (RFC 6902) Transformation
```java
package com.example.json;

import jakarta.json.*;
import java.io.StringReader;

public final class JsonPatchDemo {
    public static JsonObject applyPatchOperations(JsonObject originalDoc) {
        // Constructing RFC 6902 patch operations: replace status, add discount
        JsonPatch patch = Json.createPatchBuilder()
                .replace("/status", "SHIPPED")
                .add("/discount", 15)
                .remove("/temporaryNotes")
                .build();

        // Applies operations and returns a new immutable JsonObject
        return patch.apply(originalDoc);
    }
}
```

### Example 3: Edge Case - Preserving Large 64-Bit Integers
```java
package com.example.json;

import jakarta.json.Json;
import jakarta.json.JsonObject;

public final class LongPrecisionDemo {
    public static void demonstratePrecision() {
        // Long value exceeding JavaScript Number.MAX_SAFE_INTEGER (9007199254740991)
        long snowflakeId = 1854920194857201942L;

        // SAFE: Serialize numeric identifiers as String when communicating with web/JS clients
        JsonObject payload = Json.createObjectBuilder()
                .add("entityId", String.valueOf(snowflakeId)) // Immune to IEEE-754 precision clipping
                .add("rawInteger", snowflakeId) // May lose lower bits in JavaScript engines!
                .build();

        System.out.println(payload);
    }
}
```

### Example 4: Failure Case - Missing Null Checks in JSON-P
```java
// ANTI-PATTERN: Calling getString directly on an explicit JSON null throws NullPointerException
public static void unsafeRead(JsonObject obj) {
    // If JSON is: {"middleName": null}
    // obj.isNull("middleName") is TRUE, but obj.getString("middleName") throws NullPointerException!
    if (!obj.isNull("middleName")) {
        String name = obj.getString("middleName");
    }
}
```

---

## 6. Best Practices
1. **Always Transmit 64-Bit IDs as Strings to Web Clients**: To prevent truncation by browser JavaScript engines ($> 2^{53} - 1$), format `long` primary keys as strings.
2. **Use Streaming Parsers for Payloads > 50 MB**: Use `JsonParser` and `JsonGenerator` instead of building in-memory DOMs when parsing large log exports or bulk datasets.
3. **Check `isNull()` Before Value Extraction**: Never invoke `getString()` or `getJsonNumber()` on a key without checking `obj.isNull(key)`.
4. **Enforce Strict Character Encoding (UTF-8)**: RFC 8259 Section 8.1 mandates UTF-8 encoding for all JSON exchanged between systems.
5. **Leverage JSON Merge Patch for Simple Updates**: Use RFC 7396 Merge Patch for standard resource updates, and RFC 6902 JSON Patch for transactional array mutations.
6. **Prefer BigDecimal for Monetary JSON Values**: Never map currency amounts to standard floating-point variables.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Large 64-bit `Long` truncated in browser | JavaScript `Number` loses precision beyond $2^{53} - 1$ | Serialize large 64-bit integers as JSON Strings |
| `getString()` on explicit `null` | Throws `NullPointerException` in Jakarta JSON-P | Check `obj.isNull("field")` before extracting |
| Assuming key order is preserved | RFC 8259 states JSON object keys are strictly unordered | Never write business logic dependent on key ordering |
| Loading 500 MB JSON via `JsonReader` | Allocates millions of AST nodes, causing OutOfMemoryError | Process with event-driven streaming `JsonParser` |
| Floating point arithmetic on money | IEEE-754 binary representation introduces decimal rounding errors | Parse numbers using `BigDecimal` |
| Trailing commas in JSON | RFC 8259 syntax strictly forbids trailing commas in objects and arrays | Ensure generator does not emit trailing commas |
| Unescaped control characters in strings | Invalidates JSON grammar; parser throws syntax error | Ensure quotes, newlines, and backslashes are properly escaped |
| Parsing untrusted JSON with recursive nesting | Deeply nested objects can cause StackOverflowError | Set parser depth limits on input streams |

---

## 8. Performance & Security Notes
- **JSON Bomb / Stack Overflow Attacks**: An adversary can send a payload with 10,000 nested brackets `[[[[...]]]]`. Without depth limits, parsers exhaust thread stack space and crash. Ensure parser maximum nesting depth is constrained (typically $\le 64$).
- **Memory Footprint of DOM Trees**: A 10 MB JSON document text string typically expands to 50–100 MB of heap memory when parsed into an object tree due to object headers and reference pointers.
- **UTF-8 Byte Scanning**: Modern JSON parsers scan ASCII token bytes directly using SIMD instructions (e.g., AVX2 / NEON) before creating Java `String` objects, dramatically speeding up parsing.

---

## 9. Comparison / When to Use What

| API / Library | Parsing Model | Memory Overhead | Performance | Best Use Case |
|---|---|---|---|---|
| **JSON-P Object Model** | DOM Tree (`JsonObject`) | High (~5x raw size) | Fast | Small payload inspection, JSON Patch/Pointer |
| **JSON-P Streaming** | Pull Cursor (`JsonParser`)| Minimal (< 1 KB) | Very Fast | Gigabyte file processing, ETL streaming |
| **Jackson Databind** | Data Binding / Mapping | Moderate | Fastest | Standard Java microservices, POJO/Record mapping |
| **Gson** | Data Binding / Mapping | Moderate | Fast | Android, zero-dependency lightweight apps |

---

## 10. Interview Questions

<details>
<summary>1. What are the six data types supported by the JSON specification (RFC 8259)?</summary>
The six types defined by RFC 8259 are:
1. <b>Object</b>: An unordered collection of key-value pairs enclosed in curly braces <code>{...}</code>.
2. <b>Array</b>: An ordered sequence of zero or more values enclosed in square brackets <code>[...]</code>.
3. <b>String</b>: A sequence of Unicode characters enclosed in double quotation marks <code>"..."</code>.
4. <b>Number</b>: Integer or floating-point representation in base 10 (scientific notation allowed).
5. <b>Boolean</b>: The exact literal tokens <code>true</code> or <code>false</code>.
6. <b>Null</b>: The exact literal token <code>null</code>.
</details>

<details>
<summary>2. Why can transferring a 64-bit Java long in JSON cause data corruption in web applications?</summary>
In the JSON specification, numbers have no size limit. However, the standard JavaScript runtime represents all numbers as IEEE-754 64-bit double-precision floats, which provide only 53 bits of mantissa precision (up to 9,007,199,254,740,991). When a Java <code>long</code> (such as a database Snowflake ID like <code>1854920194857201942L</code>) is parsed in a browser, the lowest digits are rounded to zeroes, corrupting primary keys. The industry standard fix is serializing 64-bit IDs as JSON strings.
</details>

<details>
<summary>3. What is the difference between JSON Pointer (RFC 6901) and JSONPath?</summary>
<b>JSON Pointer (RFC 6901)</b> is an official IETF standard specifying a simple, deterministic path syntax (e.g., <code>/users/0/name</code>) for identifying exactly one single element within a JSON document.<br>
<b>JSONPath</b> is an expressive query language (modeled after XPath, e.g., <code>$.users[?(@.age > 21)].name</code>) that supports predicates, filtering, wildcards, and returning sets of elements.
</details>

<details>
<summary>4. How does JSON-P's streaming API (JsonParser) achieve zero-allocation parsing?</summary>
<code>JsonParser</code> acts as a stateful pull-parser. Instead of constructing <code>JsonObject</code>, <code>JsonArray</code>, or <code>String</code> instances for every element, it scans the underlying byte buffer and returns primitive token events (<code>START_OBJECT</code>, <code>KEY_NAME</code>, etc.). The application reads values directly from internal char buffers, avoiding temporary heap object allocations.
</details>

<details>
<summary>5. What is the difference between JSON Patch (RFC 6902) and JSON Merge Patch (RFC 7396)?</summary>
<b>JSON Patch (RFC 6902)</b> is a list of explicit mutation operations (<code>add</code>, <code>remove</code>, <code>replace</code>, <code>move</code>, <code>copy</code>, <code>test</code>) applied sequentially to a document. It allows precise array index modifications.<br>
<b>JSON Merge Patch (RFC 7396)</b> sends a partial JSON document representing the target state: keys with values are updated, and keys with <code>null</code> are removed. It is simpler for flat updates, but cannot partially modify arrays.
</details>

<details>
<summary>6. Are JSON object keys ordered according to RFC 8259?</summary>
No. RFC 8259 explicitly states that JSON objects are an <i>unordered</i> collection of zero or more name/value pairs. Compliant JSON parsers and generators are allowed to re-order keys. Code that relies on key order for verification or signing is fundamentally flawed unless canonical JSON ordering (RFC 8785) is applied.
</details>

<details>
<summary>7. How does JSON-P handle numeric values without losing decimal precision?</summary>
Jakarta JSON-P provides the <code>JsonNumber</code> interface. Rather than coercing all numbers to <code>double</code>, <code>JsonNumber</code> retains the underlying textual representation and provides exact conversion methods: <code>bigDecimalValue()</code> and <code>bigIntegerValue()</code>, ensuring financial and decimal accuracy.
</details>

<details>
<summary>8. What is a JSON Denial of Service (DoS) attack, and how is it mitigated?</summary>
A JSON DoS attack exploits parser vulnerabilities using deeply nested structures (e.g., thousands of opening brackets <code>[[[[...]]]]</code>) or enormous string/array lengths to induce stack exhaustion (<code>StackOverflowError</code>) or heap memory saturation (<code>OutOfMemoryError</code>). It is mitigated by configuring parser thresholds for maximum nesting depth and maximum string lengths.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Write a JSON-P method to parse a list of user emails from an in-memory JSON document.</summary>

```java
public static List<String> extractEmails(String json) {
    try (JsonReader reader = Json.createReader(new StringReader(json))) {
        JsonObject root = reader.readObject();
        JsonArray users = root.getJsonArray("users");
        List<String> emails = new ArrayList<>();
        for (JsonValue val : users) {
            JsonObject userObj = val.asJsonObject();
            if (!userObj.isNull("email")) {
                emails.add(userObj.getString("email"));
            }
        }
        return emails;
    }
}
```
</details>

<details>
<summary>Exercise 2: Implement a JSON Pointer lookup with fallback default value.</summary>

```java
public static String getNestedString(JsonObject doc, String pointerPath, String defaultValue) {
    try {
        JsonPointer pointer = Json.createPointer(pointerPath);
        if (pointer.containsValue(doc)) {
            JsonValue val = pointer.getValue(doc);
            return val.getValueType() == JsonValue.ValueType.STRING ? ((JsonString) val).getString() : val.toString();
        }
    } catch (Exception ignored) {}
    return defaultValue;
}
```
</details>

<details>
<summary>Exercise 3: Write an event-driven JsonParser loop counting total orders in a streaming file.</summary>

```java
public static long countOrdersStreaming(InputStream jsonStream) {
    long orderCount = 0;
    try (JsonParser parser = Json.createParser(jsonStream)) {
        while (parser.hasNext()) {
            if (parser.next() == JsonParser.Event.KEY_NAME && "orderId".equals(parser.getString())) {
                orderCount++;
            }
        }
    }
    return orderCount;
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Specification**: RFC 8259; 6 types (Object, Array, String, Number, Boolean, Null).
- **Long Precision**: Numbers $> 2^{53} - 1$ must be transferred as strings to prevent JavaScript truncation.
- **Jakarta JSON-P**: `JsonReader`/`JsonObject` for DOM trees; `JsonParser` for high-speed zero-allocation streaming.
- **Null Safety**: Always check `obj.isNull("key")` before reading values.
- **RFC Standards**: JSON Pointer (RFC 6901) for path lookups; JSON Patch (RFC 6902) for atomic document mutations.
- **Encoding**: UTF-8 is mandatory; keys are strictly unordered.

---

## 13. Further Reading
- IETF RFC 8259: The JavaScript Object Notation (JSON) Data Interchange Format
- Jakarta JSON Processing (JSON-P) Specification 2.1 / JSR 374
- IETF RFC 6901: JavaScript Object Notation (JSON) Pointer
- IETF RFC 6902: JavaScript Object Notation (JSON) Patch
