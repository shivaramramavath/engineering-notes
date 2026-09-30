# Pre & Post Hooks

The basic hook syntax — running code before or after a given operation, attached directly to the schema.

## `pre` — runs before the operation

```js
userSchema.pre("save", function (next) {
  console.log("About to save:", this.name);
  next();
});
```

```js
const user = new User({ name: "Alice" });
await user.save(); // logs "About to save: Alice" first, then actually saves
```

`this` refers to the document being saved (for document middleware — full distinction in `02-document-vs-query-vs-aggregate-middleware.md`). Calling `next()` is what lets the operation continue — forgetting it hangs the operation forever, the exact same "must call next()" rule as Express middleware (`06-express/02-middleware.md`).

### The modern alternative: returning a Promise, no `next` needed

```js
userSchema.pre("save", async function () {
  console.log("About to save:", this.name);
  // no next() needed — an async function (or one returning a Promise) signals
  // completion by resolving, and an error by rejecting/throwing
});
```

Modern Mongoose supports both styles: a callback-style hook using `next()`, or an `async` function that Mongoose awaits automatically. The `async` form is generally cleaner for hooks doing asynchronous work (like the password-hashing example in `03-common-hook-patterns.md`), since you don't need to manually call `next()` at every possible exit path, including inside a `try/catch`.

---

## `post` — runs after the operation

```js
userSchema.post("save", function (doc, next) {
  console.log("Saved:", doc._id);
  next();
});
```

```js
userSchema.post("save", function (doc) {
  console.log("Saved:", doc._id); // async form, no next needed
});
```

`post` hooks receive the resulting document (or, for query middleware, the query result) as their first argument — useful for side effects that should happen _after_ something is confirmed persisted (sending a welcome email after a user is created, for example).

---

## Error handling inside hooks

```js
userSchema.pre("save", async function () {
  if (await isEmailBlacklisted(this.email)) {
    throw new Error("This email domain is not allowed");
  }
});
```

```js
const user = new User({ email: "test@blacklisted.com" });
try {
  await user.save();
} catch (err) {
  console.log(err.message); // "This email domain is not allowed"
}
```

Throwing inside an `async` hook (or calling `next(err)` in the callback style) stops the operation entirely — the document is never actually saved, and the error propagates out of `.save()` to be caught normally by whatever called it.

```js
userSchema.pre("save", function (next) {
  if (someCondition) {
    return next(new Error("Something's wrong")); // callback-style error propagation
  }
  next();
});
```

---

## Hooking multiple operations

```js
userSchema.pre(["save", "updateOne"], function (next) {
  console.log("A save or updateOne is about to happen");
  next();
});
```

An array of operation names applies the same hook to each — though be aware `save` and `updateOne` are actually different middleware _types_ under the hood (document vs. query middleware, `02-document-vs-query-vs-aggregate-middleware.md`), and `this` behaves differently for each even within the same shared hook function.

---

## `post` hooks and error-handling middleware (a special signature)

```js
userSchema.post("save", function (error, doc, next) {
  if (error.code === 11000) {
    next(new Error("A user with this email already exists"));
  } else {
    next(error);
  }
});
```

A `post` hook with **three** parameters (`error, doc, next`) is specifically an error-handling post hook — it only fires when the operation it's attached to actually failed, letting you transform or react to the error at the schema level. This is one place where the duplicate-key-to-friendly-message translation from `08-errors/05-turning-errors-into-friendly-responses.md` could alternatively live — directly on the schema, rather than in the application's error-handling middleware — though centralizing it at the Express/service layer (as that file recommends) is usually clearer and more consistent than spreading translation logic across individual schemas.

---

## Order of execution with multiple hooks

```js
userSchema.pre("save", function (next) {
  console.log("first");
  next();
});
userSchema.pre("save", function (next) {
  console.log("second");
  next();
});
```

```js
await user.save();
// "first"
// "second"
// (the actual save happens)
```

Multiple `pre` hooks for the same event run in the order they were registered — same principle as Express middleware ordering (`06-express/02-middleware.md`).

## Common mistakes

- **Forgetting `next()` in a callback-style hook** — hangs the operation forever, with no error and no completion.
- **Mixing `next()` calls with returning a Promise from the same hook function** — pick one style (callback-with-`next`, or `async`/Promise-returning) and be consistent within a given hook.
- **Using an arrow function for a hook that needs `this`** — same rule as everywhere else in Mongoose (instance methods, virtuals, custom validators): arrow functions don't bind their own `this`, breaking access to the document/query being processed.
- **Not realizing a three-parameter `post` hook has special error-handling semantics** — different from the normal two-parameter `post(doc, next)` form.

## Quick summary

- `pre`/`post` hooks attach behavior directly to the schema, guaranteed to run for every relevant operation regardless of call site
- Use a regular `function`, not an arrow function, since hooks rely on Mongoose's `this` binding
- The modern `async`-function hook style avoids manual `next()` calls; the older callback style still works and is common in existing code
- Throwing (or calling `next(err)`) inside a `pre` hook stops the operation entirely — nothing is saved
- A three-parameter `post` hook (`error, doc, next`) is a special error-handling hook, only firing when the operation failed

## Next

**`02-document-vs-query-vs-aggregate-middleware.md`** covers exactly which hook type fires for which method — essential for understanding why a hook sometimes doesn't run when you'd expect it to.
