# Cleaning and Normalization

## Concept
Cleaning removes text that adds noise to embeddings, and normalization makes equivalent text look identical. Both happen **after loading and before chunking** (or as an early transformation in the ingestion pipeline).

The goal is *not* pretty text. The goal is that chunks contain meaningful content that matches real questions.

## Prerequisites
- [loaders.md](loaders.md)

## What to Remove

| Noise | Why it hurts | Typical fix |
|---|---|---|
| Repeated headers, footers, page numbers | Pollutes every chunk with the same text | Regex or line-frequency filter |
| Navigation menus, cookie banners, ads | Web boilerplate matches many queries wrongly | Main-content extraction |
| Legal or confidentiality boilerplate | Same text on every page | Remove or tag |
| Empty or near-empty pages | Produce useless chunks | Drop below a minimum length |
| Duplicate documents or sections | Return the same answer repeatedly, waste storage | Hash-based dedup |
| Extraction artifacts | Garbled characters, stray control codes | Normalize and filter |

## What to Normalize

| Issue | Fix |
|---|---|
| Unicode variants (curly quotes, ligatures, non-breaking spaces) | `text.normalize("NFKC")` |
| Excess whitespace and blank lines | Collapse runs of whitespace |
| Hyphenated line breaks (`retrie-\nval`) | Rejoin words |
| Inconsistent line breaks inside paragraphs | Join lines, keep paragraph breaks |
| Mixed encodings | Decode to UTF-8 at load time |

## Example
```ts
function cleanText(text: string): string {
  return text
    .normalize("NFKC")                          // unify ligatures, curly quotes, nbsp
    .replace(/-\n(\p{L})/gu, "$1")              // rejoin hyphenated words
    .replace(/[ \t]+/g, " ")                    // collapse spaces
    .replace(/\n{3,}/g, "\n\n")                 // collapse blank lines
    .trim();
}

for (const doc of documents) {
  doc.setContent(cleanText(doc.text));          // setContent: verify on your version; otherwise assign doc.text
}
```

Remove repeated page furniture by finding lines that appear on many pages:
```ts
function removeBoilerplate(documents: Document[], threshold = 0.5): void {
  const counts = new Map<string, number>();
  for (const d of documents) {
    const unique = new Set(d.text.split("\n").map((l) => l.trim()).filter(Boolean));
    for (const line of unique) counts.set(line, (counts.get(line) ?? 0) + 1);
  }
  const boilerplate = new Set(
    [...counts].filter(([, c]) => c > threshold * documents.length).map(([l]) => l),
  );
  for (const d of documents) {
    d.setContent(
      d.text.split("\n").filter((l) => !boilerplate.has(l.trim())).join("\n"),
    );
  }
}
```
(`Document` is imported from `"llamaindex"`.) Beware with very small corpora: with only one or two documents the threshold removes everything.

## Preserve Structure
Do **not** flatten everything into one blob. Headings, lists, tables and code blocks give chunkers and LLMs valuable structure.
- Keep Markdown headings.
- Keep paragraph breaks.
- Keep code blocks intact.

## Important Rules
- Clean conservatively. Removing real content is worse than keeping some noise.
- Make cleaning deterministic and versioned, so a rerun produces the same text. This matters for hash-based change detection (see [ingestion-pipeline.md](ingestion-pipeline.md)).
- Spot-check before and after on real documents.
- Keep the raw source so you can re-clean later without re-fetching.

## Common Mistakes
- Over-aggressive regexes that delete numbers, units or list items.
- Using `\w` in regexes on non-English text: it matches ASCII only in JavaScript. Use `\p{L}` with the `u` flag.
- Lowercasing and stripping punctuation. Modern embedding models don't need it and it can hurt (names, acronyms, code).
- Removing stop words. Not needed for embedding models.
- Cleaning differently between runs, which makes every document look "changed".

## Related / Next
- [chunking.md](chunking.md)
- [metadata.md](metadata.md)
