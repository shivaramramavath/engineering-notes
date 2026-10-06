# Component Variants

A button has a style (primary, outline, destructive) and a size (sm, md, lg). Multiply that by hover, disabled, and dark mode and you get dozens of class combinations. Writing them as nested ternaries gets unreadable fast. **class-variance-authority (`cva`)** turns them into a declarative config with full TypeScript support.

## The problem

```tsx
// Don't
<button
  className={`px-4 py-2 rounded ${
    variant === "primary" ? "bg-blue-600 text-white" :
    variant === "outline" ? "border" :
    "bg-red-600 text-white"
  } ${size === "sm" ? "text-sm" : "text-base"}`}
/>
```

No type safety, no defaults, impossible to extend cleanly.

## `cva` basics

```bash
npm install class-variance-authority
```

```tsx
import { cva, type VariantProps } from "class-variance-authority"

const badgeVariants = cva(
  // base classes — always applied
  "inline-flex items-center rounded-md px-2 py-0.5 text-xs font-medium",
  {
    variants: {
      variant: {
        default: "bg-primary text-primary-foreground",
        secondary: "bg-secondary text-secondary-foreground",
        outline: "border text-foreground",
      },
      size: {
        sm: "text-xs",
        lg: "text-sm px-3 py-1",
      },
    },
    defaultVariants: {
      variant: "default",
      size: "sm",
    },
  }
)

badgeVariants()                          // default + sm
badgeVariants({ variant: "outline" })    // outline + sm
badgeVariants({ size: "lg" })            // default + lg
```

`badgeVariants(...)` just returns a class string. It isn't tied to React, so you can also apply it to a `<Link>` or an `<a>`.

## Wiring it into a component

```tsx
import * as React from "react"
import { cva, type VariantProps } from "class-variance-authority"
import { cn } from "@/lib/utils"

const badgeVariants = cva(/* ... as above ... */)

type BadgeProps = React.ComponentProps<"span"> & VariantProps<typeof badgeVariants>

export function Badge({ className, variant, size, ...props }: BadgeProps) {
  return <span className={cn(badgeVariants({ variant, size }), className)} {...props} />
}
```

- `React.ComponentProps<"span">` gives you every native span prop (and `ref` in React 19 — no `forwardRef` needed).
- `VariantProps<typeof badgeVariants>` derives `variant` and `size` types **from the config**. Add a variant and the type updates automatically.
- Passing `className` **last** to `cn` lets callers override the defaults.

```tsx
<Badge variant="outline" size="lg" className="uppercase" />
<Badge variant="nope" />   // ✗ TypeScript error
```

## Compound variants

When a combination needs its own style:

```tsx
const alertVariants = cva("rounded-lg border p-4", {
  variants: {
    tone: { info: "", danger: "" },
    emphasis: { subtle: "", strong: "" },
  },
  compoundVariants: [
    { tone: "danger", emphasis: "strong", class: "bg-destructive text-white" },
    { tone: "danger", emphasis: "subtle", class: "bg-destructive/10 text-destructive" },
  ],
  defaultVariants: { tone: "info", emphasis: "subtle" },
})
```

Use these sparingly. If most combinations need custom handling, the "variants" are probably separate components.

## Why `cn()` matters here

`cva` joins strings; it does not resolve conflicts. The override behavior comes from `twMerge` inside `cn`:

```tsx
badgeVariants({ variant: "default" })   // "... px-2 ... bg-primary"
cn(badgeVariants(), "px-6")             // "px-2" removed, "px-6" wins
```

## `asChild` — change the element, keep the styles

Sometimes a `Button` should *look* like a button but *be* a link:

```tsx
import { Link } from "react-router"

<Button asChild>
  <Link to="/settings">Settings</Link>
</Button>
```

`asChild` (a Radix convention) uses `Slot` to merge the Button's props, classes, and event handlers **onto the child** instead of rendering a `<button>`. You get valid HTML (`<a class="...">`), not `<button><a></a></button>`, which is invalid and breaks keyboard behavior.

```tsx
import { Slot } from "@radix-ui/react-slot"

function Button({ asChild = false, className, variant, size, ...props }: ButtonProps) {
  const Comp = asChild ? Slot : "button"
  return <Comp className={cn(buttonVariants({ variant, size }), className)} {...props} />
}
```

Your generated code may import `Slot` from the unified `radix-ui` package instead, depending on the shadcn version. Both work the same way.

**Rules for `asChild`:** the child must be a single element, and it must forward props and `ref` to a DOM node. A custom component that swallows props will silently lose the behavior.

## Adding your own variant

Because shadcn code is yours, adding `success` is just a config edit:

```tsx
variants: {
  variant: {
    default: "...",
    destructive: "...",
    success: "bg-success text-success-foreground hover:bg-success/90",
  },
}
```

Define the `--success` token in your CSS first — see [09-design-system](./09-design-system.md).

## Common mistakes

- **Dynamic class strings** inside variants (`` `bg-${tone}-500` ``). Tailwind only generates classes it can find as full strings in your source.
- **Forgetting `className` last** in `cn(...)`. Caller overrides stop working.
- **Too many boolean props** (`isPrimary`, `isLarge`, `isGhost`). One `variant` prop with named values stays understandable and can't hold contradictory states.
- **`asChild` on a component that doesn't forward props.** The styles never reach the DOM.
- **Variants for behavior.** Variants are for appearance. If `variant="danger"` also changes what `onClick` does, that's two concerns in one prop.
- **Using `variant` names tied to color** (`blue`, `red`). Prefer meaning (`default`, `destructive`) so a rebrand doesn't rename your API.

## Quick summary

- `cva(base, { variants, compoundVariants, defaultVariants })` returns a function that produces class strings.
- `VariantProps<typeof x>` gives you type-safe props from the same config.
- Always combine with `cn()` so `className` overrides win.
- `asChild` renders your styles on a different element — use it for links styled as buttons.
- Name variants by meaning, not color.

## Next

[02 — Dialogs and modals](./02-dialogs-and-modals.md)
