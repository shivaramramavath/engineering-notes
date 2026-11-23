# Module 28: Type Narrowing & Control Flow Analysis Cheatsheet

> **Quick Reference**: Complete reference for TypeScript Type Guards, CFA (Control Flow Analysis), Discriminated Unions, and Assertion Functions.

---

## 1. Standard Built-in Type Guards

| Guard | Runtime Check | Narrows |
|---|---|---|
| `typeof x === "string"` | Primitive inspection | `string`, `number`, `bigint`, `boolean`, `symbol`, `undefined`, `function` |
| `x instanceof Class` | Prototype chain check | Instances of classes (e.g. `Date`, `Error`, `RegExp`) |
| `"prop" in obj` | Property existence check | Narrows unions where property exists on a branch |
| `Array.isArray(x)` | Array check | Narrows `unknown` to `any[]` / `unknown[]` |
| `x === "literal"` | Equality check | Literal narrowing against string/number unions |
| `x != null` | Loose null check | Narrows away both `null` and `undefined` |

---

## 2. Custom User-Defined Type Guards (`is`)

```typescript
interface Cat {
  name: string;
  meow(): void;
}
interface Dog {
  name: string;
  bark(): void;
}

// User-Defined Type Guard
export function isCat(pet: Cat | Dog): pet is Cat {
  return (pet as Cat).meow !== undefined;
}

function speak(pet: Cat | Dog) {
  if (isCat(pet)) {
    pet.meow(); // Narrows pet to Cat
  } else {
    pet.bark(); // Narrows pet to Dog
  }
}
```

---

## 3. Assertion Functions (`asserts condition` / `asserts val is T`)

```typescript
export function assertNonNull<T>(val: T, msg?: string): asserts val is NonNullable<T> {
  if (val === null || val === undefined) {
    throw new Error(msg || "Value must not be null or undefined");
  }
}

function processValue(val: string | null) {
  assertNonNull(val);
  console.log(val.toUpperCase()); // CFA knows val is string!
}
```

---

## 4. Discriminated Union & Exhaustiveness Checking

```typescript
type Shape =
  | { kind: "circle"; radius: number }
  | { kind: "square"; side: number }
  | { kind: "rectangle"; width: number; height: number };

function getArea(shape: Shape): number {
  switch (shape.kind) {
    case "circle":
      return Math.PI * shape.radius ** 2;
    case "square":
      return shape.side ** 2;
    case "rectangle":
      return shape.width * shape.height;
    default: {
      // Exhaustiveness guarantee:
      const _exhaustiveCheck: never = shape;
      throw new Error(`Unhandled shape: ${_exhaustiveCheck}`);
    }
  }
}
```\n