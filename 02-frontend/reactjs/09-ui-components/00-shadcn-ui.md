# shadcn/ui

shadcn/ui is **not a component library you install**. It's a collection of well-built components that a CLI **copies into your project**. After that, they're your files: edit them, rename them, delete half of them.

Under the hood each component combines:

- **Radix UI** (or a similar headless primitive) for behavior and accessibility
- **Tailwind CSS** for styling
- **class-variance-authority (cva)** for variants
- **`cn()`** helper for merging classes

## Why this model exists

Traditional libraries (MUI, Chakra, Ant) ship compiled components with a styling API you have to fight when the design doesn't match. With shadcn:

| | Traditional library | shadcn/ui |
|---|---|---|
| Where the code lives | `node_modules` | Your repo (`components/ui/`) |
| Customizing | Theme overrides, `sx`, `styled()` | Edit the file |
| Upgrades | `npm update` | You re-add or merge manually |
| Bundle | Whole library (tree-shaken) | Only what you added |

The trade-off is real: **you own the maintenance**. You get total control, but nobody pushes bug fixes into your copy for you.

## Setup (Vite + React + TypeScript + Tailwind v4)

```bash
npm create vite@latest my-app -- --template react-ts
cd my-app
npm install tailwindcss @tailwindcss/vite
npm install -D @types/node
```

Replace the contents of `src/index.css`:

```css
@import "tailwindcss";
```

Configure the `@/` alias in `vite.config.ts`:

```ts
import path from "path"
import tailwindcss from "@tailwindcss/vite"
import react from "@vitejs/plugin-react"
import { defineConfig } from "vite"

export default defineConfig({
  plugins: [react(), tailwindcss()],
  resolve: {
    alias: { "@": path.resolve(__dirname, "./src") },
  },
})
```

And in both `tsconfig.json` and `tsconfig.app.json`:

```json
{
  "compilerOptions": {
    "baseUrl": ".",
    "paths": { "@/*": ["./src/*"] }
  }
}
```

Then initialize shadcn:

```bash
npx shadcn@latest init
```

The CLI asks a few questions (base color, etc.), writes `components.json`, updates your CSS with theme variables, and creates `src/lib/utils.ts`.

> Setup steps shift between releases. If something here disagrees with the CLI output, trust the CLI and the [official Vite guide](https://ui.shadcn.com/docs/installation/vite).

## Adding components

```bash
npx shadcn@latest add button dialog dropdown-menu
```

Files land in `src/components/ui/`. Use them like normal components:

```tsx
import { Button } from "@/components/ui/button"

export function Example() {
  return <Button variant="outline" onClick={() => console.log("hi")}>Save</Button>
}
```

## The pieces

### `cn()` — class merging

```ts
// src/lib/utils.ts
import { clsx, type ClassValue } from "clsx"
import { twMerge } from "tailwind-merge"

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
}
```

- `clsx` handles conditionals: `cn("p-2", isActive && "bg-primary")`
- `twMerge` resolves Tailwind conflicts so the **last class wins**: `cn("px-2", "px-4")` → `"px-4"`

Without `twMerge`, both `px-2` and `px-4` end up in the DOM and CSS source order, not your intent, decides the winner.

### `components.json`

Tells the CLI where things go:

```json
{
  "style": "new-york",
  "tailwind": { "css": "src/index.css", "cssVariables": true },
  "aliases": {
    "components": "@/components",
    "ui": "@/components/ui",
    "utils": "@/lib/utils",
    "hooks": "@/hooks"
  }
}
```

If you move folders, update the aliases or `add` will put files in the old location.

### CSS variables for theming

Colors are semantic tokens (`--background`, `--primary`, `--muted`, …) defined in your CSS and consumed as Tailwind classes (`bg-background`, `text-muted-foreground`). Change the variable, the whole app changes. Details in [09-design-system](./09-design-system.md) and [theming and dark mode](../07-styling/04-theming-and-dark-mode.md).

## Anatomy of a generated component

Open `components/ui/button.tsx` after adding it. Roughly:

```tsx
const buttonVariants = cva("inline-flex items-center justify-center ...", {
  variants: { variant: { default: "...", outline: "..." }, size: { default: "...", sm: "..." } },
  defaultVariants: { variant: "default", size: "default" },
})

function Button({ className, variant, size, asChild = false, ...props }: ButtonProps) {
  const Comp = asChild ? Slot : "button"
  return <Comp className={cn(buttonVariants({ variant, size, className }))} {...props} />
}
```

Three things to notice: it's a plain function component that spreads native props, it takes `className` so callers can override, and it's driven by `cva`. That's the same template for nearly every component. See [01-component-variants](./01-component-variants.md).

## Customizing the right way

1. **Small tweaks** (one place): pass `className` at the call site.
2. **Consistent change** (everywhere): edit the file in `components/ui/`.
3. **New variant**: add it to the `cva` config.

Because the code is yours, commit it. Reviewing a diff of `components/ui/` after re-adding a component is how you "upgrade":

```bash
npx shadcn@latest add button --overwrite
git diff src/components/ui/button.tsx   # keep your changes, take theirs
```

## Common mistakes

- **Treating it like an npm package.** There is no `import { Button } from "shadcn-ui"`. Components come from *your* `@/components/ui`.
- **Forgetting the alias setup.** Errors like `Cannot find module '@/lib/utils'` mean the `tsconfig` *and* Vite alias are not both configured.
- **Overwriting customized components blindly.** `--overwrite` replaces your edits. Commit first.
- **Editing the Radix behavior by accident.** Keep changes to classes and composition; if you rewrite event or focus handling, you've taken ownership of the accessibility bugs too.
- **Dynamic class names.** `` `bg-${color}-500` `` is invisible to Tailwind's scanner. Use full class names or variants.
- **Mixing multiple styling systems.** Pick Tailwind + tokens; adding styled-components beside it makes overrides unpredictable.

## Quick summary

- shadcn/ui = CLI that copies Radix + Tailwind components into your repo.
- You own the code: maximum flexibility, your responsibility to maintain.
- `cn()` = `clsx` + `tailwind-merge`; use it for every `className`.
- `components.json` controls paths; theme lives in CSS variables.
- Customize via `className`, then the file, then `cva` variants.

## Next

[01 — Component variants](./01-component-variants.md)
