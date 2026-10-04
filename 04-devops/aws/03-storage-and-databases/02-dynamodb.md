# DynamoDB: Serverless NoSQL

DynamoDB is a fully managed key-value and document database built for **predictable, single-digit-millisecond access at any scale**, with no servers, patching or capacity planning (unless you want it). It is the natural partner of [Lambda](../02-compute/02-lambda.md): no connection pools, scales to zero cost-wise when idle (in on-demand mode), and IAM-based access.

The catch, and the thing this note is really about: **DynamoDB rewards you for knowing your access patterns up front and punishes you for treating it like a relational database.** There are no joins, no ad-hoc queries, and the cheapest operations are the ones that use your key.

Prerequisites: [IAM](../01-foundations/03-iam.md); helpful: [Lambda](../02-compute/02-lambda.md).

---

## Core model

| Concept | Meaning |
|---|---|
| **Table** | Collection of items. Schemaless apart from the key. |
| **Item** | One record: a set of attributes. Max size **400 KB**. |
| **Attribute** | A name/value pair; types include string, number, binary, boolean, list, map, sets, null. |
| **Partition key (PK)** | Required. Hashed to decide **which physical partition** stores the item. |
| **Sort key (SK)** | Optional. Items sharing a PK are stored together, **sorted by SK**. |
| **Primary key** | PK alone, or PK + SK. Must uniquely identify an item. |

```
Table: Orders          PK: customerId     SK: orderId
 ┌────────────┬──────────────┬───────────────────────────┐
 │ customerId │ orderId      │ attributes…               │
 ├────────────┼──────────────┼───────────────────────────┤
 │ c#1        │ 2026-09-01#a │ total=40, status=SHIPPED  │
 │ c#1        │ 2026-09-14#b │ total=15, status=NEW      │   ← same PK, sorted by SK
 │ c#2        │ 2026-08-30#c │ total=99, status=NEW      │
 └────────────┴──────────────┴───────────────────────────┘
```

With this design, "all orders for customer c#1, newest first" is one fast query. "All orders with status NEW across all customers" is **not** supported by the key. You need an index or a different design.

---

## Reading and writing

```ts
import { DynamoDBClient } from "@aws-sdk/client-dynamodb";
import {
  DynamoDBDocumentClient, PutCommand, GetCommand, QueryCommand, UpdateCommand,
} from "@aws-sdk/lib-dynamodb";

const ddb = DynamoDBDocumentClient.from(new DynamoDBClient({}), {
  marshallOptions: { removeUndefinedValues: true },
});

// Write (replaces any existing item with the same key)
await ddb.send(new PutCommand({
  TableName: "Orders",
  Item: { customerId: "c#1", orderId: "2026-09-14#b", total: 15, status: "NEW" },
}));

// Read one item by full primary key
const { Item } = await ddb.send(new GetCommand({
  TableName: "Orders",
  Key: { customerId: "c#1", orderId: "2026-09-14#b" },
}));

// Query: one partition, optionally a sort-key range
const { Items, LastEvaluatedKey } = await ddb.send(new QueryCommand({
  TableName: "Orders",
  KeyConditionExpression: "customerId = :c AND orderId BETWEEN :a AND :b",
  ExpressionAttributeValues: { ":c": "c#1", ":a": "2026-09-01", ":b": "2026-09-30~" },
  ScanIndexForward: false,   // newest first
  Limit: 25,
}));
```

The `lib-dynamodb` **Document Client** converts normal JS objects to DynamoDB's typed format (`{ S: "x" }`, `{ N: "1" }`); the low-level client makes you do that by hand.

### Query vs Scan: the most important distinction

| | Query | Scan |
|---|---|---|
| Needs | The partition key (+ optional sort-key condition) | Nothing |
| Cost | Reads only matching items | **Reads the whole table**, then filters |
| Use | Your normal access path | Rare admin/analytics jobs, small tables |

A `FilterExpression` is applied **after** reading, so you still pay (in read capacity) for everything read, not just what's returned. If you find yourself scanning in a request path, your model or indexes are wrong.

### Updates, conditions and atomic counters

```ts
await ddb.send(new UpdateCommand({
  TableName: "Orders",
  Key: { customerId: "c#1", orderId: "2026-09-14#b" },
  UpdateExpression: "SET #s = :new, updatedAt = :now ADD version :one",
  ConditionExpression: "#s = :expected",                 // optimistic locking / state machine guard
  ExpressionAttributeNames: { "#s": "status" },          // 'status' is a reserved word
  ExpressionAttributeValues: { ":new": "SHIPPED", ":expected": "NEW", ":now": Date.now(), ":one": 1 },
}));
```

- **`ConditionExpression`** makes writes conditional. A failed condition throws `ConditionalCheckFailedException`, which is the tool for idempotency ("create only if not exists": `attribute_not_exists(pk)`), optimistic locking, and state-transition guards.
- Use `ExpressionAttributeNames` (`#s`) whenever an attribute name is a **reserved word** (`status`, `name`, `size`, `data`, ...). Otherwise you get a cryptic validation error.
- `ADD` / `SET x = x + :n` perform **atomic increments** server-side, so don't read-modify-write counters yourself.

### Pagination

A `Query`/`Scan` returns at most **1 MB** per call. If `LastEvaluatedKey` is present there's more. Pass it back as `ExclusiveStartKey`. `Limit` caps items *evaluated*, not necessarily items returned after filtering. Not paginating is a silent data-loss bug.

---

## Capacity and billing

Two modes, switchable on a table (with limits on how often you can switch):

| Mode | How it bills | Choose when |
|---|---|---|
| **On-demand** | Per request | New, spiky or unpredictable workloads; the sensible default to start |
| **Provisioned** | You set read/write capacity per second (optionally autoscaled) | Steady, predictable traffic where it's cheaper |

Capacity units, the rule of thumb:

- **1 Read Capacity Unit (RCU)** = one strongly consistent read of up to **4 KB** per second (an eventually consistent read costs half; a transactional read costs double).
- **1 Write Capacity Unit (WCU)** = one write of up to **1 KB** per second (transactional writes cost double).
- Items are rounded **up** to the next unit, so big items cost proportionally more. Keep items lean, and don't store large blobs in DynamoDB. Put them in [S3](./01-s3.md) and store the key.

**Consistency:** reads are **eventually consistent** by default. You can request a **strongly consistent** read (`ConsistentRead: true`) on tables and LSIs, at double the cost. **Global secondary indexes only support eventual consistency.**

There's also a **Standard-IA table class** (cheaper storage, pricier requests) for rarely accessed data. Check the current pricing page for numbers.

---

## Indexes: more access patterns on the same data

| | LSI (local) | GSI (global) |
|---|---|---|
| Partition key | Same as the table | **Any** attribute |
| Sort key | Different | Any attribute |
| Created | **Only at table creation** | Any time |
| Consistency | Strong or eventual | **Eventual only** |
| Capacity | Shares the table's | Its own (provisioned) / billed separately |

A GSI is effectively a **copy of your data re-keyed**, maintained asynchronously. Example: to answer "orders by status", add a GSI with PK `status` and SK `createdAt`. But beware **low-cardinality keys** like `status = NEW`, which funnel a lot of traffic to a few partitions. Add a shard suffix or a more selective key.

Project only the attributes you need into the GSI (`KEYS_ONLY`, `INCLUDE`, or `ALL`) to save storage and write cost. Every write to the base table that touches indexed attributes is also a write to the index.

---

## Data modeling: think in access patterns

Relational thinking: normalise first, query later. DynamoDB: **list your queries first, then design keys to serve them.**

1. Write down every access pattern ("get user by id", "list a user's orders newest first", "get order with its line items").
2. Choose PK/SK so the common ones are single `Get`/`Query` calls.
3. Add GSIs for the remaining patterns.
4. Accept denormalisation and duplication, since storage is cheap and joins don't exist.

**Single-table design** stores several entity types in one table, distinguished by key prefixes, so related items sit in the same partition and one `Query` fetches them together:

```
PK            SK                     data
USER#42       PROFILE                name=Asha, email=…
USER#42       ORDER#2026-09-14#b     total=15, status=NEW
USER#42       ORDER#2026-09-01#a     total=40, status=SHIPPED
```

`Query PK = USER#42` returns the profile and all orders; `begins_with(SK, "ORDER#")` returns only orders. It's powerful but harder to read and evolve. Use it when you've got well-understood access patterns and want maximum efficiency. For simpler apps, **one table per entity is perfectly legitimate**, and easier to maintain. Don't adopt single-table design as a cargo cult.

### Hot partitions

Throughput is spread across partitions by PK hash. A key that receives disproportionate traffic (a celebrity user, a single `status` value, a date-only PK for "today's data") throttles even if the table's overall capacity is fine. Pick **high-cardinality, evenly accessed partition keys**; shard with a suffix (`#0..#N`) if you must.

---

## Other features worth knowing

- **TTL**: mark an attribute (epoch seconds) as expiry; DynamoDB deletes expired items in the background at no write cost. Deletion isn't instant (it can lag), so filter expired items in queries if correctness matters.
- **Streams**: ordered change feed (insert/modify/delete) that can trigger Lambda: build projections, sync to search, send events. Delivery is at-least-once, so be idempotent.
- **Transactions** (`TransactWriteItems`/`TransactGetItems`): all-or-nothing across multiple items/tables, at roughly double the capacity cost, with item-count limits. Use sparingly.
- **Batch operations** (`BatchWriteItem`, `BatchGetItem`): reduce round trips but are **not transactional**, and may return **`UnprocessedItems`** you must retry.
- **Point-in-time recovery (PITR)** and on-demand **backups**: turn PITR on for important tables.
- **Global tables**: multi-region, multi-active replication, with last-writer-wins conflict resolution.
- **DAX**: in-memory cache in front of DynamoDB for microsecond reads on read-heavy workloads.
- **Encryption** at rest is always on; access control is IAM, including fine-grained conditions such as `dynamodb:LeadingKeys` to restrict a caller to its own partition key.
- **Local development**: DynamoDB Local lets you test without an AWS account (behaviour differs slightly from the real service).

---

## When *not* to use DynamoDB

- You need **ad-hoc queries, joins, complex reporting**, or your access patterns are unknown and changing → use [RDS/Aurora](./03-rds-and-aurora.md) (or export to S3 + Athena for analytics).
- Heavy **full-text search** → OpenSearch alongside it.
- Large objects → S3 with metadata in DynamoDB.

---

## Common mistakes

- **Using Scan** in request paths.
- Designing tables like relational schemas, then needing joins.
- Hot or low-cardinality partition keys.
- Not handling `LastEvaluatedKey` (silently missing data).
- Forgetting `ExpressionAttributeNames` for reserved words.
- Read-modify-write races instead of conditional or atomic updates.
- Using `PutItem` where you meant "create only if absent". `Put` overwrites silently, so add a `ConditionExpression`.
- Storing big payloads, which inflates read/write cost and risks the 400 KB limit.
- Assuming GSI reads are strongly consistent (they're not) and getting stale results right after a write.
- Ignoring **throttling** (`ProvisionedThroughputExceededException`): the SDK retries with backoff, but persistent throttling means a hot key or under-provisioned capacity.
- Treating TTL as precise.
- Not turning on PITR before you need it.

---

## Debugging

| Symptom | Check |
|---|---|
| `ValidationException` about reserved keywords/attribute names | Use `ExpressionAttributeNames` |
| `ValidationException: key element does not match the schema` | The `Key` must be exactly the table's PK (+SK) with the right types |
| `ConditionalCheckFailedException` | Your condition did its job. Handle it as a normal outcome, don't treat it as a crash |
| `ProvisionedThroughputExceededException` / throttling | CloudWatch `ThrottledRequests`, **Contributor Insights** to find hot keys; consider on-demand or better key distribution |
| Query returns fewer items than expected | Pagination (`LastEvaluatedKey`), filter applied after `Limit`, or eventual consistency on a GSI |
| Item "not found" right after writing via GSI | GSIs are eventually consistent; read the base table with a strongly consistent read |
| `AccessDeniedException` | IAM: the table ARN and `table/<name>/index/*` for index access (see [IAM](../01-foundations/03-iam.md)) |
| Unexpectedly high bill | Scans, large items, GSIs multiplying writes, provisioned capacity left high, or on-demand with a runaway loop |

---

## Quick Summary

- DynamoDB is a managed key-value/document store: fast and scalable **when you query by key**.
- Primary key = **partition key (+ sort key)**; the PK decides distribution, the SK gives ordered range queries within a partition.
- **Query** by key, avoid **Scan**; always handle **pagination**.
- Design from **access patterns**; add **GSIs** for extra patterns (eventually consistent); denormalise; single-table design is an option, not a requirement.
- Use **conditional writes** for idempotency, optimistic locking, and atomic updates; use `ExpressionAttributeNames` for reserved words.
- **On-demand** mode to start; keep items small (400 KB hard limit, cost scales with size); put blobs in S3.
- Avoid hot partitions; enable **PITR**; use TTL, Streams and transactions where they fit.
- Pick RDS/Aurora when you need relational flexibility.

**Next:** [RDS and Aurora](./03-rds-and-aurora.md)