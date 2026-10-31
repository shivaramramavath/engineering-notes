# Role-Based Access Control (RBAC)

Authentication told us **who** the caller is. Authorization decides **what they may do**. RBAC is the simplest widely used model: users get **roles**, roles carry **permissions**, and code checks permissions (or, in the smallest apps, roles directly).

**Prerequisites:** [00-authn-vs-authz.md](../01-fundamentals/00-authn-vs-authz.md), [02-jwt-access-tokens.md](../03-tokens/02-jwt-access-tokens.md)

---

## The model

```text
User ──has──► Role ──grants──► Permission ──on──► Resource/Action
Alice         editor           articles:create
                               articles:update
                               articles:publish
```

Why add the middle layer instead of giving users permissions directly: when the business says "editors can now archive articles", you change **one role**, not thousands of users.

| Term | Meaning | Example |
|---|---|---|
| Role | A named job function | `user`, `editor`, `admin` |
| Permission | One allowed action on a kind of resource | `articles:publish` |
| Assignment | A user-to-role link | Alice → `editor` |

---

## Start simple: a role on the user

This repo's user model already has a role:

```js
role: { type: String, enum: ["user", "admin"], default: "user" }
```

That is enough for many apps. Grow only when a real requirement appears:

| Need | Model |
|---|---|
| Few fixed levels | Single `role` string (current) |
| A user can hold several | `roles: [String]` |
| Permissions change without deploys, custom roles per customer | Roles and permissions in the database |
| Different role per organization/workspace | Membership collection (below) |

---

## Define roles in code

```js
// authz/roles.js
export const ROLES = Object.freeze({ USER: "user", EDITOR: "editor", ADMIN: "admin" });

export const PERMISSIONS = Object.freeze({
  [ROLES.USER]:   ["articles:read", "profile:update"],
  [ROLES.EDITOR]: ["articles:read", "articles:create", "articles:update", "profile:update"],
  [ROLES.ADMIN]:  ["articles:read", "articles:create", "articles:update", "articles:delete", "users:read", "users:update", "profile:update"],
});

export const can = (role, permission) => PERMISSIONS[role]?.includes(permission) ?? false;
```

Why **code** rather than the database to begin with: it is versioned, reviewed in pull requests, testable, and can't be changed by an attacker who gets write access to a data store. Move it to the DB only for customer-defined roles.

The `?? false` makes an unknown role or unknown permission **deny by default**, which is the rule for every authorization check.

### Role hierarchy (optional)

Roles that include everything below them:

```js
const RANK = { user: 1, editor: 2, admin: 3 };
export const atLeast = (role, min) => (RANK[role] ?? 0) >= RANK[min];
```

Hierarchies are convenient but rigid (the *second* special role breaks the line). Explicit permission lists per role are clearer.

---

## Where the role lives at request time

The access JWT carries `role` ([02-jwt-access-tokens.md](../03-tokens/02-jwt-access-tokens.md)), so basic checks need no database call:

```js
req.user = { sub, sid, role }       // from the verified token
```

But a role in a token is a **cached copy** that stays valid up to the token's lifetime:

```text
10:00  Admin demotes Bob to "user" in MongoDB
10:00-10:09  Bob's access token still says role: "admin"
10:10  token expires; refresh reads the NEW role from MongoDB
```

| Operation | Source for the role |
|---|---|
| Ordinary routes | The JWT claim (fast; staleness ≤ token TTL is acceptable) |
| Admin, destructive, financial routes | **Database** (or a step-up check): demotion applies immediately |
| Right after a role change | Also revoke the user's sessions so their next refresh reissues tokens ([04-logout-and-revocation.md](../04-sessions/04-logout-and-revocation.md)) |

Never trust a role sent by the client in a body, query, header, or localStorage. Only the signed token or the database counts.

---

## Per-organization roles (multi-tenant)

In a SaaS app, "admin" almost always means "admin **of this organization**", not of the whole system.

```js
// models/membership.model.js
const membershipSchema = new mongoose.Schema({
  userId: { type: ObjectId, ref: "User", required: true },
  orgId:  { type: ObjectId, ref: "Org",  required: true },
  role:   { type: String, enum: ["member", "manager", "owner"], required: true },
});
membershipSchema.index({ userId: 1, orgId: 1 }, { unique: true });
```

The role must then be looked up **for the organization in the request**:

```js
const membership = await Membership.findOne({ userId: req.user.sub, orgId: req.params.orgId });
if (!membership) return res.status(404).end();               // not a member: don't reveal the org exists
```

Global roles (platform `admin`) and tenant roles are separate concepts: keep different names (`platformAdmin` vs org `owner`) to avoid confusing them in code. Don't put per-org roles in the JWT; a user can belong to many orgs and memberships change.

---

## Granting roles safely

Role assignment is itself the most sensitive authorization decision.

- **No self-service**: never accept `role` from registration or profile-update bodies (mass assignment). Pick fields explicitly:

```js
const { email, password } = registerSchema.parse(req.body);     // role is not in the schema
await User.create({ email, passwordHash });                      // role defaults to "user"
```

- **Prevent privilege escalation**: a user may only grant roles at or below their own, and only to users in their scope.
- **Require step-up** and **notify** on role changes ([05-step-up-auth.md](../05-mfa/05-step-up-auth.md)).
- **Audit-log** who changed what, when ([05-logging-and-audit.md](../08-hardening/05-logging-and-audit.md)).
- **Bootstrap the first admin out of band** (a CLI script or migration), never through a public endpoint or "first user wins".
- Require MFA for privileged roles.

---

## What RBAC can't express

RBAC answers "may an *editor* update *articles*?". It cannot answer:

| Question | Needs |
|---|---|
| May Alice edit **this** article (hers) but not Bob's? | Ownership / resource-level rule |
| May a manager approve expenses **under $1000**? | Attribute-based rule |
| May this user view documents **shared with them**? | Relationship-based rule |
| May this action happen **only during business hours / from the office network**? | Context conditions |

Trying to force these into roles causes **role explosion** (`editor-of-team-a`, `editor-of-team-b`, `editor-under-1000`...). The next note adds policies and ownership checks on top of RBAC.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Role checks only in the frontend | Anyone can call the API directly |
| Trusting a client-supplied role | Instant privilege escalation |
| Allowing `role` in mass-assigned update bodies | Self-promotion to admin |
| Using a global `admin` role for org admins | Cross-tenant access |
| Unknown role/permission treated as allowed | Fail-open authorization |
| Not revoking after a demotion | Stale elevated access up to the token TTL |
| Hard-coding role names as strings across the code | Typos silently deny/allow; use constants |
| First-user-is-admin endpoints | Takeover of fresh deployments |
| Giving every internal user `admin` "for convenience" | No least privilege |

## Debugging

| Symptom | Cause |
|---|---|
| New role has no effect | Old access token still carries the previous role; refresh, or re-login |
| User still admin after demotion | Route trusts the JWT claim; check the DB for sensitive routes |
| `403` for everyone | Role constants mismatched (`"Admin"` vs `"admin"`), or `req.user` missing because `authenticate` didn't run first |
| User sees other orgs' data | Role looked up globally instead of per membership; queries not scoped by `orgId` |

---

## Quick summary

- RBAC = users → roles → permissions; check permissions, not role names, where you can.
- Define roles in code, deny by default, never trust client-supplied roles.
- JWT `role` is a cached copy: fine for routine routes, re-check the database for sensitive ones.
- Multi-tenant apps need per-organization memberships, not one global role.
- Roles can't express ownership or conditions; add policies next.

**Next:** [01-permissions-and-policies.md](./01-permissions-and-policies.md)
