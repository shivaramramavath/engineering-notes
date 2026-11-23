# Module 28: Generics & Type Parameter Cheatsheet

> **Quick Reference**: Quick syntax guide for generic functions, interfaces, classes, type constraints (`extends`), default parameters, and the `infer` keyword.

---

## 1. Generic Functions & Arrow Syntax

```typescript
// Named Generic Function
function identity<T>(arg: T): T {
  return arg;
}

// Generic Arrow Function (JSX safe: comma prevents tag confusion in .tsx)
const identityArrow = <T,>(arg: T): T => arg;

// Multiple Generics
function pair<K, V>(key: K, value: V): [K, V] {
  return [key, value];
}
```

---

## 2. Generic Constraints (`extends`)

```typescript
// Constraint to objects with 'length' property
interface HasLength {
  length: number;
}
function logLength<T extends HasLength>(item: T): number {
  return item.length;
}

// Constrained by another type parameter (keyof)
function getProperty<T, K extends keyof T>(obj: T, key: K): T[K] {
  return obj[key];
}
```

---

## 3. Generic Classes & Interfaces

```typescript
interface Repository<T, Id = string> {
  findById(id: Id): Promise<T | null>;
  save(entity: T): Promise<void>;
}

class InMemoryStore<T, Id extends string | number> implements Repository<T, Id> {
  private map = new Map<Id, T>();

  async findById(id: Id): Promise<T | null> {
    return this.map.get(id) ?? null;
  }

  async save(entity: T): Promise<void> {
    // Save logic
  }
}
```

---

## 4. The `infer` Keyword in Conditional Types

```typescript
// Unpack Array Element
type ElementOf<T> = T extends (infer E)[] ? E : T;
type Num = ElementOf<number[]>; // number

// Unpack Promise Return
type UnpackPromise<T> = T extends Promise<infer U> ? U : T;
type Str = UnpackPromise<Promise<string>>; // string

// Extract First Argument of Function
type FirstArg<T> = T extends (first: infer A, ...rest: any[]) => any ? A : never;
type FArg = FirstArg<(x: boolean, y: number) => void>; // boolean
```\n