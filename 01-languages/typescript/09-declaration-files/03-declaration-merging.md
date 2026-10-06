# Declaration Merging

When TypeScript sees **two or more declarations with the same name** in the same scope, it sometimes *merges* them into one definition instead of reporting a duplicate. This is the mechanism behind module augmentation, behind typing functions that have properties, and behind the difference between `interface` and `type`. Knowing which combinations merge, and how, saves a lot of confusion.

**Prerequisites:**
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md)
- [Interface vs type](../04-objects-and-interfaces/01-interface-vs-type.md)
- [Namespaces](../08-modules/03-namespaces.md)

---

## What merges with what

A declaration creates one or more of three things: a **type**, a **value**, or a **namespace**. Merging is allowed when the declarations occupy compatible slots.

| Combination | Merges? | Notes |
|---|---|---|
| interface + interface | yes | the common case |
| namespace + namespace | yes | exported members combine |
| namespace + class | yes | namespace must come **after** the class |
| namespace + function | yes | namespace must come **after** the function |
| namespace + enum | yes | |
| enum + enum | yes | only for non-`const` enums, and only one declaration may omit an initializer on its first member |
| class + interface | yes | the interface members are added to the class instance type |
| type alias + anything of the same name in the type slot | **no** | "Duplicate identifier" |
| class + class | **no** | |
| function + function | overloads, not a merge | |

The important consequence: **interfaces are open, type aliases are closed.** You can add to an interface from another file. You cannot add to a type alias.

## Interface merging

```ts
interface User {
  id: number;
}

interface User {
  name: string;
}

const u: User = { id: 1, name: "Asha" };   // both properties required
```

Rules:

- **Properties** with the same name must have the **same type**, otherwise: "Subsequent property declarations must have the same type" (TS2717).
- **Methods** with the same name become **overloads** of that method.
- Non-function members must be unique, or have identical types.

### Overload ordering

When overloads merge, **later declarations come first**. Within one declaration the order is preserved. There is one exception: signatures with a parameter of a single string-literal type are moved to the very top.

```ts
interface Cloner { clone(animal: Animal): Animal; }
interface Cloner { clone(animal: Sheep): Sheep; }
interface Cloner { clone(animal: Dog): Dog; clone(animal: Cat): Cat; }

// merged as:
// clone(animal: Dog): Dog;
// clone(animal: Cat): Cat;
// clone(animal: Sheep): Sheep;
// clone(animal: Animal): Animal;
```

The order matters because the compiler picks the **first matching** overload. A more specific signature declared in an earlier file ends up *after* later ones and may never be chosen. See [function overloads](../02-functions/03-function-overloads.md).

## Namespace merging

Multiple `namespace` blocks with the same name combine. Only `export`ed members are visible across blocks:

```ts
namespace Animals {
  export class Dog {}
}
namespace Animals {
  export class Cat {}
}

new Animals.Dog();
new Animals.Cat();
```

### Merging a namespace with a function

A function can carry properties. This is the standard way to type "a callable that also has members":

```ts
function counter(start: number) {
  return start;
}

namespace counter {
  export let calls = 0;
  export const defaultStart = 0;
}

counter(1);
counter.calls;
```

The namespace must be declared **after** the function (and in the same file for non-ambient code). Otherwise: "A namespace declaration cannot be located prior to a class or function with which it is merged" (TS2434).

### Merging a namespace with a class

This adds static-like members, including nested types:

```ts
class Album {
  label = new Album.AlbumLabel();
}

namespace Album {
  export class AlbumLabel {}
}
```

### Merging a namespace with an enum

```ts
enum Color { Red, Green }

namespace Color {
  export function parse(s: string): Color {
    return s === "red" ? Color.Red : Color.Green;
  }
}
```

Because namespaces that contain values compile to runtime objects, these patterns are affected by the tooling caveats in [namespaces](../08-modules/03-namespaces.md#tooling-caveats) (per-file transpilers, `erasableSyntaxOnly`).

## Class and interface merging

An interface with the same name as a class adds members to the class's **instance type**:

```ts
class Greeter {
  name = "world";
}

interface Greeter {
  greet(): string;
}

Greeter.prototype.greet = function () {
  return `Hello ${this.name}`;
};

new Greeter().greet();   // typed as string
```

This is the foundation of the [mixins](../05-classes/06-mixins.md) pattern. The danger is that the compiler does **not** check that the merged members exist at runtime. In the example above, forgetting the `prototype.greet` assignment compiles fine and fails when called.

## Where merging matters in practice

- **Module and global augmentation** are interface merging across files. See [global and module augmentation](./02-global-and-module-augmentation.md).
- **Library typings** use namespace + function/class merging to describe objects like `express()` (a function with properties) and `moment` style APIs.
- **Declaration files for plugins** extend a host's interface so that plugin options or methods appear in its types.

## `interface` vs `type`, reconsidered

Merging is the one semantic difference that matters in day-to-day use:

```ts
type Config = { debug: boolean };
type Config = { verbose: boolean };   // error: Duplicate identifier 'Config'

interface Settings { debug: boolean }
interface Settings { verbose: boolean }   // fine: merged
```

- If a type is **meant to be extended** by consumers (library options, plugin hooks, a global like `Window`), declare it as an `interface`.
- If you want to guarantee **no one can silently add to it**, use a `type` alias, or keep the interface inside a module and do not export it for augmentation.
- Accidental merging is a real source of bugs in global scripts: two files each declare `interface Options` and silently combine.

## Common mistakes

- **Declaring the namespace before the function or class it merges with** (TS2434).
- **Expecting type aliases to merge.** They do not.
- **Redeclaring a property with a different type** across merged interfaces (TS2717).
- **Merging an interface into a class without implementing the members.** Compiles, throws at runtime.
- **Relying on overload order across files.** Later declarations win position, so a specific overload added in an earlier-loaded file may be shadowed.
- **Merging into a `const enum` or mixing initializers** inconsistently across enum declarations.
- **Creating accidental global merges** by declaring a common name (`Options`, `Config`, `User`) in a script file. Make the file a module so its declarations stay local.

## Debugging

- "Duplicate identifier 'X'": the two declarations cannot merge (type alias involved, or class + class). Rename one, or convert aliases to interfaces.
- "Subsequent property declarations must have the same type": find the other declaration, usually in a `.d.ts` or `node_modules`, and align the type.
- To see what was merged, hover the name or use "Go to definition", which lists all declaration sites.
- To find hidden declarations, search for `interface X` across `node_modules/@types` and your own `.d.ts` files. Global names often collide with DOM or Node typings.
- Unexpected overload resolution: reorder the declarations, or put the most specific signatures in the **last** declaration so they come first.

## Quick summary

- Same-name declarations merge when their slots are compatible: interface + interface, namespace + namespace/class/function/enum, enum + enum, class + interface.
- Interfaces are open and type aliases are closed. That is the practical difference that matters.
- Merged methods become overloads, with later declarations first.
- A namespace must come after the class or function it merges with.
- Merging only affects types. The compiler does not check that merged members exist at runtime.
- This is the machinery behind augmentation.

**Next:** [Third-party types](./04-third-party-types.md)