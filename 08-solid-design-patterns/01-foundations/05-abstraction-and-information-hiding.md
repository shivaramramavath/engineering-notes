# Abstraction and Information Hiding

> **One-line summary:** Information hiding is the principle of segregating design decisions that are volatile or likely to change behind stable interfaces, ensuring that internal data structures, algorithms, and vendor mechanics remain private design secrets.

| Attribute | Specification |
|---|---|
| **Level** | Foundations / Core |
| **Prerequisites** | [01-oop-pillars.md](01-oop-pillars.md), [04-programming-to-interfaces.md](04-programming-to-interfaces.md) |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Complete |
| **Category** | Foundations |
| **Also Known As** | Parnas Information Hiding, Secret Encapsulation |
| **Languages Tested** | Java 25 (LTS) / TypeScript 5.8 (Node 24 native) / Python 3.14 |

---

## Table of Contents
- [1 The Problem](#1-the-problem)
- [2 Why It Exists](#2-why-it-exists)
- [3 What It Is](#3-what-it-is)
- [4 The Problem Without It](#4-the-problem-without-it)
- [5 How It Solves It](#5-how-it-solves-it)
- [6 How It Works](#6-how-it-works)
- [7 Real-World Examples](#7-real-world-examples)
- [8 When to Use and When NOT to Use](#8-when-to-use-and-when-not-to-use)
- [9 Variations and Language Idioms](#9-variations-and-language-idioms)
- [10 Relationship to Other Principles and Patterns](#10-relationship-to-other-principles-and-patterns)
- [11 Common Mistakes and Anti-Patterns](#11-common-mistakes-and-anti-patterns)
- [12 Testing and Design Verification](#12-testing-and-design-verification)
- [13 Performance, Concurrency and Security Notes](#13-performance-concurrency-and-security-notes)
- [14 Interview Questions](#14-interview-questions)
- [15 Practice Exercises](#15-practice-exercises)
- [16 Summary / Cheat Sheet](#16-summary--cheat-sheet)
- [17 Further Reading](#17-further-reading)

---

## 1 The Problem

An engineering team building an enterprise service mesh designs an internal configuration registry. The developer stores backend service endpoints using a raw in-memory `Map<String, String[]>`, where each array element holds a positional configuration attribute:
- Index `0`: Hostname
- Index `1`: Port
- Index `2`: Protocol (`http`, `grpc`, `tcp`)

Over six months, 40 different microservices import and parse this raw data structure directly:
```java
String host = registry.rawConfig.get("db")[0];
int port = Integer.parseInt(registry.rawConfig.get("db")[1]);
```

Then, the platform team must add TLS certificate configuration and support multi-region DNS failover addresses. Changing the storage format from a single `String[]` array to an IPv4/IPv6 dual-stack record breaks all 40 consuming microservices. The entire organization spends three weeks coordinating a synchronized deployment to fix array out-of-bounds exceptions.

The failure was not lack of unit tests—it was **leaking a volatile design secret** to external callers.

---

## 2 Why It Exists

In 1972, David L. Parnas published his landmark paper, *"On the Criteria To Be Used in Decomposing Systems into Modules"*. Parnas demonstrated that modularity should not be based on flowchart execution steps (e.g. step 1: read, step 2: process, step 3: write).

Instead, Parnas proposed that:
> *"Every module should be designed to hide a secret from the rest of the system."*

A "secret" is any design decision that is subject to change:
- Data structure layouts (e.g. array vs hash table vs B-tree).
- Hardware or operating system interfaces.
- Third-party vendor protocol specifics.
- Complex parsing and sorting algorithms.

---

## 3 What It Is

- **Information Hiding**: A design principle where volatile decisions and internal data structures are completely concealed inside modules.
- **Abstraction**: The outward-facing conceptual model that exposes essential features while ignoring irrelevant details.

```mermaid
graph LR
    subgraph Caller[Client Consumer]
        App[Application Logic]
    end
    subgraph Boundary[Abstraction Barrier / Public Interface]
        API[getEndpoint(service) -> EndpointConfig]
    end
    subgraph Secret[Hidden Design Secrets]
        Storage[(Map / Cache / B-Tree)]
        Parsing[Regex / Protocol Decoders]
        Security[Key Decryption]
    end
    App --> Boundary
    Boundary --> Secret
```

---

## 4 The Problem Without It

Here is the naive design that leaks its internal array layout:

```java
// File: InfoHidingWithoutApp.java
import java.util.*;

class NaiveConfigRegistry {
    // Secret exposed: config is stored as a Map of String to String arrays
    public Map<String, String[]> rawConfig = new HashMap<>();
}

public class InfoHidingWithoutApp {
    public static void main(String[] args) {
        NaiveConfigRegistry registry = new NaiveConfigRegistry();
        // Index 0: host, Index 1: port, Index 2: protocol
        registry.rawConfig.put("db", new String[]{"localhost", "5432", "postgres"});

        // Callers tightly coupled to secret array layout:
        String host = registry.rawConfig.get("db")[0];
        int port = Integer.parseInt(registry.rawConfig.get("db")[1]);
        System.out.println("Naive caller coupled to secret: " + host + ":" + port);
    }
}
```

```text
Naive caller coupled to secret: localhost:5432
```

### The Change Request: Add IPv6 and Dynamic DNS Failover
When adding dynamic multi-host DNS failover:
1. Every caller indexing into `rawConfig.get("db")[0]` crashes or receives invalid data.
2. The internal storage structure cannot be modified without breaking all external clients.

---

## 5 How It Solves It

By encapsulating the storage mechanism behind an immutable `EndpointConfig` domain type, the internal map structure remains an isolated secret.

### Java Implementation
```java
// File: InfoHidingWithApp.java
import java.util.*;

record EndpointConfig(String host, int port, String protocol) {}

class SecureConfigRegistry {
    // The internal storage mechanism is a private design secret
    private final Map<String, EndpointConfig> storage = new HashMap<>();

    public void register(String service, String host, int port, String protocol) {
        storage.put(service, new EndpointConfig(host, port, protocol));
    }

    public Optional<EndpointConfig> getEndpoint(String service) {
        return Optional.ofNullable(storage.get(service));
    }
}

public class InfoHidingWithApp {
    public static void main(String[] args) {
        SecureConfigRegistry registry = new SecureConfigRegistry();
        registry.register("db", "localhost", 5432, "postgres");

        registry.getEndpoint("db").ifPresent(ep -> {
            System.out.println("Information hidden cleanly: " + ep.host() + ":" + ep.port());
        });
    }
}
```

### TypeScript Implementation
```typescript
// File: InfoHidingWithApp.ts
interface EndpointConfig {
    readonly host: string;
    readonly port: number;
    readonly protocol: string;
}

class SecureConfigRegistry {
    private readonly storage = new Map<string, EndpointConfig>();

    register(service: string, host: string, port: number, protocol: string): void {
        this.storage.set(service, { host, port, protocol });
    }

    getEndpoint(service: string): EndpointConfig | undefined {
        return this.storage.get(service);
    }
}

const reg = new SecureConfigRegistry();
reg.register("db", "localhost", 5432, "postgres");
const ep = reg.getEndpoint("db");
if (ep) {
    console.log(`TS Info hidden cleanly: ${ep.host}:${ep.port}`);
}
```

### Python Implementation
```python
# File: info_hiding_with_app.py
from dataclasses import dataclass
from typing import Optional

@dataclass(frozen=True)
class EndpointConfig:
    host: str
    port: int
    protocol: str

class SecureConfigRegistry:
    def __init__(self):
        self._storage: dict[str, EndpointConfig] = {}

    def register(self, service: str, host: str, port: int, protocol: str) -> None:
        self._storage[service] = EndpointConfig(host, port, protocol)

    def get_endpoint(self, service: str) -> Optional[EndpointConfig]:
        return self._storage.get(service)

reg = SecureConfigRegistry()
reg.register("db", "localhost", 5432, "postgres")
ep = reg.get_endpoint("db")
if ep:
    print(f"Python Info hidden cleanly: {ep.host}:{ep.port}")
```

```text
Information hidden cleanly: localhost:5432
TS Info hidden cleanly: localhost:5432
Python Info hidden cleanly: localhost:5432
```

---

## 6 How It Works

### Information Hiding vs Encapsulation vs Abstraction

```
+------------------------------------------------------------------------------------------+
| Information Hiding (Architectural Philosophy):                                           |
| "Identify volatile design decisions (secrets) and encapsulate them behind modules."     |
|                                                                                          |
| Encapsulation (Mechanical Tool):                                                         |
| "Use access modifiers (private, package-private) and immutable types to enforce the     |
| boundary."                                                                               |
|                                                                                          |
| Abstraction (Mental Model):                                                              |
| "Present a simplified, cohesive view of the system that omits implementation noise."     |
+------------------------------------------------------------------------------------------+
```

---

## 7 Real-World Examples

### 1. Business Domain: Encryption Key Management Service
An enterprise identity platform manages cryptographic keys. The exact storage mechanism (AWS KMS hardware security module, HashiCorp Vault, or local AES-GCM keystore) is hidden behind `KeyProvider.getKey(keyId)`. When regulatory requirements mandate migrating from software keys to FIPS 140-2 Level 3 hardware modules, zero business applications require code changes.

### 2. Standard Library: `java.util.HashMap` Bucket Treeification
In Java 8, `HashMap` was updated so that when hash collisions exceed 8 entries in a single bucket, the bucket dynamically transitions from a linked list to a Red-Black tree (`TreeNode`), reducing worst-case lookups from $O(n)$ to $O(\log n)$. Because internal nodes were an information-hidden secret, millions of Java programs ran faster on Java 8 with zero source code changes.

### 3. Modern Cloud / Infrastructure: Container Runtimes (CRI)
Kubernetes coordinates container workloads via the Container Runtime Interface (CRI). Whether the underlying node uses `containerd`, `CRI-O`, or Kata Containers (microVMs) is an information-hidden secret. Kubernetes controllers execute pods without knowing the Linux cgroups mechanics used beneath.

---

## 8 When to Use and When NOT to Use

### Hide Information When:
- A data structure is complex or likely to be refactored for performance.
- An operation depends on third-party SDKs, hardware, or external network services.
- The business algorithm contains proprietary calculations that must remain centralized.

### Avoid / Over-Engineering Signals:
- **Trivial Data Aggregates**: Hiding `x` and `y` inside a 2D `Point` class by turning them into private fields with `getX()` and `getY()` provides no information hiding. A Point is defined by its coordinates.

---

## 9 Variations and Language Idioms

- **Java**: Java Modules (`module-info.java`) introduced in Java 9 elevate information hiding to the package and JAR boundary. Even `public` classes cannot be accessed across module boundaries unless explicitly exported.
- **Python**: Package `__all__` lists in `__init__.py` hide internal helper modules from `from package import *`.
- **Rust**: Fields and modules are private by default; items must be explicitly marked `pub` or `pub(crate)` to escape their scope.

---

## 10 Relationship to Other Principles and Patterns

- **Single Responsibility Principle (SRP)**: Each class hides a single reason to change.
- **Facade Pattern**: Hides the complex interaction of multiple subsystem classes behind a simplified API.
- **Law of Demeter**: Prevents callers from piercing information-hiding boundaries.

---

## 11 Common Mistakes and Anti-Patterns

| Mistake | Why It Breaks | Production Fix |
|---|---|---|
| **Leaking Collection Mutation** | Returning an internal `List` allows callers to modify it without the owning class knowing. | Return unmodifiable views (`Collections.unmodifiableList()`) or defensive copies. |
| **Exposing Serialization Formats** | Allowing callers to parse raw JSON or XML strings directly inside business logic. | Parse into typed domain entities before passing downstream. |
| **Leaky Exceptions** | Letting low-level exceptions like `SQLException` escape across architectural boundaries. | Catch low-level exceptions and wrap them in high-level domain exceptions (`DataAccessException`). |
| **Boolean Flags Controlling Internals** | Adding flags like `isTestEnvironment` or `useCache` exposes internal implementation branching. | Use polymorphism or separate strategies. |
| **Temporal Coupling Secrets** | Requiring callers to call `init()` then `configure()` then `execute()` leaks internal lifecycle states. | Enforce correct state transitions via constructor or builder. |
| **Direct Database Entity Exposure** | Passing ORM database entities (`@Entity`) directly to REST API responses leaks database schema details. | Map entities to dedicated Data Transfer Objects (DTOs). |

---

## 12 Testing and Design Verification

Information hiding makes tests resilient to refactoring. When tests depend only on public behavior and not internal data structures, refactoring internal representations never breaks existing test suites:

```java
// Testing behavior, not internal data structures
class SecureConfigRegistryTest {
    void testRegistrationAndRetrieval() {
        SecureConfigRegistry reg = new SecureConfigRegistry();
        reg.register("api", "api.internal.net", 443, "https");

        Optional<EndpointConfig> ep = reg.getEndpoint("api");
        assert ep.isPresent();
        assert ep.get().port() == 443;
        // The test does not know or care whether storage is a Map, SQLite, or Trie.
    }
}
```

---

## 13 Performance, Concurrency and Security Notes

- **Performance**: Hiding representation secrets allows the author to swap slow algorithms for high-performance caches without changing client code.
- **Security**: Information hiding is the primary foundation of the Principle of Least Privilege. Hiding cryptographic key representations prevents memory dumping and reflection-based exfiltration.

---

## 14 Interview Questions

<details>
<summary><strong>1. How does David Parnas's definition of Information Hiding differ from OOP Encapsulation?</strong></summary>

**Answer:**  
- **Encapsulation** is a *programming language mechanism* for restricting direct access to an object's fields (e.g. using the `private` keyword).
- **Information Hiding** is an *architectural design principle* for identifying system decisions that are volatile (algorithms, hardware interfaces, data structures) and designing modules specifically to isolate those decisions from other modules.
</details>

<details>
<summary><strong>2. What is a "Leaky Abstraction"? Give a real-world example.</strong></summary>

**Answer:**  
Formulated by Joel Spolsky ("The Law of Leaky Abstractions"), it states that all non-trivial abstractions leak internal details to some degree. For example, TCP abstracts an unreliable IP network into a reliable stream of packets. However, when a network cable is unplugged, the abstraction leaks through latency spikes, timeouts, and `ConnectionReset` errors that the programmer must handle.
</details>

<details>
<summary><strong>3. How does exception handling relate to information hiding?</strong></summary>

**Answer:**  
Throwing low-level infrastructural exceptions (like `java.sql.SQLException` or `com.mongodb.MongoTimeoutException`) from a business domain interface leaks the implementation secret that the underlying storage is a relational database or MongoDB. Proper information hiding catches low-level exceptions and translates them into domain exceptions (e.g. `UserNotFoundException`).
</details>

<details>
<summary><strong>4. Why should you avoid exposing raw ORM entities directly in REST API controllers?</strong></summary>

**Answer:**  
Exposing ORM entities leaks database column names, foreign key relationships, and internal database structures directly to HTTP clients. It also risks unintentional updates (mass assignment vulnerabilities) and breaking API contracts whenever a database column is renamed.
</details>

<details>
<summary><strong>5. What is the difference between a secret and a public contract?</strong></summary>

**Answer:**  
- A **public contract** is the formal interface, method signature, and behavioral invariants that a module guarantees to external callers.
- A **secret** is the internal machinery, data structure, or external dependency used to fulfill that contract, which can be modified without altering the public contract.
</details>

<details>
<summary><strong>6. How did Java 9 Modules improve information hiding?</strong></summary>

**Answer:**  
Prior to Java 9, any `public` class in a JAR could be accessed by any other class on the classpath, preventing library authors from creating package-internal helpers. Java 9 modules allow authors to declare which packages are `exports` to external modules; unexported packages cannot be accessed even via reflection.
</details>

<details>
<summary><strong>7. How does Information Hiding facilitate parallel development across large teams?</strong></summary>

**Answer:**  
When teams agree on the interface boundary (the contract), each team can implement its module concurrently without needing to coordinate internal design decisions. Team A can build an in-memory mock while Team B writes the real database engine, keeping dependencies strictly orthogonal.
</details>

<details>
<summary><strong>8. What is the risk of exposing an internal iterator vs an unmodifiable list?</strong></summary>

**Answer:**  
An iterator may expose `iterator.remove()`, which allows external callers to mutate internal state during iteration, potentially corrupting collection invariants or causing `ConcurrentModificationException`.
</details>

---

## 15 Practice Exercises

### Exercise 1: Spot the Leaked Secret (Easy)
Identify what implementation secret is leaked by this interface:
```java
public interface ProductCatalog {
    java.sql.ResultSet queryProductsBySql(String sqlQuery);
}
```
<details>
<summary><strong>Solution</strong></summary>

The interface leaks that the catalog uses a relational SQL database by exposing `java.sql.ResultSet` and accepting raw SQL queries.  
**Fix:** Define domain query methods returning domain objects: `List<Product> findByCategory(Category category)`.
</details>

### Exercise 2: Refactor Leaky Internal State (Medium)
Protect the internal `HashSet` secret in this `RoleManager`:
```java
public class RoleManager {
    public Set<String> roles = new HashSet<>();
}
```
<details>
<summary><strong>Solution</strong></summary>

```java
public class RoleManager {
    private final Set<String> roles = new HashSet<>();

    public void addRole(String role) {
        if (role == null || role.isBlank()) throw new IllegalArgumentException();
        roles.add(role.toUpperCase());
    }

    public boolean hasRole(String role) {
        return roles.contains(role.toUpperCase());
    }

    public Set<String> getRoles() {
        return Collections.unmodifiableSet(roles);
    }
}
```
</details>

### Exercise 3: Encapsulate Cache Eviction Strategy (Hard)
Design an in-memory cache where the eviction algorithm (LRU vs LFU) is an information-hidden secret that can be toggled without changing the cache's public `put()` and `get()` methods.
<details>
<summary><strong>Solution</strong></summary>

```java
public class Cache<K, V> {
    private interface EvictionPolicy<K> { void recordAccess(K key); K evict(); }
    private static class LruPolicy<K> implements EvictionPolicy<K> { /* LRU logic */ }
    private static class LfuPolicy<K> implements EvictionPolicy<K> { /* LFU logic */ }

    private final Map<K, V> data = new HashMap<>();
    private final EvictionPolicy<K> evictionPolicy;

    public Cache(boolean useLru) {
        this.evictionPolicy = useLru ? new LruPolicy<>() : new LfuPolicy<>();
    }

    public void put(K key, V value) {
        data.put(key, value);
        evictionPolicy.recordAccess(key);
    }

    public V get(K key) {
        evictionPolicy.recordAccess(key);
        return data.get(key);
    }
}
```
</details>

### Exercise 4: Information Hiding in TypeScript DTOs (Medium)
Explain how to transform a database row into a secure public DTO in TypeScript so that the user's password hash and internal database ID are hidden.
<details>
<summary><strong>Solution</strong></summary>

```typescript
interface DatabaseUserRow {
    db_id: number;
    username: string;
    password_hash: string;
    created_at: Date;
}

interface PublicUserDto {
    readonly username: string;
    readonly memberSinceYear: number;
}

function toPublicDto(row: DatabaseUserRow): PublicUserDto {
    return {
        username: row.username,
        memberSinceYear: row.created_at.getFullYear()
    };
}
```
</details>

### Exercise 5: Spot the Leaky Exception (Easy)
Why is throwing `FileNotFoundException` from a `UserAvatarService.getAvatar(String userId)` a leaky abstraction?
<details>
<summary><strong>Solution</strong></summary>

It leaks the secret that avatars are stored on a local filesystem disk. If avatars are moved to an S3 bucket or database, the method cannot throw `FileNotFoundException` without breaking callers. It should throw a domain exception: `AvatarNotFoundException`.
</details>

### Exercise 6: Complete Python Information Hiding (Hard)
Implement a `SecureSessionStore` in Python that stores session tokens internally using an HMAC-SHA256 hash, ensuring that callers cannot see raw session tokens stored in memory.
<details>
<summary><strong>Solution</strong></summary>

```python
import hmac
import hashlib
from typing import Optional

class SecureSessionStore:
    def __init__(self, secret_salt: bytes):
        self._salt = secret_salt
        self._store: dict[str, str] = {} # Hashed token -> User ID

    def _hash_token(self, token: str) -> str:
        return hmac.new(self._salt, token.encode(), hashlib.sha256).hexdigest()

    def create_session(self, user_id: str, raw_token: str) -> None:
        hashed = self._hash_token(raw_token)
        self._store[hashed] = user_id

    def validate_session(self, raw_token: str) -> Optional[str]:
        hashed = self._hash_token(raw_token)
        return self._store.get(hashed)
```
</details>

---

## 16 Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
| INFORMATION HIDING CHEAT SHEET                                                          |
| Principle: Decompose systems so that every module hides a volatile design secret.       |
|                                                                                         |
| Secrets to Hide:                                                                        |
| - Exact data structure layouts (arrays, hash tables, linked lists).                     |
| - Third-party vendor network protocols, SDKs, and storage drivers.                      |
| - Concrete serialization formats (JSON, XML, Protobuf).                                 |
|                                                                                         |
| Benefits:                                                                               |
| - Independent parallel development without inter-team friction.                         |
| - Safe internal refactoring and optimization with zero caller impact.                   |
| - Defense in depth and isolation of security vulnerabilities.                           |
+-----------------------------------------------------------------------------------------+
```

---

## 17 Further Reading

- Parnas, David L. (1972). *"On the Criteria To Be Used in Decomposing Systems into Modules"*, Communications of the ACM.
- Spolsky, Joel (2002). *"The Law of Leaky Abstractions"*, Gwern.net / Joel on Software.
- McConnell, Steve (2004). *"Code Complete"*, 2nd Edition, Chapter 5: Design in Construction.

### In Your Language
| Language Track | Deep Dive Reference |
|---|---|
| Java | [Java Modular System (JPMS)](file:///e:/Learning/engineering-notes/01-languages/java/24-design-patterns/README.md) |
| TypeScript | [TypeScript Modules & Namespaces](file:///e:/Learning/engineering-notes/01-languages/typescript/17-design-patterns/README.md) |
| Python | [Python Encapsulation & Scopes](file:///e:/Learning/engineering-notes/01-languages/python/23-design-patterns/README.md) |

---
[← Previous: 04 - Programming to Interfaces](04-programming-to-interfaces.md) | [Section Index](README.md) | [Next: 06 - Encapsulate What Varies →](06-encapsulate-what-varies.md)
