# Nested & Subdocument Schemas

Two ways to embed structured data inside a document: a plain nested object, or a real subdocument built from its own schema. They look similar but behave differently in some important ways.

## Plain nested objects

```js
const userSchema = new mongoose.Schema({
  name: String,
  address: {
    city: String,
    zip: String,
  },
});
```

```js
const user = await User.create({
  name: "Alice",
  address: { city: "Boston", zip: "02101" },
});

user.address.city; // "Boston"
```

This is the simplest way to nest structure — Mongoose treats `address` as its own mini-schema inline, with casting and validation applied to `city`/`zip` exactly as if they were top-level fields.

---

## Real subdocuments — using a separate `Schema`

```js
const addressSchema = new mongoose.Schema({
  city: String,
  zip: String,
});

const userSchema = new mongoose.Schema({
  name: String,
  address: addressSchema,
});
```

Functionally very similar for a single embedded object, but a named, separate schema is **reusable** (as shown in `01-defining-a-schema.md`, the same `addressSchema` can back both a single field and an array), and can carry its own options, virtuals, and methods (`06-schema-methods-statics-virtuals.md`) independent of the parent.

---

## Arrays of subdocuments

```js
const orderSchema = new mongoose.Schema({
  product: String,
  quantity: Number,
  price: Number,
});

const userSchema = new mongoose.Schema({
  name: String,
  orders: [orderSchema],
});
```

```js
const user = await User.create({
  name: "Alice",
  orders: [
    { product: "Widget", quantity: 2, price: 9.99 },
    { product: "Gadget", quantity: 1, price: 19.99 },
  ],
});
```

Each element in `orders` is a real subdocument — validated individually against `orderSchema`, and each one automatically gets its **own `_id`**, unlike a plain array of primitives.

### Subdocuments get their own `_id` by default

```js
user.orders[0]._id; // a real ObjectId, auto-generated
```

```js
const order = user.orders.id(orderId); // look up a specific subdocument by its _id
```

This is genuinely useful — it means individual elements in an embedded array can be referenced, updated, or removed by ID, much like top-level documents, without needing them to be a separate collection. Disable it if truly unnecessary:

```js
const orderSchema = new mongoose.Schema(
  { product: String, quantity: Number },
  { _id: false },
);
```

---

## Updating a specific subdocument

```js
const user = await User.findById(userId);
const order = user.orders.id(orderId);
order.quantity = 5;
await user.save(); // saves the whole parent document, including the modified subdocument
```

```js
// or, directly via a query, using the positional operator (covered in 06-crud-methods/03-update-methods.md)
await User.updateOne(
  { _id: userId, "orders._id": orderId },
  { $set: { "orders.$.quantity": 5 } },
);
```

Both work; the first is more natural when you're already working with a loaded document, the second avoids loading the whole parent document just to change one nested value.

---

## Adding and removing subdocuments

```js
user.orders.push({ product: "New Item", quantity: 1, price: 5.99 });
await user.save();
```

```js
user.orders.pull(orderId); // remove by _id
user.orders.id(orderId).deleteOne(); // an alternative removal syntax on the subdocument itself
await user.save();
```

---

## Validation cascades into subdocuments

```js
const orderSchema = new mongoose.Schema({
  product: { type: String, required: true },
  quantity: { type: Number, min: 1 },
});
```

```js
user.orders.push({ product: "Widget", quantity: 0 });
await user.save(); // ValidationError — quantity fails min: 1, even though it's nested inside "orders"
```

Validation rules on a subdocument schema are enforced exactly like top-level rules — a validation failure anywhere in a nested subdocument fails the entire parent document's save.

---

## When to embed a subdocument vs. reference a separate document

This file covers the _mechanics_ of embedding; the actual decision of _whether_ to embed at all vs. reference a separate collection is covered in full in `11-relationships/02-embedding-vs-referencing-in-mongoose.md`. As a preview: embedding (what this file covers) suits data that's always accessed together with its parent and doesn't grow unboundedly; referencing suits data that's large, independently queried, or shared across multiple parents.

---

## Deeply nested structures

```js
const commentSchema = new mongoose.Schema({
  text: String,
  author: String,
});

const postSchema = new mongoose.Schema({
  title: String,
  comments: [commentSchema],
});

const blogSchema = new mongoose.Schema({
  name: String,
  posts: [postSchema],
});
```

Nesting can go several levels deep, but be cautious: every level of nesting makes querying/updating a specific deeply-nested element more awkward (longer dot-notation paths, more complex positional-operator usage), and increases the risk of eventually hitting the 16MB document size limit (`00-mongodb-basics/04-databases-collections-documents.md`) if any level contains an unboundedly growing array. Two or three levels is common and manageable; much deeper nesting is often a sign some level should be its own referenced collection instead.

## Common mistakes

- **Not realizing subdocuments in an array get their own auto-generated `_id`** — genuinely useful once known, confusing if you don't expect the extra field.
- **Loading and re-saving an entire parent document just to change one small nested value**, when a direct positional-operator update would be more efficient and avoid a full document re-validation.
- **Nesting arrays of subdocuments too deeply**, or allowing one to grow unboundedly — risks the 16MB document limit and makes updates increasingly awkward; consider referencing instead past a certain depth/size.
- **Forgetting that a validation failure anywhere in a nested subdocument fails the entire save** — a single invalid item in a large `orders` array blocks saving the whole parent document, not just that one order.

## Quick summary

- A plain nested object and a separate subdocument `Schema` behave similarly for a single embedded field, but a separate schema is reusable and can carry its own methods/virtuals
- Subdocuments in an array automatically get their own `_id`, letting you reference/update/remove them individually via `.id()`
- Validation on subdocument fields is enforced exactly like top-level validation, and a failure anywhere fails the whole parent save
- Keep nesting to a reasonable depth — very deep or unboundedly growing nested structures are often better modeled as a separate referenced collection instead

## Next

**`05-schema-options.md`** covers schema-level (not per-field) configuration — timestamps, JSON output shaping, and more.
