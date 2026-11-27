# Plugin & Registry Pattern

| Field | Value |
| :--- | :--- |
| **Category** | Enterprise Application Patterns (Extensibility) |
| **Origin** | Martin Fowler (*PoEAA*, 2002) |
| **Difficulty** | Intermediate |
| **Prerequisite(s)** | [Open/Closed Principle](file:///E:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/02-open-closed-principle.md), [Factory Method](file:///E:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/02-factory-method.md) |
| **Status** | Complete |

> **TL;DR**: A **Registry** maps string keys or types to **Plugin** implementations at startup/runtime (`register("stripe", StripePlugin)`), letting you add new formats, payment providers, or auth strategies without editing a central `switch` statement.

---

## 1. The Problem: Why Do Plugin & Registry Exist?

Every time you add a new export format (`CSV`, `PDF`, `XLSX`) or payment provider (`Stripe`, `PayPal`, `Adyen`), a central `switch (type)` statement grows longer and must be modified—violating the **Open/Closed Principle** and blocking third-party extensions.

---

## 2. What Are Plugin & Registry?

- **Plugin**: An implementation conforming to a shared extension interface (`Exporter`, `PaymentProvider`).
- **Registry**: A central lookup map (`Map<String, Plugin>`) where plugins register themselves (via config, `ServiceLoader`, decorators, or startup wiring) and callers resolve them by key (`registry.get("pdf")`).

---

## 3. How It Works

```mermaid
flowchart LR
    Boot["Startup / Config"] -->|register('csv', CsvExp)| Reg["ExporterRegistry"]
    Boot -->|register('pdf', PdfExp)| Reg
    App["ReportService"] -->|get('pdf')| Reg --> PdfExp
```

---

## 4. What Goes Wrong Without It

```python
# ❌ OCP VIOLATION: Must edit core dispatcher for every new format
def export_report(fmt: str, data: list):
    if fmt == "csv": ...
    elif fmt == "pdf": ...
    elif fmt == "xlsx": ...
```

---

## 5. Multi-Language Examples

### Java 25 LTS (`Map` Registry + `ServiceLoader` Concept)

```java
import java.util.*;

public class RegistryDemo {
    interface Exporter { String export(String data); }

    static class ExporterRegistry {
        private final Map<String, Exporter> plugins = new HashMap<>();
        void register(String key, Exporter plugin) { plugins.put(key, plugin); }
        Exporter get(String key) {
            var p = plugins.get(key);
            if (p == null) throw new IllegalArgumentException("Unknown format: " + key);
            return p;
        }
    }

    public static void main(String[] args) {
        var reg = new ExporterRegistry();
        reg.register("csv", data -> "CSV:" + data);
        reg.register("json", data -> "{\"data\":\"" + data + "\"}");
        System.out.println(reg.get("json").export("sales"));
    }
}
```

### TypeScript 5.8

```ts
type Exporter = (data: string) => string;

class ExporterRegistry {
  private readonly plugins = new Map<string, Exporter>();
  register(key: string, plugin: Exporter) { this.plugins.set(key, plugin); }
  get(key: string): Exporter {
    const p = this.plugins.get(key);
    if (!p) throw new Error('Unknown format: ' + key);
    return p;
  }
}

const reg = new ExporterRegistry();
reg.register('csv', (d) => `CSV:${d}`);
console.log(reg.get('csv')('sales'));
```

### Python 3.14 (Decorator-Driven Plugin Registry)

```python
from typing import Callable

EXPORTERS: dict[str, Callable[[str], str]] = {}

def register_exporter(name: str):
    def decorator(fn: Callable[[str], str]):
        EXPORTERS[name] = fn
        return fn
    return decorator

@register_exporter("csv")
def export_csv(data: str) -> str:
    return f"CSV:{data}"

print(EXPORTERS["csv"]("sales"))
```

---

## 6. Real-World Examples

1. **Java `java.util.ServiceLoader<T>` / JDBC `DriverManager`**: Automatically discovers database drivers from `META-INF/services/` on the classpath.
2. **Python `importlib.metadata.entry_points()` / `pytest` plugins**: Third-party packages register CLI commands or fixtures automatically.
3. **Webpack / Vite / ESLint Plugins**: Core runner invokes registered plugin hooks without knowing plugin implementations.

---

## 7. Related Patterns

- [Factory Method](file:///E:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/02-factory-method.md), [Microkernel & Plugin Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/10-micro-kernel-and-plugin.md).

---

## 8. Advantages

- **Open/Closed Compliance**: Add new plugins without editing core dispatch code.
- **Runtime Configurability**: Select implementations via config/feature flags (`PAYMENT_PROVIDER=adyen`).

---

## 9. Limitations & When NOT to Use

- **Global Mutable Registry Trap**: A global static `Registry` makes tests interfere with each other and hides dependencies (Service Locator anti-pattern). Prefer **instance-scoped registries** wired via DI!

---

## 10. Anti-Patterns & Common Mistakes

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| Using a global static Registry as a Service Locator everywhere | Hides class dependencies and breaks parallel tests | Inject the Registry (or `Map<String, Plugin>`) via constructor/DI |
| Returning `null` on missing plugin key | Causes `NullPointerException` downstream | Fail fast with a clear `UnknownPluginException` |
| Allowing silent key overwrites | Two plugins with the same key silently clobber each other | Throw on duplicate key registration unless override is explicit |
| Import-time decorator registration on unimported modules | Plugin never registers if module isn't imported! | Explicitly import plugin modules or use `entry_points` / `ServiceLoader` |

---

## 11. Comparison: Registry vs Service Locator

| Aspect | Plugin Registry | Service Locator (Anti-Pattern) |
| :--- | :--- | :--- |
| **Purpose** | Look up **extensible strategy variants** by key (`"pdf"`, `"csv"`) | Look up arbitrary application dependencies globally |
| **Wiring** | Injected into the dispatcher | Pulled statically from anywhere |

---

## 12. Design Checklist

- [ ] Is the Registry injected into callers rather than accessed as a global static singleton?
- [ ] Does `get(key)` fail fast with a descriptive error if a key is missing?

---

## 13. Interview Questions

<details>
<summary>1. How do Plugin and Registry satisfy the Open/Closed Principle?</summary>

New behaviors are added by implementing the Plugin interface and registering them, without modifying the core dispatcher code.

</details>

<details>
<summary>2. How does Java's `ServiceLoader` discover plugins?</summary>

It scans `META-INF/services/<fully-qualified-interface-name>` files across JARs on the classpath/modulepath and lazily instantiates listed implementations.

</details>

<details>
<summary>3. What is the difference between a Plugin Registry and the Service Locator anti-pattern?</summary>

A Plugin Registry maps a domain key (like `"pdf"`) to interchangeable implementations of **one** plugin interface and is injected via DI. A Service Locator is a global container used to fetch arbitrary dependencies anywhere, hiding coupling.

</details>

<details>
<summary>4. Why do Python decorator-based registries fail if a plugin file isn't imported?</summary>

Because decorators only execute when Python imports that module—if nothing imports `plugins/xlsx.py`, `@register` never runs.

</details>

<details>
<summary>5. How does Spring Boot implement the Registry pattern cleanly?</summary>

Injecting `Map<String, PaymentProvider>` or `List<Exporter>` automatically populates a registry of all beans implementing that interface.

</details>

<details>
<summary>6. Should duplicate plugin keys overwrite silently or fail?</summary>

Fail fast at startup unless explicit priority/override semantics are intended.

</details>

<details>
<summary>7. When is a simple `switch` better than a Plugin Registry?</summary>

When there are 2–3 fixed internal variants that never change independently.

</details>

<details>
<summary>8. How do you test code that uses a Plugin Registry?</summary>

Construct a fresh `ExporterRegistry` instance in each test and register only the fake/target plugins needed.

</details>

---

## 14. Practice Exercises

<details>
<summary>Exercise 1: Duplicate-Key Guard</summary>

Update `ExporterRegistry.register()` to throw an error if a key is already registered.

</details>

<details>
<summary>Exercise 2: Spring-Style `List<Plugin>` Auto-Registration</summary>

Build a `Registry` constructor taking `List<Plugin>` where each plugin exposes `String key()`.

</details>

<details>
<summary>Exercise 3: Python `importlib` Discovery</summary>

Discover and register plugins dynamically from a dictionary of module names.

</details>

---

## 15. Cheat Sheet

- **Plugin**: Shared interface + swappable implementations.
- **Registry**: `Map<Key, Plugin>` injected at startup; fail fast on missing/duplicate keys.

---

## 16. In Your Language

| Track | Relevant Deep-Dive |
| :--- | :--- |
| **Java** | [ServiceLoader & DI Maps](file:///E:/Learning/engineering-notes/03-backend/spring-boot/README.md) |
| **TypeScript** | [NestJS Dynamic Modules](file:///E:/Learning/engineering-notes/03-backend/nestjs/README.md) |
| **Python** | [Python Packaging & Entry Points](file:///E:/Learning/engineering-notes/01-languages/python/16-packaging-and-tooling/README.md) |

---

## 17. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [MVC, MVP & MVVM](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/12-mvc-mvp-mvvm.md) | [Module 09](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/README.md) | [Optimistic & Pessimistic Locking](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/14-optimistic-and-pessimistic-locking-patterns.md) |
