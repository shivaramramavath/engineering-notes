# Express Types

Express was written in plain JavaScript; its types come from the community-maintained `@types/express` package. With the right setup, `req`, `res`, and `next` are fully typed — and with a few generics, so are your params, bodies, and responses.

## Installing the types

```bash
npm install express
npm install --save-dev @types/express
```

Most popular middleware has its own `@types/*` package (`@types/cors`, `@types/cookie-parser`, `@types/morgan`). Libraries that ship their own types (like `zod`, `pino`, `helmet`) need nothing extra.

---

## The basics

```ts
import express, { Request, Response, NextFunction } from "express";

const app = express();

app.get("/health", (req: Request, res: Response) => {
  res.json({ status: "ok" });
});
```

For inline handlers passed directly to `app.get(...)`, TypeScript infers `req`/`res` for you — annotations are needed mainly in **separate controller functions**, like in `06-express/03-controllers.md`:

```ts
// controllers/userController.ts
import { Request, Response, NextFunction } from "express";

export async function getUser(req: Request, res: Response, next: NextFunction) {
  try {
    const user = await User.findById(req.params.id); // req.params.id: string
    if (!user) return res.status(404).json({ error: "User not found" });
    res.json(user);
  } catch (err) {
    next(err);
  }
}
```

---

## Typing the four parts of a request

`Request` is generic. The full signature:

```ts
Request<Params, ResBody, ReqBody, ReqQuery>
```

```ts
interface UserParams {
  id: string;
}

interface CreateUserBody {
  name: string;
  email: string;
}

interface ListQuery {
  page?: string;   // query strings are always strings (or undefined)
  limit?: string;
}

export async function createUser(
  req: Request<{}, {}, CreateUserBody>,
  res: Response,
  next: NextFunction
) {
  const { name, email } = req.body; // typed as string
  // ...
}

export async function getUser(
  req: Request<UserParams>,
  res: Response
) {
  req.params.id; // string
}

export async function listUsers(
  req: Request<{}, {}, {}, ListQuery>,
  res: Response
) {
  const page = Number(req.query.page ?? 1);
}
```

Typing the response body too:

```ts
export async function getUser(
  req: Request<UserParams>,
  res: Response<PublicUser | { error: string }>
) {
  res.json({ error: "oops" }); // ✅ allowed
  res.json({ foo: 1 });        // ❌ doesn't match the declared shape
}
```

### A tidier alias

Four positional generics get noisy. Wrap them:

```ts
// types/express.ts
import { Request } from "express";

export type TypedRequest<
  TBody = unknown,
  TParams = Record<string, string>,
  TQuery = Record<string, string | undefined>
> = Request<TParams, unknown, TBody, TQuery>;
```

```ts
export async function createUser(req: TypedRequest<CreateUserBody>, res: Response) {
  req.body.email; // typed
}
```

> **Important:** these annotations tell TypeScript what you *expect*, not what actually arrived. `req.body` is only as trustworthy as your validation. Validate first (`06-express/05-validation.md`), then rely on the types.

---

## Deriving types from a validation schema (Zod)

The cleanest approach is to define the schema once and infer the type from it — validation and typing can then never drift apart:

```ts
import { z } from "zod";

export const createUserSchema = z.object({
  name: z.string().min(1),
  email: z.string().email(),
});

export type CreateUserBody = z.infer<typeof createUserSchema>;
```

```ts
// middleware/validate.ts
import { RequestHandler } from "express";
import { ZodSchema } from "zod";

export const validateBody =
  (schema: ZodSchema): RequestHandler =>
  (req, res, next) => {
    const result = schema.safeParse(req.body);
    if (!result.success) {
      return res.status(400).json({ errors: result.error.flatten() });
    }
    req.body = result.data; // replaced with parsed, trusted data
    next();
  };
```

```ts
router.post("/", validateBody(createUserSchema), createUser);
```

---

## Typing middleware

`RequestHandler` types a regular middleware function:

```ts
import { RequestHandler } from "express";

export const requestLogger: RequestHandler = (req, res, next) => {
  console.log(`${req.method} ${req.url}`);
  next();
};
```

Error-handling middleware has **four parameters** and uses `ErrorRequestHandler`:

```ts
import { ErrorRequestHandler } from "express";

export const errorHandler: ErrorRequestHandler = (err, req, res, next) => {
  // err is `any` here — see 04-error-types.md for narrowing it
  res.status(500).json({ error: "Internal Server Error" });
};
```

Express identifies error middleware by its **arity (4 args)** — don't drop `next` even if unused. Full details in `04-error-types.md` and `06-express/04-error-handling.md`.

---

## Extending `Request` with custom properties

A common need: auth middleware attaches `req.user`, and later handlers want to read it. Out of the box, TypeScript says:

```ts
req.user; // ❌ Property 'user' does not exist on type 'Request'
```

The fix is **declaration merging** — augment Express's own `Request` interface:

```ts
// src/types/express.d.ts
import { JwtPayload } from "../auth/types";

declare global {
  namespace Express {
    interface Request {
      user?: JwtPayload;
      requestId?: string;
    }
  }
}

export {}; // makes this file a module so `declare global` works
```

Now every `Request` in the project knows about `req.user` and `req.requestId`:

```ts
export const requireAuth: RequestHandler = (req, res, next) => {
  const token = req.headers.authorization?.split(" ")[1];
  if (!token) return res.status(401).json({ error: "Unauthorized" });

  req.user = verifyToken(token); // typed
  next();
};

export async function getProfile(req: Request, res: Response) {
  const userId = req.user?.sub; // optional — might be undefined if middleware didn't run
}
```

Make sure the `.d.ts` file is actually **included** by `tsconfig.json` (`"include": ["src"]` covers it). If augmentation seems to be ignored, that's almost always the cause. For where `req.user` comes from, see `08-authentication-security/02-jwt-and-tokens.md`; for `requestId`, see `14-logging-observability/02-correlation-id.md`.

> `req.user` is typed optional (`?`) because TypeScript can't know the auth middleware ran. If you want a guaranteed type in protected routes, define an `AuthenticatedRequest` that extends `Request` with `user: JwtPayload` (non-optional) and use it only in handlers mounted behind `requireAuth`.

---

## Typing route handlers returning values

A pattern for keeping controllers focused is a small async wrapper that forwards errors, saving the repeated `try/catch`:

```ts
import { Request, Response, NextFunction, RequestHandler } from "express";

type AsyncHandler<P = any, ResB = any, ReqB = any, Q = any> = (
  req: Request<P, ResB, ReqB, Q>,
  res: Response<ResB>,
  next: NextFunction
) => Promise<unknown>;

export const asyncHandler =
  <P, ResB, ReqB, Q>(fn: AsyncHandler<P, ResB, ReqB, Q>): RequestHandler<P, ResB, ReqB, Q> =>
  (req, res, next) => {
    fn(req, res, next).catch(next);
  };
```

```ts
router.get(
  "/:id",
  asyncHandler(async (req: Request<UserParams>, res) => {
    const user = await userService.getUser(req.params.id);
    res.json(user);
  })
);
```

(Express 5 forwards rejected promises from async handlers to the error middleware automatically; on Express 4 you still need a wrapper or `try/catch` + `next(err)`.)

---

## Typing app-level things

```ts
// Environment config — validated and typed once, imported everywhere
export const config = {
  port: Number(process.env.PORT ?? 3000),
  jwtSecret: process.env.JWT_SECRET!, // ! = "I assert this is set" — better: validate at startup
} as const;
```

`process.env.X` is `string | undefined`. Rather than scattering `!` assertions, validate the whole environment once at startup (see `16-production/01-environment-management.md`) and export a typed config object.

---

## Common mistakes

- **Annotating `req.body` and assuming it's validated** — types vanish at runtime. Validate, then type.
- **Forgetting `export {}` in a `.d.ts` that uses `declare global`** — the augmentation is silently ignored.
- **`.d.ts` file not covered by `include`** — `req.user` still errors.
- **Typing `req.query` values as `number`** — query values are always `string | string[] | undefined`; convert explicitly.
- **Dropping the `next` parameter from error middleware** — Express then treats it as a normal handler and never routes errors to it.
- **Using `any` for `req`/`res`** — loses autocomplete for the whole handler.

## Quick summary

- Install `@types/express` (plus `@types/*` for middleware that lacks built-in types)
- `Request<Params, ResBody, ReqBody, Query>` types each part of the request; `Response<T>` types the body
- Infer types from Zod schemas so validation and types stay in sync
- Use `RequestHandler` / `ErrorRequestHandler` for middleware
- Extend `Request` through `declare global { namespace Express { ... } }` in a `.d.ts` file
- Types describe expectations; **validation** guarantees them

## Next

**`04-error-types.md`** covers typed custom error classes, why `catch (err)` is `unknown`, and a fully typed centralized error-handling middleware.
