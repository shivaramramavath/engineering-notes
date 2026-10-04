# `implements`

## Definition
`implements` declares that a class satisfies the shape of one or more interfaces (or object types). The compiler checks the class against the contract; nothing is inherited.

## Why It Matters
It separates **what** a component does (the interface) from **how** (the class). This is the basis of dependency inversion, testing with fakes and swappable implementations.

## Prerequisites
[classes.md](00-classes.md), [interfaces](../04-objects-and-interfaces/00-interfaces.md)

## Syntax

```ts
interface Logger {
  log(message: string): void;
}

class ConsoleLogger implements Logger {
  log(message: string): void {
    console.log(message);
  }
}
```

## Basic Example

```ts
class BadLogger implements Logger {
  // Error: Class 'BadLogger' incorrectly implements interface 'Logger'.
  //        Property 'log' is missing in type 'BadLogger'
}
```

## How It Works
- `implements` is a **check only**. It does not copy members, and it does not change the class's type. The class type is still its own structure.
- A class may implement several interfaces: `class A implements X, Y, Z`.
- It can implement type aliases of object types too (but not unions).
- It is erased at runtime: there is no interface object and no `instanceof`.

### Implementing does not infer parameter types
```ts
interface Handler {
  handle(input: string): number;
}

class H implements Handler {
  handle(input) {          // implicit any (under noImplicitAny) - not inferred from the interface
    return input.length;
  }
}
```
Annotate parameters explicitly.

### The class can have more than the interface
```ts
interface Repo { find(id: string): Promise<User | undefined> }

class PgRepo implements Repo {
  find(id: string) { return db.get(id); }
  close() { /* extra method */ }
}
```

### Optional members
If an interface member is optional, the class may omit it, but if it is present it must match.

## Important Concepts

### Programming to the interface
```ts
class UserService {
  constructor(private readonly logger: Logger) {}   // depends on the contract
}

new UserService(new ConsoleLogger());
new UserService({ log: () => {} });    // a plain object works too (structural typing)
```
Because TypeScript is structural, a class does not *need* `implements` to be accepted where an interface is expected. `implements` is still valuable because it:
1. Catches mistakes inside the class (errors appear at the class, not at the call site).
2. Documents intent.
3. Guards against the interface changing silently.

### Interface vs. abstract class
Use an interface for contracts without implementation; use an [abstract class](02-inheritance-and-abstract-classes.md) when you also want shared code or runtime `instanceof`.

### Implementing generics
```ts
interface Repository<T, ID> {
  findById(id: ID): Promise<T | undefined>;
  save(entity: T): Promise<void>;
}

class UserRepository implements Repository<User, string> {
  async findById(id: string) { return undefined; }
  async save(entity: User) {}
}
```
See [generic types](../06-generics/01-generic-types.md) and the [repository pattern](../17-design-patterns/04-repository.md).

### Implementing and extending together
```ts
class Admin extends User implements Auditable, Serializable {}
```

### Interfaces that extend classes
An interface can extend a class, inheriting its members (including private ones, so only that class or subclasses can implement it). This is rare.

### Dependency injection tokens
Interfaces vanish at runtime, so DI containers need a runtime token (a string, symbol or abstract class) to identify them; see [dependency injection](../17-design-patterns/05-dependency-injection.md).

## Common Mistakes
- Expecting `implements` to inherit method bodies or parameter types.
- Expecting `instanceof SomeInterface` to work.
- Implementing huge interfaces (split them).
- Forgetting that a method-vs-property mismatch can still compile because of bivariance.
- Implementing a union type alias (not allowed).

## Best Practices
- Define small, focused interfaces and implement several.
- Name implementations by what differs (`PostgresUserRepository`), not `UserRepositoryImpl`.
- Depend on interfaces in constructors; construct implementations at the application edge.
- Use `implements` even when not strictly required so errors surface in the class.

## Interview Questions
- What does `implements` actually do?
- Can a class implement multiple interfaces? Can it extend multiple classes?
- Is `implements` needed for a class to be assignable to an interface?
- How do you do dependency injection against an interface, given it does not exist at runtime?

## Quick Reference
```ts
class A implements I, J {}
class B extends Base implements I {}
interface I { m(x: string): void }
```

## Related Topics
- [inheritance-and-abstract-classes.md](02-inheritance-and-abstract-classes.md)
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md)
- [Dependency injection](../17-design-patterns/05-dependency-injection.md)
- [Repository pattern](../17-design-patterns/04-repository.md)
