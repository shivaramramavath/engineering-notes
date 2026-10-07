# Bytecode

`javac` doesn't produce machine code. It produces **bytecode**: compact, platform-independent instructions for an imaginary stack-based processor, stored in `.class` files. The JVM loads that bytecode, interprets it, and compiles the hot parts to real machine code ([JIT Compiler](06_jit-compiler.md)).

You don't need to write bytecode, but being able to *read* it with `javap` answers questions no source-level reasoning can: "Is this boxing?", "What does `+=` on a string compile to?", "Why does `count++` race?", "What is a lambda really?".

**Prerequisites:** [JVM Architecture](00_jvm-architecture.md), [Class Loading](01_class-loading.md).

---

## 1. Looking at bytecode with `javap`

```java
public class Demo {
    int add(int a, int b) { return a + b; }
}
```

```bash
javac Demo.java
javap -c -p Demo.class          # -c: disassemble code, -p: include private members
javap -v Demo.class             # verbose: constant pool, flags, stack/locals sizes, attributes
```

```text
int add(int, int);
  Code:
     0: iload_1
     1: iload_2
     2: iadd
     3: ireturn
```

Each line is `offset: instruction`. This method:

1. pushes local variable 1 (`a`) onto the **operand stack**,
2. pushes local variable 2 (`b`),
3. pops both, adds them, pushes the result (`iadd`),
4. returns the top of the stack (`ireturn`).

Local variable 0 is `this` in instance methods, which is why `a` and `b` are slots 1 and 2. In a `static` method, slot 0 would be `a`.

---

## 2. The execution model: a stack machine

Each method call gets a **frame** with two parts:

```text
 Frame
 ├─ Local variable array   slot 0..n: this, parameters, local variables
 └─ Operand stack          scratch space for computing; instructions push and pop here
```

Instructions work by moving values between locals and the operand stack:

```text
 iload_1     push local[1]        stack: [a]
 iload_2     push local[2]        stack: [a, b]
 iadd        pop 2, push sum      stack: [a+b]
 ireturn     pop and return       stack: []
```

There are no registers in the bytecode. (The JIT maps this to real registers later.)

### Reading the instruction names

The first letter tells you the **type**: `i` int (also byte/short/char/boolean), `l` long, `f` float, `d` double, `a` reference.

| Group | Examples |
|---|---|
| Load/store locals | `iload_1`, `aload_0`, `istore_2` |
| Constants | `iconst_0`..`iconst_5`, `bipush 10`, `ldc "text"` (from the constant pool) |
| Arithmetic | `iadd`, `isub`, `imul`, `idiv`, `ladd`, `dadd`, `iinc 2, 1` (increment a local) |
| Stack manipulation | `dup`, `pop`, `swap` |
| Branches | `ifeq`, `if_icmpgt`, `goto`, `tableswitch`, `lookupswitch` |
| Objects | `new`, `getfield`, `putfield`, `getstatic`, `putstatic`, `instanceof`, `checkcast` |
| Arrays | `newarray`, `anewarray`, `iaload`, `iastore`, `arraylength` |
| Calls | `invokevirtual`, `invokeinterface`, `invokespecial`, `invokestatic`, `invokedynamic` |
| Return/throw | `ireturn`, `areturn`, `return`, `athrow` |
| Monitors | `monitorenter`, `monitorexit` |

---

## 3. A loop and a branch

```java
static int sum(int n) {
    int s = 0;
    for (int i = 1; i <= n; i++) s += i;
    return s;
}
```

```text
     0: iconst_0
     1: istore_1          // s = 0
     2: iconst_1
     3: istore_2          // i = 1
     4: iload_2           // ← loop start: push i
     5: iload_0           //   push n
     6: if_icmpgt     19  //   if i > n jump to 19 (exit)
     9: iload_1
    10: iload_2
    11: iadd
    12: istore_1          //   s = s + i
    13: iinc          2, 1//   i++
    16: goto          4   //   back to the loop test
    19: iload_1
    20: ireturn           // return s
```

Control flow is just conditional jumps to offsets. `for`, `while`, `if`, and `switch` all compile to this style.

---

## 4. Method invocation instructions

| Instruction | Used for |
|---|---|
| `invokevirtual` | Instance method on a class type, dispatched by the object's **runtime class** (virtual dispatch, [Polymorphism](../04-oop/07_polymorphism.md)) |
| `invokeinterface` | Method called through an **interface** type |
| `invokespecial` | Constructors (`<init>`) and `super.method()` calls |
| `invokestatic` | Static methods |
| `invokedynamic` | **Late-bound** call sites: the first execution calls a *bootstrap method* that decides how the call links. Used for lambdas, string concatenation, records, and pattern switches |

Which instruction is chosen is why `final` and `private` calls are cheap to optimize, and why interface calls can cost more than class calls at megamorphic call sites ([JIT Compiler](06_jit-compiler.md)).

---

## 5. What the compiler does to your code ("desugaring")

`javac` rewrites many Java conveniences into plain bytecode. Seeing these explains behavior and performance:

| Source construct | Bytecode reality |
|---|---|
| `Integer x = 5;` / `int y = x;` | `Integer.valueOf(5)` / `x.intValue()`: **boxing allocates** (cached for -128..127) |
| `for (T t : list)` | `Iterator` calls (`iterator()`, `hasNext()`, `next()`); for arrays, an indexed loop |
| `"a" + b + "c"` | `invokedynamic` to `StringConcatFactory` (Java 9+). Before 9: a `StringBuilder` chain |
| Lambda `x -> x + 1` | A private synthetic method (`lambda$main$0`) plus `invokedynamic` to `LambdaMetafactory`, which generates a hidden class at runtime. It is **not** an anonymous inner class file |
| `switch` on a `String` | `hashCode()` lookup, then `equals` to confirm |
| Generics `List<String>` | **Erased** to `List`, with `checkcast String` inserted at use sites, plus synthetic **bridge methods** ([Type Erasure](../07-generics/03_type-erasure.md)) |
| `synchronized` block | `monitorenter` / `monitorexit` (with an exception handler to release). A `synchronized` *method* just has the `ACC_SYNCHRONIZED` flag |
| `try`-with-resources | `try`/`finally`-like code that calls `close()` and `addSuppressed` |
| Inner class accessing outer fields | A hidden `this$0` reference. Since Java 11, nest-based access control (nestmates) replaced synthetic `access$000` methods |
| `enum` | A class extending `java.lang.Enum` with a `$VALUES` array and `values()`/`valueOf` |
| `record` | A class extending `java.lang.Record`. `toString`/`equals`/`hashCode` are linked through `invokedynamic` |
| Pattern `switch` (21+) | `invokedynamic` to `SwitchBootstraps.typeSwitch`, then a normal switch on the index |
| `static final` constants | **Inlined** into callers at compile time |
| varargs | An array created at the call site |

### Example: why `count++` is not atomic

```java
class Counter {
    int count;
    void inc() { count++; }
}
```

```text
void inc();
  Code:
     0: aload_0
     1: dup
     2: getfield      #7   // Field count:I      ← READ
     5: iconst_1
     6: iadd                                      ← MODIFY
     7: putfield      #7   // Field count:I      ← WRITE
    10: return
```

A read, an add, and a write are separate instructions, and another thread can run between any two of them. That's the lost-update race from [Thread Safety](../14-concurrency/02_thread-safety.md), visible directly in the bytecode.

### Example: boxing in a loop

```java
Long total = 0L;
for (int i = 0; i < 1000; i++) total += i;     // each += unboxes, adds, and boxes a NEW Long
```

`javap -c` shows `Long.valueOf` and `longValue()` calls inside the loop. Using a primitive `long total` removes the allocations ([Wrapper Classes and Autoboxing](../01-fundamentals/04_wrapper-classes-and-autoboxing.md)).

---

## 6. The class file

A `.class` file is a structured binary:

```text
 magic number      0xCAFEBABE
 version           minor, major     ← major: 52 = Java 8, 55 = 11, 61 = 17, 65 = 21, 69 = 25
 constant pool     strings, names, numbers, and symbolic references used by the code
 access flags      public, final, interface, abstract, ...
 this / super / interfaces
 fields
 methods           each with a Code attribute (the bytecode, max stack, max locals, exception table)
 attributes        SourceFile, LineNumberTable, StackMapTable, annotations (RuntimeVisibleAnnotations),
                   Record, PermittedSubclasses, NestMembers, BootstrapMethods, ...
```

- The **major version** is why running a Java 25-compiled class on Java 17 fails with `UnsupportedClassVersionError`. Compile with `--release N` to target an older runtime ([Release Timeline](../12-modern-java/00_java-release-timeline.md)).
- The **`LineNumberTable`** maps offsets to source lines. It's what stack traces use. Compiling with `-g` also keeps local variable names for debuggers.
- **Annotations** with `RUNTIME` retention are stored as attributes ([Annotations](../13-advanced-language-features/00_annotations.md)).
- Newer language features show up as attributes: `Record`, `PermittedSubclasses` (sealed classes), `NestMembers`.

### Verification

During linking, the JVM **verifies** each method's bytecode. It checks that stack depths and types are consistent on every path, using the `StackMapTable` attribute that `javac` emits. This guarantees that, for example, you can't treat an `int` as a reference or underflow the operand stack, which is part of Java's memory safety. Hand-edited or badly generated bytecode fails with `VerifyError`.

---

## 7. Tools that read and write bytecode

| Tool | Use |
|---|---|
| `javap` | Inspect (built into the JDK) |
| **Class-File API** (`java.lang.classfile`, final in Java 24) | The JDK's official API to parse and generate class files |
| ASM, ByteBuddy, Javassist | Libraries that generate and transform bytecode. Frameworks use them for proxies, mocks (Mockito), ORM lazy loading, and instrumentation |
| Java agents (`-javaagent`) | Instrument classes as they load (profilers, APM tools, coverage like JaCoCo) |
| Decompilers (IntelliJ's built-in, CFR, Fernflower) | Recover approximate source from bytecode |

Frameworks that generate classes at runtime are why some libraries need updating when the class-file version changes. Old ASM versions can't read newer class files.

---

## 8. When bytecode knowledge pays off

- **Understanding performance claims:** boxing, string concatenation, and lambda cost are visible, and most microbenchmark folklore dissolves when you read the code.
- **Debugging weird behavior:** synthetic methods and bridge methods in stack traces, "where did this class come from", constant inlining surprises (changing a `static final` constant in a library requires recompiling dependents).
- **Understanding frameworks:** how proxies, instrumentation, and agents hook in ([Dynamic Proxies](../13-advanced-language-features/02_dynamic-proxies.md)).
- **Interview and exam questions** about `String` pooling, `finally`, or `i = i++`.

Don't try to optimize at the bytecode level. `javac` does little optimization on purpose, and the **JIT** reshapes the code at runtime, so bytecode size or instruction count is a poor predictor of speed.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Guessing what a construct compiles to | `javap -c -p` it |
| Optimizing source by bytecode instruction count | Measure with JMH instead ([Benchmarking](../20-performance/01_benchmarking-with-jmh.md)) |
| Assuming lambdas are inner-class files | They use `invokedynamic` and a synthetic method |
| Changing a `static final` constant in a dependency and not recompiling the callers | Constants are inlined. Recompile dependents |
| Reading only `javap` without `-p` and missing private/synthetic methods | Use `-p` (and `-v` for the full picture) |
| Running new-class-version code on an old JVM | `--release N` and CI on the target JVM |
| Using an old ASM/ByteBuddy with new JDK classes | Upgrade the library when moving JDK versions |

### Debugging tips

- Confusing stack trace frames like `lambda$run$0` or `access$000` or `$Proxy12` → synthetic/generated code. Find the real caller beneath it.
- To see what class a lambda actually becomes at runtime, use `-Djdk.internal.lambda.dumpProxyClasses=<dir>` (an internal option that has changed between versions), or just read the `invokedynamic` and bootstrap entries in `javap -v`.
- `javap -v` on a mysterious class shows which Java version built it (`major version`) and which attributes it carries.

---

## Quick Summary

- Bytecode is a **stack-machine** instruction set stored in `.class` files. Each method frame has locals and an operand stack. Instruction prefixes encode types (`i`, `l`, `f`, `d`, `a`).
- Use **`javap -c -p`** to read it. Control flow is conditional jumps, and calls use `invokevirtual`/`interface`/`special`/`static`/`dynamic`.
- `javac` desugars boxing, `for-each`, string concatenation, lambdas, generics (erasure + casts + bridges), enums, records, and pattern switches, and inlines constants.
- A class file has a magic number, a **version** (52 = Java 8 ... 65 = 21, 69 = 25), a constant pool, members, and attributes. The JVM **verifies** the bytecode when linking.
- Bytecode reading explains non-atomic `count++`, hidden boxing, and synthetic frames. Optimization itself happens in the **JIT**, not in `javac`.

**Next:** [Garbage Collection](04_garbage-collection.md)