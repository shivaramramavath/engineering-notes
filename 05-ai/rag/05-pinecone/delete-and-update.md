# Deleting and Updating Records

## Concept
Pinecone-level operations for removing and changing data. These are the raw API calls; the document-level workflow built on them is in `11-document-management/`.

## Prerequisites
- [upsert.md](upsert.md)
- [query.md](query.md)

Call shapes use the newer options-object style; older SDKs scope the namespace on the handle (`index.namespace("ns")...`). Verify against your installed version.

## Delete

### By ID
```ts
await index.deleteMany({
  ids: ["handbook.pdf#0042", "handbook.pdf#0043"],
  namespace: "tenant-acme",
});

await index.deleteOne({ id: "handbook.pdf#0042", namespace: "tenant-acme" });
```
(Some versions accept a plain array of IDs instead of an options object; verify.)

### All records in a namespace
```ts
await index.deleteAll({ namespace: "chat-7f3a" }); // verify: older versions use index.namespace("chat-7f3a").deleteAll()
```
`deleteAll` targets the default namespace unless the call or the index handle is scoped to one. Check which namespace it hits before running it. Recent SDK versions also offer a call to delete the namespace itself (verify the current method name, such as `deleteNamespace`).

### By metadata filter (pods only)
```ts
await index.deleteMany({ filter: { docId: { $eq: "handbook.pdf" } } });
```
**Delete-by-filter is for pod-based indexes only. It is not supported on serverless indexes.** Since serverless is the default in this repository, use the ID-based approach below.

### Delete all chunks of one document (portable approach)
Works on serverless, assuming IDs like `<docId>#<chunkNo>`. It lists IDs by prefix with `listPaginated`, then deletes them with `deleteMany`:
```ts
async function listIdsByPrefix(index: Index, prefix: string, namespace: string): Promise<string[]> {
  const ids: string[] = [];
  let paginationToken: string | undefined;
  do {
    const page = await index.listPaginated({ prefix, limit: 100, paginationToken, namespace });
    for (const v of page.vectors ?? []) if (v.id) ids.push(v.id);
    paginationToken = page.pagination?.next;
  } while (paginationToken);
  return ids;
}

async function deleteDocument(index: Index, docId: string, namespace: string) {
  const ids = await listIdsByPrefix(index, `${docId}#`, namespace);
  for (let i = 0; i < ids.length; i += 1000) {              // verify per-request delete cap
    await index.deleteMany({ ids: ids.slice(i, i + 1000), namespace });
  }
}
```
(`Index` is the type exported by `@pinecone-database/pinecone`; verify the name and generics in your version.)

## Update

### Change metadata or values for one record
```ts
await index.update({
  id: "handbook.pdf#0042",
  metadata: { status: "archived" },     // merges into existing metadata
  namespace: "tenant-acme",
});
```
- `metadata` merges fields into existing metadata.
- Passing `values` replaces the vector.
- Update works on **one record by ID**. To change many records, loop over IDs (filter-based update is not available on serverless; verify for your index type).

### Replace a record entirely
Upsert again with the same ID. This overwrites vector **and** metadata.

## Updating a Changed Document
When a document's text changes, chunk counts and boundaries change, so old and new chunks don't line up one-to-one. A safe sequence:

1. Re-chunk and re-embed the new version.
2. Upsert the new chunks.
3. Delete any old chunk IDs beyond the new set (or delete the old set first, accepting a short gap).

```ts
const oldIds = await listIdsByPrefix(index, `${docId}#`, ns);
const newIds = new Set(newRecords.map((r) => r.id));

await index.upsert({ records: newRecords, namespace: ns });
const stale = oldIds.filter((id) => !newIds.has(id));
if (stale.length > 0) {
  await index.deleteMany({ ids: stale, namespace: ns });
}
```
Upsert-then-delete-stale avoids a window where the document has no chunks.

LlamaIndex.TS has no documented `refresh` method. The documented pattern is `await index.deleteRef(docId)` followed by `await index.insert(document)`, and `IngestionPipeline` with a docstore and `DocStoreStrategy.UPSERTS` automates much of it. See `11-document-management/update-documents.md`.

## Consistency Reminder
Deletes and updates are eventually consistent. A deleted record can appear in a query for a short time afterward.

## Important Rules
- Deterministic, prefix-based IDs make delete and update tractable.
- Delete by namespace is the cheapest way to remove a tenant or session.
- Upsert overwrites; `update` merges metadata.
- Deletion is permanent. Protect production data with backups of source documents, since vectors can be regenerated from them.

## Common Mistakes
- Random IDs, so a document cannot be found for deletion.
- Assuming filter-based delete works on serverless (it does not).
- Deleting then upserting, leaving a window with missing data.
- Forgetting the namespace argument, so deletes silently target the default namespace.

## Related / Next
- `11-document-management/delete-documents.md`
- `11-document-management/update-documents.md`
- `12-chat/unloading-data-in-chat.md`
