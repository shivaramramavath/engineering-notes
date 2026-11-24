# RBAC, ABAC & Policy Enforcement in Node.js

> Architecting access control in backend services: Role-Based Access Control (RBAC), Attribute-Based Access Control (ABAC), and CASL policy evaluation.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Libraries** | `@casl/ability` v6+ |
| **Patterns** | Role Matrices, Dynamic Attribute Guards |

---

## 1. RBAC vs. ABAC Comparison

- **RBAC (Role-Based Access Control)**: Permissions assigned to static roles (`Admin`, `Editor`, `Viewer`). Check is coarse-grained: `hasRole('Admin')`.
- **ABAC (Attribute-Based Access Control)**: Permissions evaluated dynamically against subject, action, resource attributes, and environment (e.g. "A user can edit a document IF they are the author AND the document is in draft status AND the request occurs during business hours").

---

## 2. Implementing Dynamic Policy with CASL

```typescript
import { AbilityBuilder, createMongoAbility } from '@casl/ability';

export function defineAbilityFor(user: { id: string; role: string; orgId: string }) {
  const { can, cannot, build } = new AbilityBuilder(createMongoAbility);

  if (user.role === 'admin') {
    can('manage', 'all'); // Full permissions
  } else {
    // Read any public document in the organization
    can('read', 'Document', { orgId: user.orgId, isPublic: true });

    // Update document only if author
    can('update', 'Document', { authorId: user.id });

    // Cannot delete archived documents
    cannot('delete', 'Document', { isArchived: true });
  }

  return build();
}

// Express/Fastify Middleware Guard:
export function requirePermission(action: string, subjectName: string) {
  return (req, res, next) => {
    const ability = defineAbilityFor(req.user);
    if (ability.can(action, subjectName)) {
      return next();
    }
    res.status(403).json({ error: 'Forbidden: Insufficient permissions' });
  };
}
```
