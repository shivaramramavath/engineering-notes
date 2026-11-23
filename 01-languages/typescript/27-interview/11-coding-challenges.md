# Module 27: TypeScript Live Coding & Type System Challenges

> **Topic Overview**: Elite hands-on coding challenges frequently tested in Staff/Principal TypeScript technical rounds: DeepReadonly, Curry, Permutations, TupleToUnion, and QueryString Parser.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Interview & Preparation |
| **Target Level** | Advanced / Staff Level |
| **TypeScript Baseline** | TypeScript 5.0 - 5.8+ |
| **Focus Areas** | Live Coding, Type-Level Computation, Recursive Types |

---

## Live Coding Challenges

### Challenge 1: Implement `DeepReadonly<T>`
Implement a generic type that recursively converts all properties of an object or nested array into `readonly`.

```typescript
type DeepReadonly<T> = T extends Function | boolean | number | string | symbol | bigint | null | undefined
  ? T
  : T extends Array<infer U>
  ? ReadonlyArray<DeepReadonly<U>>
  : T extends Map<infer K, infer V>
  ? ReadonlyMap<DeepReadonly<K>, DeepReadonly<V>>
  : T extends Set<infer M>
  ? ReadonlySet<DeepReadonly<M>>
  : { readonly [P in keyof T]: DeepReadonly<T[P]> };

// Test Verification:
type Complex = {
  a: number;
  b: { c: string; d: number[] };
};
type ReadonlyComplex = DeepReadonly<Complex>;
// All nested levels are now immutable!
```

---

### Challenge 2: Implement `ParseQueryString<Str>` in Type-Space
Parse a query string like `"foo=1&bar=hello&baz=true"` into an object type `{ foo: "1"; bar: "hello"; baz: "true" }`.

```typescript
type ParseParam<Param extends string> = Param extends `${infer K}=${infer V}`
  ? { [P in K]: V }
  : {};

type MergeParams<A, B> = {
  [K in keyof A | keyof B]: K extends keyof A
    ? A[K]
    : K extends keyof B
    ? B[K]
    : never;
};

type ParseQueryString<S extends string> = S extends `${infer First}&${infer Rest}`
  ? MergeParams<ParseParam<First>, ParseQueryString<Rest>>
  : ParseParam<S>;

// Verification:
type Result = ParseQueryString<"id=42&action=view&sort=desc">;
// Evaluates to: { id: "42"; action: "view"; sort: "desc" }
```

---

### Challenge 3: Type-Safe Function Currying (`Curry<F>`)
```typescript
type Curry<F> = F extends (...args: infer Args) => infer Return
  ? Args extends [infer Head, ...infer Tail]
    ? Tail["length"] extends 0
      ? (arg: Head) => Return
      : (arg: Head) => Curry<(...args: Tail) => Return>
    : () => Return
  : never;

// Verification:
declare function curry<F>(fn: F): Curry<F>;
const add3 = (a: number, b: string, c: boolean): number => 123;
const curried = curry(add3);
const step1 = curried(10);        // (arg: string) => (arg: boolean) => number
const step2 = step1("hello");     // (arg: boolean) => number
const finalVal = step2(true);     // number
```\n