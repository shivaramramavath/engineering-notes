# Decorators

## Definition
A **decorator** is a function applied to a class or class member (method, accessor, field) using `@name` syntax, to observe or modify it. TypeScript supports two incompatible flavours:

1. **Standard decorators** (ECMAScript decorators), supported by default since TypeScript 5.0.
2. **Legacy decorators** (`experimentalDecorators`), the older design used by Angular, NestJS, TypeORM and others.

## Why It Matters
Frameworks use decorators for dependency injection, routing, validation and ORM mapping. You need to know which flavour your framework requires, because the signatures and compiler settings differ.

## Prerequisites
[classes.md](00-classes.md), [getters-and-setters.md](05-getters-and-setters.md), [generics](../06-generics/README.md) (read lightly first)

## Standard Decorators (TypeScript 5.0+)

No compiler flag is needed. Each decorator receives the thing being decorated and a **context** object.

### Method decorator
```ts
function logged<This, Args extends unknown[], Return>(
  target: (this: This, ...args: Args) => Return,
  context: ClassMethodDecoratorContext<This, (this: This, ...args: Args) => Return>,
) {
  const name = String(context.name);
  return function (this: This, ...args: Args): Return {
    console.log(`-> ${name}`, args);
    const result = target.call(this, ...args);
    console.log(`<- ${name}`, result);
    return result;
  };
}

class Calculator {
  @logged
  add(a: number, b: number) { return a + b; }
}

new Calculator().add(1, 2);
```
Returning a function replaces the method; returning nothing keeps the original.

### Class decorator
```ts
function sealed<T extends abstract new (...args: any) => object>(
  target: T,
  context: ClassDecoratorContext<T>,
) {
  context.addInitializer(function () {
    Object.seal(this);   // runs after the class is defined
  });
}

@sealed
class Service {}
```

### Field and accessor decorators
```ts
function defaultTo(value: number) {
  return function (_: undefined, context: ClassFieldDecoratorContext) {
    return (initial: number) => initial ?? value;   // transforms the initial value
  };
}

class Settings {
  @defaultTo(10) retries!: number;
}
```
Auto-accessors (`accessor count = 0`) are the natural target for decorators that need to intercept reads and writes.

### Decorator factories
A decorator that takes arguments is a function returning a decorator:
```ts
function tag(label: string) {
  return function (_: unknown, context: ClassMethodDecoratorContext) {
    console.log(`${label}: ${String(context.name)}`);
  };
}

class A {
  @tag("api") run() {}
}
```

### Context object
Common fields: `kind` ("class", "method", "getter", "setter", "field", "accessor"), `name`, `static`, `private`, plus `addInitializer` and `metadata` (decorator metadata, newer TypeScript versions).

### Not supported in standard decorators
- **Parameter decorators** (`constructor(@Inject() dep: Dep)`).
- Automatic emission of design-time type metadata (`emitDecoratorMetadata`).

## Legacy Decorators (`experimentalDecorators`)

Enable in `tsconfig.json`:

```json
{
  "compilerOptions": {
    "experimentalDecorators": true,
    "emitDecoratorMetadata": true
  }
}
```
`emitDecoratorMetadata` emits `design:type`, `design:paramtypes` and similar metadata (with `reflect-metadata`), which DI containers use to find constructor parameter types at runtime.

### Legacy method decorator signature
```ts
function Log(target: object, propertyKey: string, descriptor: PropertyDescriptor) {
  const original = descriptor.value;
  descriptor.value = function (...args: unknown[]) {
    console.log(propertyKey, args);
    return original.apply(this, args);
  };
}

class Service {
  @Log
  run() {}
}
```

### Legacy parameter decorator
```ts
function Body(target: object, propertyKey: string | symbol, parameterIndex: number) {}

class Controller {
  create(@Body dto: CreateUserDto) {}
}
```

### Why legacy decorators still matter
NestJS, Angular, TypeORM, class-validator and MikroORM have traditionally relied on legacy decorators plus metadata emission. Check your framework's current documentation to see which flavour and tsconfig options it requires, because support is evolving.

## Comparison

| | Standard | Legacy |
|---|---|---|
| Flag | none (TS 5.0+) | `experimentalDecorators` |
| Signature | `(value, context)` | `(target, key, descriptor)` |
| Parameter decorators | No | Yes |
| Type metadata emission | No | Yes (`emitDecoratorMetadata`) |
| Standardised in the language | Yes (TC39) | No |
| Mixed in one compilation | No | No |

You cannot use both styles in the same compilation setting; the compiler flag decides which one `@decorator` syntax means.

## Common Mistakes
- Mixing signatures: writing a legacy decorator in a project using standard decorators (or vice versa).
- Forgetting `reflect-metadata` when using DI that relies on `emitDecoratorMetadata`.
- Putting heavy logic in decorators, making behaviour hard to trace.
- Relying on decorators for type-safety; they rewrite runtime behaviour but do not change the declared types.
- Using parameter decorators in a standard-decorator project (not supported).

## Best Practices
- Follow the decorator style your framework documents; do not choose independently.
- Keep decorators small and side-effect-light; document their effects.
- Use decorator factories for configuration.
- Prefer explicit functions or composition for code you control entirely; use decorators where the framework expects them.
- Add tests for decorated behaviour.

## Interview Questions
- What is the difference between standard and legacy decorators?
- What does `emitDecoratorMetadata` do and who needs it?
- Why do parameter decorators matter for dependency injection frameworks?
- What is a decorator factory?

## Quick Reference
```ts
@dec class A {}
class A { @dec method() {}  @dec field = 1;  @dec accessor x = 0; }
function dec(value, context) { /* standard */ }
function dec(target, key, descriptor) { /* legacy */ }
// tsconfig (legacy): experimentalDecorators, emitDecoratorMetadata
```

## Related Topics
- [getters-and-setters.md](05-getters-and-setters.md)
- [Dependency injection](../17-design-patterns/05-dependency-injection.md)
- [NestJS](../20-nodejs-backend/08-nestjs.md)
- [Compiler options](../13-compiler-and-tsconfig/00-compiler-options.md)
