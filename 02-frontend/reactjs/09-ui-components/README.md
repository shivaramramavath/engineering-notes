# 09 — UI Components

Most of an application's interface is made of a small set of recurring widgets: buttons, dialogs, menus, tabs, selects, tables, toasts. Building each one correctly — keyboard behavior, focus management, ARIA, positioning, animation — is surprisingly hard, and getting it wrong excludes users. This chapter shows how to build on **accessible primitives** (Radix UI, via shadcn/ui) so you spend your time on design and product behavior instead of reinventing focus traps.

Examples are TSX, mostly with Tailwind classes.

## Prerequisites

- [`../05-component-design/`](../05-component-design/README.md) — component APIs, composition, compound components, `asChild`
- [`../07-styling/`](../07-styling/README.md) — Tailwind, design tokens, the `cn` helper
- [`../08-accessibility/`](../08-accessibility/README.md) — you'll see why these primitives exist
- [`../06-forms/`](../06-forms/README.md) — for `06-form-controls.md`

## What you'll be able to do after this chapter

- Set up shadcn/ui and understand the "you own the code" model, including how to customize and update components
- Define typed component variants with `cva`
- Build accessible dialogs, alert dialogs, and sheets; menus and selects; tabs; and a command palette
- Choose the right control for a job (select vs menu vs popover vs combobox; dialog vs page)
- Build form controls that integrate with React Hook Form
- Build data tables with TanStack Table (sorting, filtering, pagination, server-side mode)
- Add toasts and notifications that screen reader users can perceive
- Structure a small design system with documentation, tests, and contribution rules

## Reading order

| # | File | What it covers | Prerequisite |
|---|------|----------------|--------------|
| 00 | [00-shadcn-ui.md](./00-shadcn-ui.md) | The model, setup, structure, customization, updating | `../07-styling/` |
| 01 | [01-component-variants.md](./01-component-variants.md) | `cva`, compound variants, slots, `data-*` variants | 00 |
| 02 | [02-dialogs-and-modals.md](./02-dialogs-and-modals.md) | Dialog, alert dialog, sheet, forms in dialogs | 00 |
| 03 | [03-dropdowns-and-menus.md](./03-dropdowns-and-menus.md) | Dropdown menu, select, popover, context menu | 00 |
| 04 | [04-tabs.md](./04-tabs.md) | Tabs, URL-synced tabs, lazy panels | 00 |
| 05 | [05-command-palette.md](./05-command-palette.md) | `cmdk`, shortcuts, fuzzy search | 02, 03 |
| 06 | [06-form-controls.md](./06-form-controls.md) | Select, checkbox, radio, switch, combobox, date; RHF integration | `../06-forms/` |
| 07 | [07-data-tables.md](./07-data-tables.md) | TanStack Table, server-side tables, accessibility | 06 |
| 08 | [08-toasts-and-notifications.md](./08-toasts-and-notifications.md) | Toast systems, announcements, patterns | 00 |
| 09 | [09-design-system.md](./09-design-system.md) | Layers, docs, testing, versioning, governance | 00–08 |

Files `02`–`08` are independent of each other after `00` (and `01` helps with all). Read in any order based on what you're building.

## Boundary with other chapters

- **How compound components and `asChild` work internally:** [`../05-component-design/02-compound-components.md`](../05-component-design/02-compound-components.md), [`../05-component-design/01-composition-patterns.md`](../05-component-design/01-composition-patterns.md)
- **Focus, keyboard, ARIA rules the primitives implement:** [`../08-accessibility/`](../08-accessibility/README.md)
- **Portals and layering:** [`../16-advanced-react/00-portals.md`](../16-advanced-react/00-portals.md)
- **Server data behind tables and lists:** [`../12-server-state/`](../12-server-state/README.md)
- **Animation details:** [`../21-specializations/animation/`](../21-specializations/animation/README.md)
- **Interview-style questions:** [`../23-interview/`](../23-interview/README.md)

## A note on libraries

Names and APIs in this chapter (shadcn/ui, Radix, `cmdk`, Sonner, TanStack Table, `cva`, Storybook) change between versions. The **concepts** — own your components, use accessible primitives, type your variants, test behavior — are stable. Check each library's current documentation for installation commands and exact component names before copying code.

## Exercises

1. **Set up.** Initialize shadcn/ui in a Vite + Tailwind project; add `button`, `dialog`, and `input`; change the `Button` default style to match a brand color through tokens.
2. **Variants.** Add a `destructive` variant and a `xl` size to `Button` using `cva`, and a `loading` state that shows a spinner and disables the button.
3. **Confirm delete.** Build a confirmation `AlertDialog` for a destructive action, with correct focus on the cancel button and an async confirm handler with a pending state.
4. **Menu.** A row-actions dropdown (Edit, Duplicate, Delete) with keyboard shortcuts displayed and the Delete item styled as destructive.
5. **Tabs in the URL.** Tabs whose selected value lives in the `?tab=` search param, so refresh and Back work.
6. **Command palette.** Open with `Cmd/Ctrl+K`, search a list of pages and actions, run them with Enter.
7. **Table.** A users table with sortable columns, a text filter, pagination, and row selection; then convert it to server-side sorting and pagination.
8. **Toasts.** Show success, error, and promise-based toasts, with an undo action, and verify with a screen reader that they're announced.
9. **Document.** Write Storybook stories (or a simple docs page) for `Button` covering every variant and state, and add an accessibility check.

## Next

**[`../10-routing/README.md`](../10-routing/README.md)** covers multi-page navigation, which ties these UI pieces into full applications.