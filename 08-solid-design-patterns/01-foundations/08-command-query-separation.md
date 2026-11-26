# Command-Query Separation (CQS)

> **One-line summary:** Every method should either be a command that performs an action and mutates state, or a query that returns data without observable side-effects—asking a question must never change the answer.

| Attribute | Specification |
|---|---|
| **Level** | Foundations / Core |
| **Prerequisites** | [01-oop-pillars.md](01-oop-pillars.md), [07-immutability-and-value-objects.md](07-immutability-and-value-objects.md) |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Complete |
| **Category** | Foundations |
| **Also Known As** | CQS Principle, Side-Effect-Free Queries |
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

A team building a distributed message queue writes an internal helper method:
```java
public class MessageQueue {
    public Message getNextMessage() {
        Message msg = storage.poll(); // Dequeues and removes message from storage
        return msg;
    }
}
```

An operations engineer attaches a health check monitor to inspect queue activity:
```java
if (queue.getNextMessage() != null) {
    logger.info("Queue is healthy and active");
}
```

The health monitor checks the queue every 5 seconds. To the team's horror, **thousands of legitimate customer messages vanish into thin air without ever being processed**. 

Why? Because the developer assumed that calling a getter (`getNextMessage()`) was an innocuous read operation. Instead, "asking the question" removed the message from the queue. Because read operations were coupled with destructive state mutation, observability tooling silently destroyed customer transactions.

---

## 2 Why It Exists

Bertrand Meyer formulated the Command-Query Separation principle in 1988 while designing the Eiffel programming language:
> *"Asking a question should not change the answer."*

In software engineering, side-effects are the primary source of bugs. If calling a getter or read function alters database rows, increments counters, or invalidates caches, developers cannot safely inspect, log, or debug system state without triggering unexpected cascading mutations.

---

## 3 What It Is

Under CQS, every method belongs strictly to one of two categories:

| Category | Role | State Mutation | Return Value | Idempotent? |
|---|---|:---:|:---:|:---:|
| **Command** | Change state / execute action | **Yes** | `void` (or status result) | No |
| **Query** | Return data / answer question | **No (Zero observable side-effects)** | Non-void result | **Yes (Pure)** |

```mermaid
graph TD
    Client[Caller / Client] --> Query[Query: currentId()]
    Query -->|Reads State| State[(Internal State)]
    State -.->|Returns Data without mutation| Client
    
    Client --> Command[Command: increment()]
    Command -->|Mutates State| State
    State -.->|Returns void| Client
```

---

## 4 The Problem Without It

Here is the naive design where a method named like a query (`getNextId()`) has a hidden mutation side-effect:

```java
// File: CqsWithoutApp.java
class NaiveQueue {
    private int counter = 0;
    // Violates CQS: method named 'getNextId' mutates counter and returns it!
    public int getNextId() {
        return ++counter;
    }
}

public class CqsWithoutApp {
    public static void main(String[] args) {
        NaiveQueue queue = new NaiveQueue();
        
        // Developer inspects current ID
        int id1 = queue.getNextId();
        // Logger reads current ID for debug metrics
        int idLog = queue.getNextId(); 
        
        System.out.println("CQS Violation: Logger unintentionally changed state! id1=" + id1 + ", idLog=" + idLog);
    }
}
```

```text
CQS Violation: Logger unintentionally changed state! id1=1, idLog=2
```

### The Change Request: Attach a Performance Metrics Exporter
When an APM agent (Datadog/Prometheus) reads `getNextId()` every 10 seconds to expose queue metrics:
- State increments 8,640 times per day without any real business transactions occurring!
- IDs skip numbers, creating gaps in financial audit trails.

---

## 5 How It Solves It

By strictly separating the **Command** (`increment()`) from the **Query** (`currentId()`), queries become pure and safe to invoke at any time.

### Java Implementation
```java
// File: CqsWithApp.java
class CqsQueue {
    private int currentId = 0;

    // Command: mutates state, does not return the queried calculation
    public void increment() {
        this.currentId++;
    }

    // Query: pure, idempotent, zero side-effects
    public int currentId() {
        return this.currentId;
    }
}

public class CqsWithApp {
    public static void main(String[] args) {
        CqsQueue queue = new CqsQueue();
        queue.increment(); // Explicit command

        int id1 = queue.currentId();
        int idLog = queue.currentId(); // Logger inspects without side effects!

        System.out.println("CQS Respected: id1=" + id1 + ", idLog=" + idLog + " (Equal and safe)");
    }
}
```

### TypeScript Implementation
```typescript
// File: CqsWithApp.ts
class CqsQueue {
    private _currentId = 0;

    increment(): void {
        this._currentId++;
    }

    get currentId(): number {
        return this._currentId;
    }
}

const q = new CqsQueue();
q.increment();
const id1 = q.currentId;
const idLog = q.currentId;
console.log(`TS CQS Respected: id1=${id1}, idLog=${idLog} (Equal and safe)`);
```

### Python Implementation
```python
# File: cqs_with_app.py
class CqsQueue:
    def __init__(self):
        self._current_id = 0

    def increment(self) -> None:
        self._current_id += 1

    @property
    def current_id(self) -> int:
        return self._current_id

q = CqsQueue()
q.increment()
id1 = q.current_id
id_log = q.current_id
print(f"Python CQS Respected: id1={id1}, id_log={id_log} (Equal and safe)")
```

```text
CQS Respected: id1=1, idLog=1 (Equal and safe)
TS CQS Respected: id1=1, idLog=1 (Equal and safe)
Python CQS Respected: id1=1, id_log=1 (Equal and safe)
```

---

## 6 How It Works

### The Principle of Idempotency in Queries
A query method satisfies referential transparency:
$$f(x) = f(x)$$
Invoking `queue.currentId()` 1 time or 10,000 times leaves the system in the exact same state. This unlocks:
- **Safe Caching**: Query results can be cached aggressively in RAM or Redis.
- **Compiler Optimizations**: The runtime can eliminate redundant query invocations.
- **Safe Debugging**: Setting breakpoints and evaluating watch expressions in an IDE will never corrupt system data.

---

## 7 Real-World Examples

### 1. Business Domain: User Authentication vs Session Creation
- **Query**: `passwordHasher.verify(inputPassword, storedHash) -> boolean` (Pure computation; returns boolean, changes nothing).
- **Command**: `sessionManager.createSession(userId) -> void` (Mutates session database, sets cookie, emits login event).  
Separating these allows a rate-limiter to verify passwords without accidentally spawning active sessions for failed attempts.

### 2. Standard Library: `java.util.Queue` (`peek` vs `poll`)
The Java standard library deliberately provides two distinct methods:
- `queue.peek()`: **Query** (Inspects the head element without removing it; pure and repeatable).
- `queue.poll()`: **Command & Query** (Removes the head element). While standard collections sometimes combine remove & return for convenience, enterprise domain models strictly prefer separating `remove()` from `peek()`.

### 3. Modern Cloud / Infrastructure: CQRS (Command Query Responsibility Segregation)
At an architectural scale, CQS expands into **CQRS** (Greg Young / Martin Fowler). The write model (Commands updating MySQL/PostgreSQL) is physically separated from the read model (Queries reading denormalized Elasticsearch or Redis indexes).

---

## 8 When to Use and When NOT to Use

### Apply CQS When:
- Writing domain services, aggregate roots, and entities.
- Designing public APIs where unexpected side-effects cause production bugs.
- Building systems that require auditability, event sourcing, or distributed caching.

### Legitimate Pragmatic Exceptions:
- **Atomic Concurrency Operations**: In multi-threaded programming, `AtomicInteger.decrementAndGet()` or `ConcurrentMap.putIfAbsent()` must atomically mutate and return the new value to prevent race conditions.
- **Queue / Stack Popping**: `stack.pop()` returns the removed item to avoid two synchronized calls.

---

## 9 Variations and Language Idioms

- **Getters / Properties**: Modern languages (TypeScript `get`, Python `@property`, C# properties) syntactically enforce that property access looks like field access and should never perform expensive mutations or network calls.
- **Command Objects**: In behavioral patterns, commands are modeled as dedicated objects (`CreateOrderCommand`, `PayInvoiceCommand`).

---

## 10 Relationship to Other Principles and Patterns

- **CQRS (Architecture)**: Architectural evolution of CQS across database boundaries.
- **Command Pattern**: Encapsulates state-mutating requests as discrete objects.
- **Immutability**: Queries on immutable objects are mathematically guaranteed to obey CQS.

---

## 11 Common Mistakes and Anti-Patterns

| Mistake | Why It Breaks | Production Fix |
|---|---|---|
| **Lazy Initialization Side-Effects** | A getter `getInvoice()` that calculates and writes rows to the database on first access. | Pre-compute or explicitly trigger computation via a command (`generateInvoice()`). |
| **Logging that Mutates Counters** | An inspector method that increments a counter every time it is read. | Move metrics recording to an explicit command or interceptor. |
| **Authentication on Read** | A `getUser()` method that checks if a token is expired and silently revokes it. | Split into a query `isTokenExpired()` and a command `revokeToken()`. |
| **Returning Domain Entities from Commands** | Returning a freshly modified entity from `void update(id)` tempts callers to treat it as a query. | Return `void` or a status code; let callers query the entity separately. |
| **Throwing on Pure Queries** | A query throwing business exceptions when data is missing instead of returning `Optional.empty()`. | Queries should return `Optional` or null objects for absent data. |
| **Mixing Mutation in Builders** | A `builder.build()` method that writes records to a database. | Builders should only assemble in-memory objects; persistence belongs in repositories. |

---

## 12 Testing and Design Verification

CQS makes testing predictable. You test commands by asserting that state changed; you test queries by asserting the returned value:

```java
class CqsQueueTest {
    void testCommandMutatesAndQueryReflects() {
        CqsQueue q = new CqsQueue();
        assert q.currentId() == 0; // Initial query

        q.increment(); // Execute command
        assert q.currentId() == 1; // Assert state change

        // Assert query idempotency
        assert q.currentId() == 1;
        assert q.currentId() == 1;
    }
}
```

---

## 13 Performance, Concurrency and Security Notes

- **Caching**: Because queries have zero side-effects, HTTP GET endpoints can be safely cached on edge CDNs (Cloudflare, Fastly) with `Cache-Control: max-age=3600`.
- **Security**: Attackers often exploit CQS violations via Cross-Site Request Forgery (CSRF). If a GET request (query) can trigger a balance transfer (command), an image tag `<img src="/transfer?to=hacker&amt=1000">` can drain accounts. Enforcing that only POST/PUT/DELETE can mutate state prevents this vulnerability.

---

## 14 Interview Questions

<details>
<summary><strong>1. What is Command-Query Separation in your own words?</strong></summary>

**Answer:**  
CQS is a principle stating that methods should clearly separate actions from questions. A method that mutates state (a Command) should not return business data, and a method that returns data (a Query) must have no observable side-effects on system state.
</details>

<details>
<summary><strong>2. Why is CQS violated by <code>java.util.Iterator.next()</code> and why is that acceptable?</strong></summary>

**Answer:**  
`Iterator.next()` returns the current element and advances the cursor to the next element simultaneously, violating CQS. This is a pragmatic API design tradeoff in collections: separating it into `get()` and `advance()` would double the number of calls in loops and create synchronization hazards in multi-threaded collections.
</details>

<details>
<summary><strong>3. How does CQS at the method level relate to CQRS at the architecture level?</strong></summary>

**Answer:**  
- **CQS** is applied at the class/object level within a single application process (methods are commands or queries).
- **CQRS** scales this idea across the entire system architecture, using separate data models, separate database tables (or even separate database engines, like SQL for writes and Elasticsearch for reads), and separate services for handling write commands vs read queries.
</details>

<details>
<summary><strong>4. What is the danger of executing database updates inside a getter?</strong></summary>

**Answer:**  
Getters are expected to be idempotent and safe to call repeatedly. If a getter executes a database write (e.g. lazy-updating an 'accessed_at' column), running a debugger, an IDE variable evaluator, a serialization library, or a logging statement will trigger unexpected database writes and lock contention.
</details>

<details>
<summary><strong>5. What should a Command return under strict CQS?</strong></summary>

**Answer:**  
Under strict CQS (as defined by Bertrand Meyer), a Command should return `void`. In modern pragmatic enterprise systems, commands often return a command execution result (success/failure, validation errors, or the generated ID of the new entity).
</details>

<details>
<summary><strong>6. How does CQS help HTTP REST API design?</strong></summary>

**Answer:**  
CQS maps directly to HTTP method semantics:
- HTTP `GET` requests are **Queries**: they must be safe, idempotent, and cacheable.
- HTTP `POST`, `PUT`, `DELETE`, and `PATCH` are **Commands**: they perform state mutations and are not safe to execute repeatedly without intent.
</details>

<details>
<summary><strong>7. How does CQS improve code readability and maintainability?</strong></summary>

**Answer:**  
When reading code, a developer knows immediately that invoking a query has zero risk of corrupting state. If something goes wrong with state corruption, developers only need to audit the commands, drastically narrowing the debugging search space.
</details>

<details>
<summary><strong>8. Why is <code>AtomicInteger.incrementAndGet()</code> a necessary violation of CQS?</strong></summary>

**Answer:**  
In concurrent programming, separating `increment()` and `get()` into two separate method calls would introduce a race condition: another thread could modify the value between the increment and the get. Combining them into an atomic hardware-level compare-and-swap (CAS) operation is essential for lock-free thread safety.
</details>

---

## 15 Practice Exercises

### Exercise 1: Spot the CQS Violation (Easy)
Identify the CQS violation in this authentication service:
```java
public class AuthService {
    public boolean checkPassword(String username, String password) {
        User u = db.find(username);
        if (u.passwordMatches(password)) {
            u.incrementLoginCount();
            db.save(u);
            return true;
        }
        return false;
    }
}
```
<details>
<summary><strong>Solution</strong></summary>

`checkPassword()` is named like a boolean query, but it silently executes a database write mutating `loginCount`. If called twice to verify a credential in a multi-step check, it records two logins.  
**Fix:** Split into a pure query `boolean verifyCredentials(...)` and a command `void recordSuccessfulLogin(...)`.
</details>

### Exercise 2: Refactor to CQS (Medium)
Refactor `ShoppingBasket` so that calculating totals does not mutate applied promotions:
```java
public class ShoppingBasket {
    public double getTotal() {
        applyDiscounts(); // Mutates internal items
        return calculateSum();
    }
}
```
<details>
<summary><strong>Solution</strong></summary>

```java
public class ShoppingBasket {
    // Command
    public void applyDiscounts() {
        // Mutates state explicitly
    }

    // Query: Pure, idempotent
    public double getTotal() {
        return calculateSum();
    }
}
```
</details>

### Exercise 3: CQS in Property Getters (Easy)
Why is the following TypeScript getter dangerous?
```typescript
class Session {
    private _lastAccess = Date.now();
    get token(): string {
        this._lastAccess = Date.now(); // Side-effect inside getter!
        return "TOKEN-XYZ";
    }
}
```
<details>
<summary><strong>Solution</strong></summary>

Property getters are expected to be pure reads. Touching `session.token` in a debug console or logging statement resets the user's idle session timer.  
**Fix:** Extract an explicit command method: `touch(): void`.
</details>

### Exercise 4: Atomic Concurrency Trade-Off (Hard)
Implement a thread-safe ticket dispenser in Java that acknowledges the necessary CQS exception using `AtomicInteger`.
<details>
<summary><strong>Solution</strong></summary>

```java
import java.util.concurrent.atomic.AtomicInteger;

public class TicketDispenser {
    private final AtomicInteger sequence = new AtomicInteger(1000);

    // Documented pragmatic exception: atomic command-and-query required for thread safety
    public int takeNextTicket() {
        return sequence.getAndIncrement();
    }

    // Pure query
    public int peekCurrentTicket() {
        return sequence.get();
    }
}
```
</details>

### Exercise 5: Refactor Pop from Stack (Medium)
Design a `Stack<T>` class that strictly adheres to CQS by providing separate methods for inspecting the top element and removing the top element.
<details>
<summary><strong>Solution</strong></summary>

```java
public class StrictCqsStack<T> {
    private final List<T> items = new ArrayList<>();

    // Command: mutates state, returns void
    public void pop() {
        if (items.isEmpty()) throw new IllegalStateException("Stack empty");
        items.remove(items.size() - 1);
    }

    // Query: pure, returns item without removal
    public T peek() {
        if (items.isEmpty()) throw new IllegalStateException("Stack empty");
        return items.get(items.size() - 1);
    }

    public void push(T item) { items.add(item); }
}
```
</details>

### Exercise 6: Python Query Decorator (Hard)
Write a Python decorator `@pure_query` that raises an exception if a method marked as a query attempts to modify any attributes on `self`.
<details>
<summary><strong>Solution</strong></summary>

```python
import functools

def pure_query(func):
    @functools.wraps(func)
    def wrapper(self, *args, **kwargs):
        before = dict(self.__dict__)
        result = func(self, *args, **kwargs)
        if self.__dict__ != before:
            raise RuntimeError(f"CQS Violation: Query method '{func.__name__}' mutated state!")
        return result
    return wrapper
```
</details>

---

## 16 Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
| COMMAND-QUERY SEPARATION (CQS) CHEAT SHEET                                              |
| Core Rule: "Asking a question should never change the answer."                          |
|                                                                                         |
| Commands:                                                                               |
| - Action-oriented (verbs: save, delete, increment, process).                           |
| - Mutates system state.                                                                 |
| - Returns void (or execution status).                                                   |
|                                                                                         |
| Queries:                                                                                |
| - Information-oriented (nouns/getters: balance, total, isValid).                        |
| - Returns domain data.                                                                  |
| - Strictly side-effect free, idempotent, safe to cache and repeat.                      |
+-----------------------------------------------------------------------------------------+
```

---

## 17 Further Reading

- Meyer, Bertrand (1988). *"Object-Oriented Software Construction"*, Prentice Hall (Chapter 11: Design by Contract and CQS).
- Fowler, Martin (2005). *"CommandQuerySeparation"*, martinfowler.com.
- Young, Greg (2010). *"CQRS Documents"*, InfoQ.

### In Your Language
| Language Track | Deep Dive Reference |
|---|---|
| Java | [Java Methods & Side-Effects](file:///e:/Learning/engineering-notes/01-languages/java/24-design-patterns/README.md) |
| TypeScript | [TypeScript Accessors & Getters](file:///e:/Learning/engineering-notes/01-languages/typescript/17-design-patterns/README.md) |
| Python | [Python Properties & Pure Functions](file:///e:/Learning/engineering-notes/01-languages/python/23-design-patterns/README.md) |

---
[← Previous: 07 - Immutability and Value Objects](07-immutability-and-value-objects.md) | [Section Index](README.md) | [Next: 09 - Design by Contract and Defensive Programming →](09-design-by-contract-and-defensive-programming.md)
