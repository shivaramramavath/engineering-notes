# SOLID, Design Principles & Design Patterns Facts (SDP_FACTS.md)

| Category | Item | Verified Status / Fact | Source / Verification Reference |
|---|---|---|---|
| **Runtime** | Java LTS | Java 25.0.1 LTS (`build 25.0.1+8-LTS-27`) verified locally on Windows; compiler `javac 25.0.1` | Local JVM environment / Oracle OpenJDK Release Cadence |
| **Runtime** | TypeScript / Node | Node v24.11.1 with native TypeScript type-stripping support | Node.js official releases (Node 22.6+ / 24+ native TS strip) |
| **Runtime** | Python | Python 3.14.0 verified locally | python.org releases |
| **Origin** | GoF Patterns | 23 classic object-oriented design patterns published in 1994 | Gamma, Helm, Johnson, Vlissides (Addison-Wesley 1994) |
| **Origin** | Enterprise Patterns (PoEAA) | Repository, Unit of Work, Data Mapper, Service Layer, DTO, Identity Map, Lazy Load | Martin Fowler (Addison-Wesley 2002) |
| **Origin** | POSA Patterns | Reactor, Proactor, Monitor Object, Half-Sync/Half-Async, Active Object | Buschmann, Meunier, Rohnert, Sommerlad, Stal / Schmidt (Wiley 1996–2000) |
| **Origin** | SOLID Acronym | Michael Feathers (circa 2004) synthesized Uncle Bob's five principles | Robert C. Martin (2000 paper, "Agile Software Development" 2002) |
| **Origin** | OCP | Open-Closed Principle formulated by Bertrand Meyer in 1988 | "Object-Oriented Software Construction" (Prentice Hall 1988) |
| **Origin** | LSP | Liskov Substitution Principle formulated by Barbara Liskov & Jeannette Wing (1994) | "A Behavioral Notion of Subtyping", ACM TOPLAS 1994 |
| **Origin** | DRY Principle | "Don't Repeat Yourself" formulated by Andy Hunt & Dave Thomas (1999) | "The Pragmatic Programmer" (Addison-Wesley 1999) |
| **Origin** | Law of Demeter | Formulated at Northeastern University in 1987 | Karl Lieberherr, Ian Holland ("Object-Oriented Programming: An Objective Sense of Style") |
| **Origin** | GRASP Principles | Information Expert, Creator, Controller, Low Coupling, High Cohesion, Polymorphism, Pure Fabrication, Indirection, Protected Variations | Craig Larman, "Applying UML and Patterns" (Prentice Hall 1997) |
| **JDK Standard Lib** | Runtime Singleton | `java.lang.Runtime.getRuntime()` returns the single JVM runtime instance | OpenJDK `java.lang.Runtime` source |
| **JDK Standard Lib** | IO Decorator | `java.io.FilterInputStream`, `BufferedInputStream(new FileInputStream(...))` wraps stream | OpenJDK `java.io` package specification |
| **JDK Standard Lib** | Stream / Collections Strategy | `java.util.Comparator` / `java.lang.Comparable` passed to `sort()` | OpenJDK `java.util.Collections`, `java.util.Comparator` |
| **JDK Standard Lib** | Integer Flyweight | `java.lang.Integer.valueOf(int)` caches values from -128 to 127 | JLS §5.1.7 Boxing Conversion / OpenJDK `IntegerCache` |
| **JDK Standard Lib** | Dynamic Proxy | `java.lang.reflect.Proxy.newProxyInstance()` creates runtime interface proxies | OpenJDK `java.lang.reflect.Proxy` |
| **JDK Standard Lib** | Servlet Chain of Responsibility | `jakarta.servlet.Filter` & `FilterChain.doFilter(request, response)` | Jakarta EE Servlet Specification |
| **Python Stdlib** | Protocol Structural Typing | `typing.Protocol` enables runtime/static duck-typing interfaces | PEP 544 (Python 3.8+) |
| **Python Stdlib** | Memoization Decorator | `functools.lru_cache` decorates callables with caching proxy | Python official `functools` documentation |
| **Python Stdlib** | Immutability Value Object | `dataclasses.dataclass(frozen=True)` enforces read-only attributes | PEP 557 (Python 3.7+) |
| **Web Platform** | Observer | DOM `EventTarget.addEventListener()` / `dispatchEvent()` | WHATWG DOM Specification |
| **Enterprise / Cloud** | Resilience Patterns | Circuit Breaker, Retry with Jitter, Bulkhead, Outbox, Saga, Strangler Fig | Michael Nygard ("Release It!", 2007) / Chris Richardson ("Microservices Patterns", 2018) |

---
*Generated & Verified for 08-solid-design-patterns on 2026-11-26.*
