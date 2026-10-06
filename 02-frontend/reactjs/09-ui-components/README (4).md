# 09 — UI Components

Almost every app needs the same set of interactive pieces: buttons, dialogs, menus, tabs, command palettes, form controls, data tables, toasts. Building them from scratch means re-solving focus management, keyboard navigation, ARIA, and positioning every time. This folder covers the **practical stack for doing that well in React**, with [shadcn/ui](https://ui.shadcn.com) as the centre of gravity.

The approach: **headless primitives for behavior + Tailwind for looks + code you own.**

```text
Radix UI / cmdk / Sonner / TanStack Table     ← behavior, a11y, state machines
          ↓
shadcn/ui components (copied into your repo)  ← styling + composition
          ↓
Your design system (tokens, variants)         ← consistency across the app
          ↓
Feature components                            ← what users actually see
```

## Prerequisites

- [Hooks](../03-hooks/README.md) and [component design](../05-component-design/README.md) — composition and compound components show up everywhere here.
- [Tailwind CSS](../07-styling/02-tailwindcss.md) — every example uses utility classes.
- [Accessibility basics](../08-accessibility/README.md) — you'll understand *why* these primitives behave the way they do.

## Contents

| # | File | What you'll learn |
|---|------|-------------------|
| 00 | [shadcn/ui](./00-shadcn-ui.md) | What it is, setup with Vite, `cn()`, the "you own the code" model |
| 01 | [Component variants](./01-component-variants.md) | `cva`, `VariantProps`, `asChild`, class merging |
| 02 | [Dialogs and modals](./02-dialogs-and-modals.md) | Dialog, AlertDialog, Sheet, controlled state, gotchas |
| 03 | [Dropdowns and menus](./03-dropdowns-and-menus.md) | DropdownMenu, ContextMenu, Popover, Select |
| 04 | [Tabs](./04-tabs.md) | Tabs, URL-synced tabs, tabs vs routes |
| 05 | [Command palette](./05-command-palette.md) | cmdk, Cmd+K, async search, combobox |
| 06 | [Form controls](./06-form-controls.md) | Input, Checkbox, Select, Switch with React Hook Form |
| 07 | [Data tables](./07-data-tables.md) | TanStack Table: sorting, filtering, pagination, server-side |
| 08 | [Toasts and notifications](./08-toasts-and-notifications.md) | Sonner, promise toasts, when *not* to use a toast |
| 09 | [Design system](./09-design-system.md) | Tokens, layers, extending shadcn into your own system |

## Suggested order

Read 00 → 01 first; everything else builds on them. After that the files are independent — jump to whichever component you're building.

## Conventions used in this folder

- Examples are TypeScript (`.tsx`) with React 19 and Tailwind CSS v4.
- Import paths use the `@/` alias (`@/components/ui/button`), which is what `shadcn init` configures.
- Component code from shadcn changes between releases. Treat the snippets as the *shape* of the pattern and check the [shadcn docs](https://ui.shadcn.com/docs) for the exact current source.
