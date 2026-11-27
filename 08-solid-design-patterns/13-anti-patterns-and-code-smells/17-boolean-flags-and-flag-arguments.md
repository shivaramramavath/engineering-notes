# Boolean Flags & Flag Arguments

| Field | Value |
| :--- | :--- |
| **Category** | Code Smells (API Design) |
| **Fix** | Separate Intention-Revealing Methods or Typed Options / Enums |

> **TL;DR**: `createUser(email, true, false, true)` is unreadable at every call site and forces the function to branch in two unrelated directions. Split into `createAdminUser(email)` and `createStandardUser(email)`, or pass a named options/enum parameter.

```ts
// ❌ Unreadable boolean flags
renderReport(data, true, false);
// ✅ Explicit methods or named options
renderPdfReport(data, { includeHeader: true });
```
