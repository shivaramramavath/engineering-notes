# 01 — The MongoDB Native Driver

Mongoose doesn't talk to MongoDB directly — it wraps the official `mongodb` Node.js driver, adding schemas, validation, and casting on top. This section covers the driver itself, at the level needed to understand what Mongoose is actually doing underneath, and to know when to drop down to it directly (`02-mongodb-vs-mongoose/02-when-to-drop-to-the-native-driver.md`).

## In this section

| File                                      | Covers                                                                                                           |
| ----------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `01-connecting-with-the-native-driver.md` | Connecting with `MongoClient`, without Mongoose in the picture at all                                            |
| `02-native-crud-methods.md`               | The raw `insertOne`/`find`/`updateOne`/`deleteOne` methods Mongoose's own methods are built on                   |
| `03-query-operators-reference.md`         | The full `$gte`/`$in`/`$exists`/etc. operator vocabulary — used identically inside Mongoose filter objects later |

## Why learn the driver at all, if this guide is about Mongoose

Every Mongoose query eventually becomes a native driver call — `User.find({ age: { $gte: 18 } })` is Mongoose casting and validating your input, then handing essentially this exact filter object to the driver underneath. Understanding the driver means:

- Mongoose's behavior stops feeling like magic — it's a well-defined layer over something you can also read directly
- The filter/operator syntax you learn here (`03-query-operators-reference.md`) is **exactly** what you'll use inside `Model.find()` later, not a different syntax to relearn
- You know how and when to reach `Model.collection` (`02-mongodb-vs-mongoose/02-when-to-drop-to-the-native-driver.md`) to run something Mongoose doesn't expose a convenient method for

## What you should be able to do after this section

- Connect to MongoDB with `MongoClient`, independent of Mongoose entirely
- Perform basic CRUD with the native driver's own methods
- Read and write filter objects using the full range of query operators — the same skill that transfers directly to Mongoose

## Next

**`02-mongodb-vs-mongoose`** covers exactly what Mongoose adds on top of everything in this section, and the trade-offs of that extra layer.
