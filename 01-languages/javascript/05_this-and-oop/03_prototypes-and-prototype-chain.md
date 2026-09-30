# Prototypes and the Prototype Chain

Every object has a hidden link, `[[Prototype]]`, to another object (or `null`). Property lookups follow that link. This is how JavaScript does **inheritance**.

```
obj  ──[[Prototype]]──►  proto  ──[[Prototype]]──►  Object.prototype  ──►  null
```

## The lookup rule

When you read `obj.prop`:

1. Look for `prop` on `obj` itself
2. If missing, look on `obj`'s prototype
3. Continue up the chain until found, or reach `null` (result: `undefined`)

```js
const animal = { eats: true, speak() { return "..."; } };
const dog = Object.create(animal);   // dog's prototype is animal
dog.barks = true;

dog.barks;     // own property
dog.eats;      // inherited from animal
dog.speak();   // inherited method
dog.flies;     // undefined (end of chain)
```

## Reading and setting the prototype

```js
Object.getPrototypeOf(dog) === animal;    // true (standard way)
Object.setPrototypeOf(dog, other);        // works but slow; avoid in hot code
dog.__proto__;                            // legacy accessor; avoid in new code
Object.create(proto);                     // create with a chosen prototype
Object.create(null);                      // no prototype at all
```

## Writes go to the object itself

```js
dog.eats = false;        // creates an OWN property, does not change animal
delete dog.eats;         // own removed, inherited value visible again
```

Exception: an inherited **setter** or a non-writable inherited property changes what assignment does.

## `prototype` vs `[[Prototype]]`

These are different things.

| | What it is | Exists on |
|---|-----------|-----------|
| `[[Prototype]]` (`Object.getPrototypeOf(x)`) | the link an object uses for lookups | every object |
| `Fn.prototype` | the object that becomes `[[Prototype]]` of instances created with `new Fn()` | functions and classes |

```js
function Dog() {}
const d = new Dog();

Object.getPrototypeOf(d) === Dog.prototype;   // true
Dog.prototype.constructor === Dog;            // true
d.constructor === Dog;                        // true (inherited)
```

## The built-in chains

```
[1, 2]      ─►  Array.prototype    ─►  Object.prototype  ─►  null
function f  ─►  Function.prototype ─►  Object.prototype  ─►  null
new Date()  ─►  Date.prototype     ─►  Object.prototype  ─►  null
"text"      ─►  String.prototype   ─►  Object.prototype  ─►  null   (via temporary wrapper)
```

That is why `[].map`, `f.call` and `obj.toString` exist without being defined by you.

## Checking relationships

```js
"eats" in dog;                          // true (own or inherited)
Object.hasOwn(dog, "eats");             // false (inherited)
animal.isPrototypeOf(dog);              // true
dog instanceof Dog;                     // checks Dog.prototype in dog's chain
```

## Shadowing

```js
dog.speak = function () { return "Woof"; };   // own property hides animal.speak
dog.speak();                                  // "Woof"
Object.getPrototypeOf(dog).speak.call(dog);   // call the inherited one
```

## Iterating and prototypes

```js
for (const key in dog) { ... }                 // includes inherited enumerable keys
for (const key of Object.keys(dog)) { ... }    // own only
```

## Sharing methods to save memory

```js
function Counter() { this.n = 0; }
Counter.prototype.inc = function () { this.n++; };   // one function shared by all instances

new Counter().inc === new Counter().inc;   // true
```

Methods defined inside the constructor (`this.inc = function...`) create a new function per instance.

## Extending built-ins

```js
Array.prototype.last = function () { return this[this.length - 1]; };   // avoid
```

Modifying built-in prototypes can conflict with future standards or other libraries. Prefer helper functions or subclasses.

## Prototype pollution (security)

If code merges untrusted keys into an object, a payload such as `{"__proto__": {"admin": true}}` can modify `Object.prototype` and affect **every** object.

Defenses: `Object.create(null)` dictionaries, `Map`, validate keys, block `__proto__`, `constructor`, `prototype`, use `Object.hasOwn`. See `22_security/03_prototype-pollution.md`.

## Inspecting in DevTools

`console.dir(obj)` expands `[[Prototype]]`. In the console, `Object.getPrototypeOf(obj)` and `obj.constructor.name` give quick answers.

## Object.create with descriptors

```js
const proto = { hello() { return "hi"; } };
const obj = Object.create(proto, {
  id: { value: 1, enumerable: true },
});
```

## Prototype pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Confusing `prototype` with `[[Prototype]]` | Wrong mental model | `getPrototypeOf(x)` vs `Fn.prototype` |
| Mutable objects on a prototype (`Fn.prototype.list = []`) | Shared across all instances | Create them in the constructor |
| `for...in` picking inherited keys | Unexpected properties | `Object.keys`, `hasOwn` |
| `__proto__` in new code | Legacy, security risk | `Object.getPrototypeOf` / `create` |
| `setPrototypeOf` in hot paths | Deoptimizes | `Object.create`, classes |
| Extending `Object.prototype` | Breaks `for...in`, libraries | Never do it |
| `obj.hasOwnProperty` on null-prototype objects | Method missing | `Object.hasOwn` |

## Key takeaways

- Objects delegate missing property lookups to their prototype chain
- `Fn.prototype` is the object assigned to instances' `[[Prototype]]`
- Reads walk the chain; writes create own properties
- Use `Object.getPrototypeOf` and `Object.create`, not `__proto__`
- Guard against prototype pollution when handling untrusted keys

**Next:** [Constructor Functions](./04_constructor-functions.md)
