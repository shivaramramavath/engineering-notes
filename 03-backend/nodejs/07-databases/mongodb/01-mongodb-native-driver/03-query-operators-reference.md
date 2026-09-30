# Query Operators Reference

The full `$`-prefixed operator vocabulary used inside a filter object — identical whether you're writing raw driver code, a `mongosh` query, or a Mongoose `Model.find()` call. This file is the reference; `07-filter-conditions/` later covers using these specifically within Mongoose in much greater depth.

## Comparison operators

```js
{
  age: {
    $eq: 30;
  }
} // equal to (rarely written explicitly — { age: 30 } is shorthand for this)
{
  age: {
    $ne: 30;
  }
} // not equal to
{
  age: {
    $gt: 18;
  }
} // greater than
{
  age: {
    $gte: 18;
  }
} // greater than or equal
{
  age: {
    $lt: 65;
  }
} // less than
{
  age: {
    $lte: 65;
  }
} // less than or equal
{
  status: {
    $in: ["active", "pending"];
  }
} // matches any value in the array
{
  status: {
    $nin: ["banned", "deleted"];
  }
} // matches none of the values in the array
```

```js
// combined — a range
{ age: { $gte: 18, $lte: 65 } }
```

---

## Logical operators

```js
{
  $and: [{ age: { $gte: 18 } }, { status: "active" }];
}
{
  $or: [{ status: "active" }, { status: "pending" }];
}
{
  $nor: [{ status: "banned" }, { status: "deleted" }];
}
{
  age: {
    $not: {
      $lt: 18;
    }
  }
}
```

`$and` is often implicit — listing multiple fields in one object already means "all of these must match":

```js
{ age: { $gte: 18 }, status: "active" }    // same as $and: [{age...}, {status...}]
```

`$and` is only strictly necessary when you need multiple conditions on the **same** field, or an explicit `$or`/`$nor` alongside other conditions.

---

## Element operators

```js
{
  email: {
    $exists: true;
  }
} // the field is present (even if null)
{
  deletedAt: {
    $exists: false;
  }
} // the field is absent entirely
{
  age: {
    $type: "int";
  }
} // the field's BSON type matches
```

`$exists` is the standard way to query for the common soft-delete pattern (`15-patterns-and-architecture/02-soft-delete.md`) — "not deleted" means "the `deletedAt` field doesn't exist."

---

## Array operators

```js
{ tags: "vip" }                          // matches if "vip" is ANY element in the array
{ tags: { $all: ["vip", "verified"] } }     // array contains ALL of these values
{ tags: { $size: 3 } }                        // array has exactly 3 elements
{ "scores": { $elemMatch: { $gte: 80, $lt: 90 } } }   // an element satisfying multiple conditions at once
```

```js
// querying an array of embedded documents
{ "orders": { $elemMatch: { status: "shipped", total: { $gt: 100 } } } }
```

`$elemMatch` matters specifically when a **single array element** must satisfy multiple conditions together — without it, MongoDB would allow different elements to each satisfy one condition, which is usually not what you mean.

---

## Evaluation operators

```js
{ name: { $regex: "^A", $options: "i" } }     // regex match, case-insensitive
{ $expr: { $gt: ["$spent", "$budget"] } }        // compare two fields on the SAME document
{ $text: { $search: "mongodb tutorial" } }         // full-text search (requires a text index)
```

`$expr` is worth calling out specifically — a normal filter compares a field against a literal value; `$expr` lets you compare **two fields on the same document** against each other, which an ordinary filter object can't express at all.

---

## Nested/dot-notation fields

```js
{ "address.city": "Boston" }
{ "orders.status": "shipped" }
```

Dot notation reaches into embedded documents and arrays of embedded documents without needing a special operator — this same syntax works identically in Mongoose filters, and in schema paths.

---

## A combined, realistic example

```js
{
  status: { $in: ["active", "pending"] },
  age: { $gte: 18, $lt: 65 },
  "address.city": "Boston",
  tags: { $all: ["verified"] },
  deletedAt: { $exists: false },
}
```

Read as: active or pending users, aged 18-64, living in Boston, tagged verified, not soft-deleted — a single filter object combining comparison, array, element, and nested-field conditions together, exactly the kind of filter you'll write constantly once these become `Model.find({...})` calls in Mongoose.

## Quick summary

- Comparison (`$gte`/`$in`/etc.), logical (`$and`/`$or`/`$nor`), element (`$exists`/`$type`), array (`$all`/`$size`/`$elemMatch`), and evaluation (`$regex`/`$expr`/`$text`) operators cover almost everything you'll need to express in a filter
- Multiple fields in one object are an implicit `$and`; explicit `$and`/`$or`/`$nor` are needed for same-field multi-conditions or genuine either/or logic
- `$elemMatch` is required when one array element must satisfy multiple conditions together
- Dot notation reaches into nested documents/arrays without any special operator
- Every operator here works identically inside a Mongoose `Model.find()` filter — nothing new to learn syntactically once you get there

## Section complete

That covers the native driver: connecting, CRUD methods, and the query operator vocabulary. **`02-mongodb-vs-mongoose`** covers exactly what Mongoose adds on top of all of this.
