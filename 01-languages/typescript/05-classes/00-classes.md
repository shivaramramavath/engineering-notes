# Classes

## Definition
A **class** is a template for creating objects with shared structure and behaviour. TypeScript adds type annotations, visibility modifiers and a few conveniences (such as parameter properties) on top of JavaScript classes.

## Why It Matters
Classes are widely used for services, domain models, error types, framework components (NestJS, Angular) and anything that couples state with behaviour. Understanding how fields, constructors and `this` are typed prevents the most common class bugs.

## Prerequisites
[objects](../01-fundamentals/03-objects.md), [interfaces](../04-objects-and-interfaces/00-interfaces.md), [this parameters](../02-functions/04-this-parameters.md)

## Syntax

```ts
class User {
  id: number;
  name: string;
  email?: string;

  constructor(id: number, name: string) {
    this.id = id;
    this.name = name;
  }

  greet(): string {
    return `Hi, I'm ${this.name}`;
  }
}

const u = new User(1, "Ada");
```

## Basic Example

```ts
const u = new User(1, "Ada");
u.greet();            // "Hi, I'm Ada"
u.name = 42;          // Error: number is not assignable to string
new User(1);          // Error: Expected 2 arguments
```

## How It Works

### A class creates both a value and a type
```ts
const ctor: typeof User = User;   // the constructor function (value)
const instance: User = new User(1, "Ada");   // the instance type
```
`User` as a type means "an instance of User". `typeof User` is the class constructor, including its statics.

### Fields must be initialised (`strictPropertyInitialization`)
With `strict`, every non-optional field must be assigned in the constructor or at its declaration:

```ts
class A {
  x: number;                 // Error: not definitely assigned in the constructor
  y: number = 0;             // ok
  z?: number;                // ok
  w!: number;                // definite assignment assertion: "I'll set it elsewhere"
}
```
Use `!` only when something outside the constructor (a framework, dependency injection, `init()`) guarantees assignment.

### Parameter properties
Add a modifier to a constructor parameter to declare and assign a field in one step:

```ts
class User {
  constructor(
    public readonly id: number,
    public name: string,
    private passwordHash: string,
  ) {}
}
```
This is equivalent to declaring the three fields and assigning them. Some tooling (for example type-stripping runtimes and the `erasableSyntaxOnly` option) rejects parameter properties because they generate runtime code; check your toolchain if you use them.

### Field initialisers and `useDefineForClassFields`
With a target of `ES2022` or later, fields are emitted using JavaScript's native class fields (define semantics), which run in a specific order relative to the constructor and `super()`. Be careful when a subclass field overrides a base-class field that the base constructor reads.

### Methods and `this`
Methods live on the prototype and receive `this` when called as `obj.method()`. Passing them as callbacks loses `this`:

```ts
const greet = u.greet;
greet();   // this is undefined at runtime

// fixes
const bound = u.greet.bind(u);
setTimeout(() => u.greet());
```
An arrow-function property binds `this` per instance:

```ts
class Counter {
  count = 0;
  inc = () => { this.count++; };   // safe to pass around
}
```
Trade-off: one function per instance and not shared on the prototype.

### Optional methods and fields
```ts
class Plugin {
  name = "plugin";
  init?(): void;
}
```

### Overloaded constructors
```ts
class Point {
  x: number;
  y: number;

  constructor();
  constructor(x: number, y: number);
  constructor(x = 0, y = 0) {
    this.x = x;
    this.y = y;
  }
}
```
Often a [static factory method](04-static-members.md) is clearer than overloads.

### Classes are structural
Two classes with the same public shape are interchangeable (private/protected members make them nominal); see [structural typing](../04-objects-and-interfaces/04-structural-typing.md).

### Class expressions
```ts
const Logger = class {
  log(msg: string) { console.log(msg); }
};
```

### Generic classes
```ts
class Box<T> {
  constructor(public value: T) {}
}
const b = new Box("x");   // Box<string>
```
See [generic types](../06-generics/01-generic-types.md).

## Common Mistakes
- Forgetting to initialise fields, then using `!` everywhere to silence the error.
- Losing `this` when passing methods as callbacks.
- Reading a subclass field in a base-class constructor (it is not initialised yet).
- Using a class when a plain object and functions would do.
- Expecting `instanceof` to work with interfaces (it only works with classes).

## Best Practices
- Prefer small classes with a single responsibility.
- Initialise fields at declaration or in the constructor.
- Make fields `readonly` unless they must change.
- Use plain objects, functions and discriminated unions for data; use classes for stateful services and things that need `instanceof`.
- Keep the constructor simple; avoid async work or side effects in it.

## Interview Questions
- What does `typeof MyClass` represent versus `MyClass`?
- What is `strictPropertyInitialization`?
- What are parameter properties?
- How can `this` be lost in a class method and how do you prevent it?

## Quick Reference
```ts
class A {
  x = 0;                                  // field with initialiser
  y?: string;                             // optional field
  z!: number;                             // definite assignment (trust me)
  constructor(public readonly id: number) {}   // parameter property
  method(): void {}
  arrow = () => {};
}
```

## Related Topics
- [access-modifiers.md](01-access-modifiers.md)
- [inheritance-and-abstract-classes.md](02-inheritance-and-abstract-classes.md)
- [static-members.md](04-static-members.md)
- [Generic types](../06-generics/01-generic-types.md)
