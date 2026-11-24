# Concept Template — NestJS Curriculum

> **Source of Truth** for all concept notes in `03-backend/nestjs/`. Every concept note must follow this exact 13-section structure.

---

```markdown
# [Module NN]: [Concept Title]

> [Short 1-2 sentence executive summary explaining the concept, its role in NestJS architecture, and the primary problem it solves.]

| Metadata | Details |
| :--- | :--- |
| **NestJS Version** | NestJS 11+ (v10 differences noted where applicable) |
| **Target Runtime** | Node.js >= 20.0.0 (Node.js 22 LTS recommended) |
| **HTTP Platform** | Express 5 (default) / Fastify 5 compatible |
| **Module Scope** | Global / Feature / Dynamic / Provider-level |
| **TypeScript Config** | `experimentalDecorators: true`, `emitDecoratorMetadata: true` |

---

## 1. Concept Overview (Why before How)
[Explain the core problem this concept solves. Contrast the naive or unmanaged approach with the idiomatic NestJS architectural pattern. Explain when to use and when NOT to use.]

---

## 2. Architecture & Data Flow Diagram
```
+----------------------------------------------------------------------------------------------------+
|                                      NestJS Architecture Flow                                      |
|                                                                                                    |
|  [ Inbound Request ] ──> [ Middleware ] ──> [ Guard ] ──> [ Interceptor (Pre) ] ──> [ Pipe ]      |
|                                                                                          │         |
|                                                                                          ▼         |
|  [ Exception Filter ] <── [ Interceptor (Post) ] <── [ Controller Handler / Service ] ◄──┘         |
+----------------------------------------------------------------------------------------------------+
```

---

## 3. Core Mechanics & Deep Dive
[In-depth technical breakdown of the APIs, decorators, metadata reflection (`Reflect.getMetadata`), lifecycle hooks, and dependency injection mechanisms involved.]

---

## 4. Complete Code Examples (5 Levels)

### 4.1 Basic Example (Hello World / Minimal Setup)
```typescript
// Minimal reproducible setup
```

### 4.2 Real-World Production Example
```typescript
// Enterprise scenario with DI, validation, error handling, and config
```

### 4.3 Edge Case Handling
```typescript
// Handling race conditions, nullability, dynamic scopes, or partial payloads
```

### 4.4 Failure Case & Debugging
```typescript
// Demonstrating an intentional failure mode, error stack, and the programmatic fix
```

### 4.5 Idiomatic Production Recipe
```typescript
// Gold-standard implementation following SOLID and NestJS best practices
```

---

## 5. Framework Internals (Under the Hood)
[Explain how NestJS executes this feature internally: metadata storage, DI container resolution, module compilation, execution context transitions, or platform adapter delegation.]

---

## 6. Common Pitfalls & Mistakes Table

| Mistake | Why It Happens | Idiomatic Fix |
| :--- | :--- | :--- |
| **1. Missing `@Injectable()`** | Omitting the decorator prevents TypeScript from emitting constructor metadata. | Always decorate service classes with `@Injectable()`. |
| **2. Circular Dependency Crash** | Two modules or services import each other directly. | Use `forwardRef(() => OtherService)` in constructors. |
| **3. Request-Scope Performance Drain** | Marking services `Scope.REQUEST` forces per-request instantiation. | Keep services `Scope.DEFAULT` (Singleton) and pass request context via parameters or AsyncLocalStorage. |
| **4. Middleware vs Interceptor Confusion**| Trying to access DI providers in standard Express middleware. | Use Interceptors or Guards when dependency injection is needed. |
| **5. Unhandled Promise in Async Pipe**| Forgetting `async/await` causes unhandled rejections to bypass filters. | Ensure all asynchronous transformations return resolved promises. |
| **6. Express 5 Wildcard Mismatch** | Using raw `*` in Nest 11 route paths causes `path-to-regexp` v8 failure. | Always name wildcards: `@Get('files/*path')`. |

---

## 7. Performance & Memory Impact
[Discuss execution overhead, allocations, GC pressure, provider scoping cost, and caching recommendations.]

---

## 8. Security Considerations
[Security analysis: OWASP API Top 10 vulnerabilities, injection prevention, authorization bypass risks, and data sanitization.]

---

## 9. Testing Strategies (Unit & E2E)

### Unit Test (`Test.createTestingModule`)
```typescript
// Unit test with mocked providers
```

### E2E Integration Test (`supertest`)
```typescript
// E2E test with real or in-memory application context
```

---

## 10. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: [Technical Interview Question]</strong></summary>

**Answer:**  
[Staff-level explanation detailing internals, trade-offs, and production considerations.]
</details>

[Total of 8 questions with `<details>` tags]

---

## 11. Hands-on Exercises

<details>
<summary><strong>Exercise 1 (Easy): [Title]</strong></summary>

**Prompt:** [Task description]  
**Solution:**
```typescript
// Solution code
```
</details>

[Total of 6 exercises: 2 Easy, 2 Medium, 2 Hard with `<details>` solutions]

---

## 12. Version Differences & Migration Notes (Nest 10 vs Nest 11)
[Specific breaking changes, deprecated APIs, Express 5 vs 4 nuances, and upgrade steps.]

---

## 13. Further Reading & References
- [NestJS Official Documentation](https://docs.nestjs.com/)
- [NestJS GitHub Repository](https://github.com/nestjs/nest)
```
