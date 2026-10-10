# Pinecone Setup

## Prerequisites
- A Pinecone account and an API key (Pinecone console → API Keys).
- Node 20+ (verify exact minimum for your SDK version) and the TypeScript project from `SETUP.md`: ESM (`"type": "module"` in `package.json`), run with `npx tsx`.

## Install
```bash
npm i @pinecone-database/pinecone       # official SDK
npm i llamaindex @llamaindex/openai @llamaindex/pinecone   # LlamaIndex.TS integration
npm i -D typescript tsx @types/node
```
(Verify: package names and versions on npm. Integrations are separate `@llamaindex/<name>` packages.)

Older tutorials use older SDK versions that differ in API shape, notably in how you get an index handle and how `upsert` is called. Check the version you installed with `npm ls @pinecone-database/pinecone`, and compare against the docs for that version.

## API Key and Configuration
Never hard-code keys.

```bash
# .env  (add to .gitignore)
PINECONE_API_KEY=pcsk_...
PINECONE_INDEX_NAME=docs
PINECONE_NAMESPACE=demo
OPENAI_API_KEY=sk-...
```

No dotenv package is needed. Node can load the file itself:

```bash
npx tsx --env-file=.env connect.ts
# or: node --env-file=.env dist/connect.js
```

```ts
import { Pinecone } from "@pinecone-database/pinecone";

const pc = new Pinecone({ apiKey: process.env.PINECONE_API_KEY! });
```

For serverless indexes you do **not** need an "environment" string as in older pod-based tutorials. You choose a cloud and region when creating the index. If a tutorial constructs the client as `new Pinecone({ apiKey, environment: "..." })`, it was written for pod-based indexes in older SDK versions; verify against your installed version before copying it.

## Connect to an Index
```ts
// By name (older style; the SDK looks up the host)
const byName = pc.index("docs");

// By host (skips the lookup; preferred in production)
const model = await pc.describeIndex("docs"); // newer versions: pc.indexes.describe("docs") (verify)
const index = pc.index({ host: model.host }); // newer API
```
The `pc.index({ host })` form is the newer one; `pc.index("docs")` is the older one. Verify against your installed version. Cache the host in config for production services so every request doesn't need a control-plane call.

## Verify the Connection
```ts
console.log(await pc.listIndexes());          // verify result shape per version
console.log(await index.describeIndexStats());
```
`describeIndexStats()` shows the dimension, total vector count, and per-namespace counts. It is the first thing to run when something looks wrong.

## Keys and Environments
- Use separate projects (and keys) for dev, staging and production.
- Give each environment its own index name, or its own project, so tests never touch real data.
- Rotate keys if they leak. Keys grant full access to the project (verify: whether your plan supports scoped or role-based keys).
- Keep the key on the server. Browser code must call your own backend, which holds the key.

## Common Mistakes
- Following an old tutorial (old SDK call shapes, `environment` in the constructor).
- Committing the API key or `.env`.
- Connecting to the wrong project and seeing an empty index.
- Querying immediately after upsert and assuming data is missing (see [upsert.md](upsert.md)).

## Related / Next
- [indexes.md](indexes.md)
- `16-production/security.md`
