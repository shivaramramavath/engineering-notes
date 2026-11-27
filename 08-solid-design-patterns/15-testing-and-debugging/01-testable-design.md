# Testable Design

| Field | Value |
| :--- | :--- |
| **Category** | Testing & Design |
| **Origin** | Misko Hevery (*Guide to Testability*) / Gary Bernhardt (*Functional Core, Imperative Shell*) |

> **TL;DR**: Code becomes effortlessly testable when you **inject collaborators** (no `new DbClient()` inside constructors), **inject non-determinism** (`clock`, `random`, `uuid`), and keep business rules in a **Functional Core** separate from I/O.

## Four Rules of Testable Design
1. **No Hidden Work in Constructors**: Constructors only assign fields—never open DB connections or read env files.
2. **Inject Clock & Randomness**: Pass `Supplier<Instant>` / `() => number` so expiration tests never use `Thread.sleep()`.
3. **Separate Pure Logic from I/O**: Test pricing/tax/validation with 0 mocks.
4. **Law of Demeter**: Ask for only the collaborators you directly use, not `app.getConfig().getDb().getPool()`.
