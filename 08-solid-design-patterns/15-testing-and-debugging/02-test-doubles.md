# Test Doubles (Dummy, Stub, Spy, Mock, Fake)

| Field | Value |
| :--- | :--- |
| **Category** | Testing Patterns |
| **Origin** | Gerard Meszaros (*xUnit Test Patterns*, 2007) |

> **TL;DR**: Not every test double is a "Mock". Prefer **Fakes** (`InMemoryOrderRepo`) and **Stubs** (canned return values) over **Mocks** (`verify(repo).save(...)`), because mocks couple tests to internal implementation details.

| Double | Purpose | Example |
| :--- | :--- | :--- |
| **Dummy** | Fills required parameter, never called | `new DummyLogger()` |
| **Stub** | Returns canned answers to calls | `clock = () => 1000` |
| **Spy** | Records calls made to it for later assertion | `sentEmails.push(msg)` |
| **Mock** | Pre-programmed with strict call-order/count expectations | `expect(mock.save).toHaveBeenCalledTimes(1)` |
| **Fake** | Working lightweight implementation (not for prod) | `InMemoryUserRepository`, SQLite `:memory:` |

## Over-Mocking Trap
Mocking your own internal helper classes breaks tests on every harmless refactoring. Only double **external/slow boundaries** (HTTP, DB, Clock, Email).
