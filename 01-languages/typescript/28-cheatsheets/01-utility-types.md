# Module 28: Utility Types Quick Reference Cheatsheet

> **Quick Reference**: Complete cheat sheet for TypeScript built-in utility types, mapped types, and standard library helpers.

---

## 1. Built-in Object Transformation Utilities

| Utility | Description | Source Definition |
|---|---|---|
| `Partial<T>` | Makes all properties in `T` optional | `type Partial<T> = { [P in keyof T]?: T[P]; }` |
| `Required<T>` | Makes all properties in `T` required | `type Required<T> = { [P in keyof T]-?: T[P]; }` |
| `Readonly<T>` | Makes all properties in `T` immutable | `type Readonly<T> = { readonly [P in keyof T]: T[P]; }` |
| `Record<K, T>` | Constructs an object type with keys `K` of type `T` | `type Record<K extends keyof any, T> = { [P in K]: T; }` |
| `Pick<T, K>` | Selects a subset of keys `K` from `T` | `type Pick<T, K extends keyof T> = { [P in K]: T[P]; }` |
| `Omit<T, K>` | Removes a subset of keys `K` from `T` | `type Omit<T, K extends keyof any> = Pick<T, Exclude<keyof T, K>>` |

---

## 2. Union & Conditional Utilities

| Utility | Description | Source Definition |
|---|---|---|
| `Exclude<T, U>` | Excludes from `T` those types assignable to `U` | `type Exclude<T, U> = T extends U ? never : T` |
| `Extract<T, U>` | Extracts from `T` those types assignable to `U` | `type Extract<T, U> = T extends U ? T : never` |
| `NonNullable<T>` | Strips `null` and `undefined` from `T` | `type NonNullable<T> = T & {}` *(TS 4.8+)* |

---

## 3. Function & Class Inspection Utilities

| Utility | Description | Example Usage |
|---|---|---|
| `Parameters<T>` | Obtains parameters of a function as a tuple | `type P = Parameters<(a: string, b: number) => void>; // [string, number]` |
| `ReturnType<T>` | Obtains the return type of a function | `type R = ReturnType<() => Promise<boolean>>; // Promise<boolean>` |
| `ConstructorParameters<T>` | Obtains constructor arguments of class `T` | `type CP = ConstructorParameters<typeof RegExp>; // [string, string?]` |
| `InstanceType<T>` | Obtains instance type of a constructor function | `type Inst = InstanceType<typeof Date>; // Date` |
| `Awaited<T>` | Recursively unrolls promises (`Promise<T>`) | `type Val = Awaited<Promise<Promise<number>>>; // number` |

---

## 4. String Manipulation Utilities

| Utility | Input Example | Output |
|---|---|---|
| `Uppercase<S>` | `"hello world"` | `"HELLO WORLD"` |
| `Lowercase<S>` | `"HELLO WORLD"` | `"hello world"` |
| `Capitalize<S>` | `"user"` | `"User"` |
| `Uncapitalize<S>` | `"User"` | `"user"` |

---

## 5. Key Homomorphic Rule
Homomorphic mapped types (`[P in keyof T]`) preserve existing property modifiers (`readonly` and `?`) from `T`.
```typescript
interface Sample {
  readonly id: string;
  count?: number;
}
type Homomorphic<T> = { [K in keyof T]: T[K] }; // Preserves readonly & optional!
type NonHomomorphic<T> = Record<keyof T, boolean>; // Strips modifiers!
```\n