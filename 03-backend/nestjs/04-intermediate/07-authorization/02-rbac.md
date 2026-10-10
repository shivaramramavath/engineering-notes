# RBAC (Role-Based Access Control)

In RBAC you assign users **roles**, and roles carry **permissions**. Instead of deciding access per user, you decide per role: "editors can publish, admins can delete users." It's the most common authorization model because it's easy to explain, administer, and audit. Its limit is equally well known: roles describe **who someone is**, not **which records** they may touch.

Prerequisites: [Authorization fundamentals](./01-authorization-fundamentals.md), [guards](../../03-core-concepts/01-request-pipeline/04-guards.md) (the `Reflector` + decorator pattern is introduced there).

## Simple roles on the user

For many apps, a role enum on the user is enough:

```ts
// auth/role.enum.ts
export enum Role {
  User = 'user',
  Editor = 'editor',
  Admin = 'admin',
}
```

```ts
// auth/decorators/roles.decorator.ts
import { Reflector } from '@nestjs/core';
import { Role } from '../role.enum';

export const Roles = Reflector.createDecorator<Role[]>();   // typed metadata decorator (Nest 10+)
```

```ts
// auth/guards/roles.guard.ts
@Injectable()
export class RolesGuard implements CanActivate {
  constructor(private readonly reflector: Reflector) {}

  canActivate(ctx: ExecutionContext): boolean {
    const required = this.reflector.getAllAndOverride(Roles, [ctx.getHandler(), ctx.getClass()]);
    if (!required?.length) return true;                       // no @Roles() → authentication only

    const { user } = ctx.switchToHttp().getRequest();
    return !!user && required.some((role) => user.roles?.includes(role));
  }
}
```

```ts
@Controller('posts')
export class PostsController {
  @Roles([Role.Editor, Role.Admin])
  @Post()
  create(@Body() dto: CreatePostDto) {}

  @Roles([Role.Admin])
  @Delete(':id')
  remove(@Param('id') id: string) {}
}
```

Register after the authentication guard so `req.user` exists:

```ts
providers: [
  { provide: APP_GUARD, useClass: JwtAuthGuard },     // 1. authenticate
  { provide: APP_GUARD, useClass: RolesGuard },       // 2. authorize (runs in registration order)
],
```

Guard order within one level matters: authentication must run first. (Using the string-based `SetMetadata` variant works the same way; see [custom decorators](../../03-core-concepts/01-request-pipeline/10-custom-decorators.md).)

### Deny by default for protected controllers

The guard above lets routes **without** `@Roles()` through (authentication only). That's reasonable for "any logged-in user" routes, but dangerous for admin areas where forgetting the annotation exposes everything. Options:

- Put `@Roles([Role.Admin])` on the **controller** of an admin module, so new routes inherit it.
- Or invert the default: require an explicit declaration on every route (`@Roles([...])` or `@AnyAuthenticated()`), failing closed when metadata is missing, and enforce it with a test that scans routes.

## Permission-based RBAC (recommended as you grow)

Hard-coding **role names** in controllers couples code to the org chart: adding a `moderator` role means editing every route. Instead, check **permissions** in code and map roles to permissions in one place:

```ts
// auth/permissions.ts
export enum Permission {
  PostCreate = 'post:create',
  PostPublish = 'post:publish',
  PostDelete = 'post:delete',
  UserManage = 'user:manage',
}

export const ROLE_PERMISSIONS: Record<Role, Permission[]> = {
  [Role.User]: [],
  [Role.Editor]: [Permission.PostCreate, Permission.PostPublish],
  [Role.Admin]: Object.values(Permission),
};
```

```ts
export const RequirePermissions = Reflector.createDecorator<Permission[]>();

@Injectable()
export class PermissionsGuard implements CanActivate {
  constructor(private readonly reflector: Reflector) {}

  canActivate(ctx: ExecutionContext): boolean {
    const needed = this.reflector.getAllAndOverride(RequirePermissions, [ctx.getHandler(), ctx.getClass()]);
    if (!needed?.length) return true;

    const { user } = ctx.switchToHttp().getRequest();
    const granted = new Set((user?.roles ?? []).flatMap((r: Role) => ROLE_PERMISSIONS[r] ?? []));
    return needed.every((p) => granted.has(p));               // require ALL listed permissions
  }
}
```

```ts
@RequirePermissions([Permission.PostPublish])
@Post(':id/publish')
publish(@Param('id') id: string) {}
```

Now routes say **what is needed** (`post:publish`); a single table says **which roles have it**. Adding a role or changing a grant is a one-line change. If permissions must be editable at runtime by admins, store the role-permission mapping in the database ([below](#database-model)) instead of a constant.

## Role hierarchies

Often "admin implies editor implies user". Model it explicitly rather than listing every role everywhere:

```ts
const HIERARCHY: Record<Role, Role[]> = {
  [Role.Admin]: [Role.Editor, Role.User],
  [Role.Editor]: [Role.User],
  [Role.User]: [],
};

const effectiveRoles = (roles: Role[]) =>
  new Set(roles.flatMap((r) => [r, ...HIERARCHY[r]]));
```

Check against the effective set. Hierarchies make permissions easier to reason about but can hide privilege escalation; keep them shallow.

## Database model

For dynamic roles/permissions:

```text
users            roles                 permissions            join tables
├── id           ├── id                ├── id                  user_roles(user_id, role_id)
├── ...          ├── name (unique)     ├── key ('post:create') role_permissions(role_id, permission_id)
```

Considerations:

- Index the join tables; the permission lookup runs on (nearly) every request ([indexing](../02-database-foundations/06-indexing-and-query-basics.md)).
- **Cache** a user's effective permissions briefly (per request or short TTL in Redis) and **invalidate** on role changes. Loading roles and permissions with joins on every request adds up.
- Use **seed/migrations** to define the built-in roles and permissions so they're versioned with the code ([migrations](../02-database-foundations/05-migrations.md)).
- Multi-tenant apps often need **per-tenant role assignments** (`user_roles(user_id, role_id, tenant_id)`); a user may be an admin of tenant A and a plain member of tenant B. Evaluate permissions in the context of the **current tenant** ([multi-tenancy](../../08-architecture-and-patterns/04-real-world-patterns/08-multi-tenancy.md)).

## Where do roles come from at request time?

| Source | Pros | Cons |
|--------|------|------|
| **Claims in the JWT** | No lookup per request | **Stale** until the token expires; revoking a role isn't immediate |
| **Loaded from the DB/cache per request** (in the strategy/guard) | Always current | An extra query/cache read per request |

For low-risk apps with short access tokens (minutes), JWT claims are fine. For sensitive operations (admin actions, money movement) **re-read current roles** from the source of truth. A common compromise: roles in the token for coarse routing, authoritative check against fresh data for high-impact actions. Changing roles should also **revoke refresh tokens/sessions** so stale claims don't outlive the change ([refresh tokens](../06-authentication/05-refresh-tokens-and-logout.md)).

## Assigning roles safely

- Only authorized users may change roles, via a **dedicated endpoint**, never through the generic user update (mass assignment: `PATCH /users/me { "role": "admin" }`). Don't include `role` in user-editable DTOs ([DTOs](../../03-core-concepts/02-validation-and-serialization/01-dto.md)).
- Prevent **privilege escalation**: an actor should not grant roles higher than their own, and the **last admin** shouldn't be removable.
- Log every role change with actor, target, old and new values ([audit logging](../../08-architecture-and-patterns/04-real-world-patterns/01-audit-logging.md)).
- Don't expose a default admin account with a known password; seed the first admin through a one-time, secured process.

## The limits of RBAC

RBAC answers "can editors publish posts?" It cannot answer:

- "Can **this** editor edit **this** post?" (ownership)
- "Can editors edit posts only in **their** team/tenant?"
- "Can a draft be edited but a published post not?" (state)

These need attributes of the **resource** and **context**, which is the domain of [ABAC and policies](./03-abac-and-policies.md). The usual combination:

```ts
@RequirePermissions([Permission.PostUpdate])         // coarse: RBAC in a guard
@Patch(':id')
async update(@Param('id') id: string, @Body() dto: UpdatePostDto, @CurrentUser() user: AuthUser) {
  const post = await this.posts.findOrFail(id);
  this.policy.assertCanUpdate(user, post);           // fine: ownership/state in the service
  return this.posts.update(post, dto);
}
```

### Role explosion

If you keep creating roles like `editor-of-team-a`, `read-only-editor`, `editor-without-delete`, you're encoding attributes as roles. That's the sign to move those distinctions into policies or per-resource grants instead of multiplying roles.

## Testing

Unit-test the guard with a real `Reflector` and decorated handlers ([testing pipeline components](../01-testing/04-testing-pipeline-components.md)): allowed role, disallowed role, multiple roles, no roles, no metadata, class-level vs handler-level decorators, and **unauthenticated** requests. In E2E tests assert the matrix of endpoints × roles, including denials ([E2E testing](../01-testing/06-e2e-testing.md)).

```ts
it.each([
  [[Role.User], false],
  [[Role.Editor], true],
  [[Role.Admin], true],
])('roles %j → allowed=%s', (roles, allowed) => {
  expect(guard.canActivate(ctxFor(handler, { roles }))).toBe(allowed);
});
```

## Common mistakes

- **Role names scattered through controllers** instead of permissions mapped centrally.
- **Roles read from stale JWTs** for sensitive operations.
- **RolesGuard running before authentication**, or missing entirely on a new module.
- **No `@Roles()` on an admin controller** and an allow-by-default guard.
- **Letting users edit their own `role`** via a generic update endpoint.
- **Role explosion** instead of attribute-based rules.
- **Using RBAC alone for ownership** ("editors can edit posts" becomes "editors can edit anyone's").
- **No audit trail** for role changes; **no escalation limits**.
- **Querying roles from the database on every request without caching.**
- **Forgetting tenant context** in multi-tenant role checks.

## Debugging

- `403` for a user who should have access: log `user.roles`, the required metadata, and the guard order. Check that metadata is read from both handler and class, and that the claim/role name matches the enum value exactly (`'admin'` vs `'ADMIN'`).
- Guard never denies: it's not registered, or the route has no metadata and your default is "allow".
- `req.user` undefined in `RolesGuard`: the authentication guard didn't run first, or the route is `@Public()`.
- Permission checks slow: profile the role/permission lookup; add caching and indexes.
- Role changes not taking effect: stale token claims; shorten token lifetime or revoke/refresh.

## Quick Summary

- RBAC: users → roles → permissions. Implement with a decorator (`Reflector.createDecorator`) + guard reading metadata from handler and class.
- Prefer **permission checks** in routes with a central role→permission map over hard-coded role names; keep hierarchies shallow.
- Source roles from fresh data for sensitive actions; JWT claims go stale; revoke tokens on role changes.
- Assign roles through guarded, audited endpoints with escalation limits; never via generic user updates.
- RBAC can't express ownership or state; pair it with resource-level policies (ABAC) and avoid role explosion.

## Next

[ABAC and policies →](./03-abac-and-policies.md)