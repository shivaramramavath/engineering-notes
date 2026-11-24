# Spring Boot Learning Track: Technical Conventions & Engineering Guidelines

## 1. Governance & Standards Heritage

This curriculum adapts the rigorous engineering conventions of:
1. `01-languages/java/` (Java 25 LTS baseline, memory models, bytecode internals, JVM tuning).
2. `03-backend/nestjs/` (Modular backend architecture, dependency injection, enterprise routing).
3. `03-backend/fastapi/` (Strict typing, async I/O, Pydantic/OpenAPI validation standards).

---

## 2. Technical Metadata Schema

Every concept note in this track begins with the authoritative metadata table:

```markdown
| Metadata | Value |
| :--- | :--- |
| **Level** | [Beginner / Intermediate / Advanced / Expert] |
| **Prerequisites** | [Relative link to prerequisite note](file:///...) |
| **Time to Read** | [XX minutes] |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 25.0.1, Spring Boot 3.4.0, JUnit 5.11 |
```

---

## 3. Strict 13-Section Concept Schema

Every concept note MUST contain all 13 sections in exact numerical order:

1. **Title & Frontmatter Summary**
2. **Table of Contents**
3. **1. What & Why**: Real-world problem, architectural rationale, historical evolution.
4. **2. Core Concepts**: Formal contracts, mental models, Spring container invariants.
5. **3. How It Works Internally**: JVM mechanics, Spring ApplicationContext refresh phases, reflection/bytecode, CGLIB/JDK dynamic proxy creation, architecture diagram.
6. **4. Syntax & Usage**: Configuration annotations (`@Configuration`, `@Bean`, `@Component`), XML vs JavaConfig vs properties/yaml.
7. **5. Practical Examples**: Exactly 5 verified, comprehensive examples:
   - *Example 1: Basic Idiomatic Usage*
   - *Example 2: Production Enterprise Scenario*
   - *Example 3: Edge Case & Boundary Handling*
   - *Example 4: Failure & Anti-Pattern Demonstration with Fix*
   - *Example 5: High-Performance / Modern Implementation (Virtual Threads, Records)*
8. **6. Best Practices**: Concrete architectural rules and standards.
9. **7. Common Mistakes & Pitfalls**: Table with at least 6 detailed rows (`Mistake`, `Why It Fails`, `Practical Remedy`).
10. **8. Performance & Security Notes**: GC allocations, startup impact, thread-safety, OWASP guardrails, CVE mitigations.
11. **9. Comparison / When to Use What**: Trade-off comparison matrix vs alternative patterns (e.g., MVC vs WebFlux, RestClient vs WebClient).
12. **10. Interview Questions**: 8 progressive technical questions (Basic to Staff level) with comprehensive answers in collapsible `<details>`.
13. **11. Practice Exercises**: 6 progressive exercises (2 Easy, 2 Medium, 2 Hard) with verified solutions in `<details>`.
14. **12. Summary / Cheat Sheet**: High-density reference summary.
15. **13. Further Reading**: Official documentation links, RFCs, and Spring guides.

---

## 4. Strict 5-Section Reference Project Schema

Every reference architecture in `17-projects/` adheres to:

1. **Metadata & Production Baseline**: Target versions, complexity, dependencies.
2. **Section 1: Architectural Overview & Domain Design**:
   - 1.1 System Architecture ASCII / Mermaid diagram.
   - 1.2 Multi-module or Layered Directory Layout.
3. **Section 2: Complete Production Implementation**:
   - Buildable `pom.xml` / `build.gradle`.
   - Complete domain models, entities, DTO records, repositories, services, controllers, exception handlers.
4. **Section 3: Automated Testing Suite**:
   - Unit tests, `@SpringBootTest`, Slice tests (`@WebMvcTest`, `@DataJpaTest`), Testcontainers integration tests.
5. **Section 4: Production Infrastructure & Deployment**:
   - Multi-stage Dockerfile (Jenter / Distroless / Buildpacks), `compose.yaml`, Kubernetes manifests.
6. **Section 5: Operational Runbook & Verification**:
   - cURL verification commands, Actuator metrics endpoints, health checks.

---

## 5. Markdown-Only Repository Invariant

- **Zero non-markdown files inside `03-backend/spring-boot/`** (except root `tree.txt`).
- All Java code, XML configurations, YAML definitions, Dockerfiles, and SQL migrations are encapsulated inside syntax-highlighted code blocks (` ```java `, ` ```xml `, ` ```yaml `).
