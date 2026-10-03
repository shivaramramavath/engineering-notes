# Text Blocks

A **text block** is a multi-line string literal delimited by `"""`. It removes the noise of `\n`, `+` and escaped quotes when embedding JSON, SQL, HTML or any multi-line text. Standard since **Java 15**.

```java
String json = """
        {
          "name": "Ada",
          "age": 36
        }
        """;
```

Result: `{\n  "name": "Ada",\n  "age": 36\n}\n`

Compare with the old way:

```java
String json = "{\n" +
              "  \"name\": \"Ada\",\n" +
              "  \"age\": 36\n" +
              "}\n";
```

## Syntax rules

| Rule | Detail |
|------|--------|
| Opening delimiter | `"""` followed by a **line break** (only whitespace may follow it on that line) |
| Content | Starts on the next line |
| Closing delimiter | `"""` anywhere after the content |
| Type | An ordinary `String`, indistinguishable from other strings at runtime |
| Quotes | Single `"` and even `""` need no escaping |

```java
String ok  = """
    She said "hello".
    """;
String bad = """Hello""";          // ERROR: no line break after the opening """
```

## How indentation is handled

The compiler removes **incidental** indentation: the common leading whitespace shared by all lines. What remains is **essential** indentation, which you keep.

```java
String html = """
        <html>
          <body>
            <p>Hello</p>
          </body>
        </html>
        """;
```

```
|        <html>               ┐
|          <body>             │  The smallest indentation across all non-blank lines
|            <p>Hello</p>     │  (including the line with the closing """) is removed.
|          </body>            │
|        </html>              │  Here that is 8 spaces.
|        """;                 ┘
```

Result:

```
<html>
  <body>
    <p>Hello</p>
  </body>
</html>
```

### The closing delimiter controls indentation and the final newline

```java
String a = """
        text
        """;                    // "text\n"          (closing """ on its own line, trailing newline)

String b = """
        text""";                // "text"            (no trailing newline)

String c = """
        text
    """;                        // "    text\n"      (closing """ is indented less: 4 spaces are KEPT)
```

Moving the closing `"""` to the left reduces the stripped indentation, and moving it to the end of the last line removes the final newline.

### Other normalization

- **Trailing spaces** on each line are removed (use `\s` to keep one)
- **Line endings** become `\n` regardless of the source file's line endings
- Mixing tabs and spaces in the indentation makes the result confusing; use one

## Escape sequences

All normal escapes work (`\n`, `\t`, `\"`, `\\`, `\u0041`), plus two new ones:

| Escape | Meaning |
|--------|---------|
| `\<line terminator>` | Line continuation: joins the next line, no newline inserted |
| `\s` | A single space; protects trailing whitespace from stripping |

```java
String oneLine = """
        This is a very long sentence \
        written over two source lines.
        """;
// "This is a very long sentence written over two source lines.\n"

String padded = """
        name:\s\s\s
        age:\s\s\s\s
        """;                        // trailing spaces preserved

String quotes = """
        Triple quotes: \"""
        """;                        // escape at least one quote to write """
```

## Inserting values

Text blocks have no built-in interpolation. Use `formatted`, `String.format` or `replace`:

```java
String message = """
        Hello, %s!
        You have %d new messages.
        """.formatted(user, count);

String sql = """
        SELECT id, name
        FROM users
        WHERE active = true
        ORDER BY name
        """;
```

`formatted` is `String.format` as an instance method (Java 15+). See [04_string-formatting.md](./04_string-formatting.md). A literal `%` in the block must be written `%%` if you format it.

## Useful companion methods

| Method | Purpose |
|--------|---------|
| `stripIndent()` | Applies the same indentation rule to any string |
| `translateEscapes()` | Interprets escape sequences like `\n` in a string at runtime |
| `lines()` | Streams the lines of the block |
| `strip()` / `trim()` | Remove the surrounding whitespace |
| `indent(n)` | Adds or removes indentation (also normalizes line endings) |

```java
long lineCount = block.lines().count();
String compact = block.lines().map(String::strip).collect(Collectors.joining());
```

## Typical uses

| Use | Example |
|-----|---------|
| JSON test fixtures | Expected payloads in tests ([19-testing](../19-testing/README.md)) |
| SQL | Readable multi-line queries ([16-jdbc-and-databases](../16-jdbc-and-databases/README.md)) |
| HTML / XML / YAML snippets | Templates, emails |
| Regular expressions with many backslashes | Less escaping noise ([regex](./06_regex.md)) |
| Help and usage text | CLI tools |
| Code generation | Source templates |

Prefer files in `src/main/resources` for large or frequently edited text (HTML pages, long SQL scripts, templates). Text blocks are best for **small, local** pieces of text.

## Security warning

A text block is still just a string. **Never build SQL, HTML or shell commands by inserting user input** into it:

```java
String sql = """
        SELECT * FROM users WHERE name = '%s'
        """.formatted(userInput);                 // SQL injection!
```

Use prepared statements and parameters ([16-jdbc-and-databases/02_statements-and-prepared-statements.md](../16-jdbc-and-databases/02_statements-and-prepared-statements.md), [21-security/00_input-validation-and-injection.md](../21-security/00_input-validation-and-injection.md)).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Content on the same line as the opening `"""` | Compile error | Start content on the next line |
| Expecting no trailing newline | An extra `\n` at the end | Put `"""` at the end of the last content line, or `strip()` |
| Closing `"""` less indented than the text | Extra leading spaces in every line | Align it with, or to the left of, the text deliberately |
| Mixing tabs and spaces | Uneven indentation | Use spaces only |
| Trailing spaces disappear | Output differs from the source | Use `\s` |
| `%` in text passed to `formatted` | `UnknownFormatConversionException` | Write `%%` |
| User input concatenated into SQL or HTML | Injection vulnerabilities | Parameters and escaping |
| Expecting `${name}`-style interpolation | Printed literally | `formatted` or `replace` |
| Using `\n` for line continuation | An actual newline appears | End the line with `\` |
| Large text inside Java source | Hard to edit and review | Resource files |

## Key takeaways

- `"""` + line break starts a text block; it produces a normal `String`
- Incidental indentation is stripped; the closing delimiter position matters
- `\` continues a line, `\s` keeps a space, quotes need no escaping
- Insert values with `formatted`, and never concatenate untrusted input into SQL or HTML
- Use text blocks for small embedded snippets; use resource files for big ones

**Next:** [Regular Expressions](./06_regex.md)
