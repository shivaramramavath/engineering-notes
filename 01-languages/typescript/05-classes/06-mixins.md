# Mixins

## Definition
A **mixin** is a function that takes a class and returns a new class that extends it with extra behaviour. Mixins let you compose reusable capabilities without a deep inheritance chain, working around the fact that a class can `extends` only one parent.

## Why It Matters
When several unrelated classes need the same capability (timestamps, event emitting, soft delete), duplicating code or building a tall hierarchy are both poor options. Mixins offer a middle path, though composition is often simpler (see below).

## Prerequisites
[inheritance-and-abstract-classes.md](02-inheritance-and-abstract-classes.md), basic [generics](../06-generics/00-generic-functions.md) (read lightly, revisit after chapter 06)

## Syntax

```ts
// a constructor type: anything you can `new`
type Constructor<T = {}> = new (...args: any[]) => T;

function Timestamped<TBase extends Constructor>(Base: TBase) {
  return class extends Base {
    createdAt = new Date();
    touch() { return new Date(); }
  };
}
```

## Basic Example

```ts
class User {
  constructor(public name: string) {}
}

const TimestampedUser = Timestamped(User);

const u = new TimestampedUser("Ada");
u.name;        // string (from User)
u.createdAt;   // Date   (from the mixin)
```

## How It Works
`Timestamped(User)` returns an anonymous class that extends `User`. The generic `TBase extends Constructor` lets TypeScript keep the base class's instance type, so the result has both sets of members.

### Applying several mixins
```ts
function Activatable<TBase extends Constructor>(Base: TBase) {
  return class extends Base {
    active = false;
    activate() { this.active = true; }
  };
}

class Account extends Activatable(Timestamped(User)) {}

const a = new Account("Ada");
a.activate();
a.createdAt;
a.name;
```

## Important Concepts

### Constraining what a mixin can extend
Require specific members on the base:

```ts
type Named = Constructor<{ name: string }>;

function Greeter<TBase extends Named>(Base: TBase) {
  return class extends Base {
    greet() { return `Hello, ${this.name}`; }
  };
}

Greeter(User);       // ok
Greeter(class {});   // Error: base lacks 'name'
```

### Mixin constructors must accept `...args: any[]`
This is a requirement for a class used as a mixin base; the `any[]` here is deliberate and contained.

### Mixins with `abstract` bases
Use an abstract constructor type if the base may be abstract:
```ts
type AbstractConstructor<T = {}> = abstract new (...args: any[]) => T;
```

### Private and protected members
Anonymous classes returned from mixins cannot have `private` or `protected` members that appear in exported declarations (TypeScript reports an error when generating `.d.ts`). Use `#private` fields or keep the mixin internal.

### Typing the result
```ts
type TimestampedInstance = InstanceType<ReturnType<typeof Timestamped>>;
```
Extracting this is clumsy, which is one cost of mixins.

### Alternative: interface merging with `Object.assign`
Older "mixin pattern" from the handbook declares an interface merging into the class and copies methods at runtime. It is untyped at the boundary and harder to follow; the function-based pattern above is preferable.

## Mixins vs. Composition

| | Mixins | Composition |
|---|---|---|
| Access to `this` of the host | Yes | Via explicit reference |
| Naming collisions | Possible (later wins) | None |
| Type complexity | Higher (generics, constructor types) | Lower |
| Testing | Test combined class | Test parts in isolation |
| Flexibility at runtime | Fixed when class is defined | Can swap parts |

```ts
// composition
class User {
  readonly audit = new AuditLog();
  constructor(public name: string) {}
}
```
Prefer composition unless you specifically need the capability to appear as members of the class itself.

## Common Mistakes
- Using mixins when plain composition would be simpler.
- Name collisions between mixins (later mixin silently overrides).
- Relying on mixin order without documenting it.
- Exporting mixin classes with private/protected members.
- Heavy use of `any` beyond the `...args: any[]` constructor requirement.

## Best Practices
- Keep mixins small and single-purpose.
- Constrain the base type to what the mixin needs.
- Document required members and ordering.
- Name them as capabilities (`Timestamped`, `Activatable`).
- Reach for composition and interfaces first.

## Interview Questions
- How do you implement a mixin in TypeScript?
- Why does the mixin constructor take `...args: any[]`?
- What are the drawbacks of mixins compared with composition?
- How do you constrain what a mixin can extend?

## Quick Reference
```ts
type Constructor<T = {}> = new (...args: any[]) => T;
const WithX = <T extends Constructor>(B: T) => class extends B { x = 1 };
class C extends WithX(Base) {}
```

## Related Topics
- [inheritance-and-abstract-classes.md](02-inheritance-and-abstract-classes.md)
- [Intersection types](../03-unions-and-narrowing/01-intersection-types.md)
- [Generic constraints](../06-generics/02-generic-constraints-and-defaults.md)
- [Design patterns](../17-design-patterns/README.md)
