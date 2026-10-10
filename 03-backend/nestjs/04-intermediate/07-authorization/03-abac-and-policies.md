# ABAC and Policies

Attribute-Based Access Control decides access from **attributes** of the user, the resource, the action, and the context, not just from a role. It's what you reach for when RBAC runs out: "authors can edit **their own** drafts", "managers can approve expenses **under their limit** for **their team**", "no deletes outside business hours". In practice, ABAC in a Nest app is a set of **policy functions**, evaluated in the right place, kept in one place.

Prerequisites: [Authorization fundamentals](./01-authorization-fundamentals.md), [RBAC](./02-rbac.md).

## Policies as plain functions

The heart of ABAC is a pure function from facts to a decision:

```ts
// posts/post.policy.ts
export type AuthUser = { id: string; roles: Role[]; teamId?: string };

export const postPolicy = {
  canRead(user: AuthUser, post: Post) {
    return post.status === 'published' || post.authorId === user.id || isAdmin(user);
  },

  canUpdate(user: AuthUser, post: Post) {
    if (isAdmin(user)) return true;
    return post.authorId === user.id && post.status === 'draft';     // owner, and only while a draft
  },

  canDelete(user: AuthUser, post: Post) {
    return isAdmin(user) || (post.authorId === user.id && post.status !== 'published');
  },
};

const isAdmin = (u: AuthUser) => u.roles.includes(Role.Admin);
```

Why plain functions are great:

- **Pure and synchronous** (given loaded data): trivial to unit-test with table-driven cases, no Nest container needed.
- **One place** for each resource's rules, instead of `if` statements across controllers.
- Easy to read in code review: the rules are the documentation.

Typical attributes:

| Kind | Examples |
|------|----------|
| **Subject** | id, roles, team, department, clearance, subscription tier |
| **Resource** | owner id, status (`draft`/`published`), tenant, sensitivity, team |
| **Action** | read, update, delete, approve |
| **Context** | time, IP/network, MFA freshness, request origin, amount thresholds |

## Where to enforce: not in the guard

Resource-level rules need the **resource**, which a guard doesn't have (it runs before the handler and before you've loaded anything). Loading the record inside a guard duplicates queries and splits logic. Enforce in the **service layer**, right after loading:

```ts
@Injectable()
export class PostsService {
  constructor(private readonly repo: PostsRepository) {}

  async update(id: string, dto: UpdatePostDto, user: AuthUser) {
    const post = await this.repo.findById(id);
    if (!post || !postPolicy.canRead(user, post)) throw new NotFoundException();      // hide existence

    if (!postPolicy.canUpdate(user, post)) throw new ForbiddenException();            // visible but not editable
    return this.repo.update(id, dto);
  }
}
```

Pattern: **load → check read access (404 if not) → check the specific action (403 if not) → act**. Keep coarse capability checks (`@RequirePermissions`) in guards ([RBAC](./02-rbac.md)); keep record-specific checks here.

### An injectable policy service

When policies need dependencies (a team lookup, feature flags), wrap them in a provider:

```ts
@Injectable()
export class PostPolicy {
  constructor(private readonly teams: TeamsService) {}

  async canUpdate(user: AuthUser, post: Post) {
    if (isAdmin(user)) return true;
    if (post.authorId === user.id && post.status === 'draft') return true;
    return this.teams.isManagerOf(user.id, post.teamId);          // attribute from another source
  }

  async assertCanUpdate(user: AuthUser, post: Post) {
    if (!(await this.canUpdate(user, post))) throw new ForbiddenException();
  }
}
```

Keep policies **side-effect free** (no writes, no emails); they decide, they don't act.

## The Nest "policies guard" pattern

You can still declare *which* policy applies to a route with metadata, then evaluate it where the data is available. The Nest docs show a guard that runs policy handlers declared via a decorator:

```ts
export interface PolicyHandler {
  handle(user: AuthUser, req: Request): boolean;
}

export const CHECK_POLICIES_KEY = 'check_policy';
export const CheckPolicies = (...handlers: PolicyHandler[]) => SetMetadata(CHECK_POLICIES_KEY, handlers);

@Injectable()
export class PoliciesGuard implements CanActivate {
  constructor(private readonly reflector: Reflector) {}

  canActivate(ctx: ExecutionContext) {
    const handlers = this.reflector.get<PolicyHandler[]>(CHECK_POLICIES_KEY, ctx.getHandler()) ?? [];
    const req = ctx.switchToHttp().getRequest();
    return handlers.every((h) => h.handle(req.user, req));
  }
}
```

This is useful for **resource-independent** rules (subject and context only: "must have a verified email", "MFA within the last 10 minutes", "only from the office network"). It **cannot** check a specific record unless the handler loads it. For record-level rules use the service-layer pattern above, or a library like [CASL](./04-casl-integration.md) that formalizes both.

## Data-level authorization: list queries

Single-record checks are easy to remember. **Lists, searches, exports, counts, and aggregates** are where leaks hide, because there's no one record to check. Authorization must be part of the **query**:

```ts
// scope by what the user may see, in the repository
async listForUser(user: AuthUser, page: number, limit: number) {
  const where = isAdmin(user)
    ? {}
    : { OR: [{ status: 'published' }, { authorId: user.id }] };       // same rule as canRead, expressed as a filter
  return this.repo.find({ where, take: limit, skip: (page - 1) * limit });
}
```

Rules:

- **Express the read policy twice consistently**: as a predicate (`canRead`) and as a query filter. Divergence between them is a classic bug (the detail endpoint denies, the list reveals). Libraries like CASL can derive the filter from the same rules ([CASL](./04-casl-integration.md)).
- **Never filter in memory after loading everything**: it's slow, leaks via counts and pagination metadata, and tempts you to skip it.
- **Tenant scoping** belongs in the data layer so it can't be forgotten ([multi-tenancy](../../08-architecture-and-patterns/04-real-world-patterns/08-multi-tenancy.md)).
- Don't forget **related data**: includes/populates/joins and GraphQL nested resolvers can expose records the parent policy didn't consider ([GraphQL auth](../../05-advanced/05-graphql/03-graphql-auth-and-security.md)).
- Consider **database row-level security** (for example PostgreSQL RLS) as defense in depth for critical isolation, keeping its trade-offs (complexity, connection/session setup) in mind.

## Field-level authorization

Different principals may see or change different **fields** of the same record.

**Reading:** shape the response per audience (separate response DTOs, or serialization groups) rather than returning one object to everyone ([serialization](../../03-core-concepts/02-validation-and-serialization/07-serialization.md)).

```ts
const toResponse = (user: AuthUser, post: Post) => ({
  id: post.id,
  title: post.title,
  ...(postPolicy.canUpdate(user, post) ? { internalNotes: post.internalNotes } : {}),
});
```

**Writing:** whitelist which fields a given principal may modify. Admin-only fields (`role`, `ownerId`, `status` overrides) must not be accepted from ordinary users even if the DTO has them:

```ts
const EDITABLE_BY_AUTHOR = ['title', 'body'] as const;
const EDITABLE_BY_ADMIN = [...EDITABLE_BY_AUTHOR, 'status', 'ownerId'] as const;

const allowed = isAdmin(user) ? EDITABLE_BY_ADMIN : EDITABLE_BY_AUTHOR;
const changes = pick(dto, allowed);
```

Reject (or ignore, deliberately) fields outside the allowed set ([mass assignment](./01-authorization-fundamentals.md)).

## Context attributes

Environment-based rules are part of ABAC:

```ts
canApprovePayment(user: AuthUser, payment: Payment, ctx: { mfaAt?: Date }) {
  const fresh = ctx.mfaAt && Date.now() - ctx.mfaAt.getTime() < 10 * 60_000;
  return user.roles.includes(Role.Finance) && payment.amount <= user.approvalLimit && !!fresh;
}
```

Pass context explicitly to policies; don't have them reach into globals like `Date.now()` hidden inside, which makes them hard to test (inject or pass the time). Step-up requirements (fresh MFA) connect to [two-factor authentication](../06-authentication/10-two-factor-authentication.md).

## Relationship-based access (ReBAC) and external engines

Sharing models ("anyone in a team that owns the folder", "viewers of the parent can view children") are about **relationships**, which are awkward as attribute checks. Options:

- Model the relationship in your database and query it in the policy ("is this user a member of `post.teamId`?"), which is fine for modest cases.
- Use a dedicated authorization system built on relationship tuples (Zanzibar-style systems such as OpenFGA or SpiceDB) when sharing/hierarchy logic gets deep or must be consistent across services.
- Use a policy engine or language (for example Open Policy Agent with Rego, or Cedar) to express policies as data/code outside the app, shared across services.

These add operational weight (another service, latency, consistency questions). Start with in-app policy functions; adopt an external engine when multiple services need the same decisions or policies must be managed by non-developers.

## Organizing policies

```text
src/posts/
├── posts.controller.ts
├── posts.service.ts          ← calls policy.assertCan...()
└── post.policy.ts            ← all rules for the Post resource
```

- One policy module per resource type; name functions by action (`canUpdate`).
- Policies take **already-loaded** data and return booleans (or throw via `assert*` helpers).
- Prefer **explicit allow rules**; end with deny.
- Share small helpers (`isAdmin`, `isOwner`) but don't build a general "rule DSL" until you need one.

## Testing

Policies are the easiest security code to test thoroughly; do it as a table:

```ts
describe('postPolicy.canUpdate', () => {
  const owner = { id: 'u1', roles: [Role.User] };
  const other = { id: 'u2', roles: [Role.User] };
  const admin = { id: 'u3', roles: [Role.Admin] };

  it.each([
    ['owner, draft',      owner, { authorId: 'u1', status: 'draft' },     true],
    ['owner, published',  owner, { authorId: 'u1', status: 'published' }, false],
    ['other user, draft', other, { authorId: 'u1', status: 'draft' },     false],
    ['admin, published',  admin, { authorId: 'u1', status: 'published' }, true],
  ])('%s → %s', (_n, user, post, expected) => {
    expect(postPolicy.canUpdate(user, post as Post)).toBe(expected);
  });
});
```

Then E2E tests that confirm the policy is actually **called** on every endpoint (including list/search/export) and that denial responses use the intended status (403 vs 404) ([E2E testing](../01-testing/06-e2e-testing.md)).

## Common mistakes

- **Putting resource-level checks in guards** that never loaded the resource, or skipping them.
- **Checking single-record endpoints but not lists, search, exports, bulk operations, or nested relations.**
- **Read policy and list filter diverging.**
- **Filtering in memory** after fetching unauthorized data.
- **Accepting admin-only fields** from ordinary users (mass assignment).
- **Policies with side effects or hidden globals** (time, DB writes), making them untestable.
- **Scattering `if (role === 'admin')`** instead of one policy per resource.
- **Trusting client-supplied owner/tenant ids.**
- **Allow-by-default** helper functions that return `true` when an attribute is missing.
- **Leaking existence** via `403` vs `404` inconsistencies.
- **Reaching for an external policy engine** before in-app functions are insufficient.

## Debugging

- Log the decision inputs on denial: user id and roles, action, resource id, relevant attributes (owner, status, tenant) and which rule matched.
- A user can read via the list but not the detail endpoint (or vice versa): compare the filter with the predicate.
- Unexpected allow: look for rules that default to `true` when data is missing (undefined owner id, null tenant).
- Intermittent behavior: policy depends on stale cached data (team membership); check cache TTLs and invalidation.
- Unit-test the exact failing combination as a new row in the table before fixing.

## Quick Summary

- ABAC decides from attributes of subject, resource, action, and context; implement as **pure policy functions**, one module per resource.
- Enforce **resource-level** rules in the service after loading (`load → 404 if unreadable → 403 if not allowed → act`); keep coarse capability checks in guards; guards can host resource-independent context rules.
- Authorization must apply to **queries** too: scope lists/search/exports by the same policy, and never filter in memory.
- Handle **fields**: shape responses per audience and whitelist writable fields per principal.
- Add ReBAC/policy engines only when relationships or cross-service consistency demand it; test policies with tables and verify wiring in E2E tests.

## Next

[CASL integration →](./04-casl-integration.md)