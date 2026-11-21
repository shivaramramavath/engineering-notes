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

## [Batch 4] - 2026-11-21 - Robustness, Generics, and Collections Track Expansion

### Added / Expanded
- **06-exceptions-and-debugging (7 concept notes + README, 405–458 lines each):**
  - Expanded `00_exception-hierarchy-and-types.md`: Throwable, Error, Exception, RuntimeException; Heap allocation and stack walk costs.
  - Expanded `01_try-catch-finally.md`: Bytecode exception table (`Exception table:`), finally block inlining, return shadowing hazards.
  - Expanded `02_throw-and-throws.md`: Rethrowing, multi-catch disjoint union typing, precision exception analysis in javac.
  - Expanded `03_try-with-resources.md`: AutoCloseable, suppressed exceptions (`Throwable.getSuppressed()`), effectively final variables (Java 9+).
  - Expanded `04_custom-exceptions.md`: Domain-driven exception hierarchies, constructor overloading, chaining causes, serialVersionUID.
  - Expanded `05_exception-handling-patterns.md`: Fail-fast vs fault-tolerant, circuit breaker, retry loops with jitter, translation/wrapping.
  - Expanded `06_stack-traces-and-debugging.md`: StackWalker API (JEP 259), lazy frames, Helpful NullPointerExceptions (JEP 358), async stack traces.
  - Updated `README.md` with complete module index.

- **07-generics (5 concept notes + README, 423–470 lines each):**
  - Expanded `00_generic-classes-and-methods.md`: Type parameters, diamond operator `<>`, generic methods, heap pollution, `@SafeVarargs`.
  - Expanded `01_bounded-types.md`: Single upper bounds, multiple bounds (`&`), class bound first rule, recursive type bounds (`<T extends Comparable<? super T>>`).
  - Expanded `02_wildcards-and-pecs.md`: Invariance vs covariance (`? extends T`) vs contravariance (`? super T`), PECS rule, wildcard capture helper.
  - Expanded `03_type-erasure.md`: Erasure to first bound or Object, synthetic bridge methods (`ACC_BRIDGE`), reification boundaries, Super Type Tokens.
  - Expanded `04_generic-patterns.md`: Typesafe Heterogeneous Containers (Effective Java Item 33), CRTP, phantom types, compile-time state machines.
  - Updated `README.md` with complete module index.

- **08-collections (16 concept notes + README, 403–560 lines each):**
  - Expanded `00_collection-hierarchy.md`: JCF blueprint, and **Java 21 Sequenced Collections (JEP 431)** (`SequencedCollection`, `SequencedSet`, `SequencedMap`).
  - Expanded `01_list.md`: Index-based access, ListIterator, subList view mechanics, unmodifiable factories.
  - Expanded `02_set.md`: Mathematical set contract, backing map pattern, hash vs tree vs enum sets.
  - Expanded `03_map.md`: Associative mapping, collection views, Java 8+ functional methods (`merge`, `computeIfAbsent`).
  - Expanded `04_queue-and-deque.md`: FIFO vs LIFO, special value vs exception methods, circular `ArrayDeque` bitwise indexing.
  - Expanded `05_comparable-and-comparator.md`: Natural ordering, fluent combinator composition (`comparingInt`, `thenComparing`, `reversed`, `nullsFirst`).
  - Expanded `06_arraylist.md`: Dynamic 1.5x array growth formula, contiguous memory cache locality, `System.arraycopy`, fail-fast iterators.
  - Expanded `07_linkedlist.md`: Doubly-linked node layout (24 bytes/node), pointer chasing cache miss penalties vs `ArrayList`.
  - Expanded `08_hashmap-and-hashset.md`: Hash spreading `(h ^ (h >>> 16))`, bitwise index masking, treeification threshold 8, untreeify threshold 6, resize doubling.
  - Expanded `09_treemap-and-treeset.md`: Red-Black tree invariants, NavigableMap range queries (`subMap`, `floorKey`, `ceilingKey`).
  - Expanded `10_priorityqueue.md`: Binary min-heap array representation, sift-up, sift-down, linear-time $O(N)$ heapify.
  - Expanded `11_specialized-collections.md`: `EnumSet` (bit vector in single `long`), `EnumMap`, `IdentityHashMap` (`==`), `WeakHashMap`, `BitSet`.
  - Expanded `12_iterators-and-fail-fast-behavior.md`: `Iterator`, `ListIterator`, `Spliterator`, `modCount` fail-fast validation.
  - Expanded `13_immutable-and-unmodifiable-collections.md`: Unmodifiable views vs true immutable collections (`List.of()`, `Set.of()`, `Map.of()`), randomized order.
  - Expanded `14_collections-and-arrays-utilities.md`: TimSort vs Dual-Pivot Quicksort, binary search mechanics, SIMD vectorized `Arrays.mismatch`.
  - Expanded `15_collection-performance-and-selection.md`: Big-O time and space matrix, per-element memory overhead table, architectural decision trees.
  - Updated `README.md` with complete module index.

### Verified
- Full verification test compiled and executed on Oracle JDK 25 LTS via `scratch/Batch4Verification.java`. All tests passed.

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

---

## [Batch 4] - 2026-11-21
### Added & Expanded
- **06-exceptions-and-debugging (7 concept notes + README, 405-458 lines each):**
  - Expanded `00_exception-hierarchy-and-types.md`: Throwable, Error, Exception, RuntimeException, checked vs unchecked debate.
  - Expanded `01_try-catch-finally.md`: Multi-catch, finally execution semantics, return in finally hazard, exception swallowing.
  - Expanded `02_throw-and-throws.md`: Method contract specification, rethrowing, exception chaining, cause preservation.
  - Expanded `03_try-with-resources.md`: AutoCloseable, suppressed exceptions (`getSuppressed()`), effectively final resources in Java 9.
  - Expanded `04_custom-exceptions.md`: Domain-specific exceptions, constructor overloads, serialization safety, hierarchy design.
  - Expanded `05_exception-handling-patterns.md`: Fault barriers, translation, circuit breaking, retry with backoff, anti-patterns.
  - Expanded `06_stack-traces-and-debugging.md`: StackWalker API (JEP 259), filtering stack frames, memory leak diagnosis, thread dumps.
  - Updated `README.md` with complete curriculum map.
- **07-generics (5 concept notes + README, 423-470 lines each):**
  - Expanded `00_generic-classes-and-methods.md`: Type parameters, generic constructors, diamond operator `<>`, compile-time type safety.
  - Expanded `01_bounded-types.md`: Upper bounds (`extends`), multiple bounds (`T extends A & B`), recursive type bounds.
  - Expanded `02_wildcards-and-pecs.md`: PECS rule (Producer Extends, Consumer Super), unbounded wildcards, wildcard capture helper idiom.
  - Expanded `03_type-erasure.md`: Bridge methods, synthetic methods, heap pollution, reification limits, runtime type loss.
  - Expanded `04_generic-patterns.md`: Typesafe Heterogeneous Container (Joshua Bloch), generic DAO, fluent builder generics.
  - Updated `README.md` with complete curriculum map.
- **08-collections (16 concept notes + README, 403-560 lines each):**
  - Expanded `00_collection-hierarchy.md`: Core hierarchy + Sequenced Collections (JEP 431 in Java 21): `SequencedCollection`, `SequencedSet`, `SequencedMap`.
  - Expanded `01_list.md`: List contract, positional access, subList structural modification hazards.
  - Expanded `02_set.md`: Set mathematical properties, uniqueness, hash-based vs tree-based sets.
  - Expanded `03_map.md`: Map mechanics, `computeIfAbsent`, `merge`, `putIfAbsent`, SequencedMap first/last entry access.
  - Expanded `04_queue-and-deque.md`: FIFO queues, double-ended deques, offer/poll vs add/remove, ArrayDeque vs LinkedList.
  - Expanded `05_comparable-and-comparator.md`: Natural ordering vs custom comparison, `Comparator.comparing()`, chained comparators, null-safe sorting.
  - Expanded `06_arraylist.md`: Dynamic array growth (1.5x), amortized O(1), arraycopy mechanics.
  - Expanded `07_linkedlist.md`: Doubly linked nodes, cache locality disadvantages, why ArrayDeque outperforms it as a queue/stack.
  - Expanded `08_hashmap-and-hashset.md`: Internal bucket array, load factor 0.75, treeification threshold 8 (Red-Black tree conversion).
  - Expanded `09_treemap-and-treeset.md`: Red-Black self-balancing BST, O(log n) performance, Range views (`subMap`, `tailMap`).
  - Expanded `10_priorityqueue.md`: Binary min-heap, sift-up/sift-down mechanics, priority task scheduling.
  - Expanded `11_specialized-collections.md`: `EnumSet` (bit vector), `EnumMap` (dense array), `IdentityHashMap`, `WeakHashMap`.
  - Expanded `12_iterators-and-fail-fast-behavior.md`: ModCount, `ConcurrentModificationException`, fail-fast vs fail-safe.
  - Expanded `13_immutable-and-unmodifiable-collections.md`: `List.of()`, `Set.of()`, `Map.of()` (compact, space-efficient, truly immutable) vs `Collections.unmodifiable*`.
  - Expanded `14_collections-and-arrays-utilities.md`: Binary search, shuffling, reversing, frequency counting, parallel sorting.
  - Expanded `15_collection-performance-and-selection.md`: Time complexity decision matrix, memory footprints, selection guide.
  - Updated `README.md` with complete curriculum map.

### Verification
- Tested and verified on JDK 25 LTS via `scratch/Batch4Verification.java`.

---

## [Batch 5] - 2026-11-21
### Added & Expanded
- **09-functional-java (9 concept notes + README, 400-475 lines each):**
  - Expanded `00_lambda-expressions.md`: Syntax variants, lexical scoping, variable capture (effectively final), `invokedynamic` bootstrap call sites.
  - Expanded `01_functional-interfaces.md`: `@FunctionalInterface`, standard library specializations (`Function`, `Predicate`, `Supplier`, `Consumer`, primitive specializations).
  - Expanded `02_method-references.md`: 4 categories (static, bound instance, unbound instance, constructor), bytecode compilation.
  - Expanded `03_optional.md`: Anti-null safety, monadic chaining (`flatMap`, `filter`, `or`), anti-patterns (fields, method parameters, get without isPresent).
  - Expanded `04_streams-fundamentals.md`: Stream pipeline lifecycle, intermediate vs terminal operations, spliterators, fused loops.
  - Expanded `05_stream-operations.md`: Stateless vs stateful operations, flatMap, takeWhile/dropWhile, Stream Gatherers (JEP 485) custom intermediate operations (`windowFixed`, `windowSliding`, `fold`).
  - Expanded `06_collectors.md`: Downstream collectors, `groupingBy`, `partitioningBy`, `teeing` (Java 12), custom `Collector` implementation.
  - Expanded `07_parallel-streams.md`: ForkJoinPool.commonPool(), parallel spliterators, false sharing, when to use parallel streams.
  - Expanded `08_functional-patterns.md`: Function composition, currying, partial application, monads, loan pattern.
  - Updated `README.md` with complete curriculum map.
- **10-date-and-time (6 concept notes + README, 400-465 lines each):**
  - Expanded `00_java-time-overview.md`: JSR 310 architecture, immutability, thread-safety, ISO-8601 calendar system.
  - Expanded `01_local-date-time.md`: `LocalDate`, `LocalTime`, `LocalDateTime`, human-scale date math, temporal adjusters.
  - Expanded `02_instant-and-zoned-date-time.md`: Machine timeline (`Instant`), `ZonedDateTime`, `ZoneId`, `ZoneOffset`, daylight saving transitions (gap/overlap).
  - Expanded `03_duration-and-period.md`: Machine-based `Duration` (seconds/nanos) vs date-based `Period` (years/months/days).
  - Expanded `04_date-time-formatting-and-parsing.md`: `DateTimeFormatter`, thread-safe parsing, localized patterns, custom pattern letters.
  - Expanded `05_date-time-best-practices.md`: Storage standards (UTC Instant), conversion rules, legacy `java.util.Date` migration bridges.
  - Updated `README.md` with complete curriculum map.
- **11-io-and-networking (5 concept notes + README, 410-614 lines each):**
  - Expanded `00_io-streams-readers-writers.md`: InputStream, OutputStream, Reader, Writer, buffering, decorators, UTF-8 platform default (JEP 400), transferTo.
  - Expanded `01_nio-paths-and-files.md`: NIO.2 Path, Files, directory streams, walkFileTree, memory-mapped buffers (FileChannel), WatchService.
  - Expanded `02_file-processing-patterns.md`: Zero-allocation streaming transformations, memory-safe file reading, atomic replacements.
  - Expanded `03_java-serialization.md`: `Serializable`, `Externalizable`, `serialVersionUID`, gadget chain RCE vulnerabilities, `ObjectInputFilter` (JEP 290/415), Serialization Proxy Pattern.
  - Expanded `04_http-client-and-networking.md`: Modern `java.net.http.HttpClient` (JEP 321), HTTP/2 multiplexing, reactive BodyPublishers/BodyHandlers, CompletableFuture async requests, WebSockets, TCP sockets.
  - Updated `README.md` with complete curriculum map.

### Verification
- Tested and verified on JDK 25 LTS via `scratch/Batch5Verification.java`.

---

## [Batch 6] - 2026-11-21
### Added & Expanded
- **12-modern-java (7 concept notes + README, 400-502 lines each):**
  - Records (JEP 395), Sealed Classes (JEP 409), Pattern Matching for instanceof (JEP 394), Pattern Matching for switch (JEP 441), Record Patterns (JEP 440), Text Blocks (JEP 378), Foreign Function & Memory API (JEP 454).
- **13-advanced-language-features (4 concept notes + README, 402-421 lines each):**
  - Annotations, Reflection, Method Handles & VarHandles, Dynamic Proxies.
- **14-concurrency (17 concept notes + README, 400-475 lines each):**
  - Thread lifecycle, JMM happens-before, Virtual Threads (JEP 444), Structured Concurrency (JEP 480), Scoped Values (JEP 481), Locks, Atomics, Concurrent Collections, Executors, ForkJoin, CompletableFuture, Reactive Flow, Deadlock detection.
- **15-jvm-internals (8 concept notes + README, 400-452 lines each):**
  - ClassLoader hierarchy, JVM memory layout, Bytecode & class files, JIT Tiered Compilation, GC algorithms (Serial, Parallel, G1, ZGC), Safepoints, JVM options, JFR.
### Verification
- Tested and verified on JDK 25 LTS via `scratch/Batch6Verification.java`.

---

## [Batch 7] - 2026-11-21
### Added & Expanded
- **16-jdbc-and-databases (8 concept notes + README, 400-505 lines each):**
  - SQL & indexing essentials, JDBC fundamentals, Statements & PreparedStatements, ResultSet data mapping, Transactions & isolation levels, Connection pooling (HikariCP), JDBC patterns, Transitioning from JDBC to ORM.
- **17-json-and-data-formats (5 concept notes + README, 405-418 lines each):**
  - JSON basics, Jackson databind, Gson, JSON patterns & security pitfalls, Data formats comparison (JSON vs Protobuf vs Avro vs YAML).
- **18-build-and-dependencies (4 concept notes + README, 402-418 lines each):**
  - Maven, Gradle, Dependency management, CI/CD pipelines.
- **19-testing (6 concept notes + README, 402-460 lines each):**
  - Unit testing fundamentals, JUnit 5, Mocking with Mockito, Integration testing with Testcontainers, Testing patterns (TDD/BDD), Code coverage & Mutation testing (Pitest).
### Verification
- Tested and verified on JDK 25 LTS via `scratch/Batch7Verification.java`.

---

## [Batch 8] - 2026-11-21: Enterprise Engineering
### Added & Expanded
- **20-performance (6 concept notes + README, 400-440 lines each):**
  - JVM profiling (async-profiler, JFR, Flame Graphs, Safepoint bias), JMH microbenchmarking (DCE, Blackholes), Memory optimization (Compressed OOPs, @Contended, FFM), GC tuning (Generational ZGC, G1), Concurrency performance (LongAdder striping, Carrier pinning), Common performance pitfalls.
- **21-security (7 concept notes + README, 432-569 lines each):**
  - Input validation & injection defense (SQLi, Command, Path traversal, Log forging), Modern cryptography with JCA/JCE (AES-256-GCM, Ed25519, constant-time MessageDigest.isEqual), Password hashing (Argon2id, PBKDF2-HMAC-SHA512), Secrets management (char[] wiping, Vault, PKCS12), Insecure deserialization defense (JEP 290/415 allow-list filters), Dependency security & supply chain (OWASP Dependency-Check, CycloneDX SBOM), Production security checklist (non-root distroless containers, TLS 1.3, HSTS).
- **22-production-engineering (6 concept notes + README, 405-508 lines each):**
  - Structured asynchronous logging (SLF4J, Logback, JSON stdout, MDC context propagation), Configuration management (12-Factor App, hierarchy resolution, immutable records, fail-fast), Packaging and deployment (Layered Dockerfiles, jlink custom JREs, AppCDS fast startup), Observability (Micrometer metrics, OpenTelemetry distributed tracing, W3C traceparent, Kubernetes liveness/readiness probes), JVM diagnostics (jcmd, jstack, jmap, jstat, NMT, hs_err_pid crash logs), Production troubleshooting playbooks (High CPU triage, Memory leaks, Deadlocks, DB connection pool exhaustion).
- **23-design-and-clean-code (8 concept notes + README, 406-487 lines each):**
  - SOLID principles modernized with Java 21 features (Records, Sealed Interfaces, Pattern Matching), Core design principles (KISS, YAGNI, DRY, Composition over Inheritance, Law of Demeter), Clean code idioms (naming, method decomposition, parameter bounds, null-safety), Refactoring and code smells (Primitive obsession, Feature envy, Data clumps, Polymorphism), Deep immutability (JMM final field freeze, List.copyOf(), withers, thread safety), Defensive programming (Preconditions, postconditions, fail-fast, TOCTOU prevention), Common Java mistakes (String == vs equals, BigDecimal double constructor, ConcurrentModificationException, Integer cache quirks), Production code review checklist.
### Verification
- Tested and verified on Oracle JDK 25.0.1 LTS via `scratch/Batch8Verification.java` with assertions enabled.
