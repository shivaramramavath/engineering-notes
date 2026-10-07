# Variance

Variance answers one question: if `Dog` is a subtype of `Animal`, what is the relationship between `Box<Dog>` and `Box<Animal>`? It might be assignable one way, the other way, both, or neither. The answer depends on **how `Box` uses its type parameter**. Variance explains why `Dog[]` can be used as `Animal[]` but a `(d: Dog) => void` cannot be used as `(a: Animal) => void`, and why some generic errors say "Types of property 'x' are incompatible".

**Prerequisites:**
- [Assignability and subtyping](./01-assignability-and-subtyping.md)
- [Generic types](../06-generics/01-generic-types.md)
- [Strict mode](../13-compiler-and-tsconfig/01-strict-mode.md) (`strictFunctionTypes`)

---

## The four kinds

Take `Dog extends Animal` (every `Dog` is an `Animal`). For a generic type `F<T>`:

| Variance | `F<Dog>` to `F<Animal>` | `F<Animal>` to `F<Dog>` | Intuition |
|---|---|---|---|
| **Covariant** | allowed | not allowed | `T` only comes **out** (read, returned) |
| **Contravariant** | not allowed | allowed | `T` only goes **in** (parameters, written) |
| **Invariant** | not allowed | not allowed | `T` goes both in and out |
| **Bivariant** | allowed | allowed | unchecked in both directions (a deliberate looseness) |

## Covariance: values coming out

A producer of dogs is also a producer of animals:

```ts
interface Producer<T> { get(): T }

declare const dogs: Producer<Dog>;
const animals: Producer<Animal> = dogs;     // ok: whoever calls get() receives an Animal, which a Dog is
```

Return types are covariant. So are readonly properties, `ReadonlyArray<T>`, `Promise<T>`, and `Iterable<T>`.

## Contravariance: values going in

A consumer that handles any animal can handle dogs:

```ts
interface Consumer<T> { accept(value: T): void }
type Handler<T> = (value: T) => void;

declare const handleAnimal: Handler<Animal>;
const handleDog: Handler<Dog> = handleAnimal;      // ok: it can take any Animal, so it can take a Dog

declare const handleDogOnly: Handler<Dog>;
const bad: Handler<Animal> = handleDogOnly;        // error under strictFunctionTypes
```

The reverse fails because the function that only understands dogs would be handed a cat. Function **parameters** are contravariant.

```ts
// Practical case
type ClickHandler = (e: MouseEvent) => void;
const onAnyEvent = (e: Event) => console.log(e.type);

const h: ClickHandler = onAnyEvent;   // ok: a handler for any Event can handle a MouseEvent
```

## Invariance: both directions

If a type both produces and consumes `T`, only the exact type is safe:

```ts
interface Box<T> {
  get(): T;
  set(value: T): void;
}

declare const dogBox: Box<Dog>;
const animalBox: Box<Animal> = dogBox;    // would be unsafe: someone could set a Cat
```

A **mutable container** is usually invariant in principle.

## How the compiler (and you) work out variance

Look at **where** `T` appears:

| Position of `T` | Contributes |
|---|---|
| return type, readonly property | covariant |
| parameter type | contravariant |
| mutable property | both (invariant) |
| inside another generic's argument | follows that generic's variance, flipped inside a parameter |

A type is covariant if `T` appears only in covariant positions, contravariant if only in contravariant positions, and invariant if both. TypeScript computes this by *measuring*: it checks assignability of marker instantiations and caches the result.

## Where TypeScript is deliberately unsound

### Arrays are covariant (and mutable)

```ts
const dogs: Dog[] = [new Dog()];
const animals: Animal[] = dogs;     // allowed
animals.push(new Cat());            // compiles, but now `dogs` contains a Cat
```

Strictly, a mutable array should be invariant. TypeScript treats it as covariant because that matches how people write code, and the alternative would reject a lot of everyday programs. If you do not mutate, use `readonly Animal[]` (or `ReadonlyArray<Animal>`), which is **soundly** covariant.

### Methods are bivariant

Under `strictFunctionTypes`, parameters of **function-typed** properties are checked contravariantly. Parameters of **methods** declared with method syntax are still checked **bivariantly** (either direction is accepted).

```ts
interface Strict { handle: (e: Event) => void }   // property with function type: contravariant
interface Loose  { handle(e: Event): void }       // method: bivariant

const clickOnly = (e: MouseEvent) => {};

const a: Strict = { handle: clickOnly };   // error under strictFunctionTypes
const b: Loose  = { handle: clickOnly };   // allowed (bivariant)
```

The reason is compatibility: if methods were strict, `Array<Dog>` would not be assignable to `Array<Animal>`, because methods like `push(item: T)` would make `T` invariant. Bivariant methods keep the common case working at the cost of soundness.

**Practical consequence:** declare callback members as **properties with function types** when you want strictness, and as **methods** when you want leniency.

## Explicit variance annotations: `in` and `out`

TypeScript 4.7 added optional annotations on type parameters of interfaces, classes, and type aliases:

```ts
interface Producer<out T> { get(): T }                // covariant: T only appears in output
interface Consumer<in T> { accept(value: T): void }   // contravariant
interface Box<in out T> { get(): T; set(value: T): void }   // invariant
```

The compiler **verifies** the annotation against the actual use and errors if they disagree:

```ts
interface Bad<out T> { accept(value: T): void }   // error: T appears in an input position
```

Why annotate:

- **Documentation:** the intended variance becomes part of the API.
- **Better errors:** failures point at the variance contract instead of a deep structural mismatch.
- **Performance:** the compiler can skip measuring and compare type arguments directly, which can speed up checks with large or recursive generic types.
- **Stability:** a later change that accidentally flips variance fails at the declaration, not in distant callers.

You rarely need them in application code. They help in library types.

## Variance and conditional types

`infer` positions inherit variance from where they appear in the pattern:

- `infer U` in a **covariant** position (property type, return type) with several candidates produces a **union**.
- `infer U` in a **contravariant** position (function parameter) with several candidates produces an **intersection**.

That is the mechanism behind `UnionToIntersection`. See [infer](../10-advanced-types/02-infer.md).

## Designing APIs with variance in mind

- **Accept the widest type you need:** take `readonly T[]` or `Iterable<T>` instead of `T[]` when you only read. Callers can then pass arrays of subtypes.
- **Return the narrowest honest type** you can.
- **Avoid mutable generic containers** in public signatures when covariance would be convenient. Expose read-only views.
- **Prefer function-type properties** for callbacks when you want parameter types checked strictly.
- **Put `in`/`out` on library generics** where the intent is clear.

```ts
// Accepts arrays of any Animal subtype because it only reads
function names(animals: readonly Animal[]): string[] {
  return animals.map((a) => a.name);
}

names(dogs);   // ok
```

## Important rules and misconceptions

- **"Subtype relationships carry through generics automatically."** They do not. It depends on variance.
- **"Contravariance is just a theoretical curiosity."** It is exactly why handler types and callbacks behave the way they do.
- **"`strict` makes everything invariant-correct."** `strictFunctionTypes` fixes function-typed parameters, but arrays remain covariant and methods remain bivariant.
- **"Variance annotations change behavior."** They constrain and speed up checking. They do not make an unsound type sound.
- **Bivariance is a compromise,** not a design ideal.

## Common mistakes

- Assuming `Box<Dog>` is assignable to `Box<Animal>` for a mutable box.
- Mutating an array after widening it to a supertype array.
- Defining callback members as methods and then wondering why wrong handler types are accepted.
- Annotating `out T` on a type that also accepts `T` as a parameter.
- Passing a narrower handler (`(e: MouseEvent) => void`) where a broader event handler is declared.

## Debugging

- Read the error for the property or method it names (`Types of property 'set' are incompatible`). That identifies the position that breaks variance.
- Replace the generic with `Dog` and `Animal` in a small repro and check each direction.
- Add `in`/`out` annotations to see the compiler's idea of the intended variance, and let it flag the contradicting member.
- To get stricter checking, convert method syntax members to function-typed properties and turn on `strictFunctionTypes`.

## Quick summary

- Variance describes how subtyping of `T` carries over to `F<T>`: covariant (outputs), contravariant (inputs), invariant (both), or bivariant (unchecked).
- Return types and readonly data are covariant. Function parameters are contravariant under `strictFunctionTypes`. Mutable containers should be invariant.
- TypeScript deliberately treats arrays as covariant and method parameters as bivariant, trading soundness for usability.
- `in`, `out`, and `in out` (TS 4.7) document and verify variance and can speed up checking.
- Design APIs to accept read-only, widest-possible inputs.

**Next:** [Type widening and inference](./03-type-widening-and-inference.md)
