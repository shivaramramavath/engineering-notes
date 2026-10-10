# Loaders

## Concept
A **loader** (LlamaIndex calls it a *reader*) reads raw content from a source and returns `Document` objects. It is the first step of ingestion, and everything downstream depends on how well it extracts text and metadata.

```
Files / web / DB / API ──► Reader ──► [Document, Document, ...]
```

> TypeScript edition. Import paths vary between LlamaIndex.TS versions and doc pages; verify against your installed version. Readers live in `@llamaindex/readers` (install with `npm i @llamaindex/readers`).

## Prerequisites
- [Documents and nodes](../01-fundamentals/documents-and-nodes.md)

## Local Files: `SimpleDirectoryReader`
The default entry point for a folder. It picks a reader per file extension.

```ts
import { SimpleDirectoryReader } from "@llamaindex/readers/directory";

const reader = new SimpleDirectoryReader();
const documents = await reader.loadData({ directoryPath: "./data" });
```

Differences from the Python framework, stated honestly:
- The LlamaIndex.TS docs mention **no `recursive` option**. If you need nested folders, walk the tree yourself (`fs.readdir` with `{ recursive: true }` on recent Node versions, verify) and load files individually, or call `loadData` once per subfolder.
- The docs mention no `required_exts` or `filename_as_id` equivalent. Filter file paths yourself and assign IDs after loading.
- Which extensions are handled by default varies by version. You can override per extension with `fileExtToReader` (a map from extension to reader instance):

```ts
import { SimpleDirectoryReader } from "@llamaindex/readers/directory";
import { PDFReader } from "@llamaindex/readers/pdf";
import { MarkdownReader } from "@llamaindex/readers/markdown";

const documents = await new SimpleDirectoryReader().loadData({
  directoryPath: "./data",
  fileExtToReader: {
    pdf: new PDFReader(),
    md: new MarkdownReader(),
  },
});
```

Other documented file readers: `HTMLReader` (`@llamaindex/readers/html`), `DocxReader` (`@llamaindex/readers/docx`), `CSVReader` (`@llamaindex/readers/csv`).

### Stable IDs and custom metadata
Assign both yourself after loading (the document `id_` field is what updates and deletes rely on):

```ts
import path from "node:path";

for (const doc of documents) {
  const filePath = String(doc.metadata.file_path ?? doc.metadata.file_name ?? "unknown"); // key names: verify
  doc.id_ = path.basename(filePath);        // stable, derived from the source
  doc.metadata = { ...doc.metadata, source: filePath, team: "support" };
}
```

> Stable IDs matter. Without them you cannot reliably update or delete a document later. See `11-document-management/doc-ids-and-tracking.md`.

## Web Pages
No web reader is documented for LlamaIndex.TS. A hand-rolled option (verify each package and its API before relying on it): fetch the HTML, extract the main content with a readability-style library or `cheerio`, and wrap the text in a `Document`.

```ts
import { Document } from "llamaindex";
import * as cheerio from "cheerio";   // npm i cheerio (verify)

async function loadWebPage(url: string): Promise<Document> {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`Fetch failed: ${res.status} ${url}`);
  const $ = cheerio.load(await res.text());
  $("nav, header, footer, aside, script, style, noscript").remove();   // strip boilerplate
  const text = $("main, article, body").first().text().replace(/\s+\n/g, "\n").trim();
  return new Document({ text, id_: url, metadata: { source: url } });
}
```
Readability-style extractors (for example Mozilla Readability, verify) pick the main article better than a fixed selector list. Test on your own pages. You can also implement `BaseReader.loadData` to package this as a reusable reader.

## Markdown and HTML
Plain Markdown loads as text. The structure is preserved better at **chunking** time:
- `MarkdownNodeParser` splits on headings.
- For HTML, convert to text first (as above) or use `HTMLReader`; no HTML node parser is documented for LlamaIndex.TS.

See [chunking.md](chunking.md).

## Custom Readers
Implement `BaseReader` with a `loadData` method that returns `Document[]`. File readers extend `FileReader` (from `@llamaindex/core/schema`) and implement `loadDataAsContent`. This is the route for databases, internal APIs and anything without a ready reader.

## Other Sources
The Python ecosystem (LlamaHub) has many readers for Notion, Slack, Google Drive, S3 and more. LlamaIndex.TS has far fewer. Check `@llamaindex/readers` for what exists in your version; otherwise write a small custom reader around the source's own SDK.

## Choosing a Reader

| Source | Starting point |
|---|---|
| Folder of mixed files | `SimpleDirectoryReader` (flat; no documented recursion) |
| PDFs | See [pdf-loading.md](pdf-loading.md) |
| Websites | Hand-rolled fetch + extraction (verify) |
| Markdown / docs sites | `MarkdownReader` plus `MarkdownNodeParser` |
| SaaS tools and databases | Custom reader around the vendor SDK |

## Important Rules
- Always check what a reader returned. Print a few documents and read the text before building anything on it.
- Set stable document IDs and useful metadata at load time.
- Load once, then chunk; do not mix loading concerns into chunking code.

## Common Mistakes
- Never inspecting loaded text, then debugging "bad retrieval" that was really bad extraction.
- Assuming `SimpleDirectoryReader` recurses into subfolders and silently missing files.
- Letting the loader pull in navigation menus, cookie banners and footers from web pages.
- Losing source information, so answers can't cite where they came from.
- Reloading the whole corpus on every run instead of syncing changes.

## Related / Next
- [pdf-loading.md](pdf-loading.md)
- [cleaning.md](cleaning.md)
- [ingestion-pipeline.md](ingestion-pipeline.md)
