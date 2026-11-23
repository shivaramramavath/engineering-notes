# Comprehensive TypeScript Type Challenges

> An exhaustive, production-grade challenge bank for mastering the TypeScript type system: from warm-up basics to extreme type-level metaprogramming.

---

## Table of Contents
1. [Warm-up & Fundamentals](#1-warm-up--fundamentals)
2. [Easy Challenges](#2-easy-challenges)
3. [Medium Challenges](#3-medium-challenges)
4. [Hard Challenges](#4-hard-challenges)
5. [Extreme Challenges](#5-extreme-challenges)

---

## 1. Warm-up & Fundamentals

### Challenge 1.1: Hello World in Type Space
```typescript
// Task: Define a type HelloWorld that resolves to string
export type HelloWorld = string;
```

### Challenge 1.2: Awaited (Simple)
```typescript
// Task: Unwrap Promise<T>
export type SimpleAwaited<T> = T extends Promise<infer U> ? U : T;
```

---

## 2. Easy Challenges

### Challenge 2.1: Custom Pick
```typescript
// Implement MyPick<T, K> without using the built-in Pick<T, K>
export type MyPick<T, K extends keyof T> = {
  [P in K]: T[P];
};
```

### Challenge 2.2: Custom Readonly
```typescript
// Implement MyReadonly<T>
export type MyReadonly<T> = {
  readonly [P in keyof T]: T[P];
};
```

### Challenge 2.3: Tuple to Object
```typescript
// Given a readonly tuple of strings/numbers, transform it into an object where key === value
export type TupleToObject<T extends readonly (string | number | symbol)[]> = {
  [P in T[number]]: P;
};
```

### Challenge 2.4: First of Array
```typescript
// Extract first element of tuple
export type First<T extends unknown[]> = T extends [infer Head, ...unknown[]] ? Head : never;
```

---

## 3. Medium Challenges

### Challenge 3.1: Custom ReturnType
```typescript
// Implement MyReturnType<T>
export type MyReturnType<T extends (...args: any[]) => any> =
  T extends (...args: any[]) => infer R ? R : never;
```

### Challenge 3.2: Custom Omit
```typescript
// Implement MyOmit<T, K>
export type MyOmit<T, K extends keyof any> = {
  [P in keyof T as P extends K ? never : P]: T[P];
};
```

### Challenge 3.3: Deep Readonly
```typescript
// Implement DeepReadonly<T>
export type DeepReadonly<T> = {
  readonly [K in keyof T]: T[K] extends Function
    ? T[K]
    : T[K] extends object
      ? DeepReadonly<T[K]>
      : T[K];
};
```

### Challenge 3.4: Tuple to Union
```typescript
// Transform tuple [1, 2, 3] to 1 | 2 | 3
export type TupleToUnion<T extends unknown[]> = T[number];
```

### Challenge 3.5: Chainable Options
```typescript
export type Chainable<T = {}> = {
  option<K extends string, V>(
    key: K extends keyof T ? never : K,
    value: V
  ): Chainable<Omit<T, K> & Record<K, V>>;
  get(): T;
};
```

---

## 4. Hard Challenges

### Challenge 4.1: Simple Vue / Pinia Computed Type
```typescript
export declare function SimpleVue<D, C, M>(options: {
  data: (this: {}) => D;
  computed: C & ThisType<D & { [K in keyof C]: C[K] extends () => infer R ? R : never }>;
  methods: M & ThisType<D & { [K in keyof C]: C[K] extends () => infer R ? R : never } & M>;
}): any;
```

### Challenge 4.2: Currying
```typescript
export type Curried<Args, Return> = Args extends [infer Head, ...infer Tail]
  ? (arg: Head) => Curried<Tail, Return>
  : Return;

export declare function Currying<F>(fn: F): F extends (...args: infer Args) => infer Return
  ? Args["length"] extends 0
    ? () => Return
    : Curried<Args, Return>
  : never;
```

### Challenge 4.3: CamelCase String
```typescript
export type CamelCase<S extends string> = S extends `${infer Head}_${infer Tail}`
  ? `${Lowercase<Head>}${Capitalize<CamelCase<Tail>>}`
  : Lowercase<S>;
```

---

## 5. Extreme Challenges

### Challenge 5.1: Query String Parser
```typescript
type MergeParams<T, K extends string, V> = K extends keyof T
  ? { [P in keyof T]: P extends K ? (T[P] extends unknown[] ? [...T[P], V] : [T[P], V]) : T[P] }
  : T & Record<K, V>;

export type ParseQueryString<S extends string, Acc = {}> =
  S extends `${infer Param}&${infer Rest}`
    ? ParseQueryString<Rest, ParseSingleParam<Param, Acc>>
    : S extends ""
      ? Acc
      : ParseSingleParam<S, Acc>;

type ParseSingleParam<S extends string, Acc> =
  S extends `${infer Key}=${infer Val}`
    ? MergeParams<Acc, Key, Val>
    : S extends `${infer Key}`
      ? MergeParams<Acc, Key, true>
      : Acc;
```

### Challenge 5.2: Type-Level Integer Addition
```typescript
type BuildTuple<N extends number, T extends unknown[] = []> =
  T["length"] extends N ? T : BuildTuple<N, [...T, unknown]>;

export type TypeAdd<A extends number, B extends number> =
  [...BuildTuple<A>, ...BuildTuple<B>]["length"] & number;
```
---

## 6. Enterprise-Grade Type Patterns

### Challenge 6.1: Discriminated Union Event Emitter Map
```typescript
export type EventMap = {
  login: { userId: string; timestamp: number };
  logout: { userId: string };
  error: { code: number; message: string };
};

export class StrictlyTypedEmitter<TEvents extends Record<string, any>> {
  private listeners: { [K in keyof TEvents]?: Set<(payload: TEvents[K]) => void> } = {};

  public on<K extends keyof TEvents>(event: K, handler: (payload: TEvents[K]) => void): void {
    if (!this.listeners[event]) this.listeners[event] = new Set();
    this.listeners[event]!.add(handler);
  }

  public emit<K extends keyof TEvents>(event: K, payload: TEvents[K]): void {
    this.listeners[event]?.forEach((fn) => fn(payload));
  }
}
```

### Challenge 6.2: Deep Mutable (Inverse of DeepReadonly)
```typescript
export type DeepMutable<T> = {
  -readonly [K in keyof T]: T[K] extends Function
    ? T[K]
    : T[K] extends object
      ? DeepMutable<T[K]>
      : T[K];
};
```

### Challenge 6.3: RequiredByKeys (Selective Partial to Required)
```typescript
export type RequiredByKeys<T, K extends keyof any = keyof T> = Omit<T, K> & {
  [P in keyof T as P extends K ? P : never]-?: T[P];
};
```

### Challenge 6.4: OptionalByKeys (Selective Required to Partial)
```typescript
export type OptionalByKeys<T, K extends keyof any = keyof T> = Omit<T, K> & {
  [P in keyof T as P extends K ? P : never]?: T[P];
};
```

### Challenge 6.5: IsAny & IsNever Detection
```typescript
// IsAny checks 0 extends (1 & T)
export type IsAny<T> = 0 extends 1 & T ? true : false;

// IsNever checks [T] extends [never]
export type IsNever<T> = [T] extends [never] ? true : false;
```
