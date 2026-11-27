# Spaghetti Code

| Field | Value |
| :--- | :--- |
| **Category** | Anti-Pattern (Control Flow) |
| **Violates** | [KISS](file:///E:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/02-kiss-and-yagni.md), [Clean Functions](file:///E:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/01-dry-and-rule-of-three.md) |
| **Fix** | Guard Clauses, Extract Method, State Machine / ADT |

> **TL;DR**: **Spaghetti Code** has deeply nested `if/else` arrows, long procedural functions, and hidden mutations of shared variables—making it impossible to trace how execution reaches a given line.

## Symptoms & Fix
- 5+ levels of nested `if/for` blocks ("Arrow Anti-Pattern").
- **Fix**: Invert conditions into **Guard Clauses** (early returns), extract cohesive named functions, and replace boolean state flags with a `State` machine or `sealed` ADT.

```ts
// ❌ Arrow Anti-Pattern
function process(u: any) {
  if (u) { if (u.active) { if (u.paid) { return 'ok'; } } }
  return 'fail';
}
// ✅ Guard Clauses
function processClean(u: any) {
  if (!u || !u.active || !u.paid) return 'fail';
  return 'ok';
}
```
