# Contributing

## Layout

- Numbered folders define learning order. Notes inside a folder are listed in reading order in that folder's README.
- One concept per file. Merge files that overlap; split files that cover unrelated ideas.
- Topic files are prefixed with a two-digit order number starting at `00` and followed by a short, lowercase, hyphenated name, for example `03-window-frames.md`. The prefix is the reading order within the folder; `README.md` is never prefixed. Avoid `misc.md`, `notes2.md` and similar.
- When inserting a note mid-folder, renumber the files after it and update the links and the folder README.
- Every folder has a README that indexes its notes.

## Writing a note

- Write like an experienced developer explaining the topic to another developer.
- Open with what the topic is and why it matters, then follow the topic's natural flow: what, why, how, when, practical usage.
- Start simple, then go deeper. Prefer short, correct, practical examples over long theory.
- Use tables and ASCII diagrams only when they clarify.
- Include common mistakes and debugging advice, and performance, security or internals only where relevant.
- Note PostgreSQL vs MySQL differences where behavior diverges.
- Correct common misconceptions. Do not repeat content that lives in another note; link to it instead.
- Do not invent syntax, behavior or links.
- End with a short summary or key takeaways, and a pointer to the next topic when useful.
- Do not force a fixed template; skip sections that do not apply.

## Code blocks

Use language-tagged fences (`sql`, `bash`, `js`, `ts`). Keep examples minimal, correct and relevant.
