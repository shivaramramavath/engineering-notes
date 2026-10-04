# 01 — Fundamentals

The building blocks every React app is made from: components, JSX, props, children, conditional rendering, lists, events, and basic forms. By the end of this chapter you can build a static-to-interactive UI out of small components, even before you've studied state in depth.

## What you'll be able to do after this chapter

- Break a UI design into a component hierarchy
- Write JSX correctly and know what it compiles to
- Pass data down with props and pass behavior down with function props
- Build reusable wrapper components with `children`
- Show, hide, and swap UI conditionally without common pitfalls
- Render lists with stable keys
- Handle clicks, input, and form submission
- Build a simple controlled form

## Reading order

| # | File | What it covers | Prerequisite |
|---|------|----------------|--------------|
| 00 | [00-thinking-in-react.md](./00-thinking-in-react.md) | UI as a function of state; the five-step design process | `../00-setup/` |
| 01 | [01-jsx.md](./01-jsx.md) | JSX syntax rules, expressions, fragments | 00 |
| 02 | [02-components-and-props.md](./02-components-and-props.md) | Function components, props, one-way data flow | 01 |
| 03 | [03-children-and-composition.md](./03-children-and-composition.md) | `children`, containment, slots | 02 |
| 04 | [04-conditional-rendering.md](./04-conditional-rendering.md) | `if`, ternary, `&&`, early return | 02 |
| 05 | [05-lists-and-keys.md](./05-lists-and-keys.md) | `map`, keys, filtering and sorting | 02 |
| 06 | [06-events.md](./06-events.md) | Event handlers, event objects, propagation | 02 |
| 07 | [07-forms-basics.md](./07-forms-basics.md) | Controlled inputs, submit handling | 06 |

`07` uses `useState` briefly. If it's unfamiliar, read the first section of [`../02-state-and-rendering/00-state-and-snapshots.md`](../02-state-and-rendering/00-state-and-snapshots.md) alongside it, or continue to the next chapter and return.

## Exercises

1. **Profile card** — build a `ProfileCard` component with `name`, `role`, and an optional `avatarUrl` prop (fall back to initials).
2. **Layout wrapper** — create a `Card` component that wraps arbitrary `children` and accepts a `title` prop.
3. **Product list** — render an array of products with keys, show "No products" when empty, and mark out-of-stock items.
4. **Like button** — handle clicks and update a displayed count.
5. **Signup form** — a controlled form with name, email, and a terms checkbox; log the values on submit.

## Next

**[`../02-state-and-rendering/README.md`](../02-state-and-rendering/README.md)** explains how React tracks changing data and decides when to re-render.
