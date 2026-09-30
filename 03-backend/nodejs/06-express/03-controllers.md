# Controllers

As routes grow beyond a couple of lines each, putting all the logic directly in `app.get(...)` callbacks becomes unwieldy. Controllers separate _what URL triggers this_ (routing) from _what actually happens_ (the controller function) — a lightweight first step toward the layered architecture covered in `10-architecture/01-mvc-and-layered-architecture.md`.

## Without controllers: logic crammed into routes

```js
// routes/users.js
router.post("/", async (req, res) => {
  const { name, email } = req.body;

  if (!name || !email) {
    return res.status(400).json({ error: "Name and email are required" });
  }

  const existing = await User.findOne({ email });
  if (existing) {
    return res.status(409).json({ error: "Email already in use" });
  }

  const user = await User.create({ name, email });
  res.status(201).json(user);
});
```

Fine for one route. With a dozen routes each doing similar amounts of work, the route file becomes long, hard to scan, and hard to test independently of Express itself.

---

## With controllers: routing and logic separated

```js
// controllers/userController.js
export async function createUser(req, res, next) {
  try {
    const { name, email } = req.body;

    if (!name || !email) {
      return res.status(400).json({ error: "Name and email are required" });
    }

    const existing = await User.findOne({ email });
    if (existing) {
      return res.status(409).json({ error: "Email already in use" });
    }

    const user = await User.create({ name, email });
    res.status(201).json(user);
  } catch (err) {
    next(err);
  }
}

export async function getUser(req, res, next) {
  try {
    const user = await User.findById(req.params.id);
    if (!user) return res.status(404).json({ error: "User not found" });
    res.json(user);
  } catch (err) {
    next(err);
  }
}
```

```js
// routes/users.js
import { Router } from "express";
import { createUser, getUser } from "../controllers/userController.js";

const router = Router();

router.post("/", createUser);
router.get("/:id", getUser);

export default router;
```

Now `routes/users.js` reads as a clean map of "URL → handler," and each controller function can be reasoned about (and, importantly, unit tested — `13-testing/`) independently of the routing layer.

---

## A typical project structure

```
src/
├── routes/
│   ├── users.js
│   └── posts.js
├── controllers/
│   ├── userController.js
│   └── postController.js
├── models/          (database schemas — see 07-databases/)
└── app.js
```

---

## Controllers vs services: a further split, when needed

As business logic grows more complex, it's common to split it once more — controllers handle the HTTP-specific parts (reading `req`, sending `res`), and a separate **service** layer handles the actual business logic, with no knowledge of Express at all:

```js
// services/userService.js — no req/res, just business logic
export async function registerUser(name, email) {
  const existing = await User.findOne({ email });
  if (existing) {
    throw new ConflictError("Email already in use");
  }
  return User.create({ name, email });
}
```

```js
// controllers/userController.js — thin, just translates HTTP <-> service calls
export async function createUser(req, res, next) {
  try {
    const user = await registerUser(req.body.name, req.body.email);
    res.status(201).json(user);
  } catch (err) {
    next(err);
  }
}
```

This extra layer pays off once business logic needs to be reused outside an HTTP context (a background job, a CLI script, a test) — the service function works in any of those, while a controller function is tied to `req`/`res` and only makes sense inside an Express route. Full coverage of this pattern in `10-architecture/03-repository-and-service-pattern.md`.

---

## When controllers alone are enough

For a small app or a straightforward CRUD API, splitting into controllers (without a further service layer) is often the right amount of structure — introducing a service layer for a five-route app is usually premature. Add the extra layer once you notice controllers duplicating business logic across routes, or needing that logic outside of an HTTP request at all.

## Common mistakes

- **Putting all logic directly in route callbacks** — works initially, becomes unmanageable as an app grows past a handful of routes.
- **Controllers directly building HTTP responses deep inside shared business logic** — makes that logic impossible to reuse outside an HTTP request; that's exactly what a service layer separates out.
- **Over-engineering a small app with a full service/repository layered structure from day one** — reasonable for a large app, overkill for a five-route prototype.
- **Forgetting `try/catch` + `next(err)` in an async controller** — same async error-handling concern as in `02-middleware.md`, easy to forget once logic moves into a separate file.

## Quick summary

- Controllers hold the logic for what happens when a route is hit, separate from the routing definitions themselves
- Routes become a clean map of "URL → controller function"; controllers can be tested and reasoned about independently
- A further split into a service layer (no `req`/`res` at all) pays off once logic needs to be reused outside HTTP, or once controllers start duplicating logic
- Match the amount of structure to the app's actual complexity — don't add layers a small project doesn't need yet

## Next

**`04-error-handling.md`** covers Express's centralized error-handling pattern — the `next(err)` calls used throughout the controller examples here.
