# Interface Segregation Principle (ISP)

> Clients should not be forced to depend on methods they do not use.

Prefer several small, focused interfaces over one large, general-purpose one. Each client should see only the methods it needs.

## Why it matters

A "fat" interface couples every implementer and every client to things they don't care about:

- Implementers must write stubs for irrelevant methods (often throwing exceptions, which breaks [LSP](./03-liskov-substitution.md)).
- Clients are recompiled or affected when unrelated methods change.
- Fakes and mocks in tests become large.

## Example

```java
public interface MultiFunctionDevice {
    void print(Document d);
    void scan(Document d);
    void fax(Document d);
}

public class BasicPrinter implements MultiFunctionDevice {
    public void print(Document d) { /* ok */ }
    public void scan(Document d)  { throw new UnsupportedOperationException(); }
    public void fax(Document d)   { throw new UnsupportedOperationException(); }
}
```

`BasicPrinter` is forced to implement features it doesn't have. Any caller that holds a `MultiFunctionDevice` can crash.

### Segregated

```java
public interface Printer { void print(Document d); }
public interface Scanner { void scan(Document d); }
public interface Fax     { void fax(Document d); }

public class BasicPrinter implements Printer {
    public void print(Document d) { /* ... */ }
}

public class OfficeMachine implements Printer, Scanner, Fax {
    public void print(Document d) { /* ... */ }
    public void scan(Document d)  { /* ... */ }
    public void fax(Document d)   { /* ... */ }
}

// A client states exactly what it needs
class ReportJob {
    void run(Printer printer, Document d) { printer.print(d); }
}
```

```text
Before:  Client ──► MultiFunctionDevice (print, scan, fax) ◄── BasicPrinter (stubs)

After:   ReportJob ──► Printer ◄── BasicPrinter
                          ▲
                          └─────── OfficeMachine ──► Scanner, Fax
```

## Role interfaces

A good guide: shape each interface around **a client's role**, not around the implementing class.

```java
// Shaped by the class
interface UserService { create(); update(); delete(); find(); exportCsv(); resetPassword(); }

// Shaped by the clients
interface UserReader   { User find(String id); }
interface UserWriter   { void save(User u); }
interface PasswordReset { void reset(String userId); }
```

The reporting module depends on `UserReader` only. It can't accidentally call `save`, and any change to password reset doesn't affect it.

## Spotting violations

- Implementations with empty methods or `UnsupportedOperationException`.
- An interface whose methods are used by completely different clients.
- Mocks that need many unrelated stubs for a simple test.
- Interface names like `IManager` or `Service` that cover many unrelated actions.
- A change to one method forces edits in classes that never call it.

## Notes for Java

- **Default methods** don't fix a fat interface. They hide the problem by providing implementations nobody asked for.
- A class can implement many small interfaces, so splitting costs implementers little.
- Functional interfaces (`Supplier`, `Predicate`, a custom single-method interface) are the smallest form of ISP.

## Common mistakes

- **Over-splitting** into one-method interfaces everywhere, so the codebase is full of types that are never used separately. Split along *client needs*, not mechanically.
- **Splitting by implementation** (`ISmtpEmailSender`) and not by role.
- **Ignoring ISP in external or public APIs**, where a large interface is expensive to change later.
- **Confusing ISP with SRP.** SRP is about a class's reasons to change; ISP is about what each client is *forced to depend on*.

## Quick Summary

- Small, role-based interfaces; clients depend only on what they use.
- Forced stub methods are the strongest warning sign.
- Splitting interfaces also helps testing, because fakes stay small.
- Split along real client differences, not for the sake of splitting.

## Next

[05-dependency-inversion.md](./05-dependency-inversion.md)
