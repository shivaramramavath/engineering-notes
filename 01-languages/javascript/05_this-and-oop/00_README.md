# 05 · This and OOP

JavaScript's object model is **prototype based**. `this` decides which object a method works on, prototypes decide where inherited behavior comes from, and `class` is a cleaner syntax on top of both.

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 1 | [this](./01_this.md) | The four binding rules, arrows, lost `this` |
| 2 | [call, apply, bind](./02_call-apply-bind.md) | Setting `this` explicitly, partial application |
| 3 | [Prototypes and the Prototype Chain](./03_prototypes-and-prototype-chain.md) | `[[Prototype]]`, `__proto__`, lookup, `Object.create` |
| 4 | [Constructor Functions](./04_constructor-functions.md) | `new`, `prototype`, what `new` really does |
| 5 | [Classes](./05_classes.md) | Fields, methods, accessors, `static`, static blocks |
| 6 | [Inheritance](./06_inheritance.md) | `extends`, `super`, method overriding, built-in subclassing |
| 7 | [Private Fields](./07_private-fields.md) | `#field`, `#method`, `static #`, `#x in obj` |
| 8 | [Mixins and Composition](./08_mixins-and-composition.md) | Composition over inheritance, mixin functions |

## Goal

By the end you can predict the value of `this` in any call, explain how inheritance works under the hood, and choose between classes, factories and composition.

**Next:** [this](./01_this.md)
