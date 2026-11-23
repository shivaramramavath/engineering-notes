# Module 28: Testing & Debugging TypeScript Cheatsheet

> **Quick Reference**: Reference guide for Vitest, Jest type assertions, `@ts-expect-error`, and debugging compiler performance.

---

## 1. Type Testing in Vitest (`expectTypeOf` / `assertType`)

```typescript
import { test, expectTypeOf, assertType } from "vitest";

test("type-level testing", () => {
  type User = { id: string; name: string };

  // Assert type shape
  expectTypeOf<User>().toHaveProperty("id");
  expectTypeOf<User>().not.toBeAny();

  // Test assignability
  expectTypeOf({ id: "1", name: "Ada" }).toMatchTypeOf<User>();

  // Function return type checks
  const getNum = () => 42;
  expectTypeOf(getNum).returns.toBeNumber();
});
```

---

## 2. `@ts-expect-error` vs `@ts-ignore`

| Directive | Behavior | When to Use |
|---|---|---|
| `// @ts-expect-error` | Suppresses next line error; **Fails if NO error occurs** | Unit tests for invalid types, negative test cases |
| `// @ts-ignore` | Suppresses next line error unconditionally | Avoid in modern code; masks accidental fixes |

```typescript
// Correct usage in tests:
// @ts-expect-error - number is not assignable to string
const s: string = 123;
```

---

## 3. Diagnosing Slow Compilation Performance

```bash
# 1. Generate compiler trace output
tsc --extendedDiagnostics

# 2. Detailed performance trace (Chrome DevTools trace viewer)
tsc --generateTrace trace-dir

# 3. Analyze trace via @typescript/analyze-trace
npx @typescript/analyze-trace trace-dir
```\n