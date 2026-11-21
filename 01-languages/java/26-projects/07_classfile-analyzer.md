# Project 07: Bytecode Disassembler & Class-File Analyzer (`classfile-analyzer`)

> Static Java `.class` file bytecode disassembler, constant pool decoder, method instruction scanner, and attribute inspector powered by the modern Java 24+ Class-File API (JEP 484).

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-07-classfile-analyzer` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | Advanced / Production-Grade |
| **Architectural Style** | Static Analysis / Bytecode Inspector |
| **Core Concepts** | `java.lang.classfile` (Class-File API), JVM Bytecode, Constant Pool, Opcode Inspection, Method Models, Attributes |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture

Prior to Java 22–24, inspecting or generating JVM bytecode required third-party libraries such as ASM, Byte Buddy, or Javassist. With the arrival of the standardized **Class-File API** (`java.lang.classfile`, finalized in Java 24 via JEP 484 and standard in Java 25 LTS), the JDK provides first-class, high-level, and immutable models for parsing, transforming, and inspecting `.class` binaries.

This project implements a command-line Java bytecode disassembler and class-file inspector (similar to `javap -c -v`) built natively on the JDK 25 Class-File API.

```
+-------------------------------------------------------------------------------+
|                       CLASS-FILE ANALYZER ARCHITECTURE                        |
|                                                                               |
|   Target `.class` Binary / Byte Array                                         |
|         |                                                                     |
|         v                                                                     |
|   +---------------------------------------+                                   |
|   |   ClassFile.of().parse(byte[])        |                                   |
|   |   - Parses Magic Number: 0xCAFEBABE   |                                   |
|   |   - Decodes Major/Minor Version       |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   ClassModel (Root Node)              |                                   |
|   |   - Class Symbol (this_class, super)  |                                   |
|   |   - Implemented Interfaces            |                                   |
|   |   - Access Flags (public, final, etc.)|                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|            +----------+----------+                                            |
|            |                     |                                            |
|            v                     v                                            |
|   +------------------+  +-----------------------------------+                 |
|   | ConstantPool     |  | MethodModel & FieldModel Elements |                 |
|   | - UTF8, Classes, |  | - Method Name, Signature (Type)   |                 |
|   |   MethodRefs,    |  | - Access Flags                    |                 |
|   |   NameAndTypes   |  +-----------------+-----------------+                 |
|   +------------------+                    |                                   |
|                                           v                                   |
|                         +-----------------------------------+                 |
|                         | CodeModel (Method Body)           |                 |
|                         | - Stack Depth, Local Variables    |                 |
|                         | - Instruction Disassembler Loop   |                 |
|                         |   (iload, invokevirtual, return)  |                 |
|                         | - Exception Handlers Table        |                 |
|                         +-----------------+-----------------+                 |
|                                           |                                   |
|                                           v                                   |
|                         +-----------------------------------+                 |
|                         | Disassembled Output Formatter     |                 |
|                         | (Terminal Text Representation)    |                 |
|                         +-----------------------------------+                 |
+-------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Class Header Decoder (`inspector`):** Extracts class name, superclass, interfaces, flags, and major bytecode version.
- **Constant Pool Viewer (`inspector`):** Iterates over the constant pool entries (Utf8, String, MethodRef, FieldRef).
- **Instruction Disassembler (`disassembler`):** Scans the `CodeModel` of each method, printing opcode mnemonics, byte offsets, branch targets, and operand values.
- **CLI Harness (`Main`):** Reads `.class` files from disk or JAR archives and outputs human-readable assembly.

---

## 2. Complete Project Source Code

### Component 1: Analysis Domain Models (`src/main/java/com/learning/bytecode/model/`)

```java
package com.learning.bytecode.model;

import java.util.*;

public record ClassSummary(
    String className,
    String superClassName,
    List<String> interfaces,
    int majorVersion,
    int minorVersion,
    Set<String> accessFlags,
    int constantPoolSize,
    int fieldCount,
    int methodCount
) {}

public record DisassembledInstruction(
    int bci, // Bytecode Index / Offset
    String mnemonic,
    String operands
) {}

public record MethodDisassembly(
    String methodName,
    String descriptor,
    Set<String> flags,
    int maxStack,
    int maxLocals,
    List<DisassembledInstruction> instructions
) {}
```

### Component 2: Class-File API Inspector Engine (`src/main/java/com/learning/bytecode/engine/`)

```java
package com.learning.bytecode.engine;

import com.learning.bytecode.model.*;
import java.io.*;
import java.lang.classfile.*;
import java.lang.classfile.instruction.*;
import java.nio.file.*;
import java.util.*;

public final class ClassFileAnalyzer {
    private final ClassFile classFile = ClassFile.of();

    public ClassSummary summarize(byte[] bytes) {
        ClassModel cm = classFile.parse(bytes);

        Set<String> flags = new LinkedHashSet<>();
        cm.flags().flags().forEach(f -> flags.add(f.name()));

        List<String> ifaces = new ArrayList<>();
        cm.interfaces().forEach(intf -> ifaces.add(intf.asInternalName()));

        return new ClassSummary(
            cm.thisClass().asInternalName(),
            cm.superclass().map(ClassModel::thisClass).map(Object::toString).orElse("java/lang/Object"),
            Collections.unmodifiableList(ifaces),
            cm.majorVersion(),
            cm.minorVersion(),
            Collections.unmodifiableSet(flags),
            cm.constantPool().size(),
            cm.fields().size(),
            cm.methods().size()
        );
    }

    public List<MethodDisassembly> disassembleMethods(byte[] bytes) {
        ClassModel cm = classFile.parse(bytes);
        List<MethodDisassembly> results = new ArrayList<>();

        for (MethodModel mm : cm.methods()) {
            Set<String> flags = new LinkedHashSet<>();
            mm.flags().flags().forEach(f -> flags.add(f.name()));

            int maxStack = 0;
            int maxLocals = 0;
            List<DisassembledInstruction> instructions = new ArrayList<>();

            Optional<CodeModel> codeOpt = mm.code();
            if (codeOpt.isPresent()) {
                CodeModel code = codeOpt.get();
                maxStack = code.maxStack();
                maxLocals = code.maxLocals();

                for (CodeElement element : code) {
                    if (element instanceof Instruction instr) {
                        String operands = formatOperands(instr);
                        instructions.add(new DisassembledInstruction(
                            instr.opcode().bytecode(),
                            instr.opcode().name().toLowerCase(Locale.ROOT),
                            operands
                        ));
                    }
                }
            }

            results.add(new MethodDisassembly(
                mm.methodName().stringValue(),
                mm.methodType().stringValue(),
                flags,
                maxStack,
                maxLocals,
                instructions
            ));
        }

        return results;
    }

    private String formatOperands(Instruction instr) {
        if (instr instanceof BranchInstruction b) {
            return "-> " + b.target().label();
        } else if (instr instanceof ConstantInstruction c) {
            return "const: " + c.constantValue();
        } else if (instr instanceof FieldInstruction f) {
            return f.owner().asInternalName() + "." + f.name().stringValue();
        } else if (instr instanceof InvokeInstruction inv) {
            return inv.owner().asInternalName() + "." + inv.name().stringValue() + inv.type().stringValue();
        } else if (instr instanceof LoadInstruction ld) {
            return "slot #" + ld.slot();
        } else if (instr instanceof StoreInstruction st) {
            return "slot #" + st.slot();
        }
        return "";
    }
}
```

### Component 3: CLI Application Driver (`src/main/java/com/learning/bytecode/Main.java`)

```java
package com.learning.bytecode;

import com.learning.bytecode.engine.ClassFileAnalyzer;
import com.learning.bytecode.model.*;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public class Main {
    public static void main(String[] args) throws Exception {
        byte[] classBytes;

        if (args.length > 0) {
            Path p = Path.of(args[0]);
            System.out.println("Reading bytecode from: " + p.toAbsolutePath());
            classBytes = Files.readAllBytes(p);
        } else {
            // Disassemble self by default
            String resourceName = "/" + Main.class.getName().replace('.', '/') + ".class";
            try (InputStream in = Main.class.getResourceAsStream(resourceName)) {
                if (in == null) throw new IllegalStateException("Could not load bytecode for Main.class");
                classBytes = in.readAllBytes();
            }
        }

        ClassFileAnalyzer analyzer = new ClassFileAnalyzer();
        ClassSummary summary = analyzer.summarize(classBytes);

        System.out.println("=================================================");
        System.out.println("CLASS-FILE BYTECODE ANALYSIS REPORT (JDK 25 API)");
        System.out.println("=================================================");
        System.out.println("Class Name:        " + summary.className());
        System.out.println("Super Class:       " + summary.superClassName());
        System.out.println("Major Version:     " + summary.majorVersion() + " (Java " + (summary.majorVersion() - 44) + ")");
        System.out.println("Access Flags:      " + summary.accessFlags());
        System.out.println("Constant Pool:     " + summary.constantPoolSize() + " entries");
        System.out.println("Total Methods:     " + summary.methodCount());
        System.out.println("=================================================\n");

        List<MethodDisassembly> methods = analyzer.disassembleMethods(classBytes);
        for (MethodDisassembly m : methods) {
            System.out.printf("Method: %s%s (Flags: %s)\n", m.methodName(), m.descriptor(), m.flags());
            System.out.printf("  Stack=%d, Locals=%d, Instructions=%d\n", m.maxStack(), m.maxLocals(), m.instructions().size());
            for (DisassembledInstruction ins : m.instructions()) {
                System.out.printf("    %04d: %-16s %s\n", ins.bci(), ins.mnemonic(), ins.operands());
            }
            System.out.println();
        }
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
    <artifactId>classfile-analyzer</artifactId>
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
package com.learning.bytecode;

import com.learning.bytecode.engine.ClassFileAnalyzer;
import com.learning.bytecode.model.*;
import org.junit.jupiter.api.Test;

import java.io.InputStream;
import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

public class ClassFileAnalyzerTest {

    public static class SampleTarget {
        public int add(int a, int b) {
            return a + b;
        }
    }

    @Test
    void testParseTargetBytecode() throws Exception {
        String res = "/" + SampleTarget.class.getName().replace('.', '/') + ".class";
        byte[] bytes;
        try (InputStream in = SampleTarget.class.getResourceAsStream(res)) {
            assertNotNull(in);
            bytes = in.readAllBytes();
        }

        ClassFileAnalyzer analyzer = new ClassFileAnalyzer();
        ClassSummary summary = analyzer.summarize(bytes);

        assertTrue(summary.className().contains("SampleTarget"));
        assertTrue(summary.majorVersion() >= 65); // Java 21+ major version >= 65

        List<MethodDisassembly> methods = analyzer.disassembleMethods(bytes);
        MethodDisassembly addMethod = methods.stream()
            .filter(m -> m.methodName().equals("add"))
            .findFirst()
            .orElseThrow();

        assertEquals("(II)I", addMethod.descriptor());
        assertTrue(addMethod.instructions().stream().anyMatch(i -> i.mnemonic().equals("iadd")));
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
[INFO] Running com.learning.bytecode.ClassFileAnalyzerTest
[INFO] Tests run: 1, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 0.182 s
[INFO] 
[INFO] Results:
[INFO] 
[INFO] Tests run: 1, Failures: 0, Errors: 0, Skipped: 0
[INFO] 
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.bytecode.Main
=================================================
CLASS-FILE BYTECODE ANALYSIS REPORT (JDK 25 API)
=================================================
Class Name:        com/learning/bytecode/Main
Super Class:       java/lang/Object
Major Version:     69 (Java 25)
Access Flags:      [PUBLIC, SUPER]
Constant Pool:     142 entries
Total Methods:     2
=================================================

Method: <init>()V (Flags: [PUBLIC])
  Stack=1, Locals=1, Instructions=3
    0025: aload_0          slot #0
    0183: invokespecial    java/lang/Object.<init>()V
    0177: return           

Method: main([Ljava/lang/String;)V (Flags: [PUBLIC, STATIC])
  Stack=3, Locals=5, Instructions=28
    ...
```

---

[← Return to Projects Index](README.md)
