# Readers and LlamaParse

A **reader** turns an external source (a file, a folder, a database row, a web page) into `Document` objects. Everything downstream (chunking, embedding, retrieval) is only as good as the text the reader extracts. Bad PDF extraction, with tables flattened and columns interleaved, cannot be fixed by a better embedding model later.

> Prerequisites: [../01-core/02-documents-and-nodes](../01-core/02-documents-and-nodes.md).

## What a reader is

A reader is any object with a `loadData(...)` method that returns `Promise<Document[]>`. That is the whole contract, which means writing your own is easy (see "Custom readers").

File readers live in a separate package:

```bash
npm i @llamaindex/readers
```

The package provides readers for common formats, each importable by path:

```ts
import { CSVReader } from "@llamaindex/readers/csv";
import { DocxReader } from "@llamaindex/readers/docx";
import { HTMLReader } from "@llamaindex/readers/html";
import { ImageReader } from "@llamaindex/readers/image";
import { JSONReader } from "@llamaindex/readers/json";
import { MarkdownReader } from "@llamaindex/readers/markdown";
import { ObsidianReader } from "@llamaindex/readers/obsidian";
import { PDFReader } from "@llamaindex/readers/pdf";
import { TextFileReader } from "@llamaindex/readers/text";
```

Single file:

```ts
const reader = new TextFileReader();
const docs = await reader.loadData("./data/notes.txt");
```

## Loading a folder: `SimpleDirectoryReader`

`SimpleDirectoryReader` walks a directory (and its subdirectories) and delegates each file to a reader chosen by file extension.

```ts
import { SimpleDirectoryReader } from "@llamaindex/readers/directory";

const reader = new SimpleDirectoryReader();
const documents = await reader.loadData("./data");
```

To override or add readers per extension, use the object form and `fileExtToReader` (keys are extensions **without** the dot):

```ts
import { SimpleDirectoryReader } from "@llamaindex/readers/directory";
import { MarkdownReader } from "@llamaindex/readers/markdown";

const documents = await new SimpleDirectoryReader().loadData({
  directoryPath: "./data",
  fileExtToReader: {
    md: new MarkdownReader(),
  },
});
```

Notes from the docs: it reads files concurrently (up to 9 at a time), and `fileExtToReader` can both replace the reader for a known type and add support for new ones. Files with an unrecognized extension and no mapping are not read, so a "missing" file is usually a missing mapping.

## Other sources

The API reference lists readers for specific systems (for example `SimpleMongoReader`, `SimplePostgresReader`, `SimpleCosmosDBReader`, and audio-transcript readers). Their constructor options differ, so check the reference for the exact class you need rather than guessing.

I did not find a documented generic web-page reader for the TypeScript package. If you need one, write a small reader (below) with `fetch` and an HTML-to-text step.

## Custom readers

Because the contract is just `loadData`, a reader for your own system is a few lines. A database example:

```ts
import { Document } from "llamaindex";

class TicketReader {
  constructor(private db: { query(sql: string): Promise<any[]> }) {}

  async loadData(): Promise<Document[]> {
    const rows = await this.db.query(
      "select id, title, body, updated_at from tickets",
    );
    return rows.map(
      (r) =>
        new Document({
          id_: `ticket-${r.id}`, // stable ID, so re-ingestion can upsert
          text: `${r.title}\n\n${r.body}`,
          metadata: { source: "tickets", ticketId: r.id, updatedAt: r.updated_at },
          excludedEmbedMetadataKeys: ["ticketId", "updatedAt"],
          excludedLlmMetadataKeys: ["ticketId", "updatedAt"],
        }),
    );
  }
}
```

Decisions that matter here: one document per logical unit (a ticket, not one giant table dump), a deterministic `id_`, and keeping noisy metadata out of embeddings. These are what make incremental updates work later (see [02-ingestion-pipelines](./02-ingestion-pipelines.md)).

## PDFs and why plain extraction often fails

`PDFReader` extracts the text layer. That is fine for simple prose PDFs. It struggles with:

- tables (cells come out as an unstructured stream of words),
- multi-column layouts (columns interleave),
- scanned documents with no text layer,
- charts and figures.

Also, `PDFReader` uses Node-specific APIs (fs, child_process, crypto) and does not work in Edge runtimes. In Edge, import readers by file path and use a hosted parser instead.

## LlamaParse

LlamaParse is LlamaIndex's hosted document parser. It converts PDFs, scans, Office files, tables and charts into clean markdown (or structured output), using layout-aware OCR. It is a paid cloud service (with a free allowance at the time of writing; check the pricing page), and your documents are uploaded to it. Factor that into any confidentiality decision.

Get an API key from the LlamaCloud dashboard and set it:

```bash
export LLAMA_CLOUD_API_KEY="llx-..."
```

### Package status (read this first)

There are two ways you will see LlamaParse used from TypeScript:

1. **`LlamaParseReader`** (older). Many tutorials and some current pages in the framework docs still show it with `SimpleDirectoryReader`, imported from `llama-cloud-services`:
   ```ts
   import { LlamaParseReader } from "llama-cloud-services";
   const reader = new LlamaParseReader({ resultType: "markdown" });
   ```
   The `llama-cloud-services` package is marked **deprecated** in favor of the newer SDK. It still works in many setups, but don't start new projects on it without checking its current status.

2. **`@llamaindex/llama-cloud`** (current SDK). You call the parsing API directly and build `Document`s yourself. This is what the LlamaParse docs now recommend.

### Current SDK: parse a PDF into Documents

```bash
npm i @llamaindex/llama-cloud
```

```ts
import LlamaCloud from "@llamaindex/llama-cloud";
import fs from "node:fs";
import { Document } from "llamaindex";

const client = new LlamaCloud(); // reads LLAMA_CLOUD_API_KEY

async function parsePdf(path: string): Promise<Document[]> {
  const file = await client.files.create({
    file: fs.createReadStream(path),
    purpose: "parse",
  });

  // blocks until the job finishes; the SDK handles polling
  const result = await client.parsing.parse({
    file_id: file.id,
    tier: "agentic",
    version: "latest",
    expand: ["markdown"],
  });

  return result.markdown.pages.map(
    (p) =>
      new Document({
        id_: `${path}#page-${p.page_number}`,
        text: p.markdown,
        metadata: { source: path, page: p.page_number },
      }),
  );
}
```

Things to know:

- `expand` controls what comes back. `["markdown"]` gives per-page markdown. Other values (`items`, `text`, `metadata`) return the structured tree, plain text, or confidence scores; see the Retrieving Results guide.
- **`tier`** trades cost and quality. The docs describe `fast`, `cost_effective`, `agentic` and `agentic_plus`. Custom prompts (via `agentic_options.custom_prompt`) work on all but `fast`. Pick the cheapest tier that gets your hardest documents right; test on a sample.
- One `Document` per page keeps page numbers in metadata for citations. If your content spans pages (a table that crosses a page break), consider joining pages first.
- Markdown output pairs well with `MarkdownNodeParser` (see [../01-core/02-documents-and-nodes](../01-core/02-documents-and-nodes.md)), which splits on headings.

### Don't re-parse every run

Parsing costs money and time. Cache the markdown on disk keyed by file path plus a content hash, and only call the API when the file changed:

```ts
import { createHash } from "node:crypto";

async function parsePdfCached(path: string, cacheDir = "./.parse-cache") {
  const bytes = fs.readFileSync(path);
  const key = createHash("sha256").update(bytes).digest("hex");
  const cacheFile = `${cacheDir}/${key}.json`;

  if (fs.existsSync(cacheFile)) {
    const pages = JSON.parse(fs.readFileSync(cacheFile, "utf-8"));
    return pages.map(toDocument(path));
  }
  // ...call parsePdf, write the page array to cacheFile...
}
```

An ingestion pipeline's cache (next note) caches transformations, not the parse, so parse caching is your job.

## Common mistakes

**Trusting the default PDF reader for tables.** If answers about figures in a PDF are wrong, look at the extracted text before blaming retrieval.

**Copying `LlamaParseReader` snippets blindly.** They work against an older package that is deprecated. Check which package a tutorial imports.

**Missing `fileExtToReader` entries.** Folder loads silently skip unsupported extensions.

**Unstable IDs.** Readers that generate random IDs make every run look like new data.

**Uploading sensitive documents to a hosted parser** without checking data retention and compliance terms.

## Debugging

- Print `docs.length` and the first 500 characters of a few documents. Garbled or interleaved text means a reader problem, not a retrieval problem.
- For LlamaParse, inspect the markdown of one hard page by eye before ingesting hundreds.
- Authentication errors: the key must be in the same shell session that runs your script.
- Very large files can take minutes to parse; run long jobs outside request handlers.

## Quick Summary

- A reader is anything with `loadData()` returning `Document[]`; write your own freely.
- `SimpleDirectoryReader` + `fileExtToReader` loads folders; unmapped extensions are skipped.
- Plain PDF text extraction fails on tables, columns and scans; use LlamaParse for those.
- Prefer the `@llamaindex/llama-cloud` SDK for new LlamaParse code; `llama-cloud-services` is deprecated.
- Choose the parse `tier` by testing on hard samples, and cache parse results yourself.
- Stable `id_`s and clean metadata at read time pay off in every later step.

## Next

[02-ingestion-pipelines.md](./02-ingestion-pipelines.md): chaining splitters, extractors and embeddings, caching them, and ingesting incrementally without duplicates.
