# Access Modifiers

## Definition
**Access modifiers** control where a class member can be used: `public` (anywhere), `protected` (the class and its subclasses), `private` (the class only). JavaScript also has true runtime privacy with `#name` fields.

## Why It Matters
Hiding internals lets you change an implementation without breaking callers and prevents accidental misuse. Knowing the difference between TypeScript's `private` and JavaScript's `#private` matters for security and interop.

## Prerequisites
[classes.md](00-classes.md)

## Syntax

```ts
class Account {
  public owner: string;            // default; can be omitted
  protected balance = 0;
  private pin: string;
  #secret = "s3cr3t";              // ECMAScript private field
  readonly id: number;

  constructor(owner: string, pin: string, id: number) {
    this.owner = owner;
    this.pin = pin;
    this.id = id;
  }
}
```

## Basic Example

```ts
const acc = new Account("Ada", "1234", 1);

acc.owner;       // ok
acc.balance;     // Error: 'balance' is protected
acc.pin;         // Error: 'pin' is private
acc.#secret;     // Error: private identifiers are not accessible outside the class
```

## How It Works

| Modifier | Class | Subclass | Outside |
|---|---|---|---|
| `public` | yes | yes | yes |
| `protected` | yes | yes | no |
| `private` | yes | no | no |
| `#field` | yes | no | no |

```ts
class Savings extends Account {
  addInterest() {
    this.balance *= 1.01;   // ok: protected
    this.pin;               // Error: private to Account
  }
}
```

### `private` is compile-time only
TypeScript's `private` and `protected` are erased. At runtime the property exists and is reachable:

```ts
(acc as any).pin;        // works; also visible in console.log and JSON.stringify
acc["pin"];              // bracket access bypasses the check (an intentional escape hatch)
```
Do not store secrets in `private` fields and expect them to be hidden.

### `#private` is runtime-enforced
```ts
class Token {
  #value: string;
  constructor(v: string) { this.#value = v; }
  reveal() { return this.#value; }
}
```
`#value` cannot be read from outside, even with `as any`, and does not appear in `Object.keys` or `JSON.stringify`. Costs: it needs a target that supports it (or a downlevel helper), and it cannot be accessed via subclasses.

### Which one to use?

| Need | Choose |
|---|---|
| Compile-time encapsulation, ergonomic debugging, tests may peek | `private` |
| Real runtime privacy | `#private` |
| Shared with subclasses | `protected` |
| Part of the API | `public` (default) |

### `readonly`
Assignable only in the constructor or at the declaration:
```ts
class A {
  readonly id: number;
  constructor(id: number) { this.id = id; }   // ok
  change() { this.id = 2; }                   // Error
}
```
Combine with parameter properties: `constructor(private readonly db: Db) {}`.

### Private members make classes nominal
Two classes with identical shapes are not interchangeable if they declare a `private`/`protected` member separately:

```ts
class A { private x = 1 }
class B { private x = 1 }
const a: A = new B();   // Error
```

### `protected` constructors
```ts
class Base {
  protected constructor() {}
  static create() { return new Base(); }
}
new Base();        // Error
```
Useful for abstract-like bases and singleton/factory patterns.

### `private` constructors
Block `new` entirely; combine with a static factory (see [static members](04-static-members.md)).

### Visibility vs. variance
A subclass may widen visibility (`protected` -> `public`) but not narrow it.

## Common Mistakes
- Treating TypeScript `private` as a security boundary.
- Making everything `public` by default (no encapsulation).
- Making members `private` and then needing to subclass (use `protected`).
- Using `private` fields in a class that gets serialised (`JSON.stringify`), leaking data.
- Using `as any` or `["field"]` to reach private members in production code.

## Best Practices
- Default to the narrowest visibility that works.
- Keep fields private and expose behaviour through methods or [accessors](05-getters-and-setters.md).
- Use `#private` when runtime privacy is a requirement (for example, library internals, tokens).
- Mark fields `readonly` when they never change after construction.

## Security
Never rely on `private` for secrets; sensitive values should not live on long-lived objects that get logged or serialised. See [security checklist](../23-security/03-security-checklist.md).

## Interview Questions
- Difference between TypeScript `private` and `#private`?
- Can `private` members be accessed at runtime?
- What is the difference between `private` and `protected`?
- Why are classes with private members not structurally interchangeable?

## Quick Reference
```ts
public x;  protected y;  private z;  #w;  readonly r;
constructor(private readonly dep: Dep) {}
protected constructor() {}
```

## Related Topics
- [classes.md](00-classes.md)
- [inheritance-and-abstract-classes.md](02-inheritance-and-abstract-classes.md)
- [Structural typing](../04-objects-and-interfaces/04-structural-typing.md)
- [Unsafe types and assertions](../23-security/00-unsafe-types-and-assertions.md)
