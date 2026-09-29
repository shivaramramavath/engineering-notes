# Multi-Tenancy

Supporting multiple tenants (customers, organizations, workspaces) in one application — the main strategies for structuring this in MongoDB/Mongoose, and the trade-offs between them.

## What "multi-tenant" means here

An application where multiple, unrelated customers (a company using your B2B SaaS product, for example) share the same running application, but their data must never be visible to each other. The core question this file answers: how should that data separation actually be structured in MongoDB?

---

## Strategy 1: Shared collection, tenant field on every document

```js
const taskSchema = new mongoose.Schema({
  tenantId: {
    type: mongoose.Schema.Types.ObjectId,
    required: true,
    index: true,
  },
  title: String,
  completed: Boolean,
});
```

```js
await Task.find({ tenantId: currentTenantId, completed: false });
```

Every document carries a `tenantId`, and every query filters by it. The simplest approach, and the one most applications start with.

### Enforcing it automatically with query middleware

```js
// dangerous WITHOUT enforcement — one forgotten filter leaks data across tenants
await Task.find({ completed: false }); // ❌ missing tenantId — returns EVERY tenant's tasks
```

Given how catastrophic a missing filter is here (unlike soft delete's "shows some extra old data," this is "shows another customer's private data"), automatic enforcement matters even more than in the soft-delete case:

```js
import { AsyncLocalStorage } from "node:async_hooks";

const tenantContext = new AsyncLocalStorage();

taskSchema.pre(
  ["find", "findOne", "countDocuments", "updateMany"],
  function (next) {
    const tenantId = tenantContext.getStore();
    if (!tenantId) {
      return next(new Error("No tenant context set for this query"));
    }
    if (this.getFilter().tenantId === undefined) {
      this.where({ tenantId });
    }
    next();
  },
);
```

```js
// middleware sets the context once per request
app.use((req, res, next) => {
  tenantContext.run(req.user.tenantId, next);
});
```

Using `AsyncLocalStorage` (a Node.js core module) to implicitly carry the current tenant through an entire request means **every** query automatically gets tenant-filtered without each call site needing to remember to pass it — and, notably, the middleware **throws** if no tenant context exists at all, rather than silently returning unfiltered data, since silent failure here is the worst possible outcome.

### Trade-offs

| Pro                                                         | Con                                                                                                                                              |
| ----------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| Simple to set up, works well for smaller numbers of tenants | A single missing `tenantId` filter is a serious data leak — needs very deliberate enforcement                                                    |
| Easy to run cross-tenant queries/reports (admin dashboards) | All tenants share the same indexes, storage, and — at real scale — the same performance characteristics; one very large tenant can affect others |
| One schema/deployment to maintain                           | Harder to give one tenant custom fields/behavior without complicating the shared schema for everyone                                             |

---

## Strategy 2: Database-per-tenant

```js
function getTenantConnection(tenantId) {
  return mongoose.createConnection(
    `mongodb://localhost:27017/tenant_${tenantId}`,
  );
}
```

```js
const tenantConn = getTenantConnection(tenantId);
const Task = tenantConn.model("Task", taskSchema);

await Task.find({ completed: false }); // naturally scoped — this connection only ever sees this tenant's database
```

Each tenant gets their own **database** (via `createConnection()`, `03-setup/02-connecting-to-mongodb.md`), with the same schema/models compiled against a tenant-specific connection each time.

### Trade-offs

| Pro                                                                                                                         | Con                                                                                                                                                                     |
| --------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Strong isolation — a bug can't leak data across tenants by forgetting a filter, since the databases are physically separate | Managing potentially hundreds/thousands of database connections is operationally heavier                                                                                |
| Easy to back up/restore/delete one tenant's data independently                                                              | Cross-tenant queries/reporting become much harder — no single query spans multiple tenant databases                                                                     |
| A tenant can genuinely have schema variations more easily                                                                   | More moving parts: connection pooling now needs to scale per-tenant, not just per-app-instance (`14-performance/04-connection-pooling.md`'s concerns multiply directly) |

This approach tends to make sense for a smaller number of larger, higher-value tenants (enterprise customers, each needing strong isolation/compliance guarantees) rather than thousands of small ones, given the operational overhead of managing many databases and connections.

---

## Strategy 3: Collection-per-tenant

```js
function getTenantModel(tenantId) {
  return (
    mongoose.models[`Task_${tenantId}`] ||
    mongoose.model(`Task_${tenantId}`, taskSchema, `tasks_${tenantId}`)
  );
}
```

Each tenant gets their own **collection** (within the same shared database) rather than their own field-filtered rows or their own entire database — a middle ground between the two strategies above.

### Trade-offs

Similar isolation benefits to database-per-tenant (a query against the wrong collection simply returns nothing, rather than leaking data), without needing separate database connections — but MongoDB has practical limits on the total number of collections/indexes a single deployment handles gracefully, making this a poor fit for a very large number of tenants (thousands+), and it shares database-per-tenant's difficulty with cross-tenant reporting.

---

## Choosing a strategy

| Situation                                                                                                                   | Likely fit                                                              |
| --------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| Many small tenants (a typical multi-tenant SaaS product)                                                                    | Shared collection + `tenantId` field, with rigorous automatic filtering |
| A smaller number of large, high-value enterprise tenants with strict isolation/compliance needs                             | Database-per-tenant                                                     |
| A moderate number of tenants, wanting more isolation than a shared collection but without full database-per-tenant overhead | Collection-per-tenant                                                   |
| Need for cross-tenant analytics/reporting as a core product feature                                                         | Favors shared collection — much harder across separate databases        |

There's no universally correct choice — it's a real trade-off between isolation strength, operational complexity, and how the product needs to query across tenants.

---

## A hybrid approach, in practice

Many real products land on: shared collections with `tenantId` filtering for most data (the common case), combined with database-per-tenant specifically for a small number of enterprise customers with contractual isolation requirements — using different strategies for different tenant tiers rather than forcing one approach to fit every tenant.

## Common mistakes

- **Relying on developers to remember `tenantId` on every query** in the shared-collection approach, without automatic enforcement — the single most dangerous mistake in multi-tenant applications, since the failure mode is a genuine cross-customer data leak, not just a bug.
- **Choosing database-per-tenant for a product with thousands of small tenants** — the operational overhead of managing that many databases/connections becomes a real burden disproportionate to the isolation benefit for low-value tenants.
- **Not indexing `tenantId`** in the shared-collection approach — every query filters on it, so it belongs in virtually every compound index on tenant-scoped collections.
- **Building cross-tenant admin/reporting features on top of a database-per-tenant architecture** without planning for it — becomes a real engineering challenge (querying and aggregating across many separate databases) if not considered from the start.

## Quick summary

- Shared collection + `tenantId` field is the simplest, most common approach — but requires rigorous, ideally automatic (e.g. via `AsyncLocalStorage` + query middleware) enforcement, since a missed filter leaks data across customers
- Database-per-tenant gives strong isolation at the cost of real operational complexity — fits a smaller number of large, high-isolation-need tenants better than many small ones
- Collection-per-tenant is a middle ground, limited by MongoDB's practical ceiling on total collections in one deployment
- The right choice depends on tenant count, isolation/compliance requirements, and whether cross-tenant reporting is a core product need — and a hybrid, tier-based approach is common in practice

## Section complete

That covers patterns and architecture — the repository/service pattern, a full soft-delete implementation, and multi-tenancy strategies. **`16-testing`** covers testing a Mongoose-backed application properly.
