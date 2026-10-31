# Authorization Middleware Patterns

How to wire authentication, roles, permissions, ownership and step-up into Express so that the **safe path is the default** and a forgotten check is hard to write. Middleware is the delivery mechanism; the decisions themselves live in the policies from the previous notes.

**Prerequisites:** [00-rbac.md](./00-rbac.md), [01-permissions-and-policies.md](./01-permissions-and-policies.md), [02-jwt-access-tokens.md](../03-tokens/02-jwt-access-tokens.md)

---

## The pipeline

```text
Request
  │
  ▼
authenticate           who are you?                      401 if no/invalid token
  │   (sets req.user)
  ▼
requireVerified / requireMfa / requireRecentAuth   account state & assurance   403 + code
  │
  ▼
requireRole / requirePermission                   coarse "can they use this feature?"   403
  │
  ▼
loadResource           fetch the record (scoped)         404 if missing / not theirs
  │   (sets req.resource)
  ▼
authorize(policy)      may they do THIS to THIS record?  403 / 404
  │
  ▼
controller             business logic, assumes all checks passed
```

Order matters: cheap, general checks first; per-record checks last, because they need a database read.

---

## 1. `authenticate`

Verifies the access token and sets the actor ([02-jwt-access-tokens.md](../03-tokens/02-jwt-access-tokens.md)):

```js
export const authenticate = (req, res, next) => {
  const header = req.headers.authorization;
  if (!header?.startsWith("Bearer ")) return res.status(401).json({ message: "Authentication required" });

  try {
    const p = jwt.verify(header.slice(7), process.env.ACCESS_TOKEN_SECRET, {
      algorithms: ["HS256"], issuer: "your-api", audience: "your-frontend",
    });
    req.user = { sub: p.sub, sid: p.sid, role: p.role };       // only what policies need
    next();
  } catch {
    res.status(401).json({ message: "Invalid or expired access token" });
  }
};
```

`401` tells the client to refresh the token ([06-token-storage.md](../03-tokens/06-token-storage.md)). Everything after this middleware may assume `req.user` exists.

---

## 2. Role and permission guards

Middleware **factories** (functions that return middleware) keep routes declarative:

```js
import { can } from "../authz/roles.js";

export const requireRole = (...roles) => (req, res, next) =>
  roles.includes(req.user.role) ? next() : deny(res);

export const requirePermission = (permission) => (req, res, next) =>
  can(req.user.role, permission) ? next() : deny(res);

const deny = (res) => res.status(403).json({ message: "Forbidden" });
```

```js
router.get("/users", authenticate, requirePermission("users:read"), listUsers);
```

Prefer `requirePermission` over `requireRole`: when roles change, route code doesn't. Reserve `requireRole("admin")` for genuinely role-specific areas.

### Fresh role from the database (sensitive routes)

```js
export const requireFreshRole = (...roles) => async (req, res, next) => {
  const user = await User.findById(req.user.sub).select("role isActive").lean();
  if (!user?.isActive || !roles.includes(user.role)) return deny(res);
  req.user.role = user.role;                       // use the fresh value downstream
  next();
};
```

Use this on admin and destructive endpoints so a demotion or disablement applies immediately, not at token expiry ([00-rbac.md](./00-rbac.md)).

---

## 3. Account-state and assurance guards

| Guard | Checks | Response |
|---|---|---|
| `requireVerifiedEmail` | `isEmailVerified` | `403 { code: "EMAIL_NOT_VERIFIED" }` |
| `requireMfa` | session `amr` includes a second factor | `403 { code: "MFA_REQUIRED" }` |
| `requireRecentAuth(seconds)` | session `lastAuthAt` | `403 { code: "STEP_UP_REQUIRED" }` ([05-step-up-auth.md](../05-mfa/05-step-up-auth.md)) |
| `requireActiveSession` | Redis `session:{sid}` exists | `401` ([04-logout-and-revocation.md](../04-sessions/04-logout-and-revocation.md)) |

Give each failure a distinct machine-readable `code`, so the frontend can react (open the re-auth modal, show the verify-email banner) instead of showing a generic error.

```js
router.post("/billing/payout",
  authenticate,
  requireActiveSession,
  requireMfa,
  requireRecentAuth(120),
  requirePermission("billing:payout"),
  createPayout);
```

---

## 4. Loading resources safely

A loader fetches the record the route refers to, **scoped to the actor**, and attaches it for later steps:

```js
export const loadArticle = async (req, res, next) => {
  if (!mongoose.isValidObjectId(req.params.id)) return res.status(404).json({ message: "Not found" });

  const article = await Article.findOne({ _id: req.params.id, ...articleScope(req.user) });   // layer-3 scoping
  if (!article) return res.status(404).json({ message: "Not found" });

  req.resource = article;
  next();
};
```

Validating the id first avoids `CastError` crashes and gives a consistent `404`. Scoping inside the query ([01-permissions-and-policies.md](./01-permissions-and-policies.md)) means a foreign id is indistinguishable from a nonexistent one.

A reusable factory:

```js
export const load = (Model, { param = "id", scope = () => ({}) } = {}) => async (req, res, next) => {
  const id = req.params[param];
  if (!mongoose.isValidObjectId(id)) return res.sendStatus(404);
  const doc = await Model.findOne({ _id: id, ...scope(req.user) });
  if (!doc) return res.sendStatus(404);
  req.resource = doc;
  next();
};
```

---

## 5. Policy middleware

```js
export const authorize = (policy, action) => (req, res, next) => {
  const allowed = policy[action]?.(req.user, req.resource) === true;     // missing action => deny
  if (!allowed) {
    logDenied(req, action);                                              // actor, action, resource id: no secrets
    return res.status(403).json({ message: "Forbidden" });
  }
  next();
};
```

`=== true` makes truthy-but-wrong return values (`undefined`, `{}`) deny. Combine:

```js
router.get   ("/articles/:id", authenticate, load(Article, { scope: articleScope }), authorize(articlePolicy, "read"),   getArticle);
router.patch ("/articles/:id", authenticate, load(Article, { scope: articleScope }), authorize(articlePolicy, "update"), validate(updateSchema), updateArticle);
router.delete("/articles/:id", authenticate, requireFreshRole("admin"), load(Article), authorize(articlePolicy, "delete"), deleteArticle);
```

Validate (`zod`) **after** authorization so unauthenticated callers can't use validation errors to probe your schema; at minimum, authenticate first.

---

## 6. Default deny at the router level

The most dangerous bug is the route someone forgot to protect. Make protection structural:

```js
// routes/index.js
const publicRouter  = express.Router();   // login, register, refresh, forgot-password, health
const privateRouter = express.Router();

privateRouter.use(authenticate);          // EVERYTHING added here is authenticated

publicRouter.post("/auth/login", login);
privateRouter.get("/me", getMe);
privateRouter.use("/articles", articleRoutes);

app.use("/api", publicRouter);
app.use("/api", privateRouter);
```

New routes land in `privateRouter` by default; adding to `publicRouter` is a visible, reviewable decision. Add a test that walks every registered route and asserts that it requires authentication unless it's in an explicit public allowlist, or call each route unauthenticated and expect `401`.

---

## 7. Tenant scoping middleware

```js
export const requireOrgMember = (...roles) => async (req, res, next) => {
  const membership = await Membership.findOne({ userId: req.user.sub, orgId: req.params.orgId }).lean();
  if (!membership) return res.sendStatus(404);                        // hide the org's existence
  if (roles.length && !roles.includes(membership.role)) return res.sendStatus(403);
  req.org = { id: membership.orgId, role: membership.role };
  next();
};

router.get("/orgs/:orgId/projects", authenticate, requireOrgMember(), listProjects);
router.delete("/orgs/:orgId/projects/:id", authenticate, requireOrgMember("owner", "manager"), deleteProject);
```

Every query in those controllers must include `orgId: req.org.id`; scope helpers make that hard to forget.

---

## Errors and response consistency

| Situation | Status | Notes |
|---|---|---|
| No / bad / expired token | `401` | Client should refresh once |
| Authenticated, not allowed | `403` | Don't refresh; maybe show "not allowed" |
| Needs step-up / MFA / verification | `403` + `code` | Client opens the right UI |
| Not found **or** not visible to you | `404` | Prevents enumeration |
| Rate limited | `429` | `Retry-After` |

Express 5 forwards errors from rejected promises in async middleware to the error handler automatically; in Express 4 wrap async middleware so rejections don't hang the request:

```js
export const asyncHandler = (fn) => (req, res, next) => Promise.resolve(fn(req, res, next)).catch(next);
```

Central error handler: map `AuthError` types to statuses, never leak stack traces or internal ids, and log the details server-side.

---

## Anti-patterns

```js
// ❌ Authorization inside the controller, after doing work
async function deleteArticle(req, res) {
  const article = await Article.findById(req.params.id);
  await article.deleteOne();                    // oops: no check at all
}

// ❌ Role from the client
if (req.body.role === "admin") ...
if (req.headers["x-admin"] === "true") ...

// ❌ Order wrong: validation/DB work before authentication
router.post("/x", validate(schema), authenticate, handler);

// ❌ Only the UI hides the button
{user.role === "admin" && <DeleteButton />}      // fine for UX, never for security

// ❌ Silent fail-open
const allowed = policy[action]?.(user, resource);
if (allowed !== false) next();                   // undefined passes!
```

---

## Testing the pipeline

Test each route with a matrix of actors:

| Actor | Expected on `PATCH /articles/:id` (someone else's draft) |
|---|---|
| No token | `401` |
| Expired token | `401` |
| User, not owner | `404` (scoped lookup) |
| User, owner, draft | `200` |
| User, owner, published | `403` |
| Editor | `200` |
| Demoted admin (stale token) on admin route | `403` (fresh role check) |

```js
it("does not let a user read another user's order", async () => {
  const res = await request(app).get(`/api/orders/${bobsOrder.id}`).set("Authorization", `Bearer ${aliceToken}`);
  expect(res.status).toBe(404);
});
```

Test the **negative** cases most carefully: they are the security properties. See [09-testing-debugging](../09-testing-debugging/00-testing-auth.md).

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| A route forgot `authenticate` | Public data/actions by accident (use the private-router default) |
| `authorize` before `load` | `req.resource` undefined → crash, or policy skipped |
| Guard written as `if (...) next()` with no `else` | Request hangs |
| Calling `next()` and then also sending a response | "Headers already sent" and sometimes unauthorized side effects |
| Authenticating via `jwt.decode` | No signature check |
| Using `403` for missing resources | Confirms which ids exist |
| Catching errors and calling `next()` anyway | Fail-open on errors |
| Different guards on `GET` and `PATCH` of the same resource | Read leak that bypasses write rules |
| Middleware reads `req.user.role` for admin routes with stale tokens | Demotion not effective |

## Debugging

| Symptom | Cause |
|---|---|
| `Cannot read properties of undefined (reading 'role')` | `authenticate` didn't run before the guard |
| Everything `401` after deploy | `ACCESS_TOKEN_SECRET` mismatch across instances, issuer/audience mismatch, or clock skew |
| `403` only for some users | Role claim stale (re-login), or per-org membership missing |
| Request hangs | A guard neither called `next()` nor responded; async error not forwarded (Express 4) |
| `CastError: Cast to ObjectId failed` | Missing `isValidObjectId` check in the loader |

---

## Quick summary

- Pipeline: authenticate → account guards → role/permission → scoped load → policy → controller.
- Factories (`requirePermission`, `load`, `authorize`) make routes declarative; policies hold the logic.
- Default deny structurally: private router by default, explicit public allowlist.
- `401` = refresh, `403` = not allowed (with `code` for step-up), `404` = not yours or doesn't exist.
- Fresh DB checks for sensitive routes; test the negative cases hardest.

**Next:** [Hardening: rate limiting](../08-hardening/00-rate-limiting.md)
