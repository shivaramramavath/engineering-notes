# 09 — UI Components & Design System Primitives

This module covers engineering production-grade, highly accessible, and high-performance UI components for enterprise applications. Spanning the modern shadcn/ui ecosystem, component variants with CVA, modals, command palettes, toasts, TanStack Table v8, `@dnd-kit` drag-and-drop, and Floating UI positioning.

---

## What you'll be able to do after this module

- **Master the shadcn/ui Architecture**: Build accessible, customizable components by composing Radix UI primitives and Tailwind CSS v4.
- **Implement Component Variants with CVA**: Use `class-variance-authority` to build type-safe, multi-variant design system buttons and badges.
- **Architect Accessible Dialogs & Modals**: Implement focus-trapped, portal-mounted modal dialogs and flyout drawers.
- **Build Floating Tooltips & Menus**: Leverage Floating UI to achieve pixel-perfect positioning with collision detection and virtual references.
- **Implement Command Palettes**: Build kbar / cmdk style search palettes with keyboard shortcuts and instant filtering.
- **Master TanStack Table v8**: Build enterprise data grids with multi-column sorting, global filtering, column pinning, and virtualization.
- **Implement Drag and Drop with `@dnd-kit`**: Build sortable lists, Kanban boards, and 2D grid dashboards with touch and keyboard sensors.
- **Publish a Cohesive Design System**: Document components, tokens, and props with Storybook and automated visual testing.

---

## Module Syllabus

| # | Note | Description | Key Library / Engine |
|---|------|-------------|:---:|
| 00 | [00-shadcn-ui.md](./00-shadcn-ui.md) | Copy-paste component architecture, Radix UI + Tailwind CSS v4 | shadcn/ui |
| 01 | [01-component-variants.md](./01-component-variants.md) | Type-safe variants, sizes, and compound variants with CVA | `class-variance-authority` |
| 02 | [02-dialogs-and-modals.md](./02-dialogs-and-modals.md) | Focus trapping, portal mounting, escape key stacks, body scroll locking | Radix Dialog |
| 03 | [03-dropdowns-and-menus.md](./03-dropdowns-and-menus.md) | Contextual dropdowns, sub-menus, keyboard navigation, typeahead | Radix DropdownMenu |
| 04 | [04-tabs.md](./04-tabs.md) | WAI-ARIA tab panels, animated pill indicators, URL query synchronization | Radix Tabs |
| 05 | [05-command-palette.md](./05-command-palette.md) | Fast command palettes, fuzzy search, keyboard navigation (`cmdk`) | `cmdk` |
| 06 | [06-form-controls.md](./06-form-controls.md) | Accessible switches, sliders, checkboxes, and segmented controls | Radix Primitives |
| 07 | [07-data-tables.md](./07-data-tables.md) | Basic data table implementations, sorting, pagination, selection | HTML Table / CSS Grid |
| 08 | [08-toasts-and-notifications.md](./08-toasts-and-notifications.md) | Live notification queues, auto-dismiss timers, swipe-to-dismiss | Sonner / Radix Toast |
| 09 | [09-design-system.md](./09-design-system.md) | Design tokens, Storybook integration, multi-brand theme packaging | Storybook 8 |
| 10 | [10-tanstack-table-deep-dive.md](./10-tanstack-table-deep-dive.md) | Headless data grids, faceted filtering, column pinning, virtualization | TanStack Table v8 |
| 11 | [11-drag-and-drop-dnd-kit.md](./11-drag-and-drop-dnd-kit.md) | Sortable lists, Kanban boards, collision detection, `<DragOverlay>` | `@dnd-kit` |
| 12 | [12-floating-ui-popovers.md](./12-floating-ui-popovers.md) | Precision positioning: `flip`, `shift`, `arrow`, virtual text references | Floating UI |

---

## Architectural Comparison Matrix

| Component Type | Industry Standard Library | Why It Wins | Key Pattern |
| :--- | :--- | :--- | :--- |
| **Primitives** | Radix UI | Battle-tested WAI-ARIA compliance | Compound / `asChild` Slot |
| **Data Grid** | TanStack Table v8 | Headless, 100% style freedom, modular | Pipeline Row Models |
| **Drag & Drop** | `@dnd-kit` | Modular, touch & keyboard accessible | Sensors & `<DragOverlay>` |
| **Positioning**| Floating UI | Tiny (~3.5KB), collision-proof | Composable Middleware |
| **Notifications**| Sonner | Silky smooth animations, zero layout shifts | Stacked Portal Container |

---

## Boundary with Other Modules

- **Component Design Patterns**: Read [`05-component-design/`](../05-component-design/README.md) for headless architecture and the `asChild` pattern.
- **Forms & Validation**: Read [`06-forms/`](../06-forms/README.md) for form validation with Zod and TanStack Form.
- **Styling Architecture**: Read [`07-styling/06-tailwind-v4-changes-and-theming.md`](../07-styling/06-tailwind-v4-changes-and-theming.md) for Tailwind CSS v4.
- **Accessibility Verification**: Read [`08-accessibility/`](../08-accessibility/README.md) for WCAG 2.2 AA testing.

---

## Practical Exercises

1. **Sortable Task Queue (`@dnd-kit`)**: Build a vertical list of tasks with smooth drag reordering and keyboard accessibility.
2. **Enterprise Data Grid (TanStack Table)**: Implement a transaction table with multi-column sorting, pagination, and global search.
3. **Floating Context Menu (Floating UI)**: Build a right-click context menu positioned accurately at the mouse pointer coordinates.
4. **Command Palette (`cmdk`)**: Build a global `⌘K` command dialog allowing users to switch themes and navigate routes.
5. **Multi-Variant Button (CVA)**: Implement a design system button with primary, secondary, outline, and destructive variants with size modifiers.

---

## Next Steps

Advance to **[10-routing/README.md](../10-routing/README.md)** to master React Router v7, TanStack Router v1, nested layouts, and loaders.
