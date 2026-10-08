# Factory

A **factory** is a function (or method) whose job is to create objects, so callers do not need to know *which* concrete thing they get or *how* it is built. In TypeScript, factories are usually plain functions rather than class hierarchies, and the type system can make them precise: the return type can depend on the argument, and a lookup table can force you to handle every kind.

**Prerequisites:**
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md)
- [Discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)
- [Generic functions](../06-generics/00-generic-functions.md)

---

## The problem

```ts
function notify(kind: string, message: string) {
  if (kind === "email") {
    const sender = new EmailSender(smtpConfig);
    sender.send(message);
  } else if (kind === "sms") {
    const sender = new SmsSender(twilioConfig);
    sender.send(message);
  }
  // every caller repeats this branching and knows every concrete class
}
```

Creation logic scattered through the code means callers depend on every concrete class, and adding a new kind means editing every call site.

## Simple factory: one function, one decision

Define the interface the callers need, then hide the choice behind a function:

```ts
interface Notifier {
  send(message: string): Promise<void>;
}

type NotifierKind = "email" | "sms" | "push";

function createNotifier(kind: NotifierKind): Notifier {
  switch (kind) {
    case "email": return new EmailNotifier(emailConfig);
    case "sms":   return new SmsNotifier(smsConfig);
    case "push":  return new PushNotifier(pushConfig);
  }
}

await createNotifier("email").send("Welcome!");
```

Callers depend only on `Notifier` and `NotifierKind`. Because `NotifierKind` is a union and the `switch` covers all cases, TypeScript knows every path returns a value. Add `"slack"` to the union and the function fails to compile ("not all code paths return a value") until you handle it. A `default` branch with a `never` check makes this explicit ([exhaustiveness checking](../03-unions-and-narrowing/06-exhaustiveness-checking.md)).

### A lookup table instead of a `switch`

```ts
const notifierFactories: Record<NotifierKind, () => Notifier> = {
  email: () => new EmailNotifier(emailConfig),
  sms:   () => new SmsNotifier(smsConfig),
  push:  () => new PushNotifier(pushConfig),
};

const createNotifier = (kind: NotifierKind): Notifier => notifierFactories[kind]();
```

`Record<NotifierKind, ...>` forces an entry for every kind, so a missing one is a compile error. Each entry is a function, so nothing is constructed until it is needed.

## A return type that depends on the argument

When different kinds produce different **specific** types, a plain `Notifier` return loses information. Map each key to its product type and use a generic:

```ts
interface ShapeMap {
  circle: { kind: "circle"; radius: number };
  rect:   { kind: "rect"; width: number; height: number };
}

type ShapeInit = {
  [K in keyof ShapeMap]: Omit<ShapeMap[K], "kind">;
};

function createShape<K extends keyof ShapeMap>(kind: K, init: ShapeInit[K]): ShapeMap[K] {
  return { kind, ...init } as ShapeMap[K];
}

const c = createShape("circle", { radius: 2 });             // { kind: "circle"; radius: number }
const r = createShape("rect", { width: 2, height: 3 });     // { kind: "rect"; width: number; height: number }
createShape("circle", { width: 2 });                         // error: wrong init for "circle"
```

The argument selects the init shape **and** the return type. The `as` cast inside is a contained assertion: TypeScript cannot follow the correlation between `K` and the spread, but the signature guarantees it ([soundness and escape hatches](../14-type-system-internals/04-soundness-and-escape-hatches.md)).

## Factory method and static factories

A **static factory method** on a class replaces the constructor as the public way to create instances. It gives you:

- **Names** that explain intent (`Money.fromCents(500)`, `Duration.seconds(5)`).
- **Validation** before an instance exists.
- **Async setup**, since a constructor cannot be `async`.
- **Control** over caching or reusing instances.

```ts
class Connection {
  private constructor(private readonly socket: Socket) {}

  static async open(url: string): Promise<Connection> {
    const socket = await connectSocket(url);       // async work a constructor cannot do
    return new Connection(socket);
  }
}

const conn = await Connection.open("wss://example.com");
```

The `private constructor` forces everyone through the factory, so a half-initialized `Connection` cannot exist. See [classes](../05-classes/00-classes.md) and [static members](../05-classes/04-static-members.md).

For creation that can **fail**, return a result instead of throwing ([the Result pattern](../11-error-handling/02-result-pattern.md)):

```ts
class Email {
  private constructor(readonly value: string) {}

  static parse(input: string): Result<Email, "invalid-email"> {
    return /^[^@\s]+@[^@\s]+$/.test(input)
      ? { ok: true, value: new Email(input) }
      : { ok: false, error: "invalid-email" };
  }
}
```

This "smart constructor" guarantees any `Email` you hold is valid, a class-based cousin of [branded types](../10-advanced-types/07-branded-types.md).

## Factories that build from a configuration

A function that **returns a configured function or object** is a factory too, and often the most idiomatic TypeScript:

```ts
function createLogger(prefix: string, level: "debug" | "info" = "info") {
  return {
    info: (msg: string) => console.log(`[${prefix}] ${msg}`),
    debug: (msg: string) => level === "debug" && console.log(`[${prefix}] ${msg}`),
  };
}

const apiLog = createLogger("api");
const dbLog = createLogger("db", "debug");
```

Closures hold the configuration, so no class or `this` is needed.

## Registries: factories that can be extended

When the set of kinds is **open** (plugins, user-defined handlers), use a registry rather than a fixed union:

```ts
type Parser = (input: string) => unknown;
const parsers = new Map<string, Parser>();

function registerParser(format: string, parser: Parser) {
  parsers.set(format, parser);
}

function createParser(format: string): Parser {
  const parser = parsers.get(format);
  if (!parser) throw new Error(`No parser registered for "${format}"`);
  return parser;
}
```

You trade compile-time exhaustiveness for extensibility, since the keys are only known at runtime. Use a closed `Record` when the set is fixed, and a registry when it is open.

## Constructor types

To pass a class itself (not an instance) into a factory, use a constructor type:

```ts
type Constructor<T> = new (...args: any[]) => T;

function createInstance<T>(Ctor: Constructor<T>, ...args: ConstructorParameters<typeof Ctor>): T {
  return new Ctor(...args);
}
```

`ConstructorParameters` and `InstanceType` ([function and class utilities](../07-utility-types/03-function-and-class-utilities.md)) help keep the arguments and result typed. Remember that generics are erased, so you cannot write `new T()`: pass the constructor as a value ([type erasure and runtime](../14-type-system-internals/00-type-erasure-and-runtime.md)).

## Abstract factory (briefly)

An **abstract factory** creates a *family* of related objects through one interface, for example a UI kit that produces matching `Button` and `Input` for a light or dark theme:

```ts
interface UiFactory {
  button(label: string): Button;
  input(placeholder: string): Input;
}

const lightUi: UiFactory = { button: (l) => new LightButton(l), input: (p) => new LightInput(p) };
const darkUi: UiFactory  = { button: (l) => new DarkButton(l),  input: (p) => new DarkInput(p) };
```

In TypeScript this is just an object satisfying an interface. The pattern is rarely needed beyond that: pass the factory object where the family must stay consistent.

## When to use a factory

Good fits:

- Callers should depend on an **interface**, not on concrete classes.
- Creation involves **choices, configuration, validation, or async setup**.
- You need **one place** to change when a new kind is added.
- You want to **swap implementations** in tests ([dependency injection](./05-dependency-injection.md)).

Skip it when a plain object literal or a single `new` call is clear enough. A factory that wraps one constructor adds indirection with no benefit.

## Common mistakes

- A `switch` that is not exhaustive, so a new kind falls through to `undefined`.
- Returning the broad interface when callers need the specific type (use a map and a generic).
- Factories with long boolean-flag parameter lists (`create(true, false, true)`). Use an options object ([builder](./01-builder.md)).
- Hiding global state or singletons inside a factory.
- A registry whose keys are never validated, so typos fail only at runtime.
- Building factory class hierarchies for things a function would do.
- Casting inside the factory without a signature that justifies it.

## Debugging

- Hover the factory's return type at a call site. If it is the broad interface where you expected the specific type, the signature needs a generic mapping.
- For "not all code paths return a value", add the missing case or a `never` default to find which kind was forgotten.
- If a registry lookup throws, log the registered keys alongside the requested one.
- Check whether the factory is being called repeatedly in a hot path when it should be created once.

## Quick summary

- A factory hides *which* object is created and *how*. Callers depend on an interface.
- In TypeScript, prefer plain functions, `Record<Kind, () => T>` tables, and closures over class hierarchies.
- A generic over a key map makes the return type depend on the argument.
- Static factory methods give named, validated, or async construction. A private constructor forces their use.
- Use a closed table for a fixed set of kinds (compile-time exhaustiveness) and a registry for an open set.

**Next:** [Builder](./01-builder.md)
