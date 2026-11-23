# Variance (Covariance, Contravariance, and Invariance) in TypeScript

> Master mathematical variance in TypeScript: covariance, contravariance, invariance, bivariance, function parameter checking under `strictFunctionTypes`, method bivariance, and TypeScript 4.7 `in`/`out` annotations.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type System Internals |
| **Difficulty** | Advanced |
| **Prerequisites** | [06-generics/07-type-parameter-variance-annotations.md](../06-generics/07-type-parameter-variance-annotations.md), [14-type-system-internals/01-assignability-and-subtyping.md](./01-assignability-and-subtyping.md) |
| **TypeScript Baseline** | TS 2.6+ (`strictFunctionTypes`), TS 4.7+ (`in`/`out` annotations) |
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
Variance describes how the subtyping relationship between complex types (e.g. `List<Dog>` vs `List<Animal>`, or `(x: Animal) => void` vs `(x: Dog) => void`) relates to the subtyping relationship between their component types (`Dog <: Animal`).

In programming languages, misunderstanding variance leads to runtime crashes: for instance, passing a dog shelter to a function expecting an animal shelter, only for the function to attempt placing a cat in the dog shelter! TypeScript combines formal subtyping rules with pragmatic engineering trade-offs: function return types are **covariant**, function parameters are **contravariant** under `strictFunctionTypes`, object methods remain **bivariant**, and generic types can declare explicit variance markers (`in` / `out`) since TypeScript 4.7.

---

## 2. Core Concepts
- **Covariance ($A <: B \implies F<A> <: F<B>$)**:
  - The subtyping direction is preserved.
  - Return types of functions and read-only producers are covariant.
  - If `Dog <: Animal`, then `() => Dog` is assignable to `() => Animal`.
- **Contravariance ($A <: B \implies F<B> <: F<A>$)**:
  - The subtyping direction is inverted.
  - Function parameters and write-only consumers are contravariant.
  - If `Dog <: Animal`, then `(a: Animal) => void` is assignable to `(d: Dog) => void`.
- **Invariance ($F<A> <: F<B> \iff A = B$)**:
  - Types are compatible only if they are identical.
  - Mutable read/write containers (where $T$ appears in both input and output positions) are invariant.
- **Bivariance ($F<A> <: F<B>$ if $A <: B$ OR $B <: A$)**:
  - Permitted for method signatures (`method(arg: T): void`) for historical JavaScript compatibility.
- **Explicit Variance Annotations (`in` / `out`)**:
  - `out T`: Declares that `T` is only produced (covariant).
  - `in T`: Declares that `T` is only consumed (contravariant).
  - `in out T`: Declares invariance.

---

## 3. How It Works Internally

```
     Subtyping Hierarchy:     Dog <: Animal
     -------------------------------------------------------
     1. Covariant (out):      Producer<Dog>  <: Producer<Animal>
        (Outputs Dog)                           (Outputs Animal)
        Preserves direction: Dog satisfies Animal!

     2. Contravariant (in):   Consumer<Animal> <: Consumer<Dog>
        (Accepts ANY Animal)                      (Accepts Dog)
        Inverts direction: A handler that can accept ANY Animal
        is completely safe to handle a Dog!

     3. Invariant (in out):   Box<Dog> incompatible with Box<Animal>
        (Reads & Writes)
```

In TypeScript's `checker.ts`, when comparing `Type<T1>` with `Type<T2>`, the compiler calculates the structural positions of `T`. If `T` appears solely in return positions, it calculates a covariant relation. If `T` appears in parameter positions, it evaluates contravariantly. Explicit `in` and `out` keywords short-circuit this structural analysis, accelerating type checking.

---

## 4. Syntax & API Reference

```typescript
// 1. Explicit Variance Annotations (TS 4.7+)
interface Producer<out T> {
  produce(): T; // Covariant position
}

interface Consumer<in T> {
  consume(item: T): void; // Contravariant position
}

interface InvariantContainer<in out T> {
  get(): T;
  set(val: T): void;
}

// 2. Strict function parameter contravariance
type AnimalHandler = (a: Animal) => void;
type DogHandler = (d: Dog) => void;
```

---

## 5. Practical Examples

### Example 1: Function Parameter Contravariance
```typescript
class Animal { name = 'Animal'; }
class Dog extends Animal { bark() { console.log('Woof'); } }

function processDogShelter(handler: (d: Dog) => void) {
  const dog = new Dog();
  handler(dog);
}

const handleAnyAnimal = (a: Animal) => console.log(a.name);
const handleStrictDog = (d: Dog) => d.bark();

// Animal handler can handle Dog! (Contravariance)
processDogShelter(handleAnyAnimal); // Safe

// But an Animal handler CANNOT accept an arbitrary Animal if it expects a Dog!
function processGenericShelter(handler: (a: Animal) => void) {
  handler(new Animal()); // Might pass a Cat!
}

// @ts-expect-error TS2322: Type '(d: Dog) => void' is not assignable to parameter of type '(a: Animal) => void'.
processGenericShelter(handleStrictDog);
```

### Example 2: Method Bivariance vs Function Property Contravariance
```typescript
interface AnimalComparatorMethod {
  compare(a: Animal): number; // Method shorthand: BIVARIANT!
}

interface AnimalComparatorProp {
  compare: (a: Animal) => number; // Property signature: CONTRAVARIANT!
}

declare let mMethod: AnimalComparatorMethod;
declare let mProp: AnimalComparatorProp;

declare let dogComparator: { compare: (d: Dog) => number };

// Bivariant method permits unsafe assignment without warning:
mMethod = dogComparator as any;

// Property syntax strictly prevents unsafe assignment:
// @ts-expect-error TS2322: Types of property 'compare' are incompatible under strictFunctionTypes.
mProp = dogComparator;
```

### Example 3: Invariant Container Unsoundness Prevention
```typescript
interface Box<in out T> {
  value: T;
}

let dogBox: Box<Dog> = { value: new Dog() };
let animalBox: Box<Animal> = { value: new Animal() };

// If Box were covariant:
// animalBox = dogBox;
// animalBox.value = new Cat(); // Ruined dogBox at runtime!

// TypeScript enforces invariance on mutable properties:
// @ts-expect-error TS2322: Type 'Box<Animal>' is not assignable to type 'Box<Dog>'.
dogBox = animalBox;
```

### Example 4: Compiler Failure Case - Illegal Variance Annotation
```typescript
// @ts-expect-error TS1398: Invalid use of 'out' modifier. Type parameter 'T' is used in an input position.
interface BrokenProducer<out T> {
  accept(value: T): void;
}
```

### Example 5: Performance Optimization with Variance Annotations
```typescript
// Complex structural type:
interface DeepTree<out T> {
  readonly id: string;
  readonly children: ReadonlyArray<DeepTree<T>>;
  readonly value: T;
}

// With 'out T', the type checker checks variance directly: T1 <: T2
// without expanding the entire recursive structure of children!
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Arrays are Unsoundly Covariant**: In TypeScript, `Dog[]` is assignable to `Animal[]`. This is technically unsound because writing `animalArray.push(new Cat())` pollutes `Dog[]` at runtime. TypeScript permits this for historical developer convenience.
2. **Method Shorthand Bivariance**: Declaring methods using shorthand syntax (`foo(x: T): void`) turns off `strictFunctionTypes` checking for that member. Always use property syntax (`foo: (x: T) => void`) for callbacks.
3. **Implicit Invariance**: If a type parameter `T` is not used in the body of an interface (`interface Unused<T> {}`), TypeScript considers it bivariant. Two instances `Unused<string>` and `Unused<number>` are mutually assignable! Use nominal branding to fix.
4. **Variance Annotations are Compiler Hints**: `in` and `out` keywords are checked for correctness against the interface body. You cannot declare `out T` if `T` appears in a parameter position.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Declaring callback parameters as methods in interfaces | Bypasses contravariance; introduces runtime bugs | Declare as property: `callback: (x: T) => void` |
| Assuming `Array<T>` is invariant | Mutating `Animal[]` corrupts original `Dog[]` | Use `ReadonlyArray<T>` for covariant safety |
| Empty generic interfaces without variance | `Foo<A>` and `Foo<B>` are mutually assignable | Use type parameter in a member or brand it |
| Omitting `strictFunctionTypes` | All function parameters default to unsafe bivariance | Enable `"strict": true` |
| Deep recursive structural variance without `out` | Sluggish type checking in large trees | Add `out T` variance annotation |

---

## 8. Performance & Type-Checker Complexity
- Structural variance calculation requires recursively walking every property of an interface to locate where `T` appears. For deep or recursive types, this can require thousands of checks.
- Adding explicit `in` / `out` annotations cuts variance checking to a single $\mathcal{O}(1)$ check of the type parameters, accelerating compilation by up to $30\%$ in large type-level libraries.

---

## 9. Compiler Internals
In TypeScript's `checker.ts`:
- `getVariances(symbol)` computes or retrieves cached bitmasks (`VarianceFlags.Covariant`, `VarianceFlags.Contravariant`, `VarianceFlags.Invariant`, `VarianceFlags.Bivariant`).
- If explicit annotations exist (`in`/`out`), the compiler reads them during AST declaration parsing and validates them via `checkTypeParameters`.

---

## 10. Best Practices & Design Guidelines
- Always enable `"strictFunctionTypes": true` (included in `"strict": true`).
- Use `ReadonlyArray<T>` when an array is only read, ensuring safe covariance.
- Use function property signatures (`callback: (val: T) => void`) for all event handlers and callbacks.
- Annotate large, complex generic types with `out` or `in` (TS 4.7+) to assist compiler performance.
- When designing mutable stores, recognize that they are invariant.

---

## 11. Real-World Case Study
A UI state management library allowed components to register state selectors:
```typescript
interface Store<T> {
  subscribe(listener: (state: T) => void): void;
}
```
A developer wrote `interface StateListener<T> { onUpdate(state: T): void; }`. Because `onUpdate` was declared as a method, method bivariance allowed a listener expecting `SpecializedAdminState` to be registered to a store holding general `UserState`. In production, the listener invoked `state.adminToken.toLowerCase()`, crashing with a fatal `TypeError` for non-admin users.

**Remediation:**
The library refactored all listener interfaces to function property syntax:
```typescript
interface StateListener<T> {
  readonly onUpdate: (state: T) => void;
}
```
The compiler immediately caught the incompatible registration, eliminating the bug at compile time.

---

## 12. Interview Questions

<details>
<summary>1. What is covariance vs contravariance?</summary>

- **Covariance**: Subtyping order is preserved. If `Dog <: Animal`, then `F<Dog> <: F<Animal>`. (Typical for return types and read-only data).
- **Contravariance**: Subtyping order is inverted. If `Dog <: Animal`, then `F<Animal> <: F<Dog>`. (Typical for function parameters and consumers).
</details>

<details>
<summary>2. Why are function parameters contravariant?</summary>

Because if a function expects a `Dog` and calls `dog.bark()`, you can safely substitute a general handler that accepts any `Animal` (since it won't demand anything more than what an animal provides). But you cannot substitute a handler that requires a `GoldenRetriever`, because it might call methods specific to that breed on a generic dog.
</details>

<details>
<summary>3. Why does TypeScript treat object methods bivariantly by default?</summary>

For backwards compatibility with early JavaScript arrays. In JavaScript, `Array<Dog>` was treated as a subtype of `Array<Animal>`, and array methods like `push(item: T)` had to be compatible. Enforcing contravariance on methods would make `Dog[]` incompatible with `Animal[]`.
</details>

<details>
<summary>4. What do the `in` and `out` keywords do on generic type parameters (TS 4.7+)?</summary>

They explicitly declare the variance of the type parameter:
- `out T`: Covariant (output-only position)
- `in T`: Contravariant (input-only position)
- `in out T`: Invariant (both input and output positions)
They provide compiler speedups and guard against accidental variance mistakes.
</details>

<details>
<summary>5. Why are mutable properties invariant?</summary>

Because you can both read from and write to a mutable property. Reading requires covariance, while writing requires contravariance. The intersection of covariance and contravariance is invariance.
</details>

<details>
<summary>6. Is `Array<T>` soundly typed in TypeScript?</summary>

No. TypeScript allows `Dog[]` to be assigned to `Animal[]` (covariance), but `Animal[]` is mutable. Pushing a `Cat` into the `Animal[]` modifies the underlying `Dog[]`, creating a runtime type error when the dog array is read later.
</details>

<details>
<summary>7. How can you make an array strictly covariant and safe from mutation?</summary>

Use `readonly T[]` or `ReadonlyArray<T>`. Because mutation methods (`push`, `splice`) are removed, covariance is 100% type-safe.
</details>

<details>
<summary>8. What happens if a type parameter is unused in an interface?</summary>

TypeScript considers it bivariant. Two instances with completely unrelated type arguments (`Foo<string>` and `Foo<number>`) are mutually assignable.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Variance Check**: Identify whether `type Reader<T> = () => T` is covariant, contravariant, or invariant with respect to `T`.
   <details>
   <summary>Solution</summary>

   Covariant, because `T` appears solely in the return (output) position.
   </details>

2. **Fix Method Bivariance**: Convert `interface Listener<T> { onEvent(event: T): void; }` to enforce strict contravariance.
   <details>
   <summary>Solution</summary>

   ```typescript
   interface Listener<T> {
     readonly onEvent: (event: T) => void;
   }
   ```
   </details>

### Medium
3. **Explicit Variance Annotations**: Add `in` or `out` modifiers to the generic parameters of `interface Pipe<A, B> { transform: (input: A) => B }`.
   <details>
   <summary>Solution</summary>

   ```typescript
   interface Pipe<in A, out B> {
     transform: (input: A) => B;
   }
   ```
   </details>

4. **Safe Readonly Array Assignment**: Demonstrate how `ReadonlyArray<Dog>` is safely assignable to `ReadonlyArray<Animal>` while preventing `Cat` injection.
   <details>
   <summary>Solution</summary>

   ```typescript
   class Animal {}
   class Dog extends Animal {}
   class Cat extends Animal {}

   const dogs: ReadonlyArray<Dog> = [new Dog()];
   const animals: ReadonlyArray<Animal> = dogs; // Valid covariance
   // animals.push(new Cat()); // Compile error: Property 'push' does not exist on 'ReadonlyArray'.
   ```
   </details>

### Hard
5. **Invariant Box Type Constructor**: Implement a generic type `type Invariant<T>` that forces any generic wrapper `type Safe<T> = ...` to be strictly invariant without adding runtime fields.
   <details>
   <summary>Solution</summary>

   ```typescript
   export type Invariant<T> = (x: T) => T;

   export interface InvariantBox<T> {
     value: T;
     _variance: Invariant<T>;
   }
   ```
   </details>

6. **Automated Variance Diagnostic Script**: Write a Node.js utility using the TypeScript Compiler API that queries the `TypeChecker` for a given generic type and logs its inferred variance flags (`Covariant`, `Contravariant`, `Invariant`).
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function inspectVariance(sourceCode: string, typeName: string): void {
     const sourceFile = ts.createSourceFile('test.ts', sourceCode, ts.ScriptTarget.Latest, true);
     const defaultHost = ts.createCompilerHost({});
     const program = ts.createProgram(['test.ts'], { strict: true }, {
       ...defaultHost,
       getSourceFile: (name) => name === 'test.ts' ? sourceFile : defaultHost.getSourceFile(name, ts.ScriptTarget.Latest),
     });

     const checker = program.getTypeChecker();
     let targetNode: ts.InterfaceDeclaration | undefined;

     ts.forEachChild(sourceFile, node => {
       if (ts.isInterfaceDeclaration(node) && node.name.text === typeName) {
         targetNode = node;
       }
     });

     if (targetNode) {
       const type = checker.getTypeAtLocation(targetNode);
       console.log(`Inspecting variance for ${typeName}...`);
       // Internal compiler checks can inspect type.flags or type.symbol
     }
   }
   ```
   </details>
