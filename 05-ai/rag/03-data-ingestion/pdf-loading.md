# PDF Loading

## Concept
PDFs are the most common RAG source and the most error-prone. A PDF stores *where to draw characters on a page*, not *what the document means*, so extracting clean, ordered text is genuinely hard.

## Prerequisites
- [loaders.md](loaders.md)

## Kinds of PDF

| Type | Description | Approach |
|---|---|---|
| Text PDF | Has an embedded text layer | Standard text extraction |
| Scanned PDF | Pages are images | OCR required |
| Layout-heavy | Columns, tables, figures, headers | Layout-aware parser |
| Mixed | Some of each | Detect per page, or use a layout-aware parser |

Quick test: if you can select and copy text in a PDF viewer, it has a text layer. If not, it needs OCR.

## Basic Loading
`PDFReader` (from `@llamaindex/readers/pdf`; verify the path in your version) returns one `Document` per page.

```ts
import { PDFReader } from "@llamaindex/readers/pdf";

const reader = new PDFReader();
const docs = await reader.loadData("./report.pdf");

console.log(docs.length);                 // one Document per page
console.log(docs[0].metadata);            // includes page_number (other keys: verify)
console.log(docs[0].text.slice(0, 500));
```
Keep the per-page `page_number` metadata; it enables page-level citations. Assign stable IDs yourself, for example `${fileName}#p${pageNumber}` (see [loaders.md](loaders.md)).

```ts
for (const doc of docs) {
  doc.id_ = `report.pdf#p${doc.metadata.page_number}`;
  doc.metadata = { ...doc.metadata, source: "report.pdf" };
}
```

## Typical Problems and Fixes

| Problem | Symptom | Fix |
|---|---|---|
| Broken reading order | Two columns interleaved | Layout-aware parser |
| Tables flattened | Rows and columns mashed into one line | Table-aware parser; convert tables to Markdown |
| Repeated headers and footers | Same line on every page | Strip in [cleaning.md](cleaning.md) |
| Hyphenated line breaks | `retrie- val` | Rejoin split words |
| Ligatures and odd glyphs | `ﬁ` or `�` | Unicode normalization |
| Scanned pages | Empty or garbage text | OCR |
| Figures and charts | Information missing from text | Describe with a vision model, or accept the loss |
| Page-spanning sentences | Chunks cut at page boundaries | Merge pages before chunking if continuity matters |

## Parser Options (categories)
- **Basic text extractors**: `PDFReader` is the documented option in LlamaIndex.TS. Fast and free; weak on layout and tables. Other Node PDF text libraries exist (for example pdf.js-based ones); you would wrap them in a custom reader (verify which suit you).
- **Layout-aware / document-AI parsers** (LlamaParse, Unstructured, Docling and similar): better tables and columns; slower, and some are paid or hosted services. LlamaParse has a TypeScript client (verify package name and current API); I did not confirm its integration with LlamaIndex.TS. Others are typically reached via their REST APIs. Check where your data goes before sending confidential files to a hosted parser.
- **OCR engines**: for scans. Quality varies with scan quality and language. A Node OCR option (for example a Tesseract wrapper) or a hosted OCR API; verify.

The Python ecosystem has more ready-made PDF integrations than LlamaIndex.TS. Always compare 2 or 3 parsers on a handful of your *hardest* PDFs before standardizing on one.

## Verify Extraction
```ts
for (const d of docs.slice(0, 3)) {
  console.log("---", d.metadata.page_number);
  console.log(d.text.slice(0, 800));
}
```
Check: reading order, tables, missing sections, garbage characters.

## Important Rules
- Inspect extracted text from real documents before building on it.
- Keep page numbers and file names in metadata.
- Choose the parser by document type, not by habit.
- Treat confidential PDFs carefully when using hosted parsing services.

## Common Mistakes
- Assuming every PDF is a text PDF.
- Using the cheapest parser on table-heavy financial reports, then wondering why numbers are wrong.
- Dropping page metadata, which kills citations.
- Chunking raw extracted text without cleaning repeated headers and footers.

## Related / Next
- [cleaning.md](cleaning.md)
- [chunking.md](chunking.md)
- `15-debugging/debugging-rag.md`
