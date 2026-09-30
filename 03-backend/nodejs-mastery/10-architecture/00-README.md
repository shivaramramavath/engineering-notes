# Architecture

How to organize the code behind your API so it stays understandable, testable, and changeable as it grows from 5 routes to 500.

## Why architecture matters

Every Node.js project starts the same way: a few routes, a database call inside each handler, and everything works. Six months later:

```js
// ❌ the "fat route handler" — where most projects end up
app.post("/orders", async (req, res) => {
  if (!req.body.items?.length) return res.status(400).json({ error: "No items" });     // validation
  const user = await db.query("SELECT * FROM users WHERE id = $1", [req.user.id]);      // data access
  let total = 0;
  for (const item of req.body.items) {                                                   // business rules
    const p = (await db.query("SELECT * FROM products WHERE id = $1", [item.id])).rows[0];
    if (p.stock < item.qty) return res.status(409).json({ error: "Out of stock" });
    total += p.price * item.qty * (user.rows[0].isPremium ? 0.9 : 1);
  }
  await db.query("INSERT INTO orders ...");                                              // more data access
  await sendgrid.send({ to: user.rows[0].email, ... });                                  // external service
  res.status(201).json({ total });                                                       // HTTP
});
```

This handler knows about HTTP, SQL, pricing rules, stock, and email. You can't test the pricing rule without a database and an email account, can't reuse it from a background job, and can't change the database without rewriting every route.

**Architecture is about deciding where each kind of code lives, and which code is allowed to know about which.** Good structure makes the common changes (new endpoint, new database, new rule) small and local.

---

## The core ideas (they recur in every file)

| Idea | Meaning |
|---|---|
| **Separation of concerns** | HTTP handling, business rules, and data access are different jobs; keep them in different places |
| **Single responsibility** | A module has one reason to change |
| **Dependency direction** | Dependencies should point toward stable, important code (business rules), not away from it |
| **Abstraction at boundaries** | Hide volatile details (database, email provider, framework) behind interfaces you own |
| **Testability** | If something is hard to test, it's usually badly placed or badly coupled |
| **Cohesion and coupling** | Keep related things together (high cohesion); keep unrelated things independent (low coupling) |

---

## What's in this section

| File | What you learn |
|---|---|
| `01-mvc-and-layered-architecture.md` | The pragmatic default: routes → controllers → services → data access |
| `02-clean-architecture.md` | Making business rules independent of frameworks and databases |
| `03-repository-and-service-pattern.md` | The two patterns that do most of the work, with real code |
| `04-dependency-injection.md` | Wiring it all together so layers can be swapped and tested |
| `05-modular-monolith-vs-microservices.md` | Deciding how to split the system as it (and the team) grows |

Read in order: `01` is what most projects need, `02` is the stricter version, `03` and `04` are the tools that make both work, and `05` zooms out to the whole system.

---

## You don't need all of this on day one

The biggest architecture mistake isn't too little structure, it's **too much, too early**.

| Project stage | Reasonable structure |
|---|---|
| Prototype / script / weekend project | Everything in a few files. That's fine. |
| Small API (5–20 endpoints, 1–2 devs) | Layered: routes, controllers, services, models |
| Growing product (20–100+ endpoints, team of 3–10) | Layered + repositories + dependency injection, organized by feature |
| Large system / many teams | Modular monolith, possibly some services split out |

A useful rule: **add a layer when you feel a specific pain, not because a blog post said so.** Pains and the tool that fixes them:

| Pain you're feeling | Reach for |
|---|---|
| Handlers are huge and hard to read | Controllers + services (`01`) |
| Tests need a real database | Repository + dependency injection (`03`, `04`) |
| Changing the DB/ORM touches everything | Repository pattern, clean architecture (`02`, `03`) |
| Business logic is duplicated between HTTP, jobs, and CLI | Service layer / use cases (`01`, `02`) |
| Teams step on each other's code | Modules with enforced boundaries (`05`) |
| One part needs to scale or deploy independently | Extract a service (`05`) |

---

## Prerequisites

- `06-express/01-setup-and-routing.md` and `06-express/03-controllers.md`: the layers start here
- `06-express/02-middleware.md`: cross-cutting concerns (auth, logging, validation) live in middleware
- `07-databases/`: data access is the layer you'll abstract
- `09-api-development/`: the HTTP-facing conventions (validation, errors, DTOs) that the outer layer is responsible for
- `03-javascript-for-node/05-error-handling.md`: layers communicate failures through errors

---

## The running example

Examples across this section build one feature, **placing an order**, through every style, so you can see the same logic in each structure:

```
POST /api/v1/orders
  → validate the request
  → check the products exist and have stock
  → calculate the total (premium customers get 10% off)
  → save the order and reduce stock (atomically)
  → email a confirmation
  → return the created order
```

It's deliberately business-flavored: real rules, multiple data sources, and a side effect (email) that's annoying to test. This is the same kind of feature you'll build in `20-projects/07-production-api/`.

---

## A note on language and tooling

- Examples use **plain JavaScript (ES modules)**, consistent with the rest of the course. Architecture is about structure, not types, but TypeScript makes interfaces and dependency contracts explicit. See `17-typescript/02-interfaces-and-generics.md` for the typed versions.
- Examples use Express, but nothing here is Express-specific. Fastify, NestJS, and Hono all support the same layering. NestJS in particular bakes in modules and dependency injection.
- Libraries are kept to a minimum on purpose: most of this is plain functions, classes, and folders.

---

## Principles to remember

1. **Optimize for change, not for the first version.** The first version is written once; the codebase is changed for years.
2. **Keep business rules free of HTTP and database details.** This single habit gives you most of the benefit.
3. **Dependencies point inward.** Routes know about services; services don't know about Express.
4. **Organize by feature as the codebase grows.** `orders/`, `users/`, `billing/`, not one giant `controllers/` folder.
5. **Prefer boring and consistent over clever.** A new teammate should guess where code lives.
6. **Architecture is a set of trade-offs, not a set of rules.** Every pattern adds indirection; it must earn its place.

## Next

**`01-mvc-and-layered-architecture.md`** starts with the pragmatic default that most production Node.js APIs use: separating routes, controllers, services, and data access.
