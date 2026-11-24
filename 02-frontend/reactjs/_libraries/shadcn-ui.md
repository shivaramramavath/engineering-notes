# shadcn/ui — Basic to Advanced

A comprehensive reference card for the `shadcn/ui` component architecture in React 19.

> **Architecture:** Copy-Paste Component Architecture (Not an NPM package dependency)  
> **Foundation:** Radix UI Primitives + Tailwind CSS v4 + Class Variance Authority (CVA)  
> **Philosophy:** You own the code; copy components directly into your `src/components/ui/` directory  

---

## Level 0 — Concepts

- **Not an NPM Dependency**: You do not install `npm install shadcn-ui`. Instead, the CLI copies clean, accessible source code directly into your repository. You have 100% control to modify, style, or extend components.
- **The Holy Trinity**:
  1. **Radix UI**: Provides 100% WAI-ARIA accessibility, keyboard navigation, and focus traps.
  2. **Tailwind CSS**: Provides utility-first styling with dark mode and theme tokens.
  3. **Class Variance Authority (CVA)**: Provides type-safe variant matrices (primary, secondary, destructive, sizes).
- **Core Utilities**:

| Utility | File | Purpose |
| :--- | :--- | :--- |
| `cn()` | `src/lib/utils.ts` | Merges Tailwind classes cleanly using `clsx` and `tailwind-merge` |
| `cva()` | Component files | Declares variant matrices with full TypeScript inference |
| `components.json` | Project Root | Configuration file for shadcn CLI aliases and styling defaults |

---

## Level 1 — Basic Setup & Usage

### 1.1 Project Initialization
```bash
npx shadcn@latest init
```

### 1.2 Adding Components via CLI
```bash
npx shadcn@latest add button dialog dropdown-menu input
```

### 1.3 The Canonical `cn()` Utility
```ts
// src/lib/utils.ts
import { clsx, type ClassValue } from 'clsx';
import { twMerge } from 'tailwind-merge';

export function cn(...inputs: ClassValue[]): string {
  // Merges classes while intelligently overriding conflicting Tailwind utilities
  return twMerge(clsx(inputs));
}
```

---

## Level 2 — Intermediate: Component Variants with CVA

```tsx
// src/components/ui/button.tsx
import * as React from 'react';
import { Slot } from '@radix-ui/react-slot';
import { cva, type VariantProps } from 'class-variance-authority';
import { cn } from '@/lib/utils';

export const buttonVariants = cva(
  'inline-flex items-center justify-center rounded-md text-sm font-medium transition-colors focus-visible:outline-none focus-visible:ring-2 disabled:pointer-events-none disabled:opacity-50',
  {
    variants: {
      variant: {
        default: 'bg-blue-600 text-white hover:bg-blue-700',
        destructive: 'bg-red-600 text-white hover:bg-red-700',
        outline: 'border border-gray-300 bg-transparent hover:bg-gray-100',
        ghost: 'hover:bg-gray-100 text-gray-700',
      },
      size: {
        default: 'h-10 px-4 py-2',
        sm: 'h-8 px-3 text-xs',
        lg: 'h-12 px-8 text-base',
        icon: 'h-10 w-10',
      },
    },
    defaultVariants: {
      variant: 'default',
      size: 'default',
    },
  }
);

export interface ButtonProps
  extends React.ButtonHTMLAttributes<HTMLButtonElement>,
    VariantProps<typeof buttonVariants> {
  asChild?: boolean;
}

export const Button = React.forwardRef<HTMLButtonElement, ButtonProps>(
  ({ className, variant, size, asChild = false, ...props }, ref) => {
    const Comp = asChild ? Slot : 'button';
    return (
      <Comp
        className={cn(buttonVariants({ variant, size, className }))}
        ref={ref}
        {...props}
      />
    );
  }
);
Button.displayName = 'Button';
```

---

## Level 3 — Advanced: Composing Modals & Forms

```tsx
import { Button } from '@/components/ui/button';
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from '@/components/ui/dialog';

export function InviteMemberDialog() {
  return (
    <Dialog>
      <DialogTrigger asChild>
        <Button variant="outline">Invite Teammate</Button>
      </DialogTrigger>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>Invite Teammate</DialogTitle>
        </DialogHeader>
        <div className="py-4">
          <input placeholder="colleague@company.com" className="w-full border p-2 rounded text-sm" />
        </div>
        <Button className="w-full">Send Invitation</Button>
      </DialogContent>
    </Dialog>
  );
}
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Concatenating Classes without `twMerge`** | Template literals like `` `p-2 ${className}` `` fail to override conflicting padding classes properly. | Always use `cn('p-2', className)`. |
| **Treating shadcn as an Immutable Library** | Hesitating to edit components in `src/components/ui/` limits design freedom. | Edit and customize components directly; you own the code. |
| **Hardcoding Hex Colors in Components** | Breaks Tailwind dark mode and global CSS theme variables. | Use CSS variable tokens: `bg-primary text-primary-foreground`. |
| **Missing Accessible Labels on Icon Buttons** | Screen readers announce "Button" with no context. | Always add `aria-label="Delete"` on icon-only buttons. |
