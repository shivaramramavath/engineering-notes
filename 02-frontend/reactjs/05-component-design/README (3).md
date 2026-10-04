# 05 — Component Design

How to design components that other people (and future you) can use without reading their source. The earlier chapters taught how components work; this one teaches how to **shape their APIs**: which props to expose, when to compose instead of configure, how to build families of components that share state, and which older patterns you'll still meet in legacy code.

Examples are TSX. Typing techniques come from [`../04-typescript-with-react/`](../04-typescript-with-react/README.md).

## Prerequisites

- [`../01-fundamentals/03-children-and-composition.md`](../01-fundamentals/03-children-and-composition.md) — `children`, slots, and composition basics
- [`../03-hooks/05-useContext.md`](../03-hooks/05-useContext.md) and [`../03-hooks/09-custom-hooks.md`](../03-hooks/09-custom-hooks.md)
- [`../04-typescript-with-react/`](../04-typescript-with-react/README.md)

## What you'll be able to do after this chapter

- Decide when to abstract a component and when duplication is cheaper
- Design a small, predictable prop API with sensible defaults
- Support both controlled and uncontrolled usage
- Replace piles of boolean props with composition
- Build headless components and polymorphic (`as` / `asChild`) components
- Build compound components (a `Tabs` with `Tabs.List`, `Tabs.Trigger`, …) that share state through context
- Read and migrate render props, higher-order components, and container/presentational code

## Reading order

| # | File | What it covers | Prerequisite |
|---|------|----------------|--------------|
| 00 | [00-component-api-design.md](./00-component-api-design.md) | Prop design, when to abstract, controlled/uncontrolled APIs | Prerequisites above |
| 01 | [01-composition-patterns.md](./01-composition-patterns.md) | Slots, headless components, polymorphism, `asChild` | 00 |
| 02 | [02-compound-components.md](./02-compound-components.md) | Context-based component families | 01 |
| 03 | [03-legacy-component-patterns.md](./03-legacy-component-patterns.md) | Render props, HOCs, container/presentational | 01 |

`03` is mainly for reading older code and for interviews; skim it after `02` and return when you need it.

## Boundary with other chapters

- **Basic `children` and slots:** [`../01-fundamentals/03-children-and-composition.md`](../01-fundamentals/03-children-and-composition.md). This chapter builds on it without repeating it.
- **Controlled and uncontrolled form *inputs*:** [`../06-forms/00-controlled-and-uncontrolled-inputs.md`](../06-forms/00-controlled-and-uncontrolled-inputs.md). `00` here covers the same idea for **component APIs** in general.
- **Building a full UI kit (variants, tokens, Storybook):** [`../09-ui-components/09-design-system.md`](../09-ui-components/09-design-system.md).
- **Folder and layer architecture:** [`../20-frontend-architecture/`](../20-frontend-architecture/README.md).

## Exercises

1. **Refactor boolean props.** Take a `Button` with `isPrimary`, `isDanger`, `isLarge`, `isSmall`, and rewrite its API with `variant` and `size` unions.
2. **Controlled or not.** Build a `Counter` that works both uncontrolled (`defaultValue`) and controlled (`value` + `onChange`) using one shared hook.
3. **Slots.** Create a `Page` layout with `header`, `sidebar`, and `children` slots, with no `cloneElement`.
4. **Headless toggle.** Write `useDisclosure()` that returns props for a trigger and a panel (`aria-expanded`, `aria-controls`, ids), then use it to build two differently styled disclosures.
5. **Compound `Tabs`.** Build `Tabs`, `TabsList`, `TabsTrigger`, `TabsPanel` sharing state through context, with correct ARIA roles.
6. **Migrate legacy code.** Convert a render-prop `MouseTracker` and a `withAuth` HOC to custom hooks.

## Next

**[`../06-forms/README.md`](../06-forms/README.md)** applies these ideas to the most common reusable component family: form inputs.
