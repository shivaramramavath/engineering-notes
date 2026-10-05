# Function and Class Utilities

These built-in utilities **extract types out of functions and classes**: parameter lists, return types, constructor arguments, instance types. They let you derive a type from the implementation instead of writing it twice, so the two cannot drift apart.

**Prerequisites:**
- [Function types](../02-functions/00-function-types.md)
- [Classes](../05-classes/00-classes.md)
- [keyof and typeof](../06-generics/03-keyof-and-typeof.md) (you need `typeof` to get a function's type from its value)
- [`infer`](../10-advanced-types/02-infer.md) (explains how these are implemented)

---

## At a glance

| Utility | Gives you |
|---|---|
| `Parameters<F>` | tuple of the parameter types |
| `ReturnType<F>` | the return type |
| `ConstructorParameters<C>` | tuple of constructor parameter types |
| `InstanceType<C>` | the type of `new C(...)` |
| `Awaited<T>` | the type a `Promise` resolves to (recursively) |
| `ThisParameterType<F>` / `OmitThisParameter<F>` | the `this` type of a function / the function without it |
| `ThisType<T>` | marker for the `this` type inside object literal methods |
| `NoInfer<T>` | blocks inference from a position (TS 5.4+) |

## How they work

They are conditional types with `infer`:

```ts
type Parameters<T extends (...args: any) => any> =
  T extends (...args: infer P) => any ? P : never;

type ReturnType<T extends (...args: any) => any> =
  T extends (...args: any) => infer R ? R : any;
```

TypeScript pattern-matches the function type and captures the piece you ask for.

## Parameters and ReturnType

```ts
function createUser(name: string, age: number, admin = false) {
  return { id: crypto.randomUUID(), name, age, admin };
}

type CreateArgs = Parameters<typeof createUser>;
// [name: string, age: number, admin?: boolean]

type CreatedUser = ReturnType<typeof createUser>;
// { id: string; name: string; age: number; admin: boolean }
```

Key points:

- **You need `typeof`.** `ReturnType<createUser>` is an error because `createUser` is a value, not a type.
- Labels and optional markers on the parameters are preserved in the tuple.
- Index into the tuple to get one parameter: `Parameters<typeof createUser>[0]` is `string`.
- `ReturnType` of an inferred return type tracks the implementation. Change the function body and the derived type updates.

### Wrapping functions generically

This is where these utilities earn their keep:

```ts
function withLogging<F extends (...args: any[]) => any>(fn: F) {
  return (...args: Parameters<F>): ReturnType<F> => {
    console.log("calling", fn.name, args);
    return fn(...args);
  };
}

const loggedCreate = withLogging(createUser);
loggedCreate("Asha", 30); // fully typed, same signature as createUser
```

### Methods and properties

Index into the type first, then extract:

```ts
class UserService {
  async getUser(id: string) {
    return { id, name: "Asha" };
  }
}

type GetUserResult = ReturnType<UserService["getUser"]>;
// Promise<{ id: string; name: string }>
```

### Overloads and generics

- For an **overloaded** function, these utilities use the **last** signature only.
- For a **generic** function, type parameters are replaced by their constraints (or `unknown`), so `ReturnType<typeof identity>` for `<T>(x: T) => T` is `unknown`.

See [function overloads](../02-functions/03-function-overloads.md).

## Awaited

`Awaited<T>` unwraps promises recursively, and also handles `PromiseLike` thenables. It is what `await` itself uses to type its result.

```ts
async function fetchUser() {
  return { id: 1, name: "Asha" };
}

type UserPromise = ReturnType<typeof fetchUser>;  // Promise<{ id: number; name: string }>
type User = Awaited<ReturnType<typeof fetchUser>>; // { id: number; name: string }

type A = Awaited<Promise<Promise<string>>>; // string
type B = Awaited<string | Promise<number>>; // string | number
```

`Awaited<ReturnType<typeof asyncFn>>` is the standard way to name the resolved value of an async function. `Awaited` was added in TS 4.5 and is also what types `Promise.all`. See [promises](../12-async-and-iteration/01-promises.md) and [async generics](../12-async-and-iteration/03-async-generics.md).

## ConstructorParameters and InstanceType

They take the **class value** (the constructor), so use `typeof`:

```ts
class Connection {
  constructor(public host: string, public port: number) {}
  close() {}
}

type ConnArgs = ConstructorParameters<typeof Connection>; // [host: string, port: number]
type Conn = InstanceType<typeof Connection>;              // Connection
```

A common use is a generic factory:

```ts
function create<C extends new (...args: any[]) => any>(
  Ctor: C,
  ...args: ConstructorParameters<C>
): InstanceType<C> {
  return new Ctor(...args);
}

const conn = create(Connection, "localhost", 5432); // Connection
```

Note the difference: the class name used as a **type** (`Connection`) is already the instance type. `InstanceType` matters mainly in generic code where you only have the constructor type.

## `this` utilities

Rarely needed day to day, but useful when working with functions that declare a `this` parameter (see [this parameters](../02-functions/04-this-parameters.md)):

```ts
function toHex(this: Number) {
  return this.toString(16);
}

type ThisT = ThisParameterType<typeof toHex>;  // Number
type NoThis = OmitThisParameter<typeof toHex>; // () => string
```

`ThisType<T>` is different: it is an empty marker interface that tells the compiler what `this` means inside the methods of an object literal. It only has an effect with `noImplicitThis` (part of `strict`). Frameworks such as Vue's options API relied on it.

```ts
type Counter = { count: number; inc(): void };

const counter: Counter & ThisType<Counter> = {
  count: 0,
  inc() {
    this.count++; // `this` is Counter
  },
};
```

## NoInfer

`NoInfer<T>` (TS 5.4) stops a position from contributing to inference of `T`. Use it when one argument should define `T` and the others must conform to it:

```ts
function createLight<C extends string>(colors: C[], initial?: NoInfer<C>) {}

createLight(["red", "green"], "red");  // ok
createLight(["red", "green"], "blue"); // error: "blue" is not "red" | "green"
```

Without `NoInfer`, TypeScript would widen `C` to include `"blue"` and accept the call.

## Intrinsic string utilities

`Uppercase<S>`, `Lowercase<S>`, `Capitalize<S>`, and `Uncapitalize<S>` transform string literal types. They are built into the compiler rather than defined in TypeScript, and they are mostly used with template literal types. See [template literal types](../10-advanced-types/04-template-literal-types.md).

```ts
type Getter<K extends string> = `get${Capitalize<K>}`;
type G = Getter<"name">; // "getName"
```

## Common mistakes

- **Writing `ReturnType<fn>` instead of `ReturnType<typeof fn>`.** Functions and classes are values. Use `typeof`.
- **Forgetting `Awaited` for async functions.** `ReturnType` of an `async` function is `Promise<T>`, not `T`.
- **Expecting overloads to be unioned.** Only the last overload is seen.
- **Relying on `ReturnType` of a generic function.** You get the constraint, not a specialization. Use an [instantiation expression](../06-generics/05-generic-inference.md) or restructure if you need a specific instantiation.
- **Extracting from `any`-heavy signatures.** `Parameters<(...args: any[]) => void>` is `any[]`, which is no protection at all.

## Debugging

- Hover the derived type, then hover the original function to compare. If they differ, check overloads and generics first.
- If you get "Type 'X' does not satisfy the constraint '(...args: any) => any'", you passed a value or a non-function type. Add `typeof`, or index into the object first.
- For `InstanceType` constraint errors, ensure you passed the class (`typeof Foo`), not an instance.

## Quick summary

- `Parameters` and `ReturnType` extract from functions. `ConstructorParameters` and `InstanceType` do the same for classes. All take the **type** of the function or class, so use `typeof`.
- Use `Awaited<ReturnType<typeof f>>` for async results.
- They are `infer`-based conditional types. Overloads use the last signature, and generics collapse to constraints.
- Deriving types from implementations keeps wrappers, mocks, and factories in sync automatically.
- `NoInfer` (TS 5.4+) controls inference. `ThisType` is a marker for object-literal `this`.

**Next:** [Building custom utility types](./04-building-custom-utility-types.md)
