# Dependency Injection

Dependency injection (DI) means a piece of code **receives** the things it depends on (database, HTTP client, clock, logger) instead of creating or importing them itself. That's the whole idea. A "DI container" is an optional tool built on top of it.

It matters because code that builds its own dependencies is hard to test, hard to reconfigure, and hard to reason about. DI makes dependencies visible in the code's signature.

**Prerequisites:** [Classes](../05_this-and-oop/05_classes.md), [Closures](../06_closures/01_closures.md), [Singleton](./03_singleton-pattern.md), [Factory](./02_factory-pattern.md)

---

## Without DI

```js
import { db } from "./db.js";
import { sendEmail } from "./mailer.js";

export async function signup(email) {
  const existing = await db.query("SELECT id FROM users WHERE email = ?", [email]);
  if (existing.length) throw new Error("Email taken");

  await db.query("INSERT INTO users (email, created_at) VALUES (?, ?)", [email, new Date()]);
  await sendEmail(email, "Welcome!");
}
```

To test this you need a real database and mail sender, or you must mock the modules. The `new Date()` also makes assertions on `created_at` awkward.

---

## Constructor Injection

Pass dependencies in. Anything the code needs from the outside world becomes a parameter.

```js
export class SignupService {
  #users;
  #mailer;
  #now;

  constructor({ users, mailer, now = () => new Date() }) {
    this.#users = users;
    this.#mailer = mailer;
    this.#now = now;
  }

  async signup(email) {
    if (await this.#users.findByEmail(email)) throw new Error("Email taken");

    await this.#users.insert({ email, createdAt: this.#now() });
    await this.#mailer.send(email, "Welcome!");
  }
}
```

Using one options object for several dependencies keeps the constructor readable and avoids positional mistakes. `now` defaults to the real clock but can be overridden.

### Function form

If you aren't using classes, a function that returns the function works the same way ([Closures](../06_closures/02_closure-use-cases.md)).

```js
export const makeSignup = ({ users, mailer }) => async (email) => {
  // ...same body
};
```

---

## Wiring It Up: the Composition Root

Somewhere, real implementations must be created and connected. Do that **once, near the program entry point**, and nowhere else.

```js
// main.js: the composition root
import { SignupService } from "./signup-service.js";
import { PgUserRepo } from "./pg-user-repo.js";
import { SmtpMailer } from "./smtp-mailer.js";

const pool = createPool(process.env.DATABASE_URL);

const signupService = new SignupService({
  users: new PgUserRepo(pool),
  mailer: new SmtpMailer(process.env.SMTP_URL),
});

startServer({ signupService });
```

This is where "one shared instance" is decided. You get the benefits of a [Singleton](./03_singleton-pattern.md) (one pool) without any module reaching out for global state.

---

## Testing Becomes Plain Code

```js
import { test, expect } from "vitest";

test("signup stores the user and sends a welcome email", async () => {
  const saved = [];
  const sent = [];

  const service = new SignupService({
    users: {
      findByEmail: async () => null,
      insert: async (u) => void saved.push(u),
    },
    mailer: { send: async (to, body) => void sent.push({ to, body }) },
    now: () => new Date("2026-01-01T00:00:00Z"),
  });

  await service.signup("a@example.com");

  expect(saved).toEqual([{ email: "a@example.com", createdAt: new Date("2026-01-01T00:00:00Z") }]);
  expect(sent).toHaveLength(1);
});
```

No module mocking, no network, deterministic time. Module-mocking features in test runners exist, but DI often removes the need for them. See [Mocking](../21_testing/05_mocking.md).

---

## A Minimal Container (Optional)

You can wire everything by hand until the graph gets large. A small container only automates the wiring.

```js
function createContainer() {
  const providers = new Map();
  const instances = new Map();

  const container = {
    register(name, factory) {
      providers.set(name, factory);
      return container;
    },
    resolve(name) {
      if (!instances.has(name)) {
        const factory = providers.get(name);
        if (!factory) throw new Error(`No provider for "${name}"`);
        instances.set(name, factory(container));
      }
      return instances.get(name);
    },
  };
  return container;
}

const c = createContainer()
  .register("pool", () => createPool(process.env.DATABASE_URL))
  .register("users", (c) => new PgUserRepo(c.resolve("pool")))
  .register("mailer", () => new SmtpMailer(process.env.SMTP_URL))
  .register("signup", (c) => new SignupService({ users: c.resolve("users"), mailer: c.resolve("mailer") }));

c.resolve("signup");
```

Each dependency is created lazily and reused (singleton scope). A circular dependency (`a` needs `b`, `b` needs `a`) would recurse until the stack overflows, so treat that as a design problem to fix, not something for the container to solve.

Use a library container only when manual wiring becomes a real burden. Often it never does in JS.

---

## Common Mistakes

- **Service locator.** Passing the whole container into classes and letting them call `container.resolve("x")` hides dependencies again. Inject the specific things needed.
- **Importing the container (or concrete implementations) deep in the code.** Only the composition root should know concrete classes.
- **Too many dependencies.** A constructor with eight injected services usually means the class does too much.
- **Injecting what doesn't vary.** Pure helpers like `slugify` don't need injection. Inject things that do I/O, depend on time/randomness, or that you want to swap.
- **Fakes that don't match reality.** If the fake accepts anything but the real one validates, tests pass and production fails. Keep the interface small and test the real implementation separately.
- **Cycles.** If two services need each other, extract the shared part or use events ([Observer](./06_observer-pattern.md)).

---

## Quick Summary

- DI = receive dependencies instead of creating/importing them. No framework needed.
- Constructor or function parameters are the simplest mechanism.
- Build the object graph once at the composition root.
- It makes time, I/O and randomness replaceable, which is what makes tests simple.
- Containers are optional. Avoid the service-locator style.

**Next:** back to the [section overview](./README.md), or on to [Testing](../21_testing/README.md).