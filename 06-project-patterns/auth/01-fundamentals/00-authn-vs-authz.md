# Authentication vs Authorization

Two questions every protected request must answer, in this order:

1. **Authentication (authn):** who are you?
2. **Authorization (authz):** are you allowed to do this?

Mixing them up is the root of many security bugs. A request can be perfectly authenticated and still be forbidden.

**Prerequisite:** [00-project-setup.md](../00-setup/00-project-setup.md)

---

## The difference

| | Authentication | Authorization |
|---|---|---|
| Question | Who is this? | What may they do? |
| Input | Password, token, passkey, OAuth login | Identity + role/permissions + the resource |
| Happens | At login and on every request (verify credential) | On every request, after authn |
| Failure status | `401 Unauthorized` | `403 Forbidden` |
| Lives in | `authenticate` middleware | `authorize` middleware, services |

Despite its name, `401 Unauthorized` means "not authenticated". `403` means "authenticated, but not allowed".

```text
Request
   │
   ▼
Authenticate ──fail──► 401
   │ ok (req.user set)
   ▼
Authorize ─────fail──► 403
   │ ok
   ▼
Controller
```

---

## In code

```js
// authn: proves identity, sets req.user
export const authenticate = (req, res, next) => {
  /* verify access token → req.user = { sub, sid, role } */
  next();
};

// authz: decides based on that identity
export const requireRole = (...roles) => (req, res, next) => {
  if (!roles.includes(req.user.role)) {
    return res.status(403).json({ message: "Forbidden" });
  }
  next();
};

router.get("/admin/users", authenticate, requireRole("admin"), listUsers);
```

Role checks are only half of authorization. The other half is **ownership**:

```js
// authenticated user, but is this THEIR order?
const order = await Order.findById(req.params.id);
if (!order || order.userId.toString() !== req.user.sub) {
  return res.status(404).json({ message: "Not found" });
}
```

Skipping this check is called **IDOR** (insecure direct object reference) or broken object-level authorization, and it is among the most common real-world API vulnerabilities. Returning `404` instead of `403` for resources the user shouldn't know exist avoids leaking their existence.

---

## Where each concept lives in this architecture

| Piece | Concern |
|---|---|
| Argon2id password check, refresh token, MFA, OAuth login | Authentication |
| Access JWT | Carries the result of authn (`sub`, `sid`) plus a `role` claim used by authz |
| `role`, permissions, ownership checks | Authorization |
| Email verification | Authentication strength (proves the email is theirs), not authorization |

A related term: **identification** is claiming an identity (typing an email); authentication is proving it.

---

## Common mistakes and misconceptions

- **"The user is logged in, so the request is safe."** Logged in only answers who. Every endpoint still needs its own authz decision.
- **Trusting the client.** Hiding an admin button in React is UX, not security. The API must enforce it.
- **Using `401` and `403` interchangeably.** Clients (for example an Axios interceptor that refreshes on `401`) rely on the difference. A `403` should not trigger a token refresh.
- **Stale roles in the JWT.** A `role` claim lives as long as the access token (about 10 minutes). If a role is revoked, it stays effective until expiry; for sensitive actions, re-check the database.
- **Authorizing only at the route level.** Also check inside services when the rule depends on data (ownership, tenant).

---

## Quick summary

- Authn proves identity; authz decides permission. Always authn first.
- `401` = not authenticated, `403` = authenticated but forbidden.
- Roles are not enough: enforce ownership on every resource access.
- Never rely on the frontend for authorization.

**Next:** [01-credential-types.md](./01-credential-types.md)
