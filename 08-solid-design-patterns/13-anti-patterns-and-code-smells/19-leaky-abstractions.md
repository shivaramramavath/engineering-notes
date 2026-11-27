# Leaky Abstractions

| Field | Value |
| :--- | :--- |
| **Category** | Anti-Pattern (Abstraction Design) |
| **Origin** | Joel Spolsky (*The Law of Leaky Abstractions*, 2002) |
| **Fix** | Honest Interfaces, Translate Errors at Boundaries (ACL/Gateway) |

> **TL;DR**: *"All non-trivial abstractions, to some degree, are leaky."* An abstraction leaks when callers must understand its hidden internals (e.g., an ORM leaking N+1 queries and `LazyInitializationException`, or a `UserRepo` throwing `PSQLException`).

## Fix
1. Don't hide network/DB reality behind fake local-memory illusions (e.g., RPC pretending to be a fast local call).
2. Translate vendor/SQL errors at the boundary (`DuplicateEmailException` instead of `SQLIntegrityConstraintViolationException`).
