# Type Parameter Variance Annotations

> Variance annotations (`in` and `out`) explicitly declare covariance, contravariance, or invariance on type parameters, dramatically improving compiler type-checking throughput and preventing structural recursion bottlenecks.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-generic-functions.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/00-generic-functions.md), [01-generic-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/01-generic-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 4.7 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `strictFunctionTypes: true` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-Common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
Variance describes how subtyping between more complex types relates to subtyping between their components:
- If `Dog` extends `Animal`, is `List<Dog>` assignable to `List<Animal>`?
- Is `(dog: Dog) => void` assignable to `(animal: Animal) => void`?

Historically, TypeScript computed variance **implicitly and structurally**: whenever the compiler compared `Foo<A>` to `Foo<B>`, it recursively expanded the full structure of `Foo` and tested every member for assignability. For deeply nested, recursive, or large interface hierarchies, this implicit check created significant compiler performance bottlenecks.

TypeScript 4.7 introduced **Optional Variance Annotations** using the `in` and `out` keywords on type parameters. This allows developers to state the intended variance directly, giving the compiler an instant shortcut and catching structural mistakes before they cause subtle bugs.

---

## 2. Core Concepts

### The Four Variance Positions
1. **Covariant (`out T`)**: The generic type preserves the subtyping direction. `Producer<Dog>` is assignable to `Producer<Animal>`. Used for outputs, return types, and read-only properties.
2. **Contravariant (`in T`)**: The generic type reverses the subtyping direction. `Consumer<Animal>` is assignable to `Consumer<Dog>`. Used for inputs and function parameters.
3. **Invariant (`in out T` or unmarked)**: The generic type requires exact type identity in both directions. `State<T>` both produces and consumes `T`.
4. **Bivariant**: The generic type is assignable in both directions (e.g. method shorthand with `strictFunctionTypes`).

### The Syntax
```typescript
// Covariant: T only appears in output positions (out)
interface Getter<out T> {
  get(): T;
}

// Contravariant: T only appears in input positions (in)
interface Setter<in T> {
  set(value: T): void;
}

// Invariant: T appears in both input and output positions
interface Storage<in out T> {
  read(): T;
  write(item: T): void;
}
```

---

## 3. How It Works Internally

```
Subtyping: Dog extends Animal

Covariant (out T):
  Getter<Dog>  --------assignable to--------> Getter<Animal>
  (Subtype preserves direction)

Contravariant (in T):
  Setter<Animal> ------assignable to--------> Setter<Dog>
  (Subtype reverses direction: Animal consumer can handle any Dog)

Invariant (in out T):
  Box<Dog> <-------- NEVER assignable --------> Box<Animal>
  (Strict identity required)
```

When comparing `Container<A>` to `Container<B>`:
1. If `Container` has variance annotations, the compiler skips structural expansion of `Container`.
2. For `out T`, it checks if `A` is assignable to `B`.
3. For `in T`, it checks if `B` is assignable to `A`.
4. If the annotations are violated by the type body (e.g. `out T` is used as a function parameter), the compiler emits a compiler error (`TS1394`).

---

## 4. Syntax & API Reference

### Valid Type Positions
```typescript
// Valid Covariance
type Producer<out T> = () => T;
type ReadonlyContainer<out T> = { readonly value: T };

// Valid Contravariance
type Consumer<in T> = (val: T) => void;
type Predicate<in T> = (item: T) => boolean;

// Valid Invariance
type MutableCell<in out T> = {
  get(): T;
  set(val: T): void;
};
```

---

## 5. Practical Examples

### 5.1 Covariant Stream Producer
```typescript
class Animal { name = "Animal"; }
class Dog extends Animal { bark() { return "woof"; } }

interface ReadStream<out T> {
  readNext(): T | null;
}

const dogStream: ReadStream<Dog> = {
  readNext: () => new Dog(),
};

// Safe: dogStream produces Dogs, which are valid Animals
const animalStream: ReadStream<Animal> = dogStream;
```

### 5.2 Contravariant Event Sink
```typescript
interface Sink<in T> {
  drain(event: T): void;
}

const animalSink: Sink<Animal> = {
  drain: (a: Animal) => console.log(a.name),
};

// Safe: animalSink can process any Animal, so it safely accepts Dogs
const dogSink: Sink<Dog> = animalSink;
dogSink.drain(new Dog());
```

### 5.3 Invariant State Cell
```typescript
interface StateCell<in out T> {
  get(): T;
  set(val: T): void;
}

declare let dogCell: StateCell<Dog>;
declare let animalCell: StateCell<Animal>;

// @ts-expect-error TS2322: Type 'StateCell<Dog>' is not assignable to type 'StateCell<Animal>'.
animalCell = dogCell;

// @ts-expect-error TS2322: Type 'StateCell<Animal>' is not assignable to type 'StateCell<Dog>'.
dogCell = animalCell;
```

### 5.4 Failure Case: Annotation Violation
```typescript
// @ts-expect-error TS1394: Type parameter 'T' is declared as 'out' but occurs in 'in' position in type '(arg: T) => void'.
interface InvalidProducer<out T> {
  process(arg: T): void;
}
```

### 5.5 Idiomatic Pattern: High-Throughput Tree Comparison
```typescript
interface FastTreeNode<out T> {
  readonly value: T;
  readonly children: ReadonlyArray<FastTreeNode<T>>;
}

function traverse<T>(node: FastTreeNode<T>, visitor: (val: T) => void): void {
  visitor(node.value);
  for (const child of node.children) {
    traverse(child, visitor);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Annotations are strictly verified**: You cannot lie to the compiler. Declaring `out T` on an interface that accepts `T` as a parameter causes compile error `TS1394`.
2. **Method Shorthand Bivariance**: In TypeScript, `{ method(x: T): void }` is bivariant under `strictFunctionTypes`, whereas `{ method: (x: T) => void }` is contravariant. Using variance annotations highlights and enforces proper contravariant property signatures.
3. **Circular Reference Optimization**: Without variance annotations, self-referential interfaces (like deeply nested AST nodes or trees) can cause recursive comparison loops. Adding `out` cuts comparison time drastically.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Marking `out T` on mutable container | Container both reads and writes `T` | Use `in out T` or remove `out` | `TS1394: Type parameter 'T' is declared as 'out' but occurs in 'in' position...` |
| Marking `in T` on return value | Return types are outputs, not inputs | Use `out T` | `TS1394: Type parameter 'T' is declared as 'in' but occurs in 'out' position...` |
| Ignoring variance in recursive AST types | Compiler must repeatedly unroll deep ASTs | Add `out` to AST node types | Slow compile times / high memory |
| Using method syntax in contravariant sink | Method syntax allows unsafe bivariant parameter passing | Use function-valued property syntax `prop: (val: T) => void` | Unsound assignment |
| Annotating local function generics | Functions determine variance from parameter/return positions directly | Annotations belong on `type`, `interface`, or `class` | `TS1005: '=' expected.` |
| Forgetting `readonly` when claiming `out T` | Mutable properties are invariant (`in out`) | Add `readonly` to object properties | `TS1394: ...occurs in invariant position` |

---

## 8. Performance & Type-Checker Complexity
- Variance annotations provide a direct algorithmic optimization in `checkTypeRelatedTo`.
- In large codebases (e.g., UI component hierarchies or complex ORM schemas), adding `in` and `out` annotations has been benchmarked by the TypeScript team to reduce compilation times by up to **30–50%** on complex types.

---

## 9. Compiler Internals
- Parsed into `TypeParameterDeclaration.modifiers` with `SyntaxKind.InKeyword` and `SyntaxKind.OutKeyword`.
- In `src/compiler/checker.ts`:
  - `getVariance(typeParameter: TypeParameter)` retrieves the explicit variance flags (`VarianceFlags.Contravariant`, `Covariant`, `Invariant`).
  - `checkTypeRelatedTo` checks explicit variance first. If the type argument relationship matches the variance flag, assignability succeeds without structural recursion.
  - `checkVariance` verifies that the type parameter only occurs in compatible syntactic positions within the type declaration.

---

## 10. Best Practices & Design Guidelines
- Add `out` to all pure data transfer objects, read-only collections, and producer interfaces.
- Add `in` to comparators, serializer sinks, and event handlers.
- Mark `in out` explicitly when documenting invariant types to document design intent.
- Ensure all object properties in `out` types are marked `readonly`.

---

## 11. Real-World Case Study
### Fast Subtyping in Type-Safe Reactive Observable Library
```typescript
interface Observable<out T> {
  subscribe(observer: Observer<T>): { unsubscribe(): void };
  map<U>(fn: (val: T) => U): Observable<U>;
}

interface Observer<in T> {
  next(val: T): void;
  error(err: Error): void;
  complete(): void;
}

class SafeSubject<T> implements Observable<T>, Observer<T> {
  private observers: Array<Observer<T>> = [];

  subscribe(observer: Observer<T>) {
    this.observers.push(observer);
    return {
      unsubscribe: () => {
        this.observers = this.observers.filter((o) => o !== observer);
      }
    };
  }

  map<U>(fn: (val: T) => U): Observable<U> {
    const nextSubject = new SafeSubject<U>();
    this.subscribe({
      next: (val) => nextSubject.next(fn(val)),
      error: (err) => nextSubject.error(err),
      complete: () => nextSubject.complete(),
    });
    return nextSubject;
  }

  next(val: T) {
    for (const o of this.observers) o.next(val);
  }
  error(err: Error) {
    for (const o of this.observers) o.error(err);
  }
  complete() {
    for (const o of this.observers) o.complete();
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What does the <code>out</code> keyword on a type parameter mean?</summary>

The `out` modifier declares that the type parameter is **covariant**. It signifies that the type parameter only appears in output positions (such as return types or read-only properties) and that subtyping between instances of the generic preserves the component subtyping direction.
</details>

<details>
<summary>2. What does the <code>in</code> keyword on a type parameter mean?</summary>

The `in` modifier declares that the type parameter is **contravariant**. It signifies that the type parameter only appears in input positions (such as function arguments), reversing the subtyping relationship: `Consumer<SuperType>` is assignable to `Consumer<SubType>`.
</details>

<details>
<summary>3. Why are mutable properties invariant?</summary>

A mutable property `val: T` acts as both a producer (when read, outputting `T`) and a consumer (when written, inputting `T`). Because it is simultaneously covariant and contravariant, it must be **invariant**: only exact identical types can safely satisfy both operations.
</details>

<details>
<summary>4. How do variance annotations improve compiler performance?</summary>

Without annotations, the compiler must recursively expand and structurally check every member of two generic types. With annotations, the compiler can test the type arguments directly against each other in $O(1)$ time, skipping deep structural comparisons.
</details>

<details>
<summary>5. Can you apply variance annotations to generic function declarations?</summary>

No. Variance annotations are only valid on generic type aliases, interfaces, and class declarations (`type Foo<out T>`, `interface Bar<in T>`, `class Baz<out T>`).
</details>

<details>
<summary>6. What happens if you declare <code>interface Box&lt;out T&gt; { val: T; }</code> where <code>val</code> is mutable?</summary>

The compiler issues an error (`TS1394`) stating that `T` is declared as `out` but occurs in an invariant position, because a mutable property is writable.
</details>

<details>
<summary>7. What does <code>in out T</code> declare?</summary>

It explicitly declares that `T` is **invariant**, meaning instances of the generic are only assignable if their type arguments are identical.
</details>

<details>
<summary>8. How do method declarations interact with variance under <code>strictFunctionTypes</code>?</summary>

Method syntax (`foo(x: T): void`) is checked bivariantly for historical compatibility, whereas property syntax (`foo: (x: T) => void`) is checked contravariantly. Adding `in T` enforces strict contravariant property checks.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Annotate Readonly Collection (Easy)
Add the correct variance annotation to this collection interface.
<details>
<summary>Solution</summary>

```typescript
interface ReadOnlyCollection<out T> {
  get(index: number): T;
  readonly length: number;
}
```
</details>

### Exercise 2: Annotate Comparator Function (Easy)
Add the correct variance annotation to this comparator type.
<details>
<summary>Solution</summary>

```typescript
type Comparator<in T> = (a: T, b: T) => number;
```
</details>

### Exercise 3: Validated Event Handler Interface (Medium)
Create an `EventHandler<in TEvent>` interface and demonstrate why contravariance permits a base event handler to handle specific child events.
<details>
<summary>Solution</summary>

```typescript
interface DOMEvent { timestamp: number; }
interface MouseClickEvent extends DOMEvent { clientX: number; clientY: number; }

interface EventHandler<in TEvent> {
  handle(event: TEvent): void;
}

const generalHandler: EventHandler<DOMEvent> = {
  handle: (e) => console.log(e.timestamp),
};

// Safe due to contravariance:
const clickHandler: EventHandler<MouseClickEvent> = generalHandler;
clickHandler.handle({ timestamp: 1234, clientX: 10, clientY: 20 });
```
</details>

### Exercise 4: Mutable Cell Invariance Enforcement (Medium)
Declare a type-safe `Cell<in out T>` and verify that the compiler rejects both covariant and contravariant assignment.
<details>
<summary>Solution</summary>

```typescript
interface Cell<in out T> {
  get(): T;
  set(val: T): void;
}

class SuperType { id = 1; }
class SubType extends SuperType { tag = "sub"; }

declare let cSuper: Cell<SuperType>;
declare let cSub: Cell<SubType>;

// @ts-expect-error TS2322
cSuper = cSub;
// @ts-expect-error TS2322
cSub = cSuper;
```
</details>

### Exercise 5: Deep AST Node with Optimized Variance (Hard)
Design an AST expression hierarchy with explicit covariance annotations that prevents deep unrolling during type checks.
<details>
<summary>Solution</summary>

```typescript
interface AstNode<out TMetadata> {
  readonly type: string;
  readonly metadata: TMetadata;
  readonly children: ReadonlyArray<AstNode<TMetadata>>;
}

interface BaseMeta { line: number; col: number; }
interface TypeCheckedMeta extends BaseMeta { inferredType: string; }

declare const checkedTree: AstNode<TypeCheckedMeta>;
// Instant assignment check because AstNode is covariant in TMetadata:
const baseTree: AstNode<BaseMeta> = checkedTree;
```
</details>

### Exercise 6: Bi-directional Pipe Transformer (Hard)
Create a pipeline transformation interface `Transformer<in TInput, out TOutput>` and implement a composable pipe utility.
<details>
<summary>Solution</summary>

```typescript
interface Transformer<in TInput, out TOutput> {
  transform: (input: TInput) => TOutput;
}

function compose<A, B, C>(
  t1: Transformer<A, B>,
  t2: Transformer<B, C>
): Transformer<A, C> {
  return {
    transform: (a: A) => t2.transform(t1.transform(a)),
  };
}

const numToStr: Transformer<number, string> = { transform: (n) => n.toString() };
const strToLen: Transformer<string, number> = { transform: (s) => s.length };
const composed = compose(numToStr, strToLen);
console.log(composed.transform(4200)); // 4
```
</details>
