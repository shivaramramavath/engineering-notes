# Module 27: TypeScript Output & Tricky Compilation Questions

> **Topic Overview**: Code snippets designed to test subtle edge cases in compiler evaluation, type distribution, excess property checks, structural typing, and runtime vs type-time discrepancies.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Interview & Preparation |
| **Target Level** | Senior to Principal Engineer |
| **TypeScript Baseline** | TypeScript 5.0 - 5.8+ |
| **Focus Areas** | Tricky Typings, Output Prediction, Distributive Unions, Bivariance |

---

## Output Prediction Challenges

### Challenge 1: The Distributive `never` Trap
**Question**: What does `TypeA` evaluate to, and why?
```typescript
type IsNever<T> = T extends never ? true : false;
type TypeA = IsNever<never>;
```
<details>
<summary><strong>Answer & Explanation</strong></summary>

**Result**: `TypeA` evaluates to `never`, NOT `true`!

**Why**:
`never` is the empty union in TypeScript type theory. Conditional types automatically distribute over bare type parameters. When a union distributes over an empty set of members, the condition never executes, returning an empty set (`never`).

**Fix**: Wrap the parameter in a single-element tuple to prevent distributive evaluation:
```typescript
type IsNeverFixed<T> = [T] extends [never] ? true : false;
type TypeB = IsNeverFixed<never>; // true!
```
</details>

---

### Challenge 2: Object Literal Excess Property Checks
**Question**: Why does the second line fail compilation while the fourth line succeeds?
```typescript
interface Box {
  width: number;
  height: number;
}

// Line 2:
const b1: Box = { width: 10, height: 20, color: "red" };

// Line 4:
const raw = { width: 10, height: 20, color: "red" };
const b2: Box = raw;
```
<details>
<summary><strong>Answer & Explanation</strong></summary>

**Result**:
- Line 2 throws `TS2353: Object literal may only specify known properties, and 'color' does not exist in type 'Box'`.
- Line 4 compiles cleanly without error!

**Why**:
TypeScript employs **Excess Property Checking (EPC)** strictly on *fresh object literals*. The compiler assumes that if you write an inline literal with extra properties, you made a typo or passed invalid configurations. However, when assigning an existing variable (`raw`), TypeScript applies pure **structural subtyping** (duck typing): `raw` has `width` and `height` of type number, satisfying the contract of `Box`.
</details>

---

### Challenge 3: Function Parameter Contravariance vs Method Bivariance
**Question**: Why does `Comparer<Animal>` assign to `Comparer<Dog>` or vice versa?
```typescript
class Animal { name = "animal"; }
class Dog extends Animal { bark() {} }

type Callback<T> = (arg: T) => void;

let cbAnimal: Callback<Animal> = (a: Animal) => console.log(a.name);
let cbDog: Callback<Dog> = (d: Dog) => d.bark();

cbDog = cbAnimal; // Line 8: Does this compile?
cbAnimal = cbDog; // Line 9: Does this compile?
```
<details>
<summary><strong>Answer & Explanation</strong></summary>

Under `"strictFunctionTypes": true`:
- Line 8 (`cbDog = cbAnimal`) **Compiles cleanly**. A function expecting any `Animal` can safely be used where a `Dog` is supplied (since `Dog` is an `Animal`).
- Line 9 (`cbAnimal = cbDog`) **Fails compilation** with `TS2322`: Type `Animal` is not assignable to `Dog`. If `cbAnimal` were called with a `Cat`, invoking `d.bark()` would crash at runtime.

**Note on Object Methods**:
Method syntax (`{ fn(arg: T): void }`) remains intentionally **bivariant** in TypeScript to preserve compatibility with existing JavaScript arrays (e.g., `Array.prototype.push`).
</details>\n