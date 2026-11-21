# 05. Packages, Classpath, and the Java Module System (JPMS)

> Comprehensive reference and architecture guide to Java packaging, namespace isolation, classpath mechanics, classloaders, the Java Platform Module System (JPMS), and custom runtime images (`jlink`).

---

## Module Overview

Modern Java applications transition from localized namespace management with packages and JARs to architectural modularity with the Java Platform Module System (JPMS).

This module provides exhaustive coverage of compilation units, classloader delegation, modular encapsulation boundaries, and cloud-native footprint minimization with `jlink`.

---

## Document Index

| # | Note | Level | Description | Key Topics |
| :- | :--- | :--- | :--- | :--- |
| 00 | [Packages & Modern Imports](./00_packages-and-imports.md) | Beg-Int | Namespaces and Java 25 module imports | `package`, import resolution, package-private, JEP 511 `import module java.base;` |
| 01 | [Classpath & JAR Files](./01_classpath-and-jars.md) | Intermediate | Class loading, archives, and Fat JARs | Classpath, ClassLoader hierarchy, Multi-Release JARs (JEP 238), Fat JARs |
| 02 | [Java Platform Module System](./02_java-modules.md) | Int-Adv | JPMS architecture and directives | `module-info.java`, `requires`, `exports`, `opens`, strong encapsulation, SPI |
| 03 | [jlink Custom Runtime Images](./03_jlink-and-runtime-images.md) | Int-Adv | Shrinking runtimes to < 45MB | `jlink` (JEP 282), `jdeps`, `lib/modules`, Docker multi-stage builds |

---

## Conceptual Architecture Map

```
┌────────────────────────────────────────────────────────┐
│                      Module (JPMS)                     │
│  module-info.java: requires, exports, opens, provides  │
├────────────────────────────────────────────────────────┤
│                   Package Namespace                    │
│  com.company.service (Package-private isolation boundary│
├────────────────────────────────────────────────────────┤
│                   Classes & Bytecode                   │
│  .class files, compiled constant pool, FQCN references │
└────────────────────────────────────────────────────────┘
```

---

## Learning Objectives Checklist

- [x] Configure reverse-domain packages and explain package-private encapsulation.
- [x] Utilize Java 25 Module Imports (`import module java.base;`) to streamline imports.
- [x] Trace ClassLoader parent-delegation hierarchy (Bootstrap $\to$ Platform $\to$ Application).
- [x] Create and inspect executable and multi-release JAR files using `jar`.
- [x] Author comprehensive `module-info.java` descriptors using `requires`, `exports`, `opens`, and `provides`.
- [x] Eliminate Classpath Hell by enforcing eager startup validation on the modulepath.
- [x] Identify application dependencies using `jdeps --print-module-deps`.
- [x] Link lightweight standalone custom JRE runtime images under 45MB using `jlink`.
