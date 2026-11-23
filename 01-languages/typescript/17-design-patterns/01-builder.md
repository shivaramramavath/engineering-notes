# Builder Pattern and Type-Safe Fluent APIs in TypeScript

> Implementing the Builder pattern in TypeScript: fluent chaining, compile-time mandatory property tracking using phantom type states, immutability, and SQL query builders.

| Metadata | Details |
| :--- | :--- |
| **Category** | Design Patterns |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [05-classes/00-class-basics.md](../05-classes/00-class-basics.md), [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md), [10-advanced-types/00-conditional-types.md](../10-advanced-types/00-conditional-types.md) |
| **TypeScript Baseline** | TS 4.5+, TS 5.0+ |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

The **Builder Pattern** is a creational design pattern that lets you construct complex objects step by step. When constructing an object with dozens of optional configuration options, multiple validation invariants, and mandatory core properties, relying on constructor parameters results in the "Telescoping Constructor" anti-pattern:
```typescript
new ServerConfig('localhost', 8080, null, true, 30, false, null, 'debug'); // Unreadable!
```

While passing a single configuration object literal (`new ServerConfig({ host: 'localhost', port: 8080 })`) solves visual clarity, it does NOT solve:
1. **Dynamic Step-by-Step Construction**: Incrementally configuring parts across distinct functions or middleware.
2. **Compile-Time Ordering & Completeness**: Ensuring that method `.build()` CANNOT be called until all mandatory fields are provided.

In TypeScript, we elevate the Builder pattern using **Type-State Builders** (phantom types), where each fluent method call returns an updated generic builder type that tracks which properties have been satisfied at compile time.

```
Initial State:          After .setHost():         After .setPort():
Builder<{}>   ------->  Builder<{ host }>  -----> Builder<{ host, port }>
   |                           |                         |
 .build() fails             .build() fails            .build() ALLOWED!
```

---

## 2. Core Concepts

### 1. Traditional Fluent Builder
Returns `this` from every setter, allowing chained invocations:
```typescript
export class HttpRequestBuilder {
  private url: string = '';
  private method: string = 'GET';

  public setUrl(url: string): this {
    this.url = url;
    return this;
  }

  public setMethod(method: string): this {
    this.method = method;
    return this;
  }
}
```

### 2. Type-Safe Phantom State Builder
Uses generic type parameters to accumulate populated fields:
```typescript
export class SafeBuilder<TTarget, TSupplied = {}> {
  private state: Record<string, unknown> = {};

  public set<K extends keyof TTarget>(
    key: K,
    value: TTarget[K]
  ): SafeBuilder<TTarget, TSupplied & Record<K, true>> {
    this.state[key as string] = value;
    return this as any;
  }

  // Only callable when TSupplied has satisfied all keys of TTarget!
  public build(this: SafeBuilder<TTarget, Record<keyof TTarget, true>>): TTarget {
    return this.state as TTarget;
  }
}
```

---

## 3. How It Works Internally

TypeScript's `this` parameter typing allows methods to specify constraints on the instance calling them.
When a method defines `public build(this: SafeBuilder<RequiredConfig>)`, the compiler checks whether the current type of `this` is assignable to that constrained target type:

```
  const b0 = new SafeBuilder<Config>();       // TSupplied = {}
  const b1 = b0.set('host', 'localhost');    // TSupplied = { host: true }
  const b2 = b1.set('port', 8080);           // TSupplied = { host: true } & { port: true }

  b2.build() -> Assignable to Record<'host' | 'port', true>?
  YES -> Compiles successfully.
  b1.build() -> Assignable?
  NO  -> Compiler error: The 'this' context of type ... is not assignable!
```

---

## 4. Syntax & API Reference

### Immutable Builder Returning New Instances
Preventing shared mutation bugs by cloning state on each setter:
```typescript
export class ImmutableBuilder<T> {
  constructor(private readonly data: Readonly<Partial<T>> = {}) {}

  public with<K extends keyof T>(key: K, value: T[K]): ImmutableBuilder<T> {
    return new ImmutableBuilder<T>({
      ...this.data,
      [key]: value
    });
  }

  public get(): Readonly<Partial<T>> {
    return Object.freeze({ ...this.data });
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic Fluent SQL Query Builder
```typescript
export class SimpleQueryBuilder {
  private table: string = '';
  private selectedColumns: string[] = ['*'];
  private whereClauses: string[] = [];

  public from(table: string): this {
    this.table = table;
    return this;
  }

  public select(...columns: string[]): this {
    this.selectedColumns = columns;
    return this;
  }

  public where(clause: string): this {
    this.whereClauses.push(clause);
    return this;
  }

  public toSql(): string {
    if (!this.table) throw new Error('Table must be specified');
    let sql = `SELECT ${this.selectedColumns.join(', ')} FROM ${this.table}`;
    if (this.whereClauses.length > 0) {
      sql += ` WHERE ${this.whereClauses.join(' AND ')}`;
    }
    return sql;
  }
}
```

### Example 2: Real-World Type-State Configuration Builder
```typescript
interface DatabaseConfig {
  host: string;
  port: number;
  database: string;
  ssl?: boolean;
}

type MissingKeys<TTarget, TCurrent> = Exclude<keyof TTarget, keyof TCurrent>;

export class TypeSafeConfigBuilder<TTarget extends object, TCurrent extends object = {}> {
  private values: Record<string, unknown> = {};

  public set<K extends keyof TTarget>(
    key: K,
    val: TTarget[K]
  ): TypeSafeConfigBuilder<TTarget, TCurrent & Record<K, TTarget[K]>> {
    this.values[key as string] = val;
    return this as any;
  }

  public build(
    this: TypeSafeConfigBuilder<TTarget, TTarget>
  ): TTarget {
    return Object.freeze({ ...this.values }) as TTarget;
  }
}
```

### Example 3: Edge Case - Step-Wise Wizard Builder (Method Transitions)
```typescript
interface StepOne {
  setEmail(email: string): StepTwo;
}

interface StepTwo {
  setPassword(password: string): StepThree;
}

interface StepThree {
  setRole(role: 'admin' | 'user'): FinalStep;
}

interface FinalStep {
  submit(): { email: string; role: string };
}

export function startUserRegistration(): StepOne {
  let email = '';
  let password = '';
  let role: 'admin' | 'user' = 'user';

  return {
    setEmail: (e) => {
      email = e;
      return {
        setPassword: (p) => {
          password = p;
          return {
            setRole: (r) => {
              role = r;
              return {
                submit: () => ({ email, role })
              };
            }
          };
        }
      };
    }
  };
}
```

### Example 4: Failure Case - Calling `.build()` Prematurely
```typescript
interface MandatoryProfile {
  username: string;
  apiKey: string;
}

const builder = new TypeSafeConfigBuilder<MandatoryProfile>()
  .set('username', 'alice');

// @ts-expect-error TS2684: The 'this' context of type 'TypeSafeConfigBuilder<MandatoryProfile, Record<"username", string>>' is not assignable to method's 'this' of type 'TypeSafeConfigBuilder<MandatoryProfile, MandatoryProfile>'. Property 'apiKey' is missing.
builder.build();
```

### Example 5: Idiomatic Builder with Default Overrides
```typescript
interface RequestOptions {
  method: 'GET' | 'POST' | 'PUT' | 'DELETE';
  headers: Record<string, string>;
  timeoutMs: number;
  retries: number;
}

export class RequestBuilder {
  private options: RequestOptions = {
    method: 'GET',
    headers: { 'Accept': 'application/json' },
    timeoutMs: 5000,
    retries: 3
  };

  public post(): this {
    this.options.method = 'POST';
    return this;
  }

  public header(key: string, value: string): this {
    this.options.headers[key] = value;
    return this;
  }

  public timeout(ms: number): this {
    this.options.timeoutMs = ms;
    return this;
  }

  public build(): Readonly<RequestOptions> {
    return Object.freeze({
      ...this.options,
      headers: { ...this.options.headers }
    });
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Shared State in Mutable Builders**: Reusing a mutable builder across requests leaks configuration between callers. If `builder.header('Auth', token)` is called, subsequent calls without re-initializing will leak the token.
2. **Subclassing Fluent Builders**: When extending a base builder class, methods returning `this` must declare return type `this` (polymorphic this-typing), not the base class name, or subclass methods will not be chainable.
3. **Deep Cloning Incomplete Objects**: If the builder holds arrays or nested objects, mutating them directly inside builder methods can cause unexpected mutations.
4. **Memory Allocation**: Instantiating a new builder object on every method call (`ImmutableBuilder`) in tight loops creates thousands of garbage-collected objects.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Returning Void from Setters** | `public setX(val) { this.x = val; }` | `return this;` | Breaks fluent chaining (`builder.setA().setB()` fails) |
| **Premature Object Cast** | `return this.data as CompleteConfig` | Type-state phantom check or runtime validation | Returns incomplete objects missing mandatory fields |
| **Polymorphic Breakage** | `public setA(): BaseBuilder` | `public setA(): this` | In derived classes, chain reverts to base type |
| **Mutable Shared Builder** | Storing builder in global module scope | Fresh builder instance per request | Cross-request data contamination / security leaks |
| **Re-inventing Object Literals** | 20-line builder for a simple 3-field object | `{ name, age, email }` object literal | Unnecessary abstraction and maintenance overhead |
| **Ignoring Invariants in `.build()`** | Missing final consistency checks | Assert interdependent rules in `.build()` | Invalid combinations pass silently |

---

## 8. Performance & Type-Checker Complexity

- **Phantom Type Intersection**: Chaining 20+ `.set()` methods on a type-state builder creates deeply nested intersection types (`{} & Record<'a', T> & Record<'b', T> & ...`). For exceptionally large builders, simplify type intersections using mapped type normalization:
  `type Id<T> = { [K in keyof T]: T[K] };`
- **Zero Runtime Cost for Phantom Types**: Generic type tracking exists exclusively during compilation. The emitted JavaScript is a plain mutable prototype or closure.

---

## 9. Compiler Internals

TypeScript evaluates `this: Type` parameters using **Contextual This Typing**:
```typescript
class Builder<TState> {
  build(this: Builder<CompleteState>): Result;
}
```
During semantic analysis, the compiler checks if the calling expression's type is a subtype of the method's declared `this` parameter. If any mandatory property is absent from `TState`, assignability fails before checking method arguments.

---

## 10. Best Practices & Design Guidelines

1. **Use Builders for Complex Combinations**: Reserve the Builder pattern for objects with 4+ configuration options or strict construction sequences.
2. **Use Type-State Tracking for Invariants**: Enforce mandatory fields at compile time using generic accumulation.
3. **Freeze Built Instances**: Always execute `Object.freeze()` on the result of `.build()`.
4. **Support Direct Config Objects for Simple Cases**: Allow consumers to pass a full config object directly as an alternative to chaining 10 methods.

---

## 11. Real-World Case Study

### Type-Safe ElasticSearch Query DSL Builder
Building nested ElasticSearch JSON queries with boolean filters and scoring:

```typescript
export interface TermFilter { term: Record<string, string | number>; }
export interface MatchFilter { match: Record<string, string>; }

export class SearchQueryBuilder {
  private mustClauses: (TermFilter | MatchFilter)[] = [];
  private filterClauses: (TermFilter | MatchFilter)[] = [];
  private sizeLimit = 10;

  public mustMatch(field: string, query: string): this {
    this.mustClauses.push({ match: { [field]: query } });
    return this;
  }

  public filterTerm(field: string, value: string | number): this {
    this.filterClauses.push({ term: { [field]: value } });
    return this;
  }

  public limit(n: number): this {
    this.sizeLimit = n;
    return this;
  }

  public build() {
    return {
      size: this.sizeLimit,
      query: {
        bool: {
          must: [...this.mustClauses],
          filter: [...this.filterClauses]
        }
      }
    };
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. How does TypeScript enable compile-time verification that all required properties are set on a builder?</summary>

Through the Type-State pattern using phantom generics and method `this` type parameters. Each setter returns `Builder<TAccumulated & Record<Prop, Value>>`. The `.build()` method declares `this: Builder<CompleteType>`. If any property is missing, calling `.build()` causes a compiler error (`TS2684`).
</details>

<details>
<summary>2. What is the difference between returning `this` versus `new Builder()` from builder methods?</summary>

Returning `this` creates a **mutable builder** that mutates the internal object and returns the same reference, minimizing memory allocation. Returning `new Builder()` creates an **immutable builder** that copies state, preventing accidental side-effects when branching builders or sharing presets.
</details>

<details>
<summary>3. Why should method chaining in class hierarchies use `this` as the return type?</summary>

In TypeScript, returning `this` uses **polymorphic this-typing**. If `ChildBuilder` inherits from `ParentBuilder`, calling a parent method returns `ChildBuilder`, allowing continued chaining of child methods. Returning `ParentBuilder` explicitly would restrict the chain to parent methods.
</details>

<details>
<summary>4. When is a Builder pattern preferable to an Object Literal with `Partial<T>`?</summary>

When:
1. Object creation is multi-stage or spans asynchronous operations.
2. The object has complex inter-field validation rules (e.g., if `port` is set, `host` must be set).
3. The construction requires a specific sequence of operations (wizards).
4. The final object is deeply immutable and private.
</details>

<details>
<summary>5. How can you implement a Step Builder (Wizard) pattern in TypeScript?</summary>

By defining distinct interfaces for each step where each method returns the next step's interface. This enforces that step 2 cannot be called before step 1.
</details>

<details>
<summary>6. What is the memory drawback of fluent immutable builders in Node.js?</summary>

Every chained call allocates a new builder instance and shallow-copies internal state objects. In high-frequency request handling, this triggers V8 minor garbage collection cycles.
</details>

<details>
<summary>7. How do you type an SQL query builder to infer the returned row type based on `.select()` calls?</summary>

By parameterizing the builder with a generic type `TSelected = {}`. Each `.select<K extends string>(col: K)` returns `QueryBuilder<TSelected & Record<K, unknown>>`.
</details>

<details>
<summary>8. Can you combine Zod runtime validation with a TypeScript Builder?</summary>

Yes. The `.build()` method can pass the collected internal state through a Zod schema (`Schema.parse(this.state)`), ensuring both compile-time shape and runtime invariant correctness.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Fluent String Builder</summary>

**Problem:** Create a `StringBuilder` class with `.append(str)`, `.appendLine(str)`, and `.toString()` methods.

**Solution:**
```typescript
export class StringBuilder {
  private buffer: string[] = [];

  public append(str: string): this {
    this.buffer.push(str);
    return this;
  }

  public appendLine(str: string): this {
    this.buffer.push(str + '\n');
    return this;
  }

  public toString(): string {
    return this.buffer.join('');
  }
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Immutable Color Builder</summary>

**Problem:** Create an immutable RGB color builder where `.red(n)`, `.green(n)`, and `.blue(n)` return new builder instances.

**Solution:**
```typescript
export class ColorBuilder {
  constructor(
    public readonly r = 0,
    public readonly g = 0,
    public readonly b = 0
  ) {}

  public red(r: number) { return new ColorBuilder(r, this.g, this.b); }
  public green(g: number) { return new ColorBuilder(this.r, g, this.b); }
  public blue(b: number) { return new ColorBuilder(this.r, this.g, b); }

  public toRgbString() { return `rgb(${this.r},${this.g},${this.b})`; }
}
```
</details>

<details>
<summary>Exercise 3 (Medium): URL Builder with Query Params</summary>

**Problem:** Build a `UrlBuilder` that handles base URL, path concatenation, and query parameters safely.

**Solution:**
```typescript
export class UrlBuilder {
  private pathname = '';
  private queryParams = new URLSearchParams();

  constructor(private baseUrl: string) {}

  public path(p: string): this {
    this.pathname = `${this.pathname.replace(/\/$/, '')}/${p.replace(/^\//, '')}`;
    return this;
  }

  public query(key: string, val: string): this {
    this.queryParams.set(key, val);
    return this;
  }

  public build(): string {
    const url = new URL(this.pathname, this.baseUrl);
    this.queryParams.forEach((v, k) => url.searchParams.set(k, v));
    return url.toString();
  }
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Step Builder Interface Sequence</summary>

**Problem:** Model a 3-step pipeline `StepA -> StepB -> StepC -> finish()` using TypeScript interfaces.

**Solution:**
```typescript
interface StepA { setA(val: string): StepB; }
interface StepB { setB(val: number): StepC; }
interface StepC { finish(): { a: string; b: number }; }

export function makePipeline(): StepA {
  let aVal = '';
  return {
    setA: (val) => {
      aVal = val;
      return {
        setB: (bVal) => ({
          finish: () => ({ a: aVal, b: bVal })
        })
      };
    }
  };
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Dynamic Type-State Builder for Exact Interfaces</summary>

**Problem:** Create a generic type-state builder that tracks provided keys in a tuple or record, allowing `.build()` only when all required keys of target interface `T` are present.

**Solution:**
```typescript
export class StrictBuilder<TTarget extends object, TProvided = {}> {
  private data: Record<string, unknown> = {};

  public with<K extends keyof TTarget>(
    key: K,
    val: TTarget[K]
  ): StrictBuilder<TTarget, TProvided & Record<K, true>> {
    this.data[key as string] = val;
    return this as any;
  }

  public build(
    this: StrictBuilder<TTarget, Record<keyof TTarget, true>>
  ): Readonly<TTarget> {
    return Object.freeze({ ...(this.data as TTarget) });
  }
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Type-Safe Column Selecting Query Builder</summary>

**Problem:** Implement a query builder where `.select('name')` accumulates property keys and `.execute()` returns an array of objects typed strictly with those selected keys.

**Solution:**
```typescript
export class TypedSelectBuilder<TTable, TResult = {}> {
  private selectedKeys: (keyof TTable)[] = [];

  public select<K extends keyof TTable>(
    col: K
  ): TypedSelectBuilder<TTable, TResult & Pick<TTable, K>> {
    this.selectedKeys.push(col);
    return this as any;
  }

  public execute(): Promise<TResult[]> {
    return Promise.resolve([]) as Promise<TResult[]>;
  }
}
```
</details>
