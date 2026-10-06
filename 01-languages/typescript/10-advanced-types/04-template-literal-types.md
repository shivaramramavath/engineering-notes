# Template Literal Types

Template literal types use the same backtick syntax as JavaScript template strings, but at the type level. They build **string literal types** from other types, and, combined with `infer`, they let you **parse** string types. They are how libraries type route paths, event names, CSS-like values, and "snake_case to camelCase" conversions.

**Prerequisites:**
- [Literal types and const assertions](../03-unions-and-narrowing/02-literal-types-and-const-assertions.md)
- [Union types](../03-unions-and-narrowing/00-union-types.md)
- [Conditional types](./00-conditional-types.md) and [infer](./02-infer.md) (for the parsing sections)

---

## Syntax

```ts
type Greeting = `Hello, ${string}`;

const a: Greeting = "Hello, world";   // ok
const b: Greeting = "Hi, world";      // error
```

Inside `${ }` you can place `string`, `number`, `bigint`, `boolean`, `null`, `undefined`, string/number literals, or unions of these.

```ts
type Id = `user_${number}`;

const ok: Id = "user_42";
const bad: Id = "user_abc";   // error
```

## Unions expand to every combination

```ts
type Axis = "x" | "y";
type Side = "top" | "bottom";

type Position = `${Side}-${Axis}`;
// "top-x" | "top-y" | "bottom-x" | "bottom-y"
```

The result is the cross product of all unions. This is powerful but grows fast: very large cross products hit a compiler limit and produce an error about the union being too complex. Keep the input unions small.

## Intrinsic string types

Four built-in helpers transform string literal types:

| Type | Result for `"hello world"` |
|---|---|
| `Uppercase<S>` | `"HELLO WORLD"` |
| `Lowercase<S>` | `"hello world"` |
| `Capitalize<S>` | `"Hello world"` |
| `Uncapitalize<S>` | `"hello world"` |

```ts
type EventName<T extends string> = `on${Capitalize<T>}`;

type E = EventName<"click" | "focus">;   // "onClick" | "onFocus"
```

## Practical usage

### Typed event handler names

```ts
type Events = { click: MouseEvent; focus: FocusEvent };

type Handlers = {
  [K in keyof Events as `on${Capitalize<string & K>}`]: (e: Events[K]) => void;
};
// { onClick: (e: MouseEvent) => void; onFocus: (e: FocusEvent) => void }
```

The `as` clause comes from [mapped types](./03-mapped-types.md). `string & K` narrows to string keys.

### Constrain string-typed props

```ts
type CssLength = `${number}px` | `${number}rem` | `${number}%`;

function setWidth(w: CssLength) {}

setWidth("10px");     // ok
setWidth("2.5rem");   // ok
setWidth("10");       // error
```

### Typed identifiers

```ts
type UserId  = `usr_${string}`;
type OrderId = `ord_${string}`;
```

These catch accidental mix-ups for free, though they are only a convention at runtime. For stronger separation see [branded types](./07-branded-types.md).

## Parsing with `infer`

Placing `infer` inside a template literal lets you take strings apart:

```ts
type Prefix<S> = S extends `${infer P}-${string}` ? P : never;

type P = Prefix<"user-123">;   // "user"
```

### Matching rules

- Each `infer` that is **not last** captures the **shortest** possible match up to the next literal text.
- The **last** `infer` captures the **rest** of the string.
- `${string}` matches any characters; `${number}` matches a string that parses as a number.

```ts
type Split<S extends string> =
  S extends `${infer A}.${infer B}` ? [A, B] : [S];

type R = Split<"a.b.c">;   // ["a", "b.c"]
```

### Extract route parameters

A realistic use: derive parameter names from a path pattern.

```ts
type ParamNames<S extends string> =
  S extends `${string}:${infer P}/${infer Rest}`
    ? P | ParamNames<`/${Rest}`>
    : S extends `${string}:${infer P}`
      ? P
      : never;

type Params<S extends string> = { [K in ParamNames<S>]: string };

type R = Params<"/users/:userId/posts/:postId">;
// { userId: string; postId: string }
```

Trace: `"/users/:userId/posts/:postId"` first matches the first pattern. `${string}` takes `/users/`, `P` takes `userId` (up to the next `/`), and `Rest` is `posts/:postId`. The type then recurses on `/posts/:postId`, where the first pattern fails (no `/` after the parameter), and the second pattern captures `postId`. The recursion is covered in [recursive types](./05-recursive-types.md).

Used with a function, this gives call-site checking:

```ts
declare function buildPath<S extends string>(pattern: S, params: Params<S>): string;

buildPath("/users/:userId", { userId: "1" });   // ok
buildPath("/users/:userId", {});                // error: missing userId
```

### Convert casing

```ts
type CamelCase<S extends string> =
  S extends `${infer Head}_${infer Tail}`
    ? `${Lowercase<Head>}${Capitalize<CamelCase<Tail>>}`
    : Lowercase<S>;

type R = CamelCase<"user_first_name">;   // "userFirstName"
```

Applied to object keys with a mapped type, this converts a whole record from snake_case to camelCase at the type level.

### Inferring literal types from values

Inference for template literal types needs the argument to be a **literal**. A `const` or a `<S extends string>` generic keeps the literal:

```ts
declare function route<S extends string>(path: S): Params<S>;

route("/users/:id");        // S inferred as "/users/:id"
```

A variable typed as plain `string` gives `string`, and the parsing types see no structure.

## Important rules and misconceptions

**These are types, not runtime checks.** `` `user_${number}` `` does not validate anything at runtime. Data coming from outside (JSON, input, URLs) needs real validation ([runtime validation](../15-runtime-validation/README.md)).

**Template types distribute over unions** in each placeholder, as shown in the cross-product behavior.

**`string` in a placeholder is a pattern, not a literal.** `` `a${string}` `` accepts `"a"`, `"abc"`, and so on.

**Generic strings stay unresolved.** In a function body with `S extends string`, `Params<S>` is deferred until the call site provides a literal.

**Number-like matching is lenient in some ways.** `` `${number}` `` matches strings like `"1e3"` and `"-0"` that parse as numbers. Test unusual formats if exactness matters.

## Common mistakes

- **Letting the argument widen to `string`.** The parser sees no pattern and returns something vague. Use a `const` or a generic constraint.
- **Building huge unions** (large cross products) and hitting compiler limits or slow checks.
- **Using `keyof T` directly in a template literal.** Use `string & keyof T` since `keyof T` can include `number` and `symbol`.
- **Expecting regex-like power.** Template literal types do positional pattern matching only. Complex grammars are slow and fragile.
- **Treating them as validation.** They describe types, not data.

## Debugging

- Test the parser with a literal and hover the result: `type T = Params<"/a/:b">`.
- Test edge cases: empty string, no delimiter, several delimiters, trailing delimiter.
- If you get `never`, the pattern did not match the string you supplied. Check for a missing leading `/` or extra characters.
- If the result is `string` or `{}`, the input was widened. Confirm the argument is a literal.
- For "Type instantiation is excessively deep" or "union too complex", reduce unions or restructure recursion with an accumulator ([recursive types](./05-recursive-types.md)).

## Quick summary

- `` `...${T}...` `` builds string literal types, expanding unions into every combination.
- `Uppercase`, `Lowercase`, `Capitalize`, and `Uncapitalize` transform literal strings.
- With `infer`, a template literal acts as a pattern matcher: non-final `infer`s take the shortest match, the last takes the rest.
- Typical uses: event names, route parameters, casing conversion, string-shaped IDs and CSS values.
- Inputs must be literal types to be parsed, and none of this validates data at runtime.

**Next:** [Recursive types](./05-recursive-types.md)
