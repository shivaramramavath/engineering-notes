# Composition Patterns

Composition means building behavior by **combining** components instead of adding options to one big component. The basics (`children`, named slots, containment) are in [`../01-fundamentals/03-children-and-composition.md`](../01-fundamentals/03-children-and-composition.md). This file covers the patterns that go beyond them: inverting control, headless components, polymorphism (`as` and `asChild`), and wrapping.

## Prerequisites

[`../01-fundamentals/03-children-and-composition.md`](../01-fundamentals/03-children-and-composition.md), [`00-component-api-design.md`](./00-component-api-design.md), and [`../03-hooks/09-custom-hooks.md`](../03-hooks/09-custom-hooks.md)

---

## 1. Inversion of control: let the caller decide

A component that decides everything internally needs a prop for every variation. Giving the **caller** control over the parts removes the need:

```tsx
// Configuration: the component owns the structure
<Card title="Plan" price="$9" badge="Popular" footerLabel="Buy" onFooterClick={buy} />

// Composition: the caller owns the structure
<Card>
  <Card.Header>
    Plan <Badge>Popular</Badge>
  </Card.Header>
  <Card.Body>$9</Card.Body>
  <Card.Footer>
    <Button onClick={buy}>Buy</Button>
  </Card.Footer>
</Card>
```

The `Card` provides frame, spacing, and shared behavior; it doesn't know what's inside.

---

## 2. Slots through props

When regions are fixed and distinct, pass elements through named props (see the fundamentals chapter for the basics). A typed layout with an optional slot:

```tsx
type PageProps = {
  header: ReactNode;
  sidebar?: ReactNode;
  children: ReactNode;
};

function Page({ header, sidebar, children }: PageProps) {
  return (
    <div className="page">
      <header>{header}</header>
      {sidebar && <aside>{sidebar}</aside>}
      <main>{children}</main>
    </div>
  );
}
```

Use **named props** for a small, fixed set of regions; use **compound components** ([`02-compound-components.md`](./02-compound-components.md)) when the number or order of parts varies.

---

## 3. Lift content up to avoid prop drilling

If data must reach a deep component, **build that component high in the tree** and pass the finished element down:

```tsx
function App({ user }: { user: User }) {
  return (
    <Layout
      nav={<Nav userMenu={<UserMenu user={user} />} />}
    />
  );
}
```

`Layout` and `Nav` render `userMenu` without knowing about `user`. Try this before reaching for context ([`../03-hooks/05-useContext.md`](../03-hooks/05-useContext.md)).

---

## 4. Specialization by wrapping

Create specific components by **wrapping** a general one and fixing some props:

```tsx
function DangerButton(props: ComponentProps<typeof Button>) {
  return <Button variant="danger" {...props} />;
}

function SaveButton({ saving, ...props }: { saving: boolean } & ComponentProps<typeof Button>) {
  return (
    <Button disabled={saving} {...props}>
      {saving ? "Saving…" : "Save"}
    </Button>
  );
}
```

This is how design systems stay consistent while teams add convenience components. Order matters: put `{...props}` where you want callers to be able to override (usually last).

---

## 5. Headless components: behavior without markup

A **headless** component (or hook) provides state, logic, and accessibility attributes, and leaves **all rendering and styling** to you. The same behavior can then drive many designs.

### Headless as a hook

```tsx
import { useId, useState, type ComponentProps } from "react";

function useDisclosure(defaultOpen = false) {
  const [open, setOpen] = useState(defaultOpen);
  const id = useId();
  const panelId = `${id}-panel`;

  return {
    open,
    toggle: () => setOpen((o) => !o),
    getTriggerProps: () => ({
      "aria-expanded": open,
      "aria-controls": panelId,
      onClick: () => setOpen((o) => !o),
    }),
    getPanelProps: () => ({
      id: panelId,
      hidden: !open,
    }),
  };
}
```

```tsx
function FaqItem({ question, answer }: { question: string; answer: string }) {
  const { open, getTriggerProps, getPanelProps } = useDisclosure();

  return (
    <div>
      <button {...getTriggerProps()}>{question} {open ? "−" : "+"}</button>
      <p {...getPanelProps()}>{answer}</p>
    </div>
  );
}
```

The hook returns **prop getters**: objects of attributes to spread onto *your* elements. This "prop getter" pattern keeps ARIA wiring (`aria-expanded`, `aria-controls`) correct while the markup stays yours. Libraries like Radix (unstyled primitives), React Aria, Headless UI, Downshift, and TanStack Table use this idea — it's the foundation of shadcn/ui ([`../09-ui-components/00-shadcn-ui.md`](../09-ui-components/00-shadcn-ui.md)).

### Headless as a component

A component can expose the same data through children-as-function (a render prop; see [`03-legacy-component-patterns.md`](./03-legacy-component-patterns.md)) or through context with compound parts. Today, **hooks** are the most common headless interface.

**When to go headless:** you need many visual variants of the same complex behavior (menus, comboboxes, date pickers) or you're building a library. **When not to:** a simple component with one look; headless adds indirection for no gain.

---

## 6. Polymorphic components: `as`

Sometimes a component's markup should change per use — a `Button` that renders a link, a `Text` that renders `h1` or `p`. The `as` prop lets the caller choose the element:

```tsx
<Button as="a" href="/pricing">See pricing</Button>
<Text as="h2">Heading</Text>
```

The typing needs generics to give each element its correct props — see [`../04-typescript-with-react/05-advanced-typing-patterns.md`](../04-typescript-with-react/05-advanced-typing-patterns.md). Be sure the result remains semantic and accessible (a "button" that navigates should be a link).

Drawbacks: complex types, and each new `as` component (a router `Link`) needs the right props. This leads to `asChild`.

---

## 7. `asChild`: render as your own element

Radix UI's approach: instead of an `as` prop with a tag name, the component **merges its props and behavior onto the child element you pass**:

```tsx
import { Slot } from "@radix-ui/react-slot";
import type { ComponentProps } from "react";

type ButtonProps = ComponentProps<"button"> & { asChild?: boolean };

function Button({ asChild, ...props }: ButtonProps) {
  const Comp = asChild ? Slot : "button";
  return <Comp className="btn" {...props} />;
}
```

```tsx
<Button>Default button</Button>

<Button asChild>
  <Link to="/pricing">See pricing</Link>   {/* renders a router Link with the button's classes and props */}
</Button>
```

How it works: `Slot` clones its single child and merges props (className, event handlers, refs). Benefits over `as`: any component can be the child (your router's `Link`, a framework image component), with its own prop types intact and no generics needed. shadcn/ui uses this pattern widely.

Constraints: the child must be **one element** that accepts the props and `ref` being passed. Avoid writing your own `Slot` — use the Radix package.

---

## 8. Avoid `cloneElement` and `Children.map`

You'll find tutorials that inject props into children:

```tsx
// ❌ Fragile
{Children.map(children, (child) => cloneElement(child, { active: true }))}
```

Problems: it breaks when children are wrapped in a fragment, a wrapper component, or conditionally rendered, hides data flow, and fights TypeScript. Prefer:

1. **Context** (compound components — [`02-compound-components.md`](./02-compound-components.md)),
2. **Explicit props**, or
3. **Render props / prop getters** for passing data to children.

`Slot` is the narrow, well-defined exception because it handles one known child on purpose.

---

## 9. Choosing a pattern

| Need | Pattern |
|------|---------|
| Wrap arbitrary content in a frame | `children` |
| A few fixed regions (header, sidebar) | Named slot props |
| A family of parts sharing state (Tabs, Accordion, Menu) | Compound components |
| Same behavior, many looks | Headless hook / prop getters |
| Render as a different element or component | `as` or `asChild` |
| Customized version of an existing component | Wrapping |
| Pass data into children | Context, render props, or prop getters — not `cloneElement` |
| Avoid prop drilling | Lift the element up; context if needed |

---

## Common mistakes

- **Adding props for each variation of content or layout** — accept elements instead.
- **Reaching for `cloneElement`/`Children.map`** — breaks with wrappers and fragments; use context or explicit props.
- **Hand-rolling accessibility in headless code** — use a vetted primitive library for menus, dialogs, and comboboxes.
- **Overusing headless patterns** for simple components — extra abstraction with no benefit.
- **Using `as` for a button that navigates** — use a real link; semantics matter.
- **Spreading props in the wrong order** — callers can't override, or can break required attributes.
- **Passing `asChild` more than one child** — `Slot` requires exactly one element.

## Quick summary

- Compose by giving callers control of structure; configure only simple values
- Named slot props for fixed regions; compound components for flexible families
- Headless hooks return state and prop getters; you supply markup and styles
- `as` lets callers pick the element (needs generics); `asChild` merges onto your own element
- Wrap components to specialize them; keep `{...props}` where overriding makes sense
- Avoid `cloneElement`; use context or explicit props

## Next

**[`02-compound-components.md`](./02-compound-components.md)** builds a full component family that shares state through context.
