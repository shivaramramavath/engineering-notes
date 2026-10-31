# Permissions and Policies

RBAC says what a **role** can do in general. Real rules depend on the **specific resource** and the **context**: "an editor can update articles, but only their own, and not after they're published". **Policies** are functions that make that decision in one place, so it isn't scattered across controllers.

**Prerequisite:** [00-rbac.md](./00-rbac.md)

---

## Three layers of authorization

```text
1. Coarse, role-based     "Is this user allowed to use this FEATURE at all?"   → permission check
2. Resource-based         "May they act on THIS record?"                        → policy (ownership, tenant, state)
3. Data scoping           "Which records may they SEE in a list?"               → query filter
```

Most real bugs come from doing layer 1 and forgetting layers 2 and 3.

---

## Permissions: naming

Use `resource:action`, optionally with a scope:

```text
articles:read
articles:update          (any article)
articles:update:own      (only your own)
users:disable
billing:export
```

Keep them **verbs on nouns**, specific enough to grant independently, but not one per endpoint. Check permissions in code and keep the role→permission map in a single module ([00-rbac.md](./00-rbac.md)); **don't** put long permission lists in the JWT (size, staleness). Put the role in the token and expand it server-side.

---

## Policies: decision functions

A policy takes the **actor**, the **action** and the **resource**, and returns allow/deny. Plain functions are testable and need no framework:

```js
// authz/policies/article.policy.js
import { can } from "../roles.js";

export const articlePolicy = {
  read(user, article) {
    if (article.status === "published") return true;                       // public once published
    return article.authorId.equals(user.sub) || can(user.role, "articles:update");
  },

  update(user, article) {
    if (article.status === "archived") return false;                       // state rule beats everything
    if (can(user.role, "articles:update")) return true;                    // editors/admins: any article
    return article.authorId.equals(user.sub) && article.status === "draft";// authors: own drafts only
  },

  delete(user, article) {
    return can(user.role, "articles:delete");                              // admins only
  },
};
```

Principles:

| Principle | In practice |
|---|---|
| **Default deny** | A policy ends in `return false`, and unknown actions are denied |
| **One place per resource** | Controllers call the policy; they don't re-implement the rule |
| **Pure and synchronous when possible** | Load data first, then decide; trivially unit-testable |
| **Deny rules first** | Check hard denials (archived, suspended) before grants |
| **Explicit actor shape** | `{ sub, role, orgId }`, not the whole request |

### ABAC: attributes and conditions

Attribute-Based Access Control means decisions use attributes of the user, resource and environment. The policy above already does it (`status`, `authorId`). More examples:

```js
approveExpense(user, expense) {
  const limit = { manager: 1000, director: 10000 }[user.role] ?? 0;
  return expense.amount <= limit && !expense.submittedBy.equals(user.sub);   // no self-approval
},
```

"No self-approval" is a **separation of duties** rule, which is something roles can never express.

---

## Resource ownership and tenancy

Two checks come up constantly.

**Ownership**: the resource belongs to the user.

```js
article.authorId.equals(user.sub)
```

**Tenant isolation**: the resource belongs to the user's organization. Every multi-tenant query and policy includes it:

```js
document.orgId.equals(membership.orgId)
```

Mongoose ids are ObjectIds: compare with `.equals()` or `String(a) === String(b)`, never `===` on ObjectId objects (always false) or loose string/ObjectId mixing, which silently denies (or worse, with sloppy `!=` logic, allows).

---

## Layer 3: scope the query (lists and bulk)

Policies guard single records. Lists need the rule **inside the query** so unauthorized rows are never fetched:

```js
// Wrong: fetch everything, filter in memory (leaks via pagination counts, wastes memory, easy to forget)
const all = await Article.find();

// Right: authorization as a query filter
export function articleScope(user) {
  if (can(user.role, "articles:update")) return {};                          // editors/admins see all
  return { $or: [{ status: "published" }, { authorId: user.sub }] };         // others: published + own
}

const articles = await Article.find({ ...articleScope(req.user), ...userFilters });
```

The same idea secures single-record lookups and prevents IDOR (insecure direct object reference):

```js
// Scoped lookup: a foreign id simply isn't found
const order = await Order.findOne({ _id: req.params.id, userId: req.user.sub });
if (!order) return res.status(404).json({ message: "Not found" });
```

Using `404` (not `403`) for "exists but isn't yours" avoids confirming that other people's ids exist. Use `403` when the resource's existence is already public and the user simply lacks the action.

Bulk operations (`updateMany`, `deleteMany`, "delete these ids") need the same scoping in the filter, or they act on other users' records.

---

## Field-level authorization

Different actors may read or write different **fields** of the same record:

```js
// Output: only expose what this viewer may see
export function serializeUser(target, viewer) {
  const base = { id: target.id, name: target.name, picture: target.picture };
  if (viewer.sub === target.id || can(viewer.role, "users:read")) {
    return { ...base, email: target.email, createdAt: target.createdAt };
  }
  return base;
}

// Input: whitelist writable fields per actor (prevents mass assignment)
const SELF_WRITABLE = ["name", "picture"];
const ADMIN_WRITABLE = [...SELF_WRITABLE, "role", "isActive"];

const allowed = can(req.user.role, "users:update") ? ADMIN_WRITABLE : SELF_WRITABLE;
const update = Object.fromEntries(Object.entries(req.body).filter(([k]) => allowed.includes(k)));
```

Never return the raw Mongoose document: `passwordHash`, `mfa.*` and internal flags leak. Use `select: false` on secrets and explicit serializers.

---

## Relationship-based access (sharing)

"Alice can view this doc because Bob **shared** it with her" is a relationship, not a role or attribute. For small cases, model it directly:

```js
document.sharedWith.some((s) => s.userId.equals(user.sub) && s.permission !== "none")
```

For rich graphs (folders inheriting permissions, teams in teams, org hierarchies) look at ReBAC systems inspired by Google's Zanzibar (OpenFGA, SpiceDB). They answer "does user U have relation R to object O?" at scale.

---

## When to use a library or engine

| Tool | Style | Good for |
|---|---|---|
| Hand-written policies (above) | Code | Most apps; clearest and cheapest |
| **CASL** (`@casl/ability`) | Rules in JS, works on frontend and backend | Sharing a rule definition with the UI, Mongo-style conditions |
| **Casbin** | Model + policy files (RBAC/ABAC) | Declarative models across languages |
| **OPA / Rego**, **Cerbos** | External policy service | Many services needing one policy source |
| **OpenFGA / SpiceDB** | Relationship graphs | Sharing models, nested hierarchies |

Check each project's current documentation before adopting; start with plain functions and graduate when duplication or complexity justifies it. The UI can use the same *rules* to hide buttons, but only the **server** decision counts.

---

## Testing policies

Policies are pure functions, so test them as a table:

```js
const cases = [
  // [actor,                         action,   resource,                                  expected]
  [{ sub: A, role: "user" },         "update", { authorId: A, status: "draft" },          true ],
  [{ sub: A, role: "user" },         "update", { authorId: B, status: "draft" },          false],
  [{ sub: A, role: "user" },         "update", { authorId: A, status: "published" },      false],
  [{ sub: E, role: "editor" },       "update", { authorId: B, status: "published" },      true ],
  [{ sub: E, role: "editor" },       "update", { authorId: B, status: "archived" },       false],
  [{ sub: X, role: "unknown" },      "delete", { authorId: B, status: "draft" },          false],
];

test.each(cases)("%j %s %j → %s", (user, action, article, expected) => {
  expect(articlePolicy[action](user, article)).toBe(expected);
});
```

Include the **denials** and edge cases (unknown role, missing fields), since that's where leaks live. Also add integration tests that call real endpoints as user A against user B's ids and expect `404/403`.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Checking the role but not the resource | IDOR: any logged-in user reads/edits others' data |
| Scoping single records but not lists/search/export | Bulk data leak |
| Filtering in application memory after a full fetch | Leaks via counts, pagination, timing, memory |
| Duplicating rules across controllers | Rules drift; one endpoint gets forgotten |
| Comparing ObjectIds with `===` | Always false → locks out owners, or inverted logic allows others |
| Returning full documents | Field leaks (hashes, internal flags) |
| Mass-assigning request bodies | Privilege escalation, tampering with `ownerId`, `role`, `price` |
| Fail-open default branch | Any unhandled case is allowed |
| Authorizing at the start of a long operation only | State changes mid-way (TOCTOU); re-check at commit |
| Policy depending on the client for facts ("`isOwner: true`" in the body) | Trivial bypass |
| Nested resources not cross-checked (`/orders/:orderId/items/:itemId`) | Access items of another order/user by mixing ids |

## Debugging

| Symptom | Cause |
|---|---|
| Owner gets `403` on own resource | ObjectId vs string comparison, or `req.user.sub` vs `_id` field mismatch |
| List returns too much or too little | Scope filter merged incorrectly (`$or` overwritten by spread of user filters, so combine with `$and`) |
| Policy passes in tests, fails in production | Production tokens carry a different role claim format, or stale role |
| Admin can't do something | Missing permission in the role map, and default-deny worked as designed |

Log denials with context (actor, action, resource id, reason) without secrets: patterns in denied requests reveal both bugs and probing ([05-logging-and-audit.md](../08-hardening/05-logging-and-audit.md)).

---

## Quick summary

- Three layers: feature permission → resource policy → query scoping. Do all three.
- Policies are pure, default-deny functions in one place; ABAC conditions and separation of duties live there.
- Put ownership and tenant filters **inside queries**; return `404` for others' records.
- Authorize fields on output and input; never return or accept raw documents.
- Test denials as thoroughly as grants.

**Next:** [02-middleware-patterns.md](./02-middleware-patterns.md)
