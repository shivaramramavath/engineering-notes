# TypeScript Decorators

A decorator is a function that attaches behavior or metadata to a class, method, property, accessor, or parameter at the moment the class is defined. NestJS is built almost entirely on this mechanism: `@Module`, `@Controller`, `@Injectable`, `@Get`, `@Body`, and every guard or pipe binding are decorators that record metadata, which the framework later reads to wire your application together. If you understand decorators and `reflect-metadata`, Nest stops being magic.

---

## Overview

**What it is.** Decorators are a declarative syntax (`@name`) for running a function against a declaration. They can observe it, replace it, or store information about it.

**Why it exists.** They let you attach cross-cutting information (routing, validation rules, injection rules) directly next to the code it describes, instead of in separate configuration files.

**Where it is used.** NestJS, Angular, TypeORM, `class-validator`, `class-transformer`, TypeDI, InversifyJS, MobX.

**Why you should understand it.**

- Decorators run **once, when the class is defined** (at module load), not when a method is called.
- Most Nest decorators do not execute logic. They **record metadata**. Components such as the router, the DI container, and guards read that metadata later.
- Dependency injection works because the TypeScript compiler can emit constructor parameter types as metadata.
- Knowing the two decorator models (legacy and standard) explains toolchain errors you will eventually hit.

> **Two decorator systems exist.** NestJS uses TypeScript's **legacy** (`experimentalDecorators`) decorators together with `emitDecoratorMetadata`. TypeScript 5.0 added **standard** (TC39) decorators, which have a different signature and no parameter decorators. This file teaches the legacy model, because that is what you will write in NestJS. The differences are summarized in [Version / Compatibility Notes](#version--compatibility-notes).

---

## Mental Model

A decorator is a function that receives the thing being declared (or a description of it) and runs when the declaration is evaluated.

```text
 class definition is evaluated (module load)
        │
        ▼
 ┌─────────────────────────────────────────┐
 │ @Injectable()                           │   decorators run here,
 │ class UsersService {                    │   once, in a defined order
 │   constructor(private repo: Repo) {}    │
 │   @Log()                                │
 │   find() {}                             │
 │ }                                       │
 └─────────────────────────────────────────┘
        │
        │  decorators write metadata:  Reflect.defineMetadata(key, value, target)
        ▼
 ┌─────────────────────────────────────────┐
 │  metadata store (reflect-metadata)      │   "UsersService" → { injectable: true,
 │  keyed by (target, propertyKey)         │                      design:paramtypes: [Repo] }
 └─────────────────────────────────────────┘
        │
        │  later, at application bootstrap
        ▼
 ┌─────────────────────────────────────────┐
 │  Framework reads metadata and acts      │   DI container builds instances,
 │  (DI container, router, guards...)      │   router registers routes, ...
 └─────────────────────────────────────────┘
```

Think of a decorator as **a sticky note attached to a declaration**, plus optionally **a wrapper around it**. Nest's decorators are almost always sticky notes.

---

## Core Concepts

### What a Decorator Is

A decorator is a function applied with `@` syntax directly before a declaration.

```typescript
function Sealed(constructor: Function) {
  Object.seal(constructor);
  Object.seal(constructor.prototype);
}

@Sealed
class Report {}
```

`@Sealed` is equivalent to calling `Sealed(Report)` right after the class is defined.

### Enabling Legacy Decorators

```jsonc
// tsconfig.json
{
  "compilerOptions": {
    "experimentalDecorators": true,
    "emitDecoratorMetadata": true,
    "target": "ES2021"
  }
}
```

Without `experimentalDecorators`, TypeScript 5+ interprets `@decorator` as a **standard** decorator, and the signatures below will not type-check.

### The Five Decorator Kinds (Legacy)

| Kind | Applied to | Receives | May return |
|---|---|---|---|
| Class | `class X {}` | `(constructor)` | A replacement constructor, or nothing |
| Method | `foo() {}` | `(target, propertyKey, descriptor)` | A new descriptor, or nothing |
| Accessor | `get x() {}` | `(target, propertyKey, descriptor)` | A new descriptor, or nothing |
| Property | `x: string` | `(target, propertyKey)` | Nothing (return value is ignored) |
| Parameter | `foo(@D() a)` | `(target, propertyKey, parameterIndex)` | Nothing (return value is ignored) |

Details on `target`:

- For an **instance** member, `target` is the class **prototype**.
- For a **static** member, `target` is the class **constructor**.
- For a **constructor parameter**, `target` is the constructor, and `propertyKey` is `undefined`.

### Class Decorator

```typescript
function Controller(prefix: string): ClassDecorator {
  return (target) => {
    Reflect.defineMetadata('prefix', prefix, target);
  };
}

@Controller('/users')
class UsersController {}
```

### Method Decorator

```typescript
function Log(): MethodDecorator {
  return (target, propertyKey, descriptor: PropertyDescriptor) => {
    const original = descriptor.value;
    descriptor.value = function (...args: unknown[]) {
      console.log(`call ${String(propertyKey)}`, args);
      return original.apply(this, args);
    };
    return descriptor;
  };
}

class Calc {
  @Log()
  add(a: number, b: number) { return a + b; }
}
```

The decorator replaces the method with a wrapper. Note `function` and `this`: an arrow function here would lose the instance `this`.

### Property Decorator

```typescript
function Required(): PropertyDecorator {
  return (target, propertyKey) => {
    const existing: (string | symbol)[] =
      Reflect.getMetadata('required', target) ?? [];
    Reflect.defineMetadata('required', [...existing, propertyKey], target);
  };
}

class SignupForm {
  @Required() email!: string;
  @Required() password!: string;
}
```

A property decorator cannot change the property by returning a value. It can only record metadata or define accessors on the prototype. This is how `class-validator` works: decorators like `@IsEmail()` record rules, and a separate function reads them later.

### Parameter Decorator

```typescript
function Body(): ParameterDecorator {
  return (target, propertyKey, parameterIndex) => {
    const params = Reflect.getMetadata('params', target, propertyKey!) ?? [];
    params.push({ index: parameterIndex, kind: 'body' });
    Reflect.defineMetadata('params', params, target, propertyKey!);
  };
}

class UsersController {
  create(@Body() dto: unknown) {}
}
```

A parameter decorator on its own does nothing to the call. It records "parameter 0 of `create` is the request body". The framework reads that record and supplies the argument when the route is invoked. This is exactly what `@Body()`, `@Param()`, and `@Query()` do in NestJS.

### Decorator Factories

A decorator that takes arguments is a **factory**: a function that returns the decorator.

```typescript
function Get(path = ''): MethodDecorator {   // factory
  return (target, propertyKey) => {          // the actual decorator
    Reflect.defineMetadata('route', { method: 'GET', path }, target, propertyKey);
  };
}

class A {
  @Get('/list')   // Get('/list') is called first, returns the decorator
  list() {}
}
```

NestJS decorators are nearly always written with parentheses (`@Injectable()`, `@Controller('cats')`) because they are factories, even when they take no arguments.

### Composing and Evaluation Order

When decorators are stacked:

```typescript
@A()
@B()
class X {}
```

1. **Factories are evaluated top to bottom**: `A()` then `B()`.
2. **The resulting decorators are applied bottom to top**: `B`'s decorator, then `A`'s. This is function composition: `A(B(X))`.

Across a whole class, TypeScript's legacy model applies decorators in this order:

```text
1. For each INSTANCE member:   parameter decorators, then method/accessor/property decorator
2. For each STATIC member:     parameter decorators, then method/accessor/property decorator
3. Constructor parameter decorators
4. Class decorators
```

Practical consequence: **a class decorator runs after its members' decorators**, so by the time `@Controller()` executes, `@Get()` metadata on its methods already exists.

### Reflection and `reflect-metadata`

`reflect-metadata` is a library (a polyfill for the proposed `Reflect.metadata` API) that lets you attach key-value data to any object or property **without modifying the object itself**. NestJS depends on it and imports it internally.

```typescript
import 'reflect-metadata';

class A { foo() {} }

Reflect.defineMetadata('role', 'admin', A.prototype, 'foo');
Reflect.getMetadata('role', A.prototype, 'foo');        // 'admin'
Reflect.hasMetadata('role', A.prototype, 'foo');        // true
Reflect.getMetadataKeys(A.prototype, 'foo');            // ['role']
Reflect.getOwnMetadata('role', A.prototype, 'foo');     // only on this object, not its prototype chain
```

| API | Purpose |
|---|---|
| `Reflect.defineMetadata(key, value, target, propertyKey?)` | Store a value |
| `Reflect.getMetadata(key, target, propertyKey?)` | Read a value (walks the prototype chain) |
| `Reflect.getOwnMetadata(key, target, propertyKey?)` | Read a value defined directly on `target` |
| `Reflect.hasMetadata(key, target, propertyKey?)` | Check existence (walks the chain) |
| `Reflect.getMetadataKeys(target, propertyKey?)` | List keys |
| `Reflect.deleteMetadata(key, target, propertyKey?)` | Remove a value |
| `@Reflect.metadata(key, value)` | Decorator form of `defineMetadata` |

Metadata is stored in an internal `WeakMap` keyed by target and property. It does not appear on the object, so `Object.keys` and JSON serialization are unaffected.

### Compiler-Emitted Metadata (`emitDecoratorMetadata`)

When `emitDecoratorMetadata` is on, TypeScript emits three extra metadata entries **for declarations that have at least one decorator**:

| Key | Emitted for | Value |
|---|---|---|
| `design:type` | Properties, accessors, methods | The declared type's runtime constructor |
| `design:paramtypes` | Class constructors and methods | Array of parameter types |
| `design:returntype` | Methods | The return type |

```typescript
@Injectable()
class UsersService {
  constructor(private readonly repo: UsersRepository, private readonly logger: Logger) {}
}
```

The compiler adds (conceptually):

```javascript
Reflect.defineMetadata('design:paramtypes', [UsersRepository, Logger], UsersService);
```

**This one line is the foundation of NestJS dependency injection.** The DI container reads `design:paramtypes` to learn what to inject.

Type-to-runtime mapping (this explains many surprises):

| Declared TypeScript type | Emitted value |
|---|---|
| `string` | `String` |
| `number` | `Number` |
| `boolean` | `Boolean` |
| `Date` | `Date` |
| `Array<T>` / `T[]` | `Array` (element type is lost) |
| A `class` | The class itself |
| `interface`, type alias, union, intersection, generic parameter, `any`, `unknown`, object literal type | `Object` |
| `Promise<T>` (as a return type) | `Promise` |
| `void`, `undefined`, `null` | `undefined` (or `void 0`) |
| A function type | `Function` |

Two critical consequences:

1. **Interfaces become `Object`.** The container cannot tell which interface you meant, so interfaces cannot be injected by type. Use a class or an explicit token with `@Inject('TOKEN')`.
2. **Array element types are lost.** `class-validator` needs `@ValidateNested({ each: true })` and `@Type(() => Item)` to know what the items are.

---

## How It Works

```text
Compile time (tsc)                          Runtime (Node.js, at module load)
──────────────────                          ──────────────────────────────────
@Injectable()                               1. Class body evaluated
class UsersService {                        2. Member decorators run (params, methods, props)
  constructor(repo: UsersRepository) {}     3. Compiler-emitted __metadata("design:paramtypes",
}                                              [UsersRepository]) is registered
        │                                   4. Class decorators run (@Injectable)
        │ tsc emits:                        5. Metadata is now stored for later
        ▼
 UsersService = __decorate([
   Injectable(),
   __metadata("design:paramtypes", [UsersRepository])
 ], UsersService);

Application bootstrap (later)
──────────────────────────────
Container reads design:paramtypes → resolves each dependency → calls `new UsersService(...)`
```

`__decorate` is a helper TypeScript emits. It applies the decorators array **from the last element to the first**, which is why decorators apply bottom to top.

---

## Basic Example

A complete, runnable example: a property decorator that records metadata, and a function that reads it.

```typescript
// basic-decorator.ts
import 'reflect-metadata';

const REQUIRED_KEY = Symbol('required');

function Required(): PropertyDecorator {
  return (target, propertyKey) => {
    const fields: (string | symbol)[] = Reflect.getMetadata(REQUIRED_KEY, target) ?? [];
    Reflect.defineMetadata(REQUIRED_KEY, [...fields, propertyKey], target);
  };
}

function validate(obj: object): string[] {
  const fields: string[] = Reflect.getMetadata(REQUIRED_KEY, Object.getPrototypeOf(obj)) ?? [];
  return fields.filter((f) => (obj as Record<string, unknown>)[f] == null);
}

class SignupForm {
  @Required() email?: string;
  @Required() password?: string;
  nickname?: string;
}

const form = new SignupForm();
form.email = 'a@b.c';
console.log(validate(form));   // ['password']
```

```bash
npm install reflect-metadata
npm install --save-dev typescript ts-node @types/node
npx ts-node basic-decorator.ts
```

What happens:

1. When `SignupForm` is defined, `@Required()` runs for `email` and `password`, appending each name to a list stored as metadata on `SignupForm.prototype`.
2. Nothing happens to `email` or `password` themselves.
3. `validate(form)` retrieves the list from the prototype and checks which fields are missing. This is the core design of `class-validator`.

---

## Practical Examples

### 1. Basic: Method Decorator That Measures Time

```typescript
function Timed(): MethodDecorator {
  return (_target, propertyKey, descriptor: PropertyDescriptor) => {
    const original = descriptor.value as (...args: unknown[]) => unknown;
    descriptor.value = async function (this: unknown, ...args: unknown[]) {
      const start = performance.now();
      try {
        return await original.apply(this, args);
      } finally {
        console.log(`${String(propertyKey)} took ${(performance.now() - start).toFixed(1)}ms`);
      }
    };
    return descriptor;
  };
}

class ReportService {
  @Timed()
  async build() { /* ... */ }
}
```

Note that the wrapper uses `function`, not an arrow, to keep `this`, and it awaits the original so asynchronous methods are timed correctly.

### 2. Common: Mini Routing Framework (how `@Controller` and `@Get` work)

```typescript
import 'reflect-metadata';

const PREFIX = Symbol('prefix');
const ROUTES = Symbol('routes');

type Route = { method: 'GET' | 'POST'; path: string; handler: string | symbol };

const Controller = (prefix: string): ClassDecorator => (target) => {
  Reflect.defineMetadata(PREFIX, prefix, target);
};

const makeMethod = (method: Route['method']) => (path = ''): MethodDecorator =>
  (target, propertyKey) => {
    const routes: Route[] = Reflect.getMetadata(ROUTES, target.constructor) ?? [];
    routes.push({ method, path, handler: propertyKey });
    Reflect.defineMetadata(ROUTES, routes, target.constructor);
  };

const Get = makeMethod('GET');
const Post = makeMethod('POST');

@Controller('/users')
class UsersController {
  @Get()          list()   { return ['ada', 'linus']; }
  @Get('/:id')    one()    { return 'ada'; }
  @Post()         create() { return 'created'; }
}

// What the framework does at bootstrap:
function scan(controller: new () => object) {
  const prefix: string = Reflect.getMetadata(PREFIX, controller);
  const routes: Route[] = Reflect.getMetadata(ROUTES, controller) ?? [];
  for (const r of routes) {
    console.log(`${r.method} ${prefix}${r.path} -> ${String(r.handler)}`);
  }
}

scan(UsersController);
// GET /users -> list
// GET /users/:id -> one
// POST /users -> create
```

Everything the framework knows about your routes comes from metadata written by decorators. NestJS performs the same scan with additional bookkeeping.

### 3. Real-World: A 40-Line Dependency Injection Container

```typescript
import 'reflect-metadata';

type Constructor<T = unknown> = new (...args: any[]) => T;
const INJECTABLE = Symbol('injectable');

function Injectable(): ClassDecorator {
  return (target) => Reflect.defineMetadata(INJECTABLE, true, target);
}

class Container {
  private readonly instances = new Map<Constructor, unknown>();

  resolve<T>(target: Constructor<T>): T {
    if (!Reflect.getMetadata(INJECTABLE, target)) {
      throw new Error(`${target.name} is not marked @Injectable()`);
    }
    if (this.instances.has(target)) return this.instances.get(target) as T;

    const paramTypes: Constructor[] = Reflect.getMetadata('design:paramtypes', target) ?? [];
    const deps = paramTypes.map((dep, i) => {
      if (dep === undefined || dep === Object) {
        throw new Error(`Cannot resolve parameter #${i} of ${target.name}`);
      }
      return this.resolve(dep);
    });

    const instance = new target(...deps);
    this.instances.set(target, instance);   // singleton scope
    return instance;
  }
}

@Injectable()
class Logger { log(m: string) { console.log(m); } }

@Injectable()
class UsersRepository { find() { return [{ id: 1 }]; } }

@Injectable()
class UsersService {
  constructor(private readonly repo: UsersRepository, private readonly logger: Logger) {}
  list() { this.logger.log('listing'); return this.repo.find(); }
}

const container = new Container();
console.log(container.resolve(UsersService).list());
```

Explanation:

- `@Injectable()` marks the class. The compiler emits `design:paramtypes` for `UsersService` **because** it has a decorator.
- `resolve` reads the constructor's parameter types, recursively resolves each, and calls `new`.
- The error branch catches `Object` (an interface or union was used) and `undefined` (a circular import left the class undefined at evaluation time). These are the same situations that produce Nest's *"Nest can't resolve dependencies of the X (?)"* error.

NestJS adds modules (visibility rules), scopes, custom providers, async providers, and circular-dependency handling on top of this core idea. See [DI Internals](../06-internals/02-dependency-injection-internals.md).

### 4. Edge Case: Decorator Order Matters

```typescript
function Tag(name: string): MethodDecorator {
  console.log(`factory ${name}`);
  return () => console.log(`apply ${name}`);
}

class X {
  @Tag('A')
  @Tag('B')
  run() {}
}
// factory A
// factory B
// apply B
// apply A
```

Why it matters in NestJS: `@UseGuards(A, B)` runs `A` before `B`, but stacked method decorators that wrap the function (like `@Log()` above `@Timed()`) wrap in bottom-to-top order, so the one closest to the method is the innermost wrapper.

### 5. Edge Case: Decorating Without Compiler Metadata

```typescript
class Plain {                       // no decorator on the class
  constructor(private readonly x: SomeClass) {}
}
Reflect.getMetadata('design:paramtypes', Plain);   // undefined
```

`design:paramtypes` is only emitted when the declaration has **a decorator**. This is why Nest requires `@Injectable()` on providers even when it has no arguments or behavior of its own.

---

## Syntax / API / Commands

### Decorator Signatures (Legacy)

```typescript
type ClassDecorator     = <TFunction extends Function>(target: TFunction) => TFunction | void;
type MethodDecorator    = <T>(target: Object, key: string | symbol, descriptor: TypedPropertyDescriptor<T>) => TypedPropertyDescriptor<T> | void;
type PropertyDecorator  = (target: Object, key: string | symbol) => void;
type ParameterDecorator = (target: Object, key: string | symbol | undefined, index: number) => void;
```

### Configuration

| Option | Required value | Effect |
|---|---|---|
| `experimentalDecorators` | `true` | Enables legacy decorator syntax and semantics |
| `emitDecoratorMetadata` | `true` | Emits `design:*` metadata for decorated declarations |
| `useDefineForClassFields` | Check carefully | Can change property initialization semantics. Defaults depend on `target`. *Verify for your setup* |

### Tooling Commands

```bash
npm install reflect-metadata              # not needed in a Nest app (Nest imports it)
npx tsc --showConfig | grep -i decorator  # confirm effective options
npx tsc --noEmit                          # catch decorator signature errors
```

To inspect the emitted `__decorate` / `__metadata` calls, compile a small file and read the `.js` output:

```bash
npx tsc --target ES2021 --module commonjs --experimentalDecorators --emitDecoratorMetadata example.ts
```

---

## Important Rules

1. **Decorators run at class-definition time**, not at call or instantiation time. Their side effects happen once at module load.
2. **Stacked decorators: factories evaluated top to bottom, decorators applied bottom to top.**
3. **Class decorators run last.** Member and parameter metadata is already present.
4. **Parameter and property decorators cannot change the value.** They can only record metadata (or define accessors).
5. **`design:paramtypes` is emitted only for decorated declarations.** Undecorated classes have no constructor metadata.
6. **Interfaces, unions, and generic type parameters become `Object`** in emitted metadata and cannot be used for injection.
7. **A circular import can make a dependency `undefined`** at the moment metadata is emitted.
8. **`import type` erases the import**, so any class whose runtime reference you need (for metadata) must use a regular `import`.
9. **Use `function`, not arrow functions,** when your wrapper needs `this` from the instance.
10. **Legacy and standard decorators are incompatible.** A library written for one does not work in a project configured for the other.

---

## Under the Hood

### What `tsc` Emits

For:

```typescript
@Injectable()
class UsersService {
  constructor(private readonly repo: UsersRepository) {}

  @Get()
  find(@Query('q') q: string): Promise<User[]> { /* ... */ }
}
```

TypeScript emits (simplified):

```javascript
let UsersService = class UsersService {
  constructor(repo) { this.repo = repo; }
  find(q) { /* ... */ }
};

__decorate([
  Get(),
  __param(0, Query('q')),
  __metadata("design:type", Function),
  __metadata("design:paramtypes", [String]),
  __metadata("design:returntype", Promise)
], UsersService.prototype, "find", null);

UsersService = __decorate([
  Injectable(),
  __metadata("design:paramtypes", [UsersRepository])
], UsersService);
```

- `__decorate(decorators, target, key, desc)` iterates the array **from last to first**, passing the result of each into the next.
- `__param(index, decorator)` adapts a parameter decorator into a method-style decorator that receives the parameter index.
- `__metadata(key, value)` is a decorator that calls `Reflect.metadata(key, value)`.
- Note the order inside the array: `Get()`, then `__param`, then metadata. Applied bottom-to-top, the metadata is written first and `Get()` last.

### Where Metadata Lives

`reflect-metadata` keeps a global `WeakMap<object, Map<propertyKey, Map<metadataKey, value>>>`. Because it is a `WeakMap`, metadata does not prevent garbage collection of the target. The registry is attached to a single shared global, so having **two copies** of `reflect-metadata` in `node_modules` can lead to unexpected duplicates. Prefer one version via your package manager's deduplication.

### How NestJS Uses This

| Nest decorator | What it records | Who reads it |
|---|---|---|
| `@Injectable()` | Marker + triggers `design:paramtypes` emission | DI container |
| `@Module({...})` | `imports`, `providers`, `controllers`, `exports` stored under internal keys | Module scanner |
| `@Controller('x')` | Path prefix | Router explorer |
| `@Get()`, `@Post()` | HTTP method + path | Router explorer |
| `@Body()`, `@Param()` | Parameter index to source mapping | Router param factory |
| `@UseGuards()`, `@UsePipes()` | Enhancer classes to apply | Execution context creator |
| `@SetMetadata(key, value)` | Arbitrary key/value | `Reflector` in guards/interceptors |
| `@Inject(token)` | Explicit injection token for a parameter | DI container |
| `@Optional()` | Marks a dependency optional | DI container |

See [Metadata and Reflection](../06-internals/03-metadata-and-reflection.md) and [Custom Decorators](../03-core-concepts/01-request-pipeline/10-custom-decorators.md).

---

## Common Patterns

### Metadata-Only Decorators (the dominant pattern)

Record data. Let another component act on it.

```typescript
export const ROLES_KEY = 'roles';
export const Roles = (...roles: string[]) => SetMetadata(ROLES_KEY, roles);
```

A guard later reads the roles through `Reflector`. Prefer this pattern, because it keeps decorators pure and testable.

### Wrapping Decorators

Replace a method to add behavior (logging, timing, caching, retry). Use when behavior must run around the method call. Be careful with `this`, async methods, and preserving function metadata.

### Decorator Composition

Combine several decorators into one reusable decorator:

```typescript
import { applyDecorators } from '@nestjs/common';

export function Auth(...roles: string[]) {
  return applyDecorators(
    SetMetadata('roles', roles),
    UseGuards(AuthGuard, RolesGuard),
  );
}
```

`applyDecorators` is a NestJS helper. Use composition when the same set of decorators repeats across many handlers.

### Parameter Decorator Factories

```typescript
export const CurrentUser = createParamDecorator(
  (data: unknown, ctx: ExecutionContext) => ctx.switchToHttp().getRequest().user,
);
```

`createParamDecorator` is a NestJS helper. It handles the parameter-index bookkeeping shown earlier.

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Missing `experimentalDecorators` | `TS1240` / `TS1241` / `TS1270`-style decorator signature errors | TypeScript 5+ treats `@x` as a standard decorator | Set `experimentalDecorators: true` |
| Missing `emitDecoratorMetadata` | Nest: *"can't resolve dependencies of X (?)"* | `design:paramtypes` not emitted | Set `emitDecoratorMetadata: true` |
| Forgetting `@Injectable()` | Constructor deps `undefined` / unresolved | No decorator, so no metadata emitted | Add `@Injectable()` |
| Injecting an interface | *"can't resolve dependencies... Object"* | Interfaces emit `Object` | Use a class, or `@Inject('TOKEN')` with a custom provider |
| `import type { UsersRepository }` for injection | Dependency unresolved | Type-only import is erased, metadata emits `Object` | Use a regular `import` |
| Circular file imports | Dependency `undefined` at decoration time | One module evaluates before the other finishes | Break the cycle. Use `forwardRef(() => X)` as a last resort |
| Arrow function in a wrapping method decorator | `this` is `undefined` or wrong | Arrow functions capture lexical `this` | Use `function` and `.apply(this, args)` |
| Assuming a decorator runs per call | Side effect only logs once | Decorator body runs at definition time. Only the wrapper runs per call | Put per-call logic inside the returned wrapper |
| Mixing standard and legacy decorators | Signature mismatch, library silently breaks | Two incompatible models | Keep one model per project, follow the library's docs |
| Using SWC/esbuild/Vite without metadata support | DI fails only in tests or after build-tool change | Some transformers do not emit `design:*` metadata | Use `tsc`/`ts-jest`, or configure SWC with decorator metadata enabled |
| Array of classes typed as `Item[]` for validation | Nested items not validated | Element type is lost (`Array`) | Add `@Type(() => Item)` and `@ValidateNested({ each: true })` |
| Two copies of `reflect-metadata` | Metadata appears missing | Different registries | Deduplicate dependency versions |

---

## Debugging

### Common Errors

| Error | Likely cause |
|---|---|
| `Nest can't resolve dependencies of the UsersService (?). Please make sure that the argument UsersRepository at index [0] is available in the UsersModule context.` | Provider not registered/exported, interface used, or metadata missing |
| `TypeError: Reflect.getMetadata is not a function` | `reflect-metadata` was not imported before the first decorator ran |
| `TS1240: Unable to resolve signature of property decorator` | Wrong decorator signature or wrong decorator option set |
| `TS1272: A type referenced in a decorated signature must be imported with 'import type' or a namespace import when 'isolatedModules' and 'emitDecoratorMetadata' are enabled.` | Under `isolatedModules`, importing a *type-only* symbol with a normal import and using it in a decorated signature. Use `import type` **for interfaces/types**, and a normal import **for classes** |

> The `(?)` in Nest's error marks the parameter that resolved to `undefined`/unresolvable. The index tells you which constructor argument to inspect.

### Inspect Metadata Directly

```typescript
import 'reflect-metadata';

console.log(Reflect.getMetadata('design:paramtypes', UsersService));
// [ [class UsersRepository], [class Logger] ]  → healthy
// undefined                                     → no decorator or option disabled
// [ [Function: Object] ]                        → interface/union/any used
```

```typescript
console.log(Reflect.getMetadataKeys(UsersController.prototype, 'find'));
```

### Techniques

1. **Check the compiled output.** Open `dist/.../users.service.js` and look for `__metadata("design:paramtypes", [...])`. If it is missing, the compiler is not emitting metadata.
2. **Confirm the effective config:** `npx tsc --showConfig`.
3. **Check the test tool.** If the app works but tests fail, the test transformer is the usual cause (see mistakes table).
4. **Look for `undefined` in the array.** `[undefined]` points to a circular import.
5. **Bisect imports.** Temporarily replace the failing dependency's file with a minimal class to confirm whether the import graph, not your logic, is at fault.

---

## Performance

- Decorators run once at module load. Their cost is a startup cost, not a per-request cost.
- **Wrapping decorators add a call layer per invocation.** Negligible individually, but stacks of wrappers on hot paths add overhead and deepen stack traces.
- Metadata lookups (`Reflect.getMetadata`) walk the prototype chain. Frameworks cache results. If you read metadata per request in custom code, cache it at startup.
- Large applications with thousands of decorated members will see slower startup. Measure with `node --cpu-prof` before optimizing.
- Decorators that capture large closures can retain memory. Keep state in metadata or module-level singletons, not per-decorator closures that grow.

---

## Security

- **Decorators are code execution at import time.** Only import decorator libraries you trust.
- **Authorization decorators express intent, not enforcement.** `@Roles('admin')` records metadata. Nothing is protected unless a guard that reads it is also registered. A missing guard means an open endpoint.
- **Default-secure configuration beats opt-in.** Prefer a global guard with an explicit `@Public()` opt-out to per-route `@UseGuards`. This avoids forgetting a decorator on a new route.
- **Do not trust decorator metadata for data from the client.** Metadata describes your code, not user input.
- **Validation decorators describe rules.** Nothing is validated until `ValidationPipe` (or an equivalent) runs.

---

## Production Considerations

- Keep decorator metadata free of secrets and request-specific data. Metadata is global and shared across requests.
- Ensure your **production build** emits decorator metadata (verify `dist/` output once after any toolchain change).
- If you adopt SWC or another transformer for speed, confirm `decoratorMetadata`-style options are enabled, and keep `tsc --noEmit` in CI for type checking.
- Avoid clever wrapping decorators on request paths that make stack traces hard to read. Use named functions inside wrappers to keep traces useful.
- Test custom decorators directly (call the decorator against a test class, read the metadata) and through the component that consumes them.
- Plan for the eventual move to standard decorators. Prefer metadata-only decorators and Nest's helpers (`SetMetadata`, `createParamDecorator`, `applyDecorators`) so your code depends on stable Nest APIs rather than raw decorator signatures.

---

## Best Practices

### Recommended

```typescript
export const IS_PUBLIC_KEY = 'isPublic';
export const Public = () => SetMetadata(IS_PUBLIC_KEY, true);

// A guard reads it with Reflector:
const isPublic = this.reflector.getAllAndOverride<boolean>(IS_PUBLIC_KEY, [
  context.getHandler(),
  context.getClass(),
]);
```

```typescript
export function Log(): MethodDecorator {
  return (_t, key, descriptor: PropertyDescriptor) => {
    const original = descriptor.value;
    descriptor.value = async function (this: unknown, ...args: unknown[]) {
      return original.apply(this, args);
    };
    return descriptor;
  };
}
```

### Avoid

```typescript
// Logic that depends on instance state inside the decorator body
export function Cached(): MethodDecorator {
  const cache = new Map();                  // shared across ALL instances and requests
  return (_t, _k, d: PropertyDescriptor) => { /* ... */ };
}

// Arrow function losing `this`
descriptor.value = (...args: unknown[]) => original.apply(this, args);

// Interface as an injection type
constructor(private readonly repo: UserRepositoryInterface) {}
```

Why: shared mutable state in a decorator is global state. Arrow functions break `this`. Interfaces carry no runtime type for the container.

Additional guidance:

- Write **metadata-only** decorators when possible. Keep behavior in guards, interceptors, and pipes.
- Export metadata keys as constants so decorators and readers cannot drift apart.
- Use `Symbol` or namespaced strings for custom metadata keys to avoid collisions.
- Name decorators in PascalCase (`@Roles`, `@CurrentUser`).
- Keep each decorator single-purpose and compose them with `applyDecorators`.

---

## Version / Compatibility Notes

| Version | Behavior |
|---|---|
| TypeScript < 5.0 | Only the legacy model, enabled with `experimentalDecorators`. |
| TypeScript 5.0 | Adds **standard (TC39) decorators**, active when `experimentalDecorators` is **not** set. Signature is `(value, context)`. No parameter decorators. No `emitDecoratorMetadata` support for this model. |
| TypeScript 5.2 | Added support for the decorator metadata proposal (`context.metadata`, `Symbol.metadata`). *Verify exact version in the release notes.* |
| NestJS (current stable lines) | Uses the **legacy** model with `emitDecoratorMetadata`. *Verify against the official Nest documentation for your installed version, since the ecosystem may migrate.* |

| | Legacy (`experimentalDecorators`) | Standard (TS 5.0+) |
|---|---|---|
| Signature | Target-specific: `(target, key, descriptor)` etc. | `(value, context)` |
| Parameter decorators | Yes | No |
| `design:*` metadata (`emitDecoratorMetadata`) | Yes | No |
| Used by NestJS, TypeORM, class-validator, Angular (historically) | Yes | Not as the primary model |
| Spec status | Pre-standard, TypeScript-specific | TC39 proposal (verify current stage) |

Because the two models differ so much, **do not mix them in one project**, and expect Nest ecosystem libraries to follow Nest's configuration.

Toolchain notes (*verify for your versions*):

- `tsc` and `ts-jest` support legacy decorators with metadata.
- SWC supports metadata emission when configured (`jsc.transform.decoratorMetadata`).
- esbuild does not implement `emitDecoratorMetadata`. Vite/Vitest-based setups need a plugin that does (commonly an SWC-based one).

---

## Real-World Use Cases

- **NestJS** wiring: modules, controllers, providers, guards, pipes, interceptors.
- **Validation:** `class-validator` rules on DTOs.
- **Serialization:** `class-transformer` (`@Expose`, `@Exclude`, `@Type`).
- **ORMs:** TypeORM `@Entity`, `@Column`, `@ManyToOne`; Mongoose via `@nestjs/mongoose` `@Schema`, `@Prop`.
- **API documentation:** `@nestjs/swagger` `@ApiProperty`, `@ApiTags`.
- **Cross-cutting concerns:** logging, caching, retry, rate limiting, permission checks.
- **Other ecosystems:** Angular components/services, MobX stores, TypeDI/InversifyJS containers.

---

## Interview Questions

### Beginner

1. What is a decorator?
   - A function applied with `@` that runs against a class, method, property, accessor, or parameter when the class is defined, to add metadata or modify behavior.
2. What is a decorator factory?
   - A function that returns a decorator, letting you pass arguments (`@Controller('users')`).
3. When does a decorator run: at class definition or at method call?
   - At class definition (module load). Only a wrapper it installs runs per call.
4. Which `tsconfig.json` options does NestJS need for decorators?
   - `experimentalDecorators` and `emitDecoratorMetadata`.

### Intermediate

1. In what order are stacked decorators evaluated and applied?
   - Factories evaluated top to bottom, resulting decorators applied bottom to top.
2. What is `reflect-metadata` and what does it add?
   - A polyfill for the `Reflect` metadata API that stores key/value data on objects and properties without modifying them.
3. What does `emitDecoratorMetadata` emit, and when?
   - `design:type`, `design:paramtypes`, `design:returntype`, only for declarations that have at least one decorator.
4. Why can't an interface be injected in NestJS by type?
   - Interfaces are erased and emit `Object`. Use a class or an explicit token.
5. Why does NestJS require `@Injectable()` even on a class with no logic of its own?
   - It triggers metadata emission for constructor parameters and marks the class as a provider.

### Advanced

1. Walk through how `design:paramtypes` is used to build a service instance.
   - The container reads the array, recursively resolves each dependency (cached for singletons), then calls `new`.
2. What differs between legacy and standard decorators, and why does it matter for NestJS?
   - Different signatures, no parameter decorators and no `design:*` metadata in the standard model. Nest depends on both, so it remains on legacy.
3. How can a circular import cause a dependency to be `undefined` in `design:paramtypes`?
   - Metadata is emitted while the class body evaluates. If the imported module has not finished evaluating, the binding is still `undefined`.
4. How would you implement `@Roles('admin')` and the enforcement for it?
   - `SetMetadata('roles', ['admin'])` records data. A guard reads it via `Reflector.getAllAndOverride` and compares with the user's roles.
5. Why might DI work with `tsc` but fail under a Vite/esbuild-based test runner?
   - The transformer does not emit `design:*` metadata. Use a plugin or runner that supports it.

---

## Quick Reference

```text
Class decorator       (constructor)                      → runs last
Method decorator      (target, key, descriptor)          → may return new descriptor
Property decorator    (target, key)                      → record metadata only
Parameter decorator   (target, key, index)               → record metadata only
Factory               @Name(args) → returns decorator

Stacking              factories top→bottom, applied bottom→top
Runs when             class definition (module load)

reflect-metadata      defineMetadata / getMetadata / getOwnMetadata / hasMetadata
design:paramtypes     constructor/method param types  (only if decorated)
design:type           property / accessor type
design:returntype     method return type

Emitted type map      class→class, string→String, number→Number, interface/union/any→Object
Required tsconfig     experimentalDecorators: true, emitDecoratorMetadata: true
Nest helpers          SetMetadata, createParamDecorator, applyDecorators, Reflector
```

---

## Key Takeaways

- A decorator is a function that runs once when a class is defined. It usually records metadata rather than doing work.
- Nest wiring (DI, routing, guards, validation) is built from metadata that decorators write and framework components read.
- `emitDecoratorMetadata` makes the compiler emit constructor parameter types. That is what makes `constructor(private repo: Repo)` injectable.
- Metadata is emitted only for decorated declarations, so `@Injectable()` is required.
- Interfaces, unions, and `any` become `Object`, and `import type` erases the reference, so neither can serve as injection types.
- Stacked decorators: factories evaluated top to bottom, applied bottom to top. Class decorators run last.
- Legacy (`experimentalDecorators`) and standard (TypeScript 5+) decorators are different systems. NestJS uses the legacy one.
- Prefer metadata-only decorators and Nest's helpers (`SetMetadata`, `applyDecorators`, `createParamDecorator`).

---

## Related Topics

```text
01 TypeScript Essentials  (classes, types vs values, tsconfig)
      ↓
[02 TypeScript Decorators]
      ↓
02-fundamentals/06 Decorators  →  02-fundamentals/05 Dependency Injection
      ↓
03-core-concepts/01 Custom Decorators  →  06-internals/03 Metadata and Reflection
```

- [Prerequisites Overview](./README.md)
- [TypeScript Essentials](./01-typescript-essentials.md)
- [Nest Decorators](../02-fundamentals/06-decorators.md)
- [Dependency Injection](../02-fundamentals/05-dependency-injection.md)
- [Custom Decorators](../03-core-concepts/01-request-pipeline/10-custom-decorators.md)
- [Metadata and Reflection (internals)](../06-internals/03-metadata-and-reflection.md)
- [Dependency Injection Internals](../06-internals/02-dependency-injection-internals.md)
