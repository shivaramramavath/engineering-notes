# Ahead-of-Time (AOT) Compilation and GraalVM Native Images

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 13-performance |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Build Tool Reference](#4-syntax--build-tool-reference)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Standard Java applications run on the HotSpot JVM using dynamic bytecode interpretation and Just-in-Time (JIT) compilation (C1/C2 compilers). While JIT compilation delivers exceptional peak throughput over long durations:
- Cold-start times are slow (5–20 seconds) due to dynamic classpath scanning, class loading, reflection, and proxy generation.
- Memory consumption is substantial: the JVM requires significant baseline RAM (150MB–500MB) for Metaspace, JIT compiler caches (CodeCache), and internal runtime bookkeeping.
- In modern cloud-native architectures—such as AWS Lambda, Google Cloud Run, and Kubernetes event-driven autoscaling (KEDA)—these memory and cold-start characteristics incur financial penalties and latency degradation.

### Historical Evolution
Prior to Spring Boot 3.0, running Spring applications as GraalVM native executables required complex external projects like Spring Native and experimental plugins. Dynamic reflection, CGLIB proxies, and classpath condition evaluations were fundamentally incompatible with GraalVM's **Closed-World Assumption**.

With **Spring Boot 3.0**, Spring rewritten its foundational core around the **Spring AOT (Ahead-of-Time) Engine**. Spring Boot 3 inspects the application at build time, evaluates conditions, generates native-friendly Java code and reflection metadata, and compiles directly into standalone platform binaries (ELF on Linux, Mach-O on macOS) using GraalVM Native Build Tools.

### Production Relevance
GraalVM Native Images deliver:
- **Instant Cold Starts**: Boot times drop from 15 seconds to **under 40 milliseconds** ($< 0.04\text{s}$).
- **Tiny Memory Footprint**: Base memory usage drops from 350MB down to **under 35MB**.
- **Immutable Self-Contained Binaries**: No JVM or JDK installation needed in the container; builds can run inside lightweight distroless/scratch container images.

---

## 2. Core Concepts

### Foundations of Native Compilation
1. **Ahead-of-Time (AOT) Processing**: A build-time phase in Spring Boot 3 where the `ApplicationContext` is evaluated ahead-of-time. Spring generates optimized Java source code that replaces runtime reflection, bean factory scanning, and dynamic proxies with direct code invocations.
2. **GraalVM `native-image`**: The compiler utility that transforms Java bytecode into machine code using static points-to analysis.
3. **Closed-World Assumption**: GraalVM assumes that all bytecode that can ever be reached at runtime is known and inspectable during compilation. Any dynamic features (reflection, JNI, resources, serialization, dynamic proxies) not explicitly declared at compile time are stripped from the final binary.
4. **Runtime Hints (`RuntimeHints`)**: The programmatic API in Spring Framework 6 to register reflection, resources, serialization, and proxy metadata required by GraalVM.
5. **`@RegisterReflectionForBinding`**: Convenient class-level annotation to register DTOs, records, or third-party classes for reflective serialization/binding without writing manual hints.

---

## 3. How It Works Internally

### The Spring Boot 3 AOT Build Pipeline

```
SOURCE CODE & DEPENDENCIES
  |
  v
+------------------------------------------------------------------------------------+
| 1. COMPILATION (javac)                                                             |
|    Standard compilation generates .class bytecode files                            |
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 2. SPRING BOOT AOT PROCESSING (mvn spring-boot:process-aot)                        |
|    - Initializes a build-time ApplicationContext                                   |
|    - Evaluates all @ConditionalOnProperty, @ConditionalOnClass annotations        |
|    - Generates BeanDefinitions as optimized Java source files                      |
|    - Generates GraalVM metadata (reflect-config.json, resource-config.json)        |
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 3. GRAALVM STATIC ANALYSIS & POINTS-TO GRAPH                                       |
|    - Analyzes entry point: public static void main(String[] args)                  |
|    - Follows reachable code paths transitively (Tree Shaking)                      |
|    - Eliminates unused classes, methods, and fields from libraries and the JDK     |
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 4. NATIVE MACHINE CODE GENERATION (SubstrateVM)                                    |
|    - Compiles reachable bytecode to native assembly (x86_64 / arm64)               |
|    - Embeds SubstrateVM (minimal runtime with GC, thread scheduling)               |
|    - Produces a single executable binary (e.g., target/order-service)              |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Build Tool Reference

### Maven Build Configuration (`pom.xml`)
```xml
<project>
    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.4.1</version>
    </parent>

    <dependencies>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-actuator</artifactId>
        </dependency>
    </dependencies>

    <build>
        <plugins>
            <!-- Spring Boot Maven Plugin configured for Native Compilation -->
            <plugin>
                <groupId>org.graalvm.buildtools</groupId>
                <artifactId>native-maven-plugin</artifactId>
                <configuration>
                    <buildArgs>
                        <buildArg>-H:+ReportExceptionStackTraces</buildArg>
                        <buildArg>--enable-http</buildArg>
                        <buildArg>--enable-https</buildArg>
                    </buildArgs>
                </configuration>
            </plugin>
        </plugins>
    </build>

    <profiles>
        <profile>
            <id>native</id>
            <build>
                <plugins>
                    <plugin>
                        <groupId>org.springframework.boot</groupId>
                        <artifactId>spring-boot-maven-plugin</artifactId>
                        <configuration>
                            <image>
                                <builder>paketobuildpacks/builder-jammy-tiny:latest</builder>
                                <env>
                                    <BP_NATIVE_IMAGE>true</BP_NATIVE_IMAGE>
                                </env>
                            </image>
                        </configuration>
                    </plugin>
                </plugins>
            </build>
        </profile>
    </profiles>
</project>
```

### Build Execution Commands
```bash
# 1. Compile native image using local GraalVM installation
mvn -Pnative native:compile

# 2. Build lightweight OCI container image using Cloud Native Buildpacks (Docker required, NO local GraalVM needed)
mvn -Pnative spring-boot:build-image
```

---

## 5. Practical Examples

### Example 1: Registering Reflection with `@RegisterReflectionForBinding`
```java
package com.example.performance.aot;

import org.springframework.aot.hint.annotation.RegisterReflectionForBinding;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.math.BigDecimal;
import java.time.Instant;

@RestController
@RequestMapping("/api/payments")
public class PaymentController {

    // Ensures Jackson can reflectively serialize/deserialize these records in Native Image!
    @RegisterReflectionForBinding({ PaymentRequest.class, PaymentResponse.class })
    public record PaymentRequest(String accountId, BigDecimal amount, String currency) {}
    public record PaymentResponse(String transactionId, String status, Instant timestamp) {}

    @PostMapping
    public PaymentResponse processPayment(@RequestBody PaymentRequest request) {
        return new PaymentResponse("TX-9981", "AUTHORIZED", Instant.now());
    }
}
```

### Example 2: Programmatic Runtime Hints via `RuntimeHintsRegistrar`
When working with third-party libraries that use dynamic reflection or load resources dynamically:

```java
package com.example.performance.aot;

import org.springframework.aot.hint.MemberCategory;
import org.springframework.aot.hint.RuntimeHints;
import org.springframework.aot.hint.RuntimeHintsRegistrar;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.ImportRuntimeHints;

public class ThirdPartyLibraryRuntimeHints implements RuntimeHintsRegistrar {

    @Override
    public void registerHints(RuntimeHints hints, ClassLoader classLoader) {
        // 1. Register reflection for classes loaded via Class.forName()
        hints.reflection().registerType(
            com.thirdparty.crypto.CustomCipherEngine.class,
            MemberCategory.INVOKE_PUBLIC_CONSTRUCTORS,
            MemberCategory.INVOKE_PUBLIC_METHODS
        );

        // 2. Register classpath resources loaded via ClassLoader.getResourceAsStream()
        hints.resources().registerPattern("templates/invoice-template.html");
        hints.resources().registerPattern("crypto/public-keys.pem");

        // 3. Register JDK serialization if using standard ObjectInputStream
        hints.serialization().registerType(com.example.model.LegacySessionToken.class);
    }
}

// Attach the hints registrar to any Spring Configuration class:
@Configuration
@ImportRuntimeHints(ThirdPartyLibraryRuntimeHints.class)
class AotConfiguration {
}
```

### Example 3: Verifying Sub-50ms Cold Start in Logs
When launching a compiled Spring Boot native executable:

```bash
$ ./target/order-service

  .   ____          _            __ _ _
 /\\ / ___'_ __ _ _(_)_ __  __ _ \ \ \ \
( ( )\___ | '_ | '_| | '_ \/ _` | \ \ \ \
 \\/  ___)| |_)| | | | | || (_| |  ) ) ) )
  '  |____| .__|_| |_|_| |_\__, | / / / /
 =========|_|==============|___/=/_/_/_/
 :: Spring Boot ::                (v3.4.1)

2026-03-24T10:15:30.012Z  INFO 1 --- [order-service] [main] c.e.o.OrderServiceApplication : Starting AOT-processed OrderServiceApplication
2026-03-24T10:15:30.014Z  INFO 1 --- [order-service] [main] o.s.b.w.embedded.tomcat.TomcatWebServer  : Tomcat initialized with port 8080 (http)
2026-03-24T10:15:30.038Z  INFO 1 --- [order-service] [main] c.e.o.OrderServiceApplication : Started OrderServiceApplication in 0.038 seconds (process running for 0.042)
```
*Result*: Application started and opened HTTP port in **38 milliseconds**!

### Example 4: Anti-Pattern vs. Production Fix (Dynamic Reflection Failure)
```java
// ANTI-PATTERN: Loading classes dynamically via string names without hints
// GraalVM closed-world analysis strips 'SpecialFormatter' because it sees no direct compile-time reference.
// Throws: java.lang.ClassNotFoundException: com.example.SpecialFormatter at runtime!
@Service
public class DynamicPluginService {

    public void executePlugin(String className) throws Exception {
        // RUNTIME CRASH IN NATIVE IMAGE!
        Class<?> clazz = Class.forName(className);
        Object plugin = clazz.getDeclaredConstructor().newInstance();
    }
}

// PRODUCTION FIX: Explicitly register dynamic classes via RuntimeHintsRegistrar
@Component
public class SafePluginService {

    private final Map<String, Plugin> plugins = new HashMap<>();

    public SafePluginService(List<Plugin> discoveredPlugins) {
        // Inject known Spring beans directly; AOT records their bean definitions automatically!
        for (Plugin p : discoveredPlugins) {
            plugins.put(p.getName(), p);
        }
    }

    public void executePlugin(String name) {
        Plugin plugin = plugins.get(name);
        if (plugin != null) {
            plugin.run();
        }
    }
}
```

### Example 5: Testing AOT Processing in Continuous Integration
```java
package com.example.performance.aot;

import org.junit.jupiter.api.Test;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.aot.DisabledInAotMode;

@SpringBootTest
class ApplicationAotTests {

    @Test
    void standardContextLoads() {
        // Verifies context boots cleanly under normal JVM test
    }

    @Test
    @DisabledInAotMode // Use this annotation to exclude mock-heavy tests incompatible with AOT
    void complexMockitoTest() {
        // Dynamic Mockito bytecode mocks do not run in AOT mode
    }
}
```

---

## 6. Best Practices & Production Guidelines

1. **Use Cloud Native Buildpacks in CI/CD**: Building native images with `mvn spring-boot:build-image` uses Paketo Buildpacks inside Docker. This eliminates the need to install and configure GraalVM on local developer machines or CI build workers.
2. **Favor `@RegisterReflectionForBinding` on Data Transfer Objects**: Always annotate incoming/outgoing REST DTOs and Kafka payloads that are serialized via Jackson to prevent runtime `UnrecognizedPropertyException` or serialization failures.
3. **Run AOT Tests in CI**: Execute `mvn test-compile spring-boot:process-aot` in CI pipelines to catch missing reflection hints and AOT generation issues early before long native image compilation steps.
4. **Allocate Sufficient RAM for Compilation**: GraalVM `native-image` is a memory-intensive compiler. Ensure build machines or CI runners have at least **8GB to 16GB of free RAM**.
5. **Beware of Dynamic Proxies and CGLIB**: Spring AOT converts interfaces to JDK dynamic proxies. If concrete classes must be proxied (e.g. classes without interfaces), ensure they are registered for proxy generation in `RuntimeHints`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Unregistered reflection on DTO records | Jackson cannot inspect private fields in GraalVM; returns empty JSON `{}` or throws reflection errors. | Annotate classes with `@RegisterReflectionForBinding(MyDto.class)`. |
| Loading files via `new File("config.json")` | In native executables, external paths do not exist in the binary; only resources declared in hints are packaged. | Use `hints.resources().registerPattern("config.json")` and load via `ClassPathResource`. |
| Out of Memory during `native:compile` | GraalVM points-to analysis exhausts host RAM and OS terminates compiler process (`exit 137`). | Increase CI runner RAM to $\ge 8\text{GB}$ or limit threads via `-J-XX:ActiveProcessorCount=2`. |
| Using Mockito in native image tests | Mockito uses dynamic runtime bytecode generation (ByteBuddy), which violates the closed-world assumption. | Annotate unsupported tests with `@DisabledInAotMode`. |
| Classpath scanning with string wildcards | `PathMatchingResourcePatternResolver` cannot scan native binaries for class files. | Rely on Spring Boot's compiled AOT bean definition classes. |
| Forgetting HTTP/HTTPS protocol flags | Native image strips SSL/TLS certificates and cryptographic algorithms unless enabled. | Add `--enable-http` and `--enable-https` to `native-maven-plugin` build arguments. |

---

## 8. Performance & Security Notes

### Peak Throughput vs. JIT Compilation
- JIT Compilation (HotSpot C2) continuously profiles running code, inlining hot methods and performing speculative optimizations based on real production traffic patterns.
- GraalVM Native Image generates static assembly Ahead-of-Time without runtime profiling data.
- In long-running compute-heavy services, HotSpot JIT may achieve 5–15% higher peak throughput than basic Native Image, whereas Native Image delivers vastly superior startup speed and memory efficiency. (Note: GraalVM Enterprise / Oracle GraalVM supports PGO - Profile-Guided Optimization to close this gap).

### Security Enhancements of Native Images
- Native images significantly shrink the attack surface: the full JDK, compiler, Metaspace, and bytecode interpreter are completely absent from the container.
- Attackers cannot use dynamic deserialization exploits or arbitrary bytecode injection (e.g. traditional Log4Shell RCE) because non-pre-compiled bytecode cannot be evaluated by the binary.

---

## 9. Comparison / When to Use What

| Feature | Standard JVM (HotSpot) | GraalVM Native Image (AOT) |
|---|---|---|
| **Cold Start Time** | 5 – 25 seconds | **20 – 50 milliseconds** |
| **Baseline Memory (RSS)** | 200MB – 600MB | **25MB – 50MB** |
| **Peak Throughput** | Highest (JIT C2 optimization) | Very High (Equal with PGO) |
| **Build Time** | Very Fast (10 – 30 seconds) | Slow (3 – 8 minutes) |
| **Dynamic Reflection** | Fully supported at runtime | Requires compile-time `RuntimeHints` |
| **Ideal Use Case** | Long-running monoliths, batch processing | Serverless (Lambda), Kubernetes KEDA |

---

## 10. Interview Questions & Answers

### Q1: What is the "Closed-World Assumption" in GraalVM Native Image?
<details>
<summary>Click to view answer</summary>

The Closed-World Assumption states that all bytecode that will ever be executed by the application must be known and analyzed at build time. Dynamic Java capabilities such as dynamic class loading (`Class.forName`), reflection, serialization, and dynamic proxies cannot discover new code at runtime unless explicitly declared to the compiler via configuration hints (JSON metadata or Spring `RuntimeHints`).
</details>

### Q2: What is the purpose of Spring Boot 3's Ahead-of-Time (AOT) engine?
<details>
<summary>Click to view answer</summary>

The Spring AOT engine bridges the gap between Spring's dynamic programming model and GraalVM's closed-world requirement. During the build:
1. It parses configuration classes and evaluates conditional annotations (`@ConditionalOnProperty`, etc.).
2. It generates direct Java source code representing bean definitions, eliminating runtime reflection and bean scanning.
3. It automatically generates all necessary GraalVM metadata (reflection, resource, and proxy hints).
</details>

### Q3: How do you register reflection hints for a third-party class in Spring Boot 3?
<details>
<summary>Click to view answer</summary>

Implement `RuntimeHintsRegistrar`:
```java
public class MyHints implements RuntimeHintsRegistrar {
    @Override
    public void registerHints(RuntimeHints hints, ClassLoader classLoader) {
        hints.reflection().registerType(TargetClass.class, MemberCategory.INVOKE_PUBLIC_METHODS);
    }
}
```
Then attach it to any `@Configuration` class using `@ImportRuntimeHints(MyHints.class)`.
</details>

### Q4: Why are native image builds so much slower than standard `mvn package`?
<details>
<summary>Click to view answer</summary>

GraalVM performs whole-program points-to analysis. It loads the entire application classpath and the JDK standard library, builds an in-memory graph of every reachable class, method, and field, performs global dead-code elimination (tree shaking), and then compiles the remaining code into highly optimized native machine assembly (x86_64 or ARM64). This requires significant CPU and memory, taking several minutes.
</details>

### Q5: What is the function of `@RegisterReflectionForBinding`?
<details>
<summary>Click to view answer</summary>

Introduced in Spring Framework 6 / Boot 3, `@RegisterReflectionForBinding` is a class-level or method-level annotation that automatically registers the specified classes for reflective access (constructors, getters, setters, fields). It is primarily used for DTOs, domain records, and payloads that must be serialized or deserialized by Jackson or other binding frameworks in native mode.
</details>

### Q6: Can you use Mockito inside a compiled GraalVM native binary?
<details>
<summary>Click to view answer</summary>

No. Standard Mockito relies on runtime bytecode manipulation (ByteBuddy) to generate dynamic mock classes at runtime, which violates the closed-world assumption. For integration tests running in native mode (`mvn -Pnative test`), tests must use real beans, hand-crafted stubs, or be marked with `@DisabledInAotMode`.
</details>

### Q7: What are Profile-Guided Optimizations (PGO) in GraalVM?
<details>
<summary>Click to view answer</summary>

PGO is an advanced compilation technique where a native binary is first compiled with profiling instrumentation. It is run under typical production workloads to record execution profile data (hot paths, branch probabilities, allocation sites). The native image compiler then uses this profile data during a secondary compilation to optimize machine code generation, matching or exceeding HotSpot JIT peak throughput.
</details>

### Q8: What is SubstrateVM?
<details>
<summary>Click to view answer</summary>

SubstrateVM is the lightweight runtime embedded inside every GraalVM native executable. It provides essential low-level JVM facilities: thread scheduling, memory management, garbage collection (Serial GC or G1-like GC), and exception handling, running directly on the host operating system without requiring an installed JDK.
</details>

---

## 11. Practice Exercises

### Exercise 1: Register DTO Records for Native Reflection (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Annotate a Spring `@RestController` class to register two internal records (`UserDto` and `AddressDto`) for native reflection binding.

**Solution**:
```java
package com.example.performance.aot;

import org.springframework.aot.hint.annotation.RegisterReflectionForBinding;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RegisterReflectionForBinding({ UserDto.class, AddressDto.class })
public class UserEndpoint {

    @GetMapping("/user")
    public UserDto getUser() {
        return new UserDto("Alice", new AddressDto("Main St", "Seattle"));
    }
}

record UserDto(String name, AddressDto address) {}
record AddressDto(String street, String city) {}
```
</details>

### Exercise 2: Register Classpath Resource Bundle in RuntimeHints (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Implement a `RuntimeHintsRegistrar` that registers an internationalization resource bundle named `messages/errors` and a schema file `schema/order.json`.

**Solution**:
```java
package com.example.performance.aot;

import org.springframework.aot.hint.RuntimeHints;
import org.springframework.aot.hint.RuntimeHintsRegistrar;

public class InternationalizationHints implements RuntimeHintsRegistrar {

    @Override
    public void registerHints(RuntimeHints hints, ClassLoader classLoader) {
        // Register resource bundle for ResourceBundle.getBundle()
        hints.resources().registerResourceBundle("messages/errors");

        // Register JSON schema file for ClassLoader.getResourceAsStream()
        hints.resources().registerPattern("schema/order.json");
    }
}
```
</details>

### Exercise 3: Complete Dockerfile for Distroless Native Image (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Author a multi-stage Dockerfile that compiles a Spring Boot native binary using GraalVM 21 on Debian, and copies the resulting binary into Google's static distroless image (`gcr.io/distroless/static-debian12`).

**Solution**:
```dockerfile
# Stage 1: Build the native binary using GraalVM JDK
FROM ghcr.io/graalvm/native-image-community:21-ol9 AS build

WORKDIR /workspace
COPY pom.xml mvnw ./
COPY .mvn .mvn
RUN ./mvnw dependency:go-offline -B

COPY src src
RUN ./mvnw -Pnative native:compile -DskipTests

# Stage 2: Distroless minimal runtime
FROM gcr.io/distroless/static-debian12:nonroot

WORKDIR /app
COPY --from=build /workspace/target/order-service /app/order-service

# Expose ports
EXPOSE 8080 9090

# Run non-root native binary
USER nonroot:nonroot
ENTRYPOINT ["/app/order-service"]
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Feature / Annotation                        | Description & Functionality                                 |
+---------------------------------------------+-------------------------------------------------------------+
| Spring Boot AOT Engine                      | Evaluates context at build time; generates static Java code |
| Closed-World Assumption                     | All reachable code and reflection must be known at compile  |
| @RegisterReflectionForBinding               | Class-level annotation registering DTOs for reflection      |
| RuntimeHintsRegistrar                       | SPI to register programmatic reflection, resource, & proxy  |
| mvn -Pnative native:compile                 | Compiles native executable on local machine                 |
| mvn -Pnative spring-boot:build-image        | Builds native container via Cloud Native Buildpacks         |
| Sub-50ms Cold Start                         | Typical native executable startup time in production        |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Spring Boot Official Documentation: GraalVM Native Image Support
- GraalVM Native Image Reference Manual: Closed-World Assumption
- Spring Framework 6: Ahead-of-Time Optimizations & Runtime Hints
- Paketo Buildpacks for GraalVM Native Image Documentation