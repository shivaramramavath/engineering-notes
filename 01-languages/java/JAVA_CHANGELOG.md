# Java Track Changelog

All notable changes, structural migrations, and note creations in the Java learning track will be documented in this file.

---

## [Batch 1] - 2026-11-21: Structure Fixes, Templates & Meta
### Added
- `01-languages/java/JAVA_FACTS.md`: Canonical reference for Java 21-25 JEP numbers, statuses, LTS dates, and deprecations.
- `01-languages/java/JAVA_AUDIT.md`: Static analysis audit of all 270 track files, line count distribution, and defect report.
- `01-languages/java/JAVA_COMPLETION_PLAN.md`: Strategic 13-batch execution roadmap with zero approval gates.
- `01-languages/java/JAVA_DECISIONS.md`: Architectural decisions log establishing Java 25 LTS baseline, markdown-only repo policy, and template enforcement.
- `01-languages/java/JAVA_PROGRESS.md`: Batch tracking checklist and quantitative targets dashboard.
- `01-languages/java/JAVA_CHANGELOG.md`: Detailed audit and mutation log.
- `01-languages/java/_templates/concept-template.md`: Authoritative 13-section concept template tailored for Java.
- `01-languages/java/_templates/project-template.md`: Authoritative project template for production systems.

### Fixed & Renamed
- Renamed `10-date-and-time/README (1).md` to `10-date-and-time/README.md`.
- Renamed `11-io-and-networking/README (6).md` to `11-io-and-networking/README.md`.
- Renamed `12-modern-java/README (9).md` to `12-modern-java/README.md`.
- Renamed `16-jdbc-and-databases/README (5).md` to `16-jdbc-and-databases/README.md`.
- Renamed `17-json-and-data-formats/README (2).md` to `17-json-and-data-formats/README.md`.
- Renamed `29-cheatsheets/dsa-patterns-and-complexity.md` to `29-cheatsheets/09_dsa-patterns-and-complexity.md`.

### Verified
- Pre-execution backup verified at `E:\Learning\java-backup-20261121`.
- Oracle JDK 25.0.1 LTS compiler toolchain verified in scratch environment.

## [Batch 3] - 2026-11-21 - Text, OOP, and Modules Track Expansion

### Added / Expanded
- **03-strings-and-text (8 concept notes + README, 420–586 lines each):**
  - Expanded `00_string-basics.md`: Compact Strings, LATIN1/UTF16 coder, invokedynamic concatenation, password memory zeroing.
  - Expanded `01_string-methods.md`: Substring defensive copying, `isBlank()`, `strip()`, `lines()`, `splitWithDelimiters()`, `transform()`.
  - Expanded `02_string-pool-and-comparison.md`: JVM `StringTable`, Heap vs PermGen history, `.intern()`, `-XX:+UseStringDeduplication`.
  - Expanded `03_stringbuilder-and-stringbuffer.md`: `AbstractStringBuilder`, capacity growth `(old*2)+2`, reset via `setLength(0)`.
  - Expanded `04_string-formatting.md`: Formatter specifiers, `%n` platform newline, `.formatted()`, String Templates withdrawal status.
  - Expanded `05_text-blocks.md`: JEP 378, delimiter rules, 3-step compilation whitespace algorithm, `\s` and `\` line continuation.
  - Expanded `06_regex.md`: Pattern & Matcher, ReDoS defense with possessive quantifiers `++`, named groups, Stream `results()`.
  - Expanded `07_unicode-and-encoding.md`: JEP 400 UTF-8 by Default, surrogate pairs, grapheme clusters `\X`, NIO CharsetDecoder.
  - Updated `README.md` with complete architecture map and checklist.

- **04-oop (16 concept notes + README, 430–520 lines each):**
  - Expanded `00_classes-and-objects.md`: 64-bit object layout (Mark Word, Klass pointer, Compressed OOPs, 8-byte alignment).
  - Expanded `01_constructors.md`: `<init>`, chaining, instance initializers, Flexible Constructor Bodies (JEP 513).
  - Expanded `02_static.md`: Class variables on heap, `<clinit>`, thread safety, Initialization-on-demand holder idiom.
  - Expanded `03_final.md`: Blank finals, method inlining, JMM freeze action safe publication.
  - Expanded `04_encapsulation-and-access-modifiers.md`: Four access levels, package-private architectural boundaries, JPMS strong encapsulation.
  - Expanded `05_initialization-order.md`: Complete 11-step lifecycle, static vs instance blocks, constructor override hazard.
  - Expanded `06_inheritance.md`: Single inheritance, `@Override`, covariant return types, Fragile Base Class problem.
  - Expanded `07_polymorphism.md`: Dynamic dispatch, HotSpot `vtable`/`itable`, monomorphic vs megamorphic inline caching, pattern matching.
  - Expanded `08_abstract-classes.md`: Incomplete blueprints, constructors in abstract classes, Template Method pattern, skeletal implementations.
  - Expanded `09_interfaces.md`: Contracts, `default` methods, `static` methods, `private` methods (Java 9), Diamond problem conflict resolution.
  - Expanded `10_composition-and-object-relationships.md`: Dependency, Association, Aggregation, Composition, Forwarding Decorator pattern.
  - Expanded `11_enums.md`: `java.lang.Enum`, constant-specific bodies, `EnumSet` bit vector, `EnumMap`, serialization-proof singleton.
  - Expanded `12_nested-and-inner-classes.md`: Static nested vs member inner, synthetic `this$0` memory leak, Java 11 nestmates (JEP 181).
  - Expanded `13_object-class.md`: 11 methods, `wait`/`notify`, JEP 421 `finalize` removal, `Cleaner` and `AutoCloseable`.
  - Expanded `14_equals-and-hashcode.md`: Equivalence relations, 31 multiplier, HashMap treeification, symmetry violations.
  - Expanded `15_clone-and-copying.md`: Why `Cloneable` is broken, shallow vs deep copying, copy constructors, defensive copying.
  - Updated `README.md` with complete architecture map and checklist.

- **05-packages-and-modules (4 concept notes + README, 330–405 lines each):**
  - Expanded `00_packages-and-imports.md`: Namespaces, package-private boundaries, Java 25 `import module java.base;` (JEP 511).
  - Expanded `01_classpath-and-jars.md`: Classpath mechanics, ClassLoader delegation, Multi-Release JARs (JEP 238), Fat JARs.
  - Expanded `02_java-modules.md`: JPMS architecture, `module-info.java`, `requires`, `exports`, `opens`, `provides ... with`, SPI.
  - Authored new `03_jlink-and-runtime-images.md`: JEP 282 custom runtime images, `jdeps`, `lib/modules`, Docker footprint reduction to < 45MB.
  - Updated `README.md` with complete architecture map and checklist.

### Verification
- All code examples verified and test-compiled against Java 25 LTS via `javac --release 25 -Xlint:all` in scratch directory.
