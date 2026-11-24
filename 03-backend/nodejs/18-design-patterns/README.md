# Module 18: Design Patterns in Node.js

> Idiomatic implementations of classic and cloud-native software design patterns tailored for server-side Node.js and TypeScript.

---

## Module Overview

Design patterns provide battle-tested blueprints for solving recurring software architecture challenges. This module explores Creational, Structural, Behavioral, and Distributed Reliability patterns (like Circuit Breakers and Unit of Work) tailored for asynchronous Node.js backends.

---

## Contents

| Index | Note | Pattern Category | Core Focus |
| :---: | :--- | :--- | :--- |
| **01** | [01-singleton.md](./01-singleton.md) | Creational | Module-cached singletons, database pool management. |
| **02** | [02-factory.md](./02-factory.md) | Creational | Factory Method, Abstract Factory, dynamic provider instantiation. |
| **03** | [03-builder.md](./03-builder.md) | Creational | Method chaining, complex configuration construction. |
| **04** | [04-observer.md](./04-observer.md) | Behavioral | `EventEmitter`, pub/sub, decoupled event observers. |
| **05** | [05-strategy.md](./05-strategy.md) | Behavioral | Swappable algorithms, authentication strategies (Passport style). |
| **06** | [06-decorator.md](./06-decorator.md) | Structural | Dynamic behavior wrapping, caching decorators, logging decorators. |
| **07** | [07-adapter.md](./07-adapter.md) | Structural | Interface adaptation, legacy library wrapping. |
| **08** | [08-middleware.md](./08-middleware.md) | Behavioral | Chain of responsibility, pipeline processing in Express/Fastify. |
| **09** | [09-dependency-injection.md](./09-dependency-injection.md) | Structural | Inversion of Control (IoC), constructor injection, test doubles. |
| **10** | [10-circuit-breaker-and-retry.md](./10-circuit-breaker-and-retry.md) | Reliability | Circuit Breaker with `opossum`, jittered exponential backoff. |
| **11** | [11-repository-and-unit-of-work.md](./11-repository-and-unit-of-work.md) | Data Pattern | Repository interfaces, multi-repo transactions with Unit of Work. |
| **12** | [12-facade-and-command-patterns.md](./12-facade-and-command-patterns.md) | Structural/Behavioral | Facade for complex subsystems, Command bus pattern. |

---

## Next Steps
Proceed to [19-system-design](../19-system-design/README.md) to explore large-scale distributed architectures: URL shorteners, e-commerce flash sales, and multi-tenant SaaS backends.
