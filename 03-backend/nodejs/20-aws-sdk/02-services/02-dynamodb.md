# DynamoDB

DynamoDB is a managed key-value / document database that stays fast at any size, with no servers, connections or indexes you forget to tune. The trade is that **you design the table around your access patterns up front**. There are no joins, ad-hoc queries are expensive, and a query without the right key is a full scan. If you come from MongoDB or SQL, most of the learning is unlearning "model the data, query it later".

Prerequisites: [SDK v3 and credentials](../01-setup/01-sdk-v3-and-credentials.md).

```bash
npm install @aws-sdk/client-dynamodb @aws-sdk/lib-dynamodb
```

---

## Core concepts

- **Table**: a collection of items. Each item is a bag of attributes, up to 400 KB.
- **Primary key**: required, unique per item, and the *only* thing DynamoDB can look up quickly. Two shapes:
  - **Partition key (PK)** alone: simple key.
  - **PK + sort key (SK)**: composite key. Many items can share a PK, ordered by SK.
- **Partition key** decides which physical partition stores the item (it's hashed). **Sort key** orders items within that partition and allows range queries.
- **Schemaless** beyond the key: other attributes can differ per item.

```text
Table: orders       PK = userId        SK = orderId
┌──────────┬──────────────┬──────────────┬──────────────┐
│ userId   │ orderId      │ status       │ total        │
├──────────┼──────────────┼──────────────┼──────────────┤
│ u#42     │ 01J9A...     │ PAID         │ 1200         │   ← one partition,
│ u#42     │ 01J9B...     │ SHIPPED      │  800         │     sorted by orderId
│ u#77     │ 01J9C...     │ PAID         │  300         │
└──────────┴──────────────┴──────────────┴──────────────┘
```

Reads, in cost order: **GetItem** (full key) → **Query** (PK equality + optional SK condition) → **Scan** (reads everything, avoid in app paths).

---

## The document client

The low-level `DynamoDBClient` speaks DynamoDB's typed JSON (`{ "S": "hello" }`). `lib-dynamodb`'s `DynamoDBDocumentClient` converts to and from plain JS objects. Use it.

```ts
import { DynamoDBClient } from "@aws-sdk/client-dynamodb";
import { DynamoDBDocumentClient } from "@aws-sdk/lib-dynamodb";

const client = new DynamoDBClient({ region: process.env.AWS_REGION });
export const ddb = DynamoDBDocumentClient.from(client, {
  marshallOptions: { removeUndefinedValues: true }, // else `undefined` fields throw
});
```

Create it once at module scope. In the document client you use `PutCommand`, `GetCommand`, `QueryCommand` and so on **from `lib-dynamodb`**, not the same-named commands from `client-dynamodb`.

---

## Basic CRUD

```ts
import { PutCommand, GetCommand, UpdateCommand, DeleteCommand } from "@aws-sdk/lib-dynamodb";

const TableName = process.env.ORDERS_TABLE!;

// create / replace
await ddb.send(new PutCommand({
  TableName,
  Item: { userId: "u#42", orderId: "01J9A", status: "PAID", total: 1200, createdAt: Date.now() },
}));

// read one (strongly consistent if you ask)
const { Item } = await ddb.send(new GetCommand({
  TableName, Key: { userId: "u#42", orderId: "01J9A" }, ConsistentRead: true,
}));

// partial update
await ddb.send(new UpdateCommand({
  TableName,
  Key: { userId: "u#42", orderId: "01J9A" },
  UpdateExpression: "SET #s = :s, updatedAt = :t",
  ExpressionAttributeNames: { "#s": "status" },       // `status` is a reserved word
  ExpressionAttributeValues: { ":s": "SHIPPED", ":t": Date.now() },
  ReturnValues: "ALL_NEW",
}));

await ddb.send(new DeleteCommand({ TableName, Key: { userId: "u#42", orderId: "01J9A" } }));
```

**`Put` replaces the whole item.** To change some attributes, use `Update`. `Get` of a missing key returns `Item: undefined`, not an error.

### Expression syntax

- `ExpressionAttributeNames` (`#name`) are placeholders for attribute *names*. You need them for reserved words (`status`, `name`, `data`, `timestamp` and many more) or odd characters.
- `ExpressionAttributeValues` (`:val`) are placeholders for *values*. Values are never interpolated into the string, which also avoids injection.
- `UpdateExpression` clauses: `SET` (assign), `REMOVE`, `ADD` (atomic number add), `DELETE` (remove from a set).

```ts
// atomic counter; creates the attribute if missing
UpdateExpression: "ADD views :one", ExpressionAttributeValues: { ":one": 1 }
// default if absent
UpdateExpression: "SET attempts = if_not_exists(attempts, :zero) + :one"
```

---

## Conditions: DynamoDB's concurrency tool

Any write can carry a `ConditionExpression`. If it's false, the write is rejected with `ConditionalCheckFailedException`. This is how you do "insert only if new" and optimistic locking without transactions.

```ts
try {
  await ddb.send(new PutCommand({
    TableName, Item: order,
    ConditionExpression: "attribute_not_exists(userId)", // no item with this key yet
  }));
} catch (err: any) {
  if (err.name === "ConditionalCheckFailedException") { /* already exists: duplicate request */ }
  else throw err;
}

// optimistic locking
await ddb.send(new UpdateCommand({
  TableName, Key,
  UpdateExpression: "SET #s = :new, version = version + :one",
  ConditionExpression: "version = :expected",
  ExpressionAttributeNames: { "#s": "status" },
  ExpressionAttributeValues: { ":new": "SHIPPED", ":expected": 3, ":one": 1 },
}));
```

Conditional puts are also the standard building block for **idempotency**: record the message/request ID with `attribute_not_exists` and treat a failure as "already processed" ([event-driven Lambda](../03-lambda/03-event-driven-lambda.md)).

---

## Query

```ts
import { QueryCommand } from "@aws-sdk/lib-dynamodb";

// latest 20 orders for a user
const res = await ddb.send(new QueryCommand({
  TableName,
  KeyConditionExpression: "userId = :u AND orderId > :after",
  ExpressionAttributeValues: { ":u": "u#42", ":after": "01J00" },
  ScanIndexForward: false, // descending by sort key
  Limit: 20,
}));
console.log(res.Items, res.LastEvaluatedKey);
```

- The **partition key needs equality**. The sort key can use `=`, `<`, `>`, `BETWEEN`, `begins_with`. Nothing else goes in `KeyConditionExpression`.
- Sort keys that sort chronologically (ULID, ISO timestamps) make "latest N" a one-liner.
- `FilterExpression` runs **after** items are read. You still pay for everything read, and `Limit` counts items *before* filtering, so a filtered query can return fewer than `Limit` items and still have more pages.

### Pagination

A single call returns at most 1 MB. If `LastEvaluatedKey` is present there's more; pass it back as `ExclusiveStartKey`. The SDK has paginators:

```ts
import { paginateQuery } from "@aws-sdk/lib-dynamodb";

const items: Record<string, any>[] = [];
for await (const page of paginateQuery({ client: ddb }, {
  TableName,
  KeyConditionExpression: "userId = :u",
  ExpressionAttributeValues: { ":u": "u#42" },
})) {
  items.push(...(page.Items ?? []));
}
```

For API pagination, send the client an opaque cursor, e.g. base64 of `LastEvaluatedKey`, and decode it into `ExclusiveStartKey` on the next request. Validate the decoded keys; don't trust a client-supplied cursor blindly.

**No `LastEvaluatedKey` means done. An empty `Items` array does not.** A page can be empty (all filtered out) while more pages remain.

---

## Secondary indexes

When you need another access pattern ("orders by status"), add an index instead of scanning.

| | GSI (global) | LSI (local) |
|---|---|---|
| Key | Any PK + optional SK | Same PK as table, different SK |
| Created | Anytime | Only at table creation |
| Consistency | Eventually consistent only | Can be strongly consistent |
| Capacity | Own throughput | Shares the table's |

```ts
await ddb.send(new QueryCommand({
  TableName,
  IndexName: "status-createdAt-index",
  KeyConditionExpression: "#s = :s AND createdAt > :t",
  ExpressionAttributeNames: { "#s": "status" },
  ExpressionAttributeValues: { ":s": "PAID", ":t": Date.now() - 86_400_000 },
}));
```

Notes: a GSI is updated asynchronously, so a just-written item may not show up immediately. Only items that have the index's key attributes appear in the index (a "sparse" index, useful on purpose). Choose which attributes to *project* into the index; fetching non-projected attributes isn't possible from the index.

---

## Designing keys (the part that matters)

1. **List access patterns first**: "get order by id", "list a user's orders newest first", "list orders by status".
2. Pick the PK so the hot reads are `Query`/`Get` on it. Use a **high-cardinality** key (user ID, order ID), not a low-cardinality one like `status`, or all traffic lands on one partition (a hot partition) and throttles.
3. Use sort keys for ordering and "one-to-many" (a user's orders, an order's items).
4. Add GSIs for the extra patterns.
5. **Single-table design** puts several entity types in one table with generic `PK`/`SK` attributes (`PK = USER#42`, `SK = ORDER#01J9A`) so related items are fetched in one query. It's powerful but not mandatory: a table per entity is a perfectly fine start while you're learning.

### Coming from MongoDB

| MongoDB | DynamoDB |
|---|---|
| Query any field, add an index later | Query only by key/index you designed for |
| `find({ status: "PAID" })` | Needs a GSI on `status`, else a Scan |
| `$lookup` / joins | None: denormalize, or fetch with several queries |
| Update with `$set` / `$inc` | `UpdateExpression` with `SET` / `ADD` |
| 16 MB documents | 400 KB items |
| Aggregation pipeline | None built in: precompute, or stream to analytics |

---

## Batches and transactions

```ts
import { BatchWriteCommand, TransactWriteCommand } from "@aws-sdk/lib-dynamodb";

// up to 25 puts/deletes per call, NOT atomic, can partially succeed
const res = await ddb.send(new BatchWriteCommand({
  RequestItems: { [TableName]: items.map((Item) => ({ PutRequest: { Item } })) },
}));
// you MUST retry res.UnprocessedItems (with backoff) or you silently lose writes

// all-or-nothing across items (and tables)
await ddb.send(new TransactWriteCommand({
  TransactItems: [
    { Put: { TableName, Item: order, ConditionExpression: "attribute_not_exists(userId)" } },
    { Update: { TableName: "inventory", Key: { sku }, UpdateExpression: "ADD stock :neg", ExpressionAttributeValues: { ":neg": -1 } } },
  ],
}));
```

Transactions cost about double the capacity of normal writes and cap at 100 items; use them where atomicity genuinely matters.

---

## Operational things to know

- **Capacity modes**: on-demand (pay per request, no planning) is the easy default; provisioned is cheaper for steady, predictable load. Strongly consistent reads cost twice an eventually consistent read.
- **Throttling**: `ProvisionedThroughputExceededException` or `ThrottlingException` means retry with backoff (the SDK already retries; see [errors and retries](../04-production/01-errors-and-retries.md)). Persistent throttling on one key means a hot partition.
- **TTL**: designate an attribute holding an **epoch-seconds** timestamp and DynamoDB deletes expired items in the background (not instantly). Filter out expired items in reads if exactness matters.
- **Numbers**: JS numbers are fine for ordinary values. For integers beyond `Number.MAX_SAFE_INTEGER`, look at `wrapNumbers` in `unmarshallOptions` or store them as strings.
- **Streams**: change events from a table can trigger Lambda ([event-driven Lambda](../03-lambda/03-event-driven-lambda.md)).
- **Local testing**: LocalStack or `amazon/dynamodb-local` ([testing](../01-setup/02-local-development-and-testing.md)).

---

## Permissions

```json
{
  "Effect": "Allow",
  "Action": ["dynamodb:GetItem", "dynamodb:PutItem", "dynamodb:UpdateItem", "dynamodb:DeleteItem", "dynamodb:Query"],
  "Resource": [
    "arn:aws:dynamodb:ap-south-1:111122223333:table/orders",
    "arn:aws:dynamodb:ap-south-1:111122223333:table/orders/index/*"
  ]
}
```

Querying an index needs permission on the `…/index/*` resource, which people forget.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Using `Scan` for app queries | Model a key or GSI for the access pattern |
| "Status" as the partition key | High-cardinality PK; put status in a GSI or SK |
| Reserved word in an expression (`status`, `name`) | `ExpressionAttributeNames` |
| `Put` to "update" one field, wiping the rest | `UpdateCommand` |
| Treating `FilterExpression` as an index | It only trims results after reading and billing them |
| Assuming an empty page means done | Check `LastEvaluatedKey` |
| Ignoring `UnprocessedItems` / `UnprocessedKeys` | Retry them |
| `undefined` in an item throws | `removeUndefinedValues: true` |
| Mixing `client-dynamodb` commands with the document client | Import commands from `lib-dynamodb` |
| Expecting a GSI read to see a write instantly | GSIs are eventually consistent |
| Forgetting the index ARN in IAM | Add `table/NAME/index/*` |

---

## Quick summary

- Design the key from your access patterns; Get/Query on the key are cheap, Scan is not.
- PK = hashed, needs equality; SK = ordered, supports ranges.
- Use `DynamoDBDocumentClient` and `lib-dynamodb` commands; reserved words need `#names`, values use `:values`.
- `Put` replaces; `Update` patches; conditions give you insert-if-new, optimistic locking and idempotency.
- Paginate with `LastEvaluatedKey` (paginators exist); filters don't reduce cost.
- GSIs add access patterns (eventually consistent); LSIs only at creation.
- Retry `UnprocessedItems`; transactions when atomicity matters.

## Next

[SQS](./03-sqs.md): decoupling work with queues.
