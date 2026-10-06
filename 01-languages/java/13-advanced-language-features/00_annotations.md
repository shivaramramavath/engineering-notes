# Annotations

An **annotation** is metadata you attach to a program element (class, method, field, parameter, ...). It doesn't change what the code does by itself. Instead, something else (the compiler, a build tool, or a framework at runtime) **reads it and acts on it**.

```java
@Override
public String toString() { ... }          // the compiler checks that this really overrides something

@Test
void addsNumbers() { ... }                // JUnit finds and runs this

@Entity @Table(name = "users")
class User { ... }                        // an ORM maps this class to a table
```

Think of an annotation as a **sticky note** with a name and optional data, which a tool can find and interpret.

**Prerequisites:** [Interfaces](../04-oop/09_interfaces.md), [Enums](../04-oop/11_enums.md). Section 5 uses [Reflection](01_reflection.md).

---

## 1. Using annotations

```java
@Deprecated                                  // marker: no elements
@SuppressWarnings("unchecked")               // single element named `value`: shorthand
@Table(name = "users", schema = "public")    // named elements
@Roles({"ADMIN", "AUDITOR"})                 // array element
@Roles("ADMIN")                              // a one-element array may omit the braces
```

Annotations can sit on: classes, interfaces, enums, records, methods, constructors, fields, parameters, local variables, packages, modules, type parameters, and (Java 8+) **type uses** such as `List<@NonNull String>`.

Element values must be **compile-time constants**: primitives, `String`, `Class` literals, enum constants, other annotations, or arrays of those. `null` is not allowed.

---

## 2. Built-in annotations you should know

| Annotation | Purpose |
|---|---|
| `@Override` | Compile error if the method doesn't override/implement anything (catches typos and signature drift) |
| `@Deprecated(since = "2.0", forRemoval = true)` | Marks an API as discouraged; `forRemoval = true` produces stronger warnings |
| `@SuppressWarnings("unchecked")` | Silences specific compiler warnings. Keep it on the **smallest** scope |
| `@FunctionalInterface` | Compile error if the interface isn't a valid functional interface ([Functional Interfaces](../09-functional-java/01_functional-interfaces.md)) |
| `@SafeVarargs` | Declares that a generic varargs method is safe (heap pollution) |
| `@Serial` (Java 14+) | Marks serialization members so the compiler checks their signatures ([Serialization](../11-io-and-networking/03_java-serialization.md)) |

Always use `@Override`: it costs nothing and prevents a whole class of silent bugs (for example `equals(Foo o)` instead of `equals(Object o)`).

---

## 3. Declaring your own

An annotation type is declared with `@interface`:

```java
import java.lang.annotation.*;

@Retention(RetentionPolicy.RUNTIME)       // keep it available at runtime
@Target(ElementType.FIELD)                // may only be placed on fields
public @interface MaxLength {
    int value();                          // required element
    String message() default "too long";  // optional: has a default
}
```

```java
public class User {
    @MaxLength(20) private String name;
    @MaxLength(value = 100, message = "bio too long") private String bio;
}
```

Rules for elements:

- Declared like parameterless methods: `int value();`
- Allowed types: primitives, `String`, `Class<?>`, enums, other annotations, and **one-dimensional arrays** of those.
- Defaults via `default`. An element named **`value`** can be given without its name when it's the only one supplied.
- Annotation types can't extend other types or be generic.

### Meta-annotations: annotations on annotations

| Meta-annotation | Controls |
|---|---|
| `@Retention` | How long the annotation survives (see below) |
| `@Target` | Where it may be placed (`TYPE`, `FIELD`, `METHOD`, `PARAMETER`, `CONSTRUCTOR`, `LOCAL_VARIABLE`, `TYPE_USE`, `RECORD_COMPONENT`, `ANNOTATION_TYPE`, `PACKAGE`, `MODULE`, ...) |
| `@Documented` | Include it in generated Javadoc |
| `@Inherited` | A class-level annotation is also seen on **subclasses** (not interfaces, not methods) |
| `@Repeatable` | Allows the same annotation more than once (requires a container annotation) |

### Retention: the setting that most often bites

| `RetentionPolicy` | Kept in source | Kept in `.class` | Visible via reflection |
|---|---|---|---|
| `SOURCE` | Yes | No | No (e.g. `@Override`) |
| `CLASS` (**default**) | Yes | Yes | **No** |
| `RUNTIME` | Yes | Yes | **Yes** |

If you plan to read an annotation with reflection and forgot `@Retention(RUNTIME)`, it will silently *not be there*: the most common annotation bug.

```java
@Retention(RetentionPolicy.RUNTIME)
@Repeatable(Roles.class)
public @interface Role { String value(); }

@Retention(RetentionPolicy.RUNTIME)
public @interface Roles { Role[] value(); }       // container

@Role("ADMIN") @Role("AUDITOR")
class ReportService {}
```

---

## 4. Annotations and records

For record components, an annotation's `@Target` decides where it ends up. For example `@NotNull` on `record User(@NotNull String name)` applies to the component, field, accessor, or constructor parameter depending on which targets the annotation allows. If it allows several, it's propagated to all of them. This is why validation annotations work on records ([Records](../12-modern-java/01_records.md)).

---

## 5. Who reads annotations?

Annotations are inert until something processes them. There are three common processing times:

### Compile time: annotation processors

Tools that run **inside `javac`**, inspect annotated code, and typically **generate new source files** or report errors. Examples: MapStruct, Dagger, and (with a different technique that modifies the compiler's tree) Lombok.

- Zero runtime reflection, which means faster startup and friendly to native-image builds.
- Processors are configured in the build ([Maven](../18-build-and-dependencies/00_maven.md), [Gradle](../18-build-and-dependencies/01_gradle.md)). Since JDK 23, `javac` no longer runs processors it discovers on the classpath by default, so build configuration should specify them explicitly (processor path or `-processor`/`-proc:full`), or code generation silently stops.

### Runtime: reflection

A framework scans classes at startup (or on demand), reads `RUNTIME` annotations, and acts. Spring (`@Service`, `@Transactional`), JUnit (`@Test`), Jackson (`@JsonProperty`), and Hibernate (`@Entity`) all work this way.

```java
public static List<String> validate(Object obj) throws IllegalAccessException {
    List<String> errors = new ArrayList<>();

    for (Field f : obj.getClass().getDeclaredFields()) {
        MaxLength max = f.getAnnotation(MaxLength.class);   // null if absent
        if (max == null) continue;

        f.setAccessible(true);
        if (f.get(obj) instanceof String s && s.length() > max.value()) {
            errors.add(f.getName() + ": " + max.message());
        }
    }
    return errors;
}
```

The API: `isAnnotationPresent(X.class)`, `getAnnotation(X.class)`, `getAnnotationsByType(X.class)` (handles repeatables), `getDeclaredAnnotations()`. They exist on `Class`, `Method`, `Field`, `Constructor`, `Parameter`, and `RecordComponent`. See [Reflection](01_reflection.md).

### Build and tooling time

Static analyzers, IDEs, and bytecode tools read annotations from `.class` files (`CLASS` retention is enough), for example nullness checkers or code-coverage exclusions.

---

## 6. Design guidance

- Annotations suit **declarative, cross-cutting configuration**: "this method is transactional", "this field is required", "this endpoint is `GET /users`".
- They are a poor fit for **core business logic**. Behavior hidden behind annotations is harder to trace, test, and debug than explicit code.
- Keep your annotations **small and focused**, with sensible defaults, and document the retention and target.
- Remember that *nothing happens without a processor*. An annotation with no reader is just a comment that the compiler checks the syntax of.
- Annotation-driven frameworks have runtime costs (classpath scanning, reflection) and "magic" that can fail at startup instead of compile time. Expect to read stack traces through proxy and framework layers ([Dynamic Proxies](02_dynamic-proxies.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Custom annotation not visible via reflection | Add `@Retention(RetentionPolicy.RUNTIME)` |
| Annotation placed where `@Target` doesn't allow it | Check `@Target`. The compiler reports it |
| Expecting `@Inherited` to work on methods or interfaces | It applies to **class** annotations inherited from **superclasses** only |
| Repeating an annotation without `@Repeatable` | Add `@Repeatable` + container annotation |
| `@SuppressWarnings("all")` on a whole class | Suppress specific warnings on the narrowest scope |
| Missing `@Override`, then wondering why the method isn't called | Always add it |
| Using `null` or a non-constant as an element value | Only constants, enums, `Class` literals, etc. |
| Processor-based code generation stops after upgrading the JDK | Configure annotation processors explicitly in the build |
| Putting business rules into annotations that need reflection to evaluate | Keep logic in code |

### Debugging

- `getAnnotation` returns `null` → check retention, check that you queried the right element (field vs. accessor vs. constructor parameter on records), and that the annotation class is the same one (same class loader).
- "annotation type not applicable to this kind of declaration" → fix `@Target` or move the annotation.
- Annotation works in the IDE but not in the build → annotation processor configuration or classpath differs between IDE and build tool.
- Framework ignores your annotation → is it on a bean the framework actually manages? Annotations on a class instantiated manually with `new` are usually never processed.

---

## Quick Summary

- Annotations are **metadata**: they do nothing until a compiler, tool, or framework reads them.
- Declare with `@interface`. Control it with `@Retention` (**use `RUNTIME` to read via reflection**), `@Target`, `@Inherited`, and `@Repeatable`.
- Built-ins to use habitually: `@Override`, `@FunctionalInterface`, `@Deprecated`, targeted `@SuppressWarnings`.
- Three processing times: compile (annotation processors, code generation), runtime (reflection), tooling.
- Prefer them for declarative configuration, not business logic.

**Next:** [Reflection](01_reflection.md)