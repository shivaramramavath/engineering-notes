# 02 — MongoDB vs Mongoose

The last of the three MongoDB-focused folders, and the bridge into the rest of this guide. With the native driver covered in `01-mongodb-native-driver/`, this section makes explicit exactly what Mongoose adds on top of it, what that costs, and when to step back down to the driver directly.

## In this section

| File                                      | Covers                                                                                                                      |
| ----------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| `01-what-mongoose-adds.md`                | Schemas, casting, validation, and middleware — the concrete value Mongoose provides over the raw driver, and its trade-offs |
| `02-when-to-drop-to-the-native-driver.md` | Escape hatches — `Model.collection`, raw aggregation, and when bypassing Mongoose is the right call                         |

## Why this matters before learning Mongoose in depth

Every file from `03-setup/` onward teaches Mongoose as if it's the only way to talk to MongoDB — which is usually true in practice, but it's worth understanding clearly, once, that Mongoose is a **choice**, not the only option, and an **abstraction**, not a different database. Knowing exactly what that abstraction buys you (and where it can get in the way) makes you a much more effective Mongoose user than treating it as an unquestioned default.

## What you should be able to do after this section

- Explain, concretely, what Mongoose validates/casts/enforces that the native driver does not
- Recognize the situations where Mongoose's abstraction adds meaningful overhead or gets in the way
- Know how to drop down to the native driver from within a Mongoose codebase, without abandoning Mongoose entirely

## Next

**`03-setup`** begins the Mongoose-focused portion of this guide — installing it and establishing a connection.
