# Loading and Splitting

RAG starts before any model is involved: you have to get your data into **documents** and cut those documents into **chunks** small enough to embed and retrieve precisely. Most RAG quality problems trace back to this step, not the LLM.

> Checked against the LangChain JS 1.x docs. Splitters live in `@langchain/textsplitters` (v1.0.1 on npm, peer dep `@langchain/core ^1.0.0`).

**Prerequisites:** [Setup](../01-core/01-setup.md)

```text
Source (PDF, web, DB, files)
        │  load
        ▼
   Document[]   ─── pageContent + metadata
        │  split
        ▼
   chunks (Document[])  ──►  embed  ──►  vector store      (next note)
```

---

## The Document

Everything in the RAG pipeline passes around `Document` objects:

```ts
import { Document } from "@langchain/core/documents";

const doc = new Document({
  pageContent: "Dogs are great companions, known for their loyalty.",
  metadata: { source: "pets-handbook.pdf", page: 3 },
});
```

| Field | Type | Purpose |
|---|---|---|
| `pageContent` | string | The text that gets embedded and shown to the model |
| `metadata` | object | Anything you want to carry along: source, page, section, author, date, tenant id |
| `id` | string, optional | Stable identifier, useful for updates and deletes |

**Metadata is not decoration.** It is how you cite sources, filter by user or date, and delete or refresh a document later. Decide what you need before you ingest, because adding it afterwards means re-indexing.

---

## Loading

Loaders turn an external source into `Document[]`. LangChain has many integrations (files, web pages, Google Drive, Slack, Notion, and more) in the `@langchain/community` and provider packages; browse the integrations page for the one you need and install what its page says.

You are also free to **build the documents yourself**. For a PDF, the docs' approach is a small helper over `pdf-parse`, one `Document` per page:

```bash
npm i pdf-parse
```

```ts
import { readFileSync } from "node:fs";
import { Document } from "@langchain/core/documents";
import { PDFParse } from "pdf-parse";

async function loadPdfPages(filePath: string): Promise<Document[]> {
  const parser = new PDFParse({ data: new Uint8Array(readFileSync(filePath)) });
  try {
    const { pages } = await parser.getText();
    return pages.map(
      (p) =>
        new Document({
          pageContent: p.text,
          metadata: { source: filePath, page: p.num - 1 },
        }),
    );
  } finally {
    await parser.destroy();
  }
}
```

Writing the loader yourself is often the right call: a database table, an API response, or a folder of markdown files is just a loop that produces `Document`s with the metadata you choose.

### What to check after loading

Don't skip this. Print a few documents and read them.

- Is the text actually extracted (scanned PDFs may return nothing without OCR)?
- Is it clean? Headers, footers, page numbers and navigation menus repeated on every page become noise in every chunk.
- Is the metadata what you expect?

---

## Why split at all

1. **Embedding quality.** One vector per chunk. A 50-page document embedded as one vector is a blurry average. A paragraph-sized chunk has a sharp meaning.
2. **Context budget.** You send retrieved chunks to the model. Whole documents waste tokens and bury the relevant part.
3. **Model limits.** Embedding models have input limits.

But chunks that are too small lose the context that makes them understandable ("it increased by 12%": what did?). Splitting is a trade-off, and the right size depends on your content.

---

## RecursiveCharacterTextSplitter (start here)

```ts
import { RecursiveCharacterTextSplitter } from "@langchain/textsplitters";

const splitter = new RecursiveCharacterTextSplitter({
  chunkSize: 1000,
  chunkOverlap: 200,
});

const chunks = await splitter.splitDocuments(docs);
```

How it works: it tries to split on the largest natural boundary first (paragraphs), and only falls back to smaller ones (lines, sentences, words) when a piece is still too big. So chunks tend to end at sensible boundaries instead of mid-sentence.

| Option | Meaning |
|---|---|
| `chunkSize` | Max chunk length. **Characters** by default, not tokens |
| `chunkOverlap` | How much of the previous chunk is repeated at the start of the next |

- **Overlap** reduces the chance that an answer is cut in half at a boundary. 10 to 20 percent of `chunkSize` is a common starting point. It also means duplicate text in your index, so more storage and occasional near-duplicate results.
- `splitDocuments` **copies each document's metadata onto every chunk**, so source and page survive splitting.
- Use `splitText(string)` when you have a raw string instead of documents.

The docs use `chunkSize: 1000, chunkOverlap: 200` for their examples. Treat those numbers as a starting point to tune, not a rule.

---

## Other splitters in `@langchain/textsplitters`

| Splitter | Splits by | Use when |
|---|---|---|
| `RecursiveCharacterTextSplitter` | Hierarchy of separators | Default for prose |
| `CharacterTextSplitter` | One separator, character length | Simple, uniform text |
| `TokenTextSplitter` | Token count | You need chunks to respect a token budget |
| Code splitting | Language-aware separators | Source code (keeps functions and classes together) |

The docs' integrations page lists these. Check it for current class names and how to select a language for code splitting, because I did not verify those constructor details.

> The integrations page also states a Node.js version requirement for the splitters. Check it against your own Node version.

---

## Choosing chunk size

There is no universal number. Reason from the content and the questions:

- **FAQ or short facts:** smaller chunks (a few hundred characters) retrieve precisely.
- **Long-form reasoning, contracts, papers:** larger chunks keep arguments intact.
- **Structured docs (markdown, HTML, code):** split on structure (headings, functions) rather than blind length.

Then **measure**. Build a small set of real questions, check whether the right chunk shows up in the top results, and adjust. See `04-production/02-testing-and-evaluation.md`.

---

## Practical advice

- **Clean before splitting.** Strip boilerplate (headers, footers, nav) first.
- **Keep provenance.** `source`, `page` or URL, plus a section title if you have one. Your citations depend on it.
- **Add context to metadata** you will filter on later (tenant, language, date, access level).
- **Give documents stable ids** if the data changes, so you can replace instead of duplicating.
- **Re-split when you change chunking.** Changing size or overlap means re-embedding everything.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Assuming `chunkSize` is tokens | It is characters unless you use a token-based splitter |
| Never reading the loaded text | Print samples; fix extraction and boilerplate first |
| Dropping metadata | Use `splitDocuments`, not `splitText`, so metadata is copied |
| Huge chunks "to be safe" | Hurts precision and wastes context tokens |
| Tiny chunks with no overlap | Answers get cut across boundaries; context vanishes |
| Splitting structured content as plain text | Use structure-aware splitting for code and markdown |
| Indexing scanned PDFs with no text | Add OCR or the index will be empty or garbage |
| Changing chunk settings without re-indexing | Rebuild the index; old and new chunks don't mix well |

---

## Quick Summary

- RAG data flows as `Document` objects: `pageContent` + `metadata` (+ optional `id`).
- Load with an integration or your own loop; **inspect** the result.
- Split with `RecursiveCharacterTextSplitter` (`chunkSize`, `chunkOverlap`, characters by default); `splitDocuments` keeps metadata.
- Tune chunk size against real questions rather than guessing.
- Metadata decisions made now affect citations, filtering and updates later.

**Next:** [Embeddings and Vector Stores](./02-embeddings-and-vector-stores.md)