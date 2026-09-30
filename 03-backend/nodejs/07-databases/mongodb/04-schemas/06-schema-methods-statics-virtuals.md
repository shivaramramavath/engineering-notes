# Schema Methods, Statics & Virtuals

Three ways to attach custom behavior directly to a schema, so it travels with the model wherever it's used, rather than being reimplemented at every call site.

## Instance methods — behavior on a single document

```js
userSchema.methods.getFullName = function () {
  return `${this.firstName} ${this.lastName}`;
};
```

```js
const user = await User.findById(userId);
console.log(user.getFullName()); // called on a specific document instance
```

`this` inside an instance method refers to the document it's called on — note the use of a regular `function`, not an arrow function; arrow functions don't have their own `this`, so they'd fail to reference the document at all.

### A very common real example: comparing a password

```js
userSchema.methods.comparePassword = async function (candidatePassword) {
  return bcrypt.compare(candidatePassword, this.password);
};
```

```js
const user = await User.findOne({ email }).select("+password");
const isMatch = await user.comparePassword(providedPassword);
```

Attaching this directly to the schema means every part of the app that needs to check a password calls the same method, rather than duplicating the `bcrypt.compare` call (and potentially getting some detail of it wrong) in multiple places.

---

## Static methods — behavior on the model itself

```js
userSchema.statics.findByEmail = function (email) {
  return this.findOne({ email: email.toLowerCase() });
};
```

```js
const user = await User.findByEmail("Alice@Example.com");
```

`this` inside a static refers to the **model**, not a document — statics are essentially custom query methods, useful for encapsulating a commonly-repeated query (with any associated logic, like the lowercasing above) into one reusable, well-named call.

### Instance method vs static method — the distinction

```js
userSchema.methods.someMethod = function () {
  /* this = a document */
};
userSchema.statics.someMethod = function () {
  /* this = the model */
};
```

```js
user.instanceMethod(); // called on a document you already have
User.staticMethod(); // called on the model directly, often to FIND a document in the first place
```

A rough rule of thumb: if you already have a document and want to do something with/about it, it's an instance method; if you're trying to find or create documents in the first place, it's a static.

---

## Virtuals — computed properties that aren't stored

```js
userSchema.virtual("fullName").get(function () {
  return `${this.firstName} ${this.lastName}`;
});
```

```js
const user = await User.findById(userId);
console.log(user.fullName); // "Alice Smith" — computed on access, not stored in the database at all
```

A virtual behaves like a real property when accessed, but has **no corresponding field in the actual MongoDB document** — nothing is written to disk for it; it's purely derived from other, real fields whenever it's read.

### Virtuals with a setter

```js
userSchema
  .virtual("fullName")
  .get(function () {
    return `${this.firstName} ${this.lastName}`;
  })
  .set(function (value) {
    const [firstName, lastName] = value.split(" ");
    this.firstName = firstName;
    this.lastName = lastName;
  });
```

```js
user.fullName = "Bob Jones";
user.firstName; // "Bob" — the setter split it and assigned the real underlying fields
```

A setter lets a virtual be assigned to directly, translating that assignment into changes on the real fields backing it.

### Virtuals and JSON output

```js
const userSchema = new mongoose.Schema(
  { firstName: String, lastName: String },
  { toJSON: { virtuals: true } }, // from 05-schema-options.md — required for this to show up in res.json()
);
```

By default, virtuals are **not** included when a document is converted to JSON — they need `toJSON: { virtuals: true }` (or the equivalent for `toObject`) explicitly enabled, exactly as covered in the schema-options file.

### A very common real use of virtuals: `populate`

```js
// virtual populate — covered fully in 11-relationships/03-virtual-populate.md
userSchema.virtual("posts", {
  ref: "Post",
  localField: "_id",
  foreignField: "author",
});
```

Virtuals aren't only for simple computed strings — "virtual populate" uses the same mechanism to define a computed relationship to another collection, without storing a redundant reference field.

---

## Combining all three in one schema

```js
const userSchema = new mongoose.Schema(
  {
    firstName: String,
    lastName: String,
    email: String,
    password: { type: String, select: false },
  },
  { toJSON: { virtuals: true } },
);

userSchema.virtual("fullName").get(function () {
  return `${this.firstName} ${this.lastName}`;
});

userSchema.methods.comparePassword = async function (candidate) {
  return bcrypt.compare(candidate, this.password);
};

userSchema.statics.findByEmail = function (email) {
  return this.findOne({ email: email.toLowerCase() });
};

const User = mongoose.model("User", userSchema);
```

## Common mistakes

- **Using an arrow function for an instance method or virtual getter** — arrow functions don't bind their own `this`, so `this.someField` inside one won't refer to the document at all; always use a regular `function`.
- **Expecting a virtual to be queryable like a real field** — `User.find({ fullName: "Alice Smith" })` won't work, since `fullName` doesn't actually exist in the stored document; only real, indexed fields can be queried directly this way.
- **Forgetting `toJSON: { virtuals: true }`** and being confused why a virtual doesn't appear in an API response, even though `user.fullName` works fine when accessed directly in code.
- **Putting logic that should be a static method inside route/controller code instead** — duplicating a "find by X with some extra logic" query across multiple call sites, rather than defining it once as a static.

## Quick summary

- Instance methods (`schema.methods.x`) operate on a single document; `this` is the document
- Static methods (`schema.statics.x`) operate on the model itself; `this` is the model — good for reusable, encapsulated queries
- Virtuals (`schema.virtual("x").get(...)`) are computed properties with no backing field in the database, requiring `toJSON: { virtuals: true }` to appear in serialized output
- Always use regular `function` syntax (not arrow functions) for any of these, since they rely on Mongoose's own `this` binding
- Virtuals aren't queryable directly — they're purely for computed values or, via virtual populate, computed relationships

## Next

**`07-indexes-in-schemas.md`** covers declaring indexes as part of schema definition — the last piece of schema configuration this section covers.
