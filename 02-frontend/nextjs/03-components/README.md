# 03 · Components

The most important concept in the App Router: **where does each component run?** Server Components run on the server and ship no JavaScript; Client Components add interactivity in the browser. Almost every real page mixes both, so this chapter covers each kind, how to decide between them, and the patterns for combining them.

> Written for Next.js 16 and React 19. If anything in later chapters (data fetching, caching, auth) feels confusing, the cause is usually in this chapter.

## Reading order

| # | Note | You will learn |
|---|---|---|
| 00 | [Server Components](./00-server-components.md) | What they are, what they can and cannot do, how they render |
| 01 | [Client Components](./01-client-components.md) | `"use client"`, hydration, what belongs in the browser |
| 02 | [Server vs Client](./02-server-vs-client.md) | Decision guide, the module boundary, what can cross it |
| 03 | [Composition Patterns](./03-composition-patterns.md) | Pushing boundaries down, children slots, providers, streaming props |

## Use it as a reference

- "Can I use `useState` here?" → [Server vs Client](./02-server-vs-client.md)
- "I need a context provider in my layout" → [Composition Patterns](./03-composition-patterns.md)
- "Why is this prop not allowed?" → [Server vs Client](./02-server-vs-client.md), serialization section
- "A library breaks with a `use client` error" → [Composition Patterns](./03-composition-patterns.md), third-party section

## Next

[04 · Rendering](../04-rendering/README.md): how and when HTML is produced (static, dynamic, streamed).
