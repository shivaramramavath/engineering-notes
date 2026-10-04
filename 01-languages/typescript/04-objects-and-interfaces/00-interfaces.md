# Interfaces

## Definition
An **interface** names an object shape: the properties and methods a value must have. Interfaces exist only at compile time and can be extended, implemented by classes, and merged.

## Why It Matters
Interfaces are the main way to describe contracts between parts of a program: domain entities, function dependencies, class contracts and public library APIs.

## Prerequisites
[objects](../01-fundamentals/03-objects.md), [type aliases](../01-fundamentals/04-type-aliases.md)

## Syntax

```ts
interface User {
  id: number;
  name: string;
  email?: string;             // optional
  readonly createdAt: Date;   // readonly
}
```

## Basic Example

```ts
function greet(user: User): string {
  return `Hello, ${user.name}`;
}

greet({ id: 1, name: "Ada", createdAt: new Date() });   // ok
greet({ id: 1, name: "Ada" });                          // Error: 'createdAt' is missing
```

## How It Works

### Methods
Two equivalent-looking forms with a subtle difference in checking (method syntax is bivariant; property syntax is checked strictly under `strictFunctionTypes`):

```ts
interface Logger {
  log(message: string): void;            // method signature
  warn: (message: string) => void;       // property with function type
}
```
Prefer property syntax when you want strict parameter checking; method syntax is conventional for class-like contracts.

### Call and construct signatures
```ts
interface Formatter {
  (value: number): string;      // callable
  locale: string;               // plus properties
}

interface UserConstructor {
  new (name: string): User;
}
```

### Extending interfaces
```ts
interface Entity {
  id: number;
}

interface Timestamped {
  createdAt: Date;
  updatedAt: Date;
}

interface Post extends Entity, Timestamped {
  title: string;
}
```
An interface can extend several interfaces (and object type aliases). If extended members conflict, the compiler reports an error at the declaration:

```ts
interface A { x: string }
interface B extends A { x: number }   // Error: incorrectly extends
```
A subinterface may narrow a property type (for example `string` to `"a" | "b"`).

### Generic interfaces
```ts
interface ApiResponse<T> {
  data: T;
  error?: string;
}

const r: ApiResponse<User[]> = { data: [] };
```
See [generic types](../06-generics/01-generic-types.md).

### Declaration merging
Interfaces with the same name in the same scope merge into one:

```ts
interface Window { appVersion: string }   // adds to the global Window
```
This is how libraries are augmented; see [declaration merging](../09-declaration-files/03-declaration-merging.md). It is also a trap: an accidental duplicate name silently merges.

### Implementing interfaces
```ts
class Admin implements User {
  constructor(public id: number, public name: string, readonly createdAt: Date) {}
}
```
See [implements](../05-classes/03-implements.md).

### Interface vs. object type literal
For plain object shapes they are almost interchangeable; the differences are in [interface-vs-type.md](01-interface-vs-type.md).

### Hybrid and indexable interfaces
```ts
interface StringMap {
  [key: string]: string;     // see index-signatures.md
}
```

## Common Mistakes
- Declaring the same interface name twice by accident (silent merge).
- Prefixing names with `I` (`IUser`); this is not idiomatic TypeScript.
- Using interfaces for unions or tuples (not possible; use `type`).
- Creating huge interfaces instead of composing small ones with `extends`.
- Expecting interfaces to exist at runtime (you cannot use `instanceof` with them).

## Best Practices
- Name by domain meaning (`User`, `OrderLine`), not with an `I` prefix.
- Compose with `extends` rather than duplicating members.
- Keep interfaces small and focused (interface segregation).
- Export interfaces that form part of a module's public API.
- Use `readonly` for properties that should not change after creation.

## Interview Questions
- What is declaration merging and when is it useful?
- Can an interface extend multiple interfaces?
- Can a class implement an interface? Can an interface extend a class?
- Does an interface exist at runtime?

## Quick Reference
```ts
interface A { x: number; y?: string; readonly z: boolean }
interface B extends A, C { w: number }
interface F { (x: number): string }        // callable
interface Ctor { new (x: number): A }      // constructable
interface G<T> { value: T }                // generic
```

## Related Topics
- [interface-vs-type.md](01-interface-vs-type.md)
- [readonly-and-optional-properties.md](02-readonly-and-optional-properties.md)
- [Classes and implements](../05-classes/03-implements.md)
- [Declaration merging](../09-declaration-files/03-declaration-merging.md)
