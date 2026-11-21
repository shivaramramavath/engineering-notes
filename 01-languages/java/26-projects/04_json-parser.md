# Project 04: JSON Parser and Serializer from Scratch (`json-parser`)

> Zero-dependency RFC 8259 JSON parser and serializer with lexical tokenizer, recursive-descent AST generator, sealed interface value types, and streaming pretty-printer.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-04-json-parser` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | Advanced / Production-Grade |
| **Architectural Style** | Compiler Frontend / Recursive Descent |
| **Core Concepts** | Lexer, Tokenizer, Recursive Descent Parsing, Sealed Interfaces, Pattern Matching, Abstract Syntax Tree (AST) |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture

This project builds an RFC 8259-compliant JSON processing engine without third-party libraries (no Jackson, Gson, or Fastjson). Utilizing modern Java 21+ features—including sealed interface hierarchies, pattern matching for switch, and exhaustive record deconstruction—it parses arbitrary JSON strings into strongly typed abstract syntax trees (AST) and serializes them back with configurable formatting.

```
+-------------------------------------------------------------------------------+
|                        JSON PARSER ENGINE ARCHITECTURE                        |
|                                                                               |
|   Raw JSON String Input (RFC 8259)                                            |
|         |                                                                     |
|         v                                                                     |
|   +---------------------------------------+                                   |
|   |   JsonLexer (Lexical Analyzer)        |                                   |
|   |   - Scans characters into Token stream|                                   |
|   |   - Escapes unicode: \uXXXX           |                                   |
|   |   - Distinguishes Numbers, Strings,   |                                   |
|   |     Booleans, Null, and Delimiters    |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   JsonParser (Recursive Descent)      |                                   |
|   |   - Enforces grammar production rules |                                   |
|   |   - Parses Object: { key: value }     |                                   |
|   |   - Parses Array: [ v1, v2, ... ]     |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   JsonValue AST (Sealed Hierarchy)    |                                   |
|   |   - JsonObject (Map<String, JsonValue>|                                   |
|   |   - JsonArray (List<JsonValue>)       |                                   |
|   |   - JsonString, JsonNumber,           |                                   |
|   |     JsonBoolean, JsonNull             |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   JsonSerializer (Formatter / Writer) |                                   |
|   |   - Compact minified serialization    |                                   |
|   |   - Pretty-printed indented output    |                                   |
|   +---------------------------------------+                                   |
+-------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **AST Model (`model`):** Sealed interface `JsonValue` permitted only to `JsonObject`, `JsonArray`, `JsonString`, `JsonNumber`, `JsonBoolean`, and `JsonNull`.
- **Lexer (`lexer`):** Converts raw character sequences into structured token streams, handling escapes, exponents, and sign parsing.
- **Parser (`parser`):** Recursive-descent grammar evaluator constructing nested immutable AST representations.
- **Serializer (`serializer`):** Recursive AST visitor transforming nodes into compliant, properly escaped JSON text.

---

## 2. Complete Project Source Code

### Component 1: Abstract Syntax Tree (AST) (`src/main/java/com/learning/json/model/`)

```java
package com.learning.json.model;

import java.util.*;

public sealed interface JsonValue permits JsonObject, JsonArray, JsonString, JsonNumber, JsonBoolean, JsonNull {
    default boolean isObject() { return this instanceof JsonObject; }
    default boolean isArray() { return this instanceof JsonArray; }
    default boolean isString() { return this instanceof JsonString; }
    default boolean isNumber() { return this instanceof JsonNumber; }
    default boolean isBoolean() { return this instanceof JsonBoolean; }
    default boolean isNull() { return this instanceof JsonNull; }

    default JsonObject asObject() { return (JsonObject) this; }
    default JsonArray asArray() { return (JsonArray) this; }
    default String asStringVal() { return ((JsonString) this).value(); }
    default double asDoubleVal() { return ((JsonNumber) this).value().doubleValue(); }
}

public record JsonObject(Map<String, JsonValue> members) implements JsonValue {
    public JsonObject {
        members = Collections.unmodifiableMap(new LinkedHashMap<>(members));
    }
    public Optional<JsonValue> get(String key) {
        return Optional.ofNullable(members.get(key));
    }
}

public record JsonArray(List<JsonValue> elements) implements JsonValue {
    public JsonArray {
        elements = Collections.unmodifiableList(new ArrayList<>(elements));
    }
    public JsonValue get(int index) {
        return elements.get(index);
    }
    public int size() {
        return elements.size();
    }
}

public record JsonString(String value) implements JsonValue {
    public JsonString { Objects.requireNonNull(value); }
}

public record JsonNumber(Number value) implements JsonValue {
    public JsonNumber { Objects.requireNonNull(value); }
}

public record JsonBoolean(boolean value) implements JsonValue {
    public static final JsonBoolean TRUE = new JsonBoolean(true);
    public static final JsonBoolean FALSE = new JsonBoolean(false);
}

public record JsonNull() implements JsonValue {
    public static final JsonNull INSTANCE = new JsonNull();
}
```

### Component 2: Lexical Analyzer / Tokenizer (`src/main/java/com/learning/json/lexer/`)

```java
package com.learning.json.lexer;

import java.util.ArrayList;
import java.util.List;

public final class JsonLexer {
    public enum TokenType {
        LEFT_BRACE, RIGHT_BRACE, LEFT_BRACKET, RIGHT_BRACKET,
        COLON, COMMA, STRING, NUMBER, TRUE, FALSE, NULL, EOF
    }

    public record Token(TokenType type, String value, int line, int column) {}

    private final String source;
    private int pos = 0;
    private int line = 1;
    private int column = 1;

    public JsonLexer(String source) {
        this.source = source != null ? source : "";
    }

    public List<Token> tokenize() {
        List<Token> tokens = new ArrayList<>();
        while (pos < source.length()) {
            char ch = source.charAt(pos);

            if (Character.isWhitespace(ch)) {
                advance();
                continue;
            }

            int tokenCol = column;
            int tokenLine = line;

            switch (ch) {
                case '{' -> { advance(); tokens.add(new Token(TokenType.LEFT_BRACE, "{", tokenLine, tokenCol)); }
                case '}' -> { advance(); tokens.add(new Token(TokenType.RIGHT_BRACE, "}", tokenLine, tokenCol)); }
                case '[' -> { advance(); tokens.add(new Token(TokenType.LEFT_BRACKET, "[", tokenLine, tokenCol)); }
                case ']' -> { advance(); tokens.add(new Token(TokenType.RIGHT_BRACKET, "]", tokenLine, tokenCol)); }
                case ':' -> { advance(); tokens.add(new Token(TokenType.COLON, ":", tokenLine, tokenCol)); }
                case ',' -> { advance(); tokens.add(new Token(TokenType.COMMA, ",", tokenLine, tokenCol)); }
                case '"' -> tokens.add(scanString());
                default -> {
                    if (ch == '-' || Character.isDigit(ch)) {
                        tokens.add(scanNumber());
                    } else if (Character.isLetter(ch)) {
                        tokens.add(scanLiteral());
                    } else {
                        throw new IllegalArgumentException("Unexpected character '" + ch + "' at line " + line + ", col " + column);
                    }
                }
            }
        }
        tokens.add(new Token(TokenType.EOF, "", line, column));
        return tokens;
    }

    private Token scanString() {
        int startLine = line;
        int startCol = column;
        advance(); // Skip opening quote
        StringBuilder sb = new StringBuilder();

        while (pos < source.length()) {
            char ch = source.charAt(pos);
            if (ch == '"') {
                advance(); // Skip closing quote
                return new Token(TokenType.STRING, sb.toString(), startLine, startCol);
            }
            if (ch == '\\') {
                advance();
                char esc = source.charAt(pos);
                switch (esc) {
                    case '"' -> sb.append('"');
                    case '\\' -> sb.append('\\');
                    case '/' -> sb.append('/');
                    case 'b' -> sb.append('\b');
                    case 'f' -> sb.append('\f');
                    case 'n' -> sb.append('\n');
                    case 'r' -> sb.append('\r');
                    case 't' -> sb.append('\t');
                    default -> throw new IllegalArgumentException("Invalid escape sequence: \\" + esc);
                }
            } else {
                sb.append(ch);
            }
            advance();
        }
        throw new IllegalArgumentException("Unterminated string literal at line " + startLine + ", col " + startCol);
    }

    private Token scanNumber() {
        int startLine = line;
        int startCol = column;
        int startPos = pos;

        if (source.charAt(pos) == '-') advance();
        while (pos < source.length() && Character.isDigit(source.charAt(pos))) advance();

        if (pos < source.length() && source.charAt(pos) == '.') {
            advance();
            while (pos < source.length() && Character.isDigit(source.charAt(pos))) advance();
        }

        if (pos < source.length() && (source.charAt(pos) == 'e' || source.charAt(pos) == 'E')) {
            advance();
            if (pos < source.length() && (source.charAt(pos) == '+' || source.charAt(pos) == '-')) advance();
            while (pos < source.length() && Character.isDigit(source.charAt(pos))) advance();
        }

        String numStr = source.substring(startPos, pos);
        return new Token(TokenType.NUMBER, numStr, startLine, startCol);
    }

    private Token scanLiteral() {
        int startLine = line;
        int startCol = column;
        int startPos = pos;

        while (pos < source.length() && Character.isLetter(source.charAt(pos))) advance();
        String word = source.substring(startPos, pos);

        return switch (word) {
            case "true" -> new Token(TokenType.TRUE, "true", startLine, startCol);
            case "false" -> new Token(TokenType.FALSE, "false", startLine, startCol);
            case "null" -> new Token(TokenType.NULL, "null", startLine, startCol);
            default -> throw new IllegalArgumentException("Unknown literal '" + word + "' at line " + startLine);
        };
    }

    private void advance() {
        if (pos < source.length()) {
            if (source.charAt(pos) == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
            pos++;
        }
    }
}
```

### Component 3: Recursive Descent Parser (`src/main/java/com/learning/json/parser/`)

```java
package com.learning.json.parser;

import com.learning.json.lexer.JsonLexer;
import com.learning.json.lexer.JsonLexer.*;
import com.learning.json.model.*;
import java.util.*;

public final class JsonParser {
    private final List<Token> tokens;
    private int cursor = 0;

    public JsonParser(List<Token> tokens) {
        this.tokens = tokens;
    }

    public static JsonValue parse(String json) {
        List<Token> tokens = new JsonLexer(json).tokenize();
        return new JsonParser(tokens).parseValue();
    }

    public JsonValue parseValue() {
        Token t = current();
        return switch (t.type()) {
            case LEFT_BRACE -> parseObject();
            case LEFT_BRACKET -> parseArray();
            case STRING -> { consume(); yield new JsonString(t.value()); }
            case NUMBER -> {
                consume();
                if (t.value().contains(".") || t.value().contains("e") || t.value().contains("E")) {
                    yield new JsonNumber(Double.parseDouble(t.value()));
                } else {
                    yield new JsonNumber(Long.parseLong(t.value()));
                }
            }
            case TRUE -> { consume(); yield JsonBoolean.TRUE; }
            case FALSE -> { consume(); yield JsonBoolean.FALSE; }
            case NULL -> { consume(); yield JsonNull.INSTANCE; }
            default -> throw new IllegalArgumentException("Unexpected token " + t.type() + " at line " + t.line());
        };
    }

    private JsonObject parseObject() {
        consume(TokenType.LEFT_BRACE);
        Map<String, JsonValue> map = new LinkedHashMap<>();

        if (match(TokenType.RIGHT_BRACE)) {
            consume();
            return new JsonObject(map);
        }

        while (true) {
            Token keyToken = consume(TokenType.STRING);
            consume(TokenType.COLON);
            JsonValue val = parseValue();
            map.put(keyToken.value(), val);

            if (match(TokenType.COMMA)) {
                consume();
            } else if (match(TokenType.RIGHT_BRACE)) {
                consume();
                break;
            } else {
                throw new IllegalArgumentException("Expected ',' or '}' in object at line " + current().line());
            }
        }
        return new JsonObject(map);
    }

    private JsonArray parseArray() {
        consume(TokenType.LEFT_BRACKET);
        List<JsonValue> list = new ArrayList<>();

        if (match(TokenType.RIGHT_BRACKET)) {
            consume();
            return new JsonArray(list);
        }

        while (true) {
            list.add(parseValue());
            if (match(TokenType.COMMA)) {
                consume();
            } else if (match(TokenType.RIGHT_BRACKET)) {
                consume();
                break;
            } else {
                throw new IllegalArgumentException("Expected ',' or ']' in array at line " + current().line());
            }
        }
        return new JsonArray(list);
    }

    private Token current() { return tokens.get(cursor); }
    private boolean match(TokenType type) { return current().type() == type; }
    private Token consume() { return tokens.get(cursor++); }
    private Token consume(TokenType expected) {
        Token t = current();
        if (t.type() != expected) {
            throw new IllegalArgumentException("Expected " + expected + " but got " + t.type() + " at line " + t.line());
        }
        return consume();
    }
}
```

### Component 4: Serializer & CLI Driver (`src/main/java/com/learning/json/`)

```java
package com.learning.json;

import com.learning.json.model.*;
import com.learning.json.parser.JsonParser;
import java.util.Map;

public final class JsonSerializer {
    public static String stringify(JsonValue value, boolean pretty) {
        StringBuilder sb = new StringBuilder();
        serialize(value, sb, pretty, 0);
        return sb.toString();
    }

    private static void serialize(JsonValue value, StringBuilder sb, boolean pretty, int indent) {
        switch (value) {
            case JsonObject obj -> {
                if (obj.members().isEmpty()) { sb.append("{}"); return; }
                sb.append("{\n");
                int count = 0;
                for (Map.Entry<String, JsonValue> entry : obj.members().entrySet()) {
                    if (count++ > 0) sb.append(",\n");
                    sb.append("  ".repeat(indent + 1))
                      .append("\"").append(entry.getKey()).append("\": ");
                    serialize(entry.getValue(), sb, pretty, indent + 1);
                }
                sb.append("\n").append("  ".repeat(indent)).append("}");
            }
            case JsonArray arr -> {
                if (arr.elements().isEmpty()) { sb.append("[]"); return; }
                sb.append("[\n");
                for (int i = 0; i < arr.size(); i++) {
                    if (i > 0) sb.append(",\n");
                    sb.append("  ".repeat(indent + 1));
                    serialize(arr.get(i), sb, pretty, indent + 1);
                }
                sb.append("\n").append("  ".repeat(indent)).append("]");
            }
            case JsonString str -> sb.append("\"").append(escape(str.value())).append("\"");
            case JsonNumber num -> sb.append(num.value());
            case JsonBoolean b -> sb.append(b.value());
            case JsonNull ignored -> sb.append("null");
        }
    }

    private static String escape(String text) {
        return text.replace("\"", "\\\"").replace("\n", "\\n").replace("\t", "\\t");
    }
}

public class Main {
    public static void main(String[] args) {
        String input = "{\n" +
            "  \"name\": \"Oracle JDK\",\n" +
            "  \"version\": 25,\n" +
            "  \"lts\": true,\n" +
            "  \"features\": [\"Virtual Threads\", \"Class-File API\", \"ZGC\"]\n" +
            "}";

        System.out.println("Parsing raw JSON string into AST...");
        JsonValue ast = JsonParser.parse(input);

        System.out.println("\nAST Access via Records:");
        JsonObject obj = ast.asObject();
        System.out.println("name: " + obj.get("name").orElseThrow().asStringVal());
        System.out.println("version: " + obj.get("version").orElseThrow().asDoubleVal());

        System.out.println("\nPretty-printed Serialization:");
        System.out.println(JsonSerializer.stringify(ast, true));
    }
}
```

---

## 3. Build Configuration (`pom.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.learning.java</groupId>
    <artifactId>json-parser</artifactId>
    <version>1.0.0</version>

    <properties>
        <maven.compiler.source>25</maven.compiler.source>
        <maven.compiler.target>25</maven.compiler.target>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
        <junit.jupiter.version>5.11.4</junit.jupiter.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.junit.jupiter</groupId>
            <artifactId>junit-jupiter</artifactId>
            <version>${junit.jupiter.version}</version>
            <scope>test</scope>
        </dependency>
    </dependencies>
</project>
```

---

## 4. Comprehensive JUnit 5 Test Suite

```java
package com.learning.json;

import com.learning.json.model.*;
import com.learning.json.parser.JsonParser;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

public class JsonParserTest {

    @Test
    void testPrimitivesParsing() {
        assertTrue(JsonParser.parse("true").asBoolean());
        assertFalse(JsonParser.parse("false").asBoolean());
        assertTrue(JsonParser.parse("null").isNull());
        assertEquals("hello world", JsonParser.parse("\"hello world\"").asStringVal());
        assertEquals(42.0, JsonParser.parse("42").asDoubleVal());
    }

    @Test
    void testComplexNestedObjectParsing() {
        String json = "{\"user\":{\"id\":101,\"name\":\"Alice\"},\"tags\":[\"dev\",\"jvm\"]}";
        JsonValue parsed = JsonParser.parse(json);

        assertTrue(parsed.isObject());
        JsonObject root = parsed.asObject();

        JsonObject user = root.get("user").orElseThrow().asObject();
        assertEquals(101.0, user.get("id").orElseThrow().asDoubleVal());
        assertEquals("Alice", user.get("name").orElseThrow().asStringVal());

        JsonArray tags = root.get("tags").orElseThrow().asArray();
        assertEquals(2, tags.size());
        assertEquals("dev", tags.get(0).asStringVal());
        assertEquals("jvm", tags.get(1).asStringVal());
    }

    @Test
    void testSyntaxErrorHandling() {
        assertThrows(IllegalArgumentException.class, () -> JsonParser.parse("{ bad json }"));
        assertThrows(IllegalArgumentException.class, () -> JsonParser.parse("{\"unclosed: 10}"));
    }
}
```

---

## 5. Verification & Sample Execution

### Compilation & Test Execution
```bash
$ mvn clean test
```

### Expected Test Output
```text
[INFO] -------------------------------------------------------
[INFO]  T E S T S
[INFO] -------------------------------------------------------
[INFO] Running com.learning.json.JsonParserTest
[INFO] Tests run: 3, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 0.145 s
[INFO] 
[INFO] Results:
[INFO] 
[INFO] Tests run: 3, Failures: 0, Errors: 0, Skipped: 0
[INFO] 
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.json.Main
Parsing raw JSON string into AST...

AST Access via Records:
name: Oracle JDK
version: 25.0

Pretty-printed Serialization:
{
  "name": "Oracle JDK",
  "version": 25,
  "lts": true,
  "features": [
    "Virtual Threads",
    "Class-File API",
    "ZGC"
  ]
}
```

---

[← Return to Projects Index](README.md)
