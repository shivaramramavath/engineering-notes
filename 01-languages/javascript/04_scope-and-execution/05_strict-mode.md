# Strict Mode

**Strict mode** (ES5) is an opt-in variant of JavaScript that turns silent mistakes into errors and disables confusing features. Modern code should always run in it.

## Enabling it

```js
"use strict";            // top of a script: whole file
```

```js
function f() {
  "use strict";          // only this function
}
```

The directive must be the **first statement** (comments are fine before it).

## Automatically strict

| Context | Strict? |
|---------|---------|
| ES modules (`import`/`export`) | Yes, always |
| Class bodies | Yes |
| CommonJS files, classic scripts | No, unless you add the directive |
| Code with non-simple parameters (defaults, rest, destructuring) | Cannot contain `"use strict"` directive (SyntaxError) |

## What changes

| Sloppy mode | Strict mode |
|-------------|-------------|
| Assigning to an undeclared variable creates a global | `ReferenceError` |
| Writing to read-only properties fails silently | `TypeError` |
| Deleting non-configurable properties returns `false` | `TypeError` |
| Adding properties to frozen/non-extensible objects is ignored | `TypeError` |
| Duplicate parameter names allowed | `SyntaxError` |
| `this` in a plain function call is the global object | `this` is `undefined` |
| `with` statement allowed | `SyntaxError` |
| Octal literals `010` and `\010` allowed | `SyntaxError` |
| `arguments` mirrors named parameters | `arguments` is independent |
| `arguments.callee` works | `TypeError` |
| `eval` declares variables in the surrounding scope | `eval` has its own scope |
| Reserved words such as `let`, `static`, `implements` usable as names | Reserved, cannot be identifiers |

## Examples

```js
"use strict";

undeclared = 5;                         // ReferenceError

const o = Object.freeze({ a: 1 });
o.a = 2;                                // TypeError: Cannot assign to read-only property

delete Object.prototype;                // TypeError

function dup(a, a) {}                   // SyntaxError

function whoAmI() { return this; }
whoAmI();                               // undefined (sloppy: globalThis)
```

## `this` safety

```js
class Button {
  label = "OK";
  click() { console.log(this.label); }
}

const { click } = new Button();
click();   // TypeError: cannot read 'label' of undefined  (loud failure)
```

In sloppy mode a detached method would read from the global object and quietly give `undefined`.

## `arguments` decoupling

```js
function sloppy(a) { arguments[0] = 99; return a; }
sloppy(1);        // 99 (linked in sloppy mode)

function strict(a) { "use strict"; arguments[0] = 99; return a; }
strict(1);        // 1
```

## Performance and security

- Strict code is easier for engines to optimize (no `with`, predictable `arguments`)
- It prevents accidental global leaks and dangerous `this` access to `window`
- It reserves words for future syntax and blocks features that hurt static analysis

## Mixing modes

- Strictness is **lexical**: a strict function inside a sloppy file stays strict, and a function defined in strict code is strict wherever it is called
- Concatenating a strict script with sloppy scripts can change behavior. Prefer modules or per-function directives to avoid this

## Migrating a codebase

1. Add `"use strict"` per file (or move to ES modules)
2. Fix implicit globals: declare variables
3. Fix `this` assumptions in callbacks and detached methods
4. Remove `with`, `arguments.callee`, octal literals
5. Turn on ESLint rules: `strict`, `no-undef`, `no-implicit-globals`

```json
{ "type": "module" }   // package.json: Node treats .js files as ES modules (strict)
```

## Checking for strict mode

```js
const isStrict = (function () { return this === undefined; })();
```

## Strict mode pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Directive not at the top | Ignored silently | First statement in the file or function |
| Relying on sloppy `this` (global) | Throws when strict | Use `globalThis` explicitly or bind |
| Old libraries that assume sloppy mode | Break when wrapped or moved into modules | Update, or isolate them |
| `"use strict"` in a function with default parameters | `SyntaxError` | File-level directive or a module |
| Assuming TypeScript/Babel adds it always | Depends on config | Use ES modules or set the option |

## Key takeaways

- Strict mode converts silent failures into exceptions
- ES modules and classes are strict automatically
- `this` becomes `undefined` in plain function calls
- Use modules (or `"use strict"`) in every project

**Next:** [This and OOP](../05_this-and-oop/00_README.md)
