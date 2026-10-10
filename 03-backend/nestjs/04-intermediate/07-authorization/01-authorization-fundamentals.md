# Authorization Fundamentals

Authorization is a function: given **who** (the principal), **what they want to do** (the action), **on what** (the resource), and **in what situation** (the context), answer *allow* or *deny*. Everything in this section is a way of expressing and enforcing that function consistently.

```text
decide( subject: user, action: 'update', resource: post #42, context: { time, ip, tenant } ) → allow | deny
```

## The vocabulary

| Term | Meaning | Example |
|------|---------|---------|
| **Principal / subject** | The authenticated actor | User 7 with role `editor` |
| **Action** | What they attempt | `read`, `create`, `update`, `delete`, `publish` |
| **Resource** | The thing acted on | Post 42, an invoice, a tenant |
| **Context / environment** | Situational facts | Business hours, IP range, MFA recently verified |
| **Permission** | An action allowed on a resource type | `post:update` |
| **Role** | A named bundle of permissions | `editor` |
| **Policy** | A rule deciding access from attributes | "Authors can edit their own drafts" |

## Access-control models

| Model | Decides by | Strength | Limit |
|-------|------------|----------|-------|
| **ACL** (access control list) | A list per resource of who may do what | Simple, explicit | Doesn't scale to many resources/users |
| **RBAC** (role-based) | The user's roles | Easy to understand and administer | Can't express "only **their own** posts" without more |
| **ABAC** (attribute-based) | Attributes of user, resource, context | Very expressive (ownership, status, time) | More logic to design, test, and audit |
| **ReBAC** (relationship-based) | Relationships in a graph ("member of team that owns folder") | Models sharing/hierarchies naturally | Needs a relationship store/engine |

Real systems combine them: **RBAC for coarse capabilities, ABAC rules for resource-level checks**. Start with roles ([RBAC](./02-rbac.md)), add attribute rules where roles run out ([ABAC](./03-abac-and-policies.md)), and consider an engine such as CASL ([CASL](./04-casl-integration.md)) as rules multiply.

## Three places to enforce, and why you need all three

```text
            ┌─ Route level (guard):    "Is this user allowed to call POST /posts at all?"
Request ───►├─ Resource level:         "Is this user allowed to edit THIS post?"        (service/policy)
            └─ Data level:             "Which posts may they list?"                     (query scoping)
```

| Layer | Where | Typical rule |
|-------|-------|--------------|
| **Coarse** | Guard with `@Roles()`/`@RequirePermissions()` ([guards](../../03-core-concepts/01-request-pipeline/04-guards.md)) | Only editors can create posts |
| **Fine** | Service or policy, after loading the record | Only the author (or an admin) can update this post |
| **Data** | Repository/query filters | List endpoints return only the caller's tenant/records |

A guard alone **cannot** do resource-level checks: it runs before the record is loaded, and loading it in the guard duplicates queries and splits logic. Do coarse checks in guards, resource checks where the data is.

## Deny by default

- New endpoints should be **inaccessible** until explicitly allowed. Register the authentication guard globally ([guards](../../03-core-concepts/01-request-pipeline/04-guards.md)) and make permission annotations required for protected routes, so a forgotten annotation fails closed rather than open.
- A policy that doesn't match any `allow` rule means **deny**.
- Unknown roles, missing metadata, or an exception while evaluating a rule means **deny**.

## 401 vs 403 vs 404

| Status | When | Notes |
|--------|------|-------|
| `401 Unauthorized` | Not authenticated (no/invalid credentials) | Authentication failed |
| `403 Forbidden` | Authenticated, not allowed | The caller knows the resource may exist |
| `404 Not Found` | Not allowed **and you don't want to reveal existence** | Common for other tenants'/users' resources |

Returning `403` for `/invoices/123` confirms invoice 123 exists. For multi-tenant or private data, answer `404` for records the caller shouldn't know about; use `403` for actions on resources they can see but not modify. Pick a consistent policy per resource type.

## The #1 API vulnerability: broken object-level authorization (IDOR/BOLA)

```ts
// ❌ authenticated, but ANY user can read ANY order by guessing ids
@Get('orders/:id')
findOne(@Param('id') id: string) {
  return this.orders.findById(id);
}
```

The user is logged in (authentication passes), but nothing checks that the order **belongs to them**. Enumerating ids exposes everyone's data. Fix by checking ownership against the authenticated principal:

```ts
@Get('orders/:id')
async findOne(@Param('id') id: string, @CurrentUser() user: AuthUser) {
  const order = await this.orders.findById(id);
  if (!order || order.userId !== user.id) throw new NotFoundException();   // 404: don't reveal existence
  return order;
}
```

or, better, **scope the query** so a wrong record can't even be loaded:

```ts
findForUser(id: string, userId: string) {
  return this.repo.findOne({ where: { id, userId } });        // no match for other users' orders
}
```

Unguessable ids (UUIDs) reduce enumeration but are **not authorization**; ids leak through logs, URLs, and shared links.

Related traps:

- **Broken function-level authorization:** admin endpoints reachable by normal users because the check was forgotten.
- **Mass assignment / field-level:** a user updates `role` or `ownerId` by including it in the body ([DTOs and whitelisting](../../03-core-concepts/02-validation-and-serialization/02-validation-pipe.md)).
- **Excessive data exposure:** returning fields the caller shouldn't see ([serialization](../../03-core-concepts/02-validation-and-serialization/07-serialization.md)).

## Never trust the client

- Check on the **server, every request**. Hiding a button in the UI is usability, not security.
- Don't derive permissions from **request input** (`?isAdmin=true`, a `role` field in the body, an `X-User-Role` header).
- **Token claims go stale.** A role embedded in a JWT remains until the token expires, even if you've revoked it. For sensitive checks, re-read the current roles/permissions (cached briefly) from your source of truth, or keep access tokens short ([JWT](../06-authentication/04-jwt.md), [RBAC](./02-rbac.md)).
- Resource identifiers in URLs, bodies, and query params are all attacker-controlled; authorize against the **loaded** record, not what the client says about it (for example, a client-supplied `tenantId`).

## Centralize the decision

Scattered checks drift and get forgotten:

```ts
// ❌ copied in 40 places, each slightly different
if (user.role === 'admin' || post.authorId === user.id) { ... }
```

Put rules in **one place**: a policy service, per-resource policy functions, or CASL ([ABAC and policies](./03-abac-and-policies.md), [CASL](./04-casl-integration.md)). Then controllers and services ask `can(user, 'update', post)`. Benefits: one place to read, change, test, and audit.

## Multi-tenancy is authorization

In multi-tenant systems, **tenant isolation is the most important authorization rule**: every query must be scoped to the caller's tenant, derived from the authenticated principal (never from a request parameter). Enforce it at the data layer (repository filters, row-level security, or separate schemas/databases) so a forgotten `where` can't leak across tenants ([multi-tenancy](../../08-architecture-and-patterns/04-real-world-patterns/08-multi-tenancy.md)).

## Auditing and observability

- **Log denials** (who, action, resource id, reason) for security monitoring; alert on spikes (enumeration attempts).
- **Log sensitive allows** (admin actions, data exports) as an [audit trail](../../08-architecture-and-patterns/04-real-world-patterns/01-audit-logging.md).
- Keep logs free of secrets and personal data beyond what's needed.

## Testing authorization

Authorization bugs are invisible to happy-path tests. Build an **access matrix** and test it:

| Actor | `GET /orders/mine-1` | `GET /orders/other-1` | `DELETE /orders/mine-1` | `GET /admin/users` |
|-------|----------------------|------------------------|--------------------------|--------------------|
| Anonymous | 401 | 401 | 401 | 401 |
| User A (owner) | 200 | 404 | 204 | 403 |
| User B | 404 | 200 (their own) | 404 | 403 |
| Admin | 200 | 200 | 204 | 200 |

```ts
it.each([
  ['anonymous', null, 401],
  ['other user', userBToken, 404],
  ['owner', userAToken, 200],
])('GET /orders/:id as %s', async (_label, token, status) => {
  const req = request(app.getHttpServer()).get(`/orders/${order.id}`);
  if (token) req.set('Authorization', `Bearer ${token}`);
  await req.expect(status);
});
```

Test **denials** at least as thoroughly as allows ([E2E testing](../01-testing/06-e2e-testing.md)). Unit-test policy functions as pure functions with table-driven cases ([unit testing](../01-testing/02-unit-testing.md)).

## Common mistakes

- **Authenticating but not authorizing** at the resource level (IDOR/BOLA).
- **Relying on unguessable IDs** as protection.
- **Checks only in the UI** or only in some endpoints.
- **Trusting role/tenant values from the request** or from stale tokens.
- **Allow-by-default**: forgetting to annotate a new route.
- **Scattered `if (role === ...)` logic** that diverges over time.
- **Doing resource checks in guards** by re-loading records (or skipping them).
- **Leaking existence** with `403` where `404` is appropriate (or the reverse, confusing legitimate users).
- **Forgetting list endpoints, exports, search, and bulk operations** when adding checks to single-record endpoints.
- **Not testing denial cases.**

## Debugging

- A user sees data they shouldn't: find every code path that loads that resource (list, search, export, nested includes, GraphQL resolvers, background jobs) and check each is scoped.
- Intermittent 403s after role changes: stale claims in an access token; shorten lifetimes or re-check against fresh data.
- A guard "isn't running": confirm it's bound (global/controller/route) and that the route isn't marked `@Public()` ([guards](../../03-core-concepts/01-request-pipeline/04-guards.md)).
- Add temporary denial logging with the decision inputs (user id, action, resource id) to see why rules fire.

## Quick Summary

- Authorization = `decide(subject, action, resource, context) → allow | deny`.
- Models: ACL, RBAC, ABAC, ReBAC. Combine RBAC (coarse) with ABAC rules (resource-level).
- Enforce at three levels: route (guard), resource (policy in the service), and data (query scoping). A guard alone can't check ownership.
- Deny by default; `401` unauthenticated, `403` forbidden, `404` to hide existence; **IDOR/BOLA** is the classic failure.
- Never trust the client or stale token claims; centralize rules; treat tenant isolation as authorization.
- Test the access matrix, especially the denials, and log denials and sensitive actions.

## Next

[RBAC →](./02-rbac.md)