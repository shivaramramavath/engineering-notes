# 01 · Fundamentals

The mental model for everything that follows: what Next.js is, how the App Router maps folders to URLs, what each special file does, and how pages and layouts compose. Spend time here. Almost every later "why does it behave like this?" question traces back to these notes.

> Written for Next.js 16 with the App Router. The legacy Pages Router is covered in the last note, for reading older code and migrating.

## Reading order

| # | Note | You will learn |
|---|---|---|
| 00 | [Overview](./00-overview.md) | What Next.js adds to React, the big ideas, how a request flows |
| 01 | [App Router](./01-app-router.md) | The `app/` directory model, Server Components by default, your first routes |
| 02 | [Project Structure](./02-project-structure.md) | Top-level files and folders, every special file, colocation and organization |
| 03 | [Pages and Layouts](./03-pages-and-layouts.md) | `page.tsx`, `layout.tsx`, nesting, root layout, `template.tsx` |
| 04 | [Pages Router](./04-pages-router.md) | The legacy `pages/` model, how it differs, and migrating to the App Router |

## Use it as a reference

- "Which file do I create for X?" → [Project Structure](./02-project-structure.md)
- "Why did my layout not re-render?" → [Pages and Layouts](./03-pages-and-layouts.md)
- "This tutorial uses `getServerSideProps`" → [Pages Router](./04-pages-router.md)

## Next

[02 · Routing](../02-routing/README.md): dynamic routes, groups, parallel and intercepting routes, navigation.