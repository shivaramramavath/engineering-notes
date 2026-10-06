# Design System

A design system is the set of **decisions** that make an app look and behave consistently: colors, type, spacing, component APIs, and the rules for using them. Components are only the visible part. Without the shared decisions, ten developers build ten slightly different buttons.

With shadcn/ui you already have the first layer. This note covers how to turn it into **your** system without over-engineering it.

## The layers

```text
┌──────────────────────────────────────────────┐
│ Feature components   (InvoiceTable, SignupForm) │  ← app-specific
├──────────────────────────────────────────────┤
│ Composed components  (PageHeader, EmptyState,   │  ← reusable patterns
│                       ConfirmDialog, FormField) │
├──────────────────────────────────────────────┤
│ Primitives           (Button, Input, Dialog…)   │  ← components/ui (shadcn)
├──────────────────────────────────────────────┤
│ Tokens               (colors, radius, spacing,  │  ← CSS variables
│                       typography)               │
└──────────────────────────────────────────────┘
```

Each layer uses only the layers below it. Features compose patterns; patterns compose primitives; primitives read tokens. If a feature file contains raw hex colors or `px-[13px]`, a layer is being skipped.

## Design tokens

Tokens are named values for design decisions. Name them by **role**, not by value:

```css
/* index.css */
@custom-variant dark (&:is(.dark *));

:root {
  --background: oklch(1 0 0);
  --foreground: oklch(0.145 0 0);
  --primary: oklch(0.205 0 0);
  --primary-foreground: oklch(0.985 0 0);
  --muted: oklch(0.97 0 0);
  --muted-foreground: oklch(0.556 0 0);
  --destructive: oklch(0.577 0.245 27.325);
  --border: oklch(0.922 0 0);
  --ring: oklch(0.708 0 0);
  --radius: 0.625rem;
}

.dark {
  --background: oklch(0.145 0 0);
  --foreground: oklch(0.985 0 0);
  /* … override the rest … */
}

@theme inline {
  --color-background: var(--background);
  --color-foreground: var(--foreground);
  --color-primary: var(--primary);
  --color-primary-foreground: var(--primary-foreground);
  --color-muted: var(--muted);
  --color-muted-foreground: var(--muted-foreground);
  --color-destructive: var(--destructive);
  --color-border: var(--border);
  --color-ring: var(--ring);
}
```

The exact values and the list of tokens come from what `shadcn init` generated for your version; the structure is what matters:

- **Variables on `:root` / `.dark`** hold the values and switch with the theme.
- **`@theme inline`** (Tailwind v4) exposes them as utilities: `bg-primary`, `text-muted-foreground`, `border-border`.
- Components say `bg-primary`; they never know whether primary is blue or black, light or dark.

**Pairs:** many tokens come in `x` / `x-foreground` pairs (`primary` / `primary-foreground`). The foreground is guaranteed readable on the background. Keep that convention for new tokens.

More on switching themes in [theming and dark mode](../07-styling/04-theming-and-dark-mode.md).

### Adding a token

Say you need a `success` color:

```css
:root  { --success: oklch(0.62 0.17 145); --success-foreground: oklch(0.985 0 0); }
.dark  { --success: oklch(0.7 0.17 145);  --success-foreground: oklch(0.145 0 0); }

@theme inline {
  --color-success: var(--success);
  --color-success-foreground: var(--success-foreground);
}
```

Now `bg-success text-success-foreground` works everywhere, and a `success` variant on [Button or Badge](./01-component-variants.md) is a one-line addition. Check contrast in both themes.

## Owning the primitives

Since `components/ui/` is your code, the system lives in editing it deliberately:

- **Change defaults once.** Want every input 40px tall with a different radius? Edit `input.tsx` or the radius token, not every call site.
- **Add variants, don't add overrides.** If three screens pass the same `className` to a Button, that's a missing variant.
- **Keep the API boring.** Prop names that match native elements and shadcn conventions mean new developers (and docs) transfer directly.
- **Don't fork behavior.** Style and compose freely; be cautious editing focus, keyboard, and ARIA behavior that Radix provides.

## Composed components (the middle layer)

Wrap primitives into patterns that your app repeats. Typical candidates:

```tsx
// components/shared/confirm-dialog.tsx
type ConfirmDialogProps = {
  open: boolean
  onOpenChange: (open: boolean) => void
  title: string
  description: string
  confirmLabel?: string
  destructive?: boolean
  onConfirm: () => void | Promise<void>
}

export function ConfirmDialog({
  open, onOpenChange, title, description,
  confirmLabel = "Confirm", destructive, onConfirm,
}: ConfirmDialogProps) {
  return (
    <AlertDialog open={open} onOpenChange={onOpenChange}>
      <AlertDialogContent>
        <AlertDialogHeader>
          <AlertDialogTitle>{title}</AlertDialogTitle>
          <AlertDialogDescription>{description}</AlertDialogDescription>
        </AlertDialogHeader>
        <AlertDialogFooter>
          <AlertDialogCancel>Cancel</AlertDialogCancel>
          <AlertDialogAction
            className={destructive ? buttonVariants({ variant: "destructive" }) : undefined}
            onClick={onConfirm}
          >
            {confirmLabel}
          </AlertDialogAction>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>
  )
}
```

Others: `PageHeader`, `EmptyState`, `FormField` (label + control + error), `DataTable` (your column/pagination conventions wrapped once), `LoadingButton`. These encode **decisions** (what a confirmation looks like, how errors display) so features don't re-decide them.

**When to extract:** the third time you write the same composition, not the first. Premature abstraction produces components with ten props and no clear owner. See [component API design](../05-component-design/00-component-api-design.md).

## Folder layout

```text
src/
├── components/
│   ├── ui/              # shadcn primitives (CLI-managed, lightly customized)
│   └── shared/          # your composed components (ConfirmDialog, PageHeader…)
├── features/
│   └── invoices/
│       └── components/  # feature-specific UI
├── lib/utils.ts         # cn()
└── index.css            # tokens
```

Keep `ui/` close to upstream so re-adding a component produces a readable diff; put your opinions in `shared/`. How this fits a larger structure: [feature-based architecture](../20-frontend-architecture/00-feature-based-architecture.md).

## Scales: restrain the options

Consistency comes from **fewer choices**:

- **Spacing:** use Tailwind's scale (`p-4`, `gap-6`); avoid arbitrary values (`p-[13px]`) except for genuine one-offs.
- **Typography:** define a small set of text styles (page title, section heading, body, caption) and reuse them, via utility combinations or small components such as `<Heading level={2}>`.
- **Radius, shadows, z-index:** a handful of named steps. Document the z-index layers for overlays (dialogs, popovers, toasts) so they stop competing.
- **Icons:** one library (shadcn defaults to lucide-react) and consistent sizes.

## Accessibility belongs in the system

A design system is the cheapest place to fix accessibility, because the fix applies everywhere:

- Color pairs meet contrast requirements in light **and** dark mode.
- Focus rings come from the `ring` token and are never removed globally.
- Components require what accessibility requires (`DialogTitle`, labels on icon buttons).
- Motion respects `prefers-reduced-motion`.

See the [accessibility checklist](../08-accessibility/04-accessibility-checklist.md).

## Documenting it

A system nobody can discover isn't a system. Options, from light to heavy:

1. **A `/design` route in the app** that renders every variant of each component. Cheap, always in sync with the real code.
2. **Storybook** for isolated component development and visual review. Worth it for larger teams or shared libraries; overhead for a solo project.
3. **A short written guide** (in this repo: a markdown file) covering tokens, naming, when to use which component, and do/don't examples.

Whatever you choose, document *decisions and rules* ("destructive actions use AlertDialog"), not just prop tables.

## When you don't need one

For a small app or a solo project, shadcn + a few tokens + a `shared/` folder **is** your design system. Don't build a package, a docs site, and a governance process for three screens. Grow into structure when inconsistency actually starts costing you: reviewers repeating the same comments, near-duplicate components, rebrand pain.

## Evolving it

- **Treat tokens as the public API.** Renaming `--primary` breaks everything; adding tokens is safe.
- **Commit `components/ui/`** and review upstream changes as diffs when you re-add components.
- **Deprecate, don't delete**: mark a component or variant as deprecated, migrate usages, then remove.
- **Visual regression**: if the system matters, screenshot tests (for example, Playwright snapshots) catch accidental changes. See [E2E testing](../18-testing-and-debugging/06-e2e-testing-playwright.md).

## Common mistakes

- **Naming tokens by appearance** (`--blue-500`) instead of role (`--primary`), making rebrands and dark mode painful.
- **Hardcoded colors and arbitrary values** in feature code, bypassing tokens.
- **Overriding the same component the same way in many places** instead of adding a variant.
- **Premature abstraction**: shared components with many boolean props created before there's a pattern.
- **Editing `ui/` freely, then blindly re-adding** and losing customizations.
- **Dark mode as an afterthought**: tokens defined for light only, fixed per-component with `dark:` classes everywhere.
- **No contrast check** on custom tokens.
- **Building heavy infrastructure** (monorepo package, docs site) before the problem exists.

## Quick summary

- A design system is shared *decisions*: tokens, scales, component APIs, usage rules.
- Layers: tokens → primitives (`ui/`) → composed patterns (`shared/`) → features. Each uses only the one below.
- Tokens are CSS variables named by role, exposed to Tailwind with `@theme inline`; use `x` / `x-foreground` pairs.
- Customize by editing primitives, adding variants, and extracting patterns on the third repetition.
- Bake accessibility into tokens and components; document rules, not just props.
- Start small. shadcn plus tokens plus a `shared/` folder is already a real system.

## Next

Continue to [10 — Routing](../10-routing/README.md).
