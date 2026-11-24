# Project 12: Design System with Storybook (`design-system-storybook`)

> An enterprise component library showcasing accessible primitives built on Radix UI, polymorphic component variants via Class Variance Authority (CVA), Tailwind CSS styling tokens, and automated visual regression stories in Storybook.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-12-design-system` |
| **React Version** | React 19.0.0 |
| **Tested With** | Storybook 8.4, Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | Stateless Primitives + Radix Uncontrolled State |
| **Styling** | Tailwind CSS v4 + Class Variance Authority (CVA) + clsx/tailwind-merge |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Data Flow
Components are built adhering to the Open-Closed Principle and WAI-ARIA guidelines using Radix UI primitives. Styling variants are declared systematically with `class-variance-authority` (CVA), while `asChild` composition delegates DOM rendering without extra wrapper elements.

```
[ Radix UI Primitive (Accessibility / Focus Management) ]
                           │
                           ▼
[ CVA Variant Definitions (Primary / Secondary / Destructive / Sizes) ]
                           │
                           ▼
[ Tailwind Merge (Clean class resolution without collision) ]
                           │
                           ▼
[ Exported UI Component + Storybook Stories ]
```

### 1.2 Component Structure
```text
src/
├── lib/
│   └── utils.ts              # cn() utility (clsx + tailwind-merge)
├── components/
│   ├── Button/
│   │   ├── Button.tsx        # CVA polymorphic button
│   │   └── Button.stories.tsx# Storybook variant matrix
│   ├── Dialog/
│   │   ├── Dialog.tsx        # Radix Dialog wrapper
│   │   └── Dialog.stories.tsx# Modal interaction story
│   └── Badge/
│       ├── Badge.tsx         # Pill badges with intent colors
│       └── Badge.stories.tsx # Status stories
└── index.ts                  # Public package exports
```

---

## 2. Complete Project Source Code

### 2.1 Utility (`src/lib/utils.ts`)
```ts
import { clsx, type ClassValue } from 'clsx';
import { twMerge } from 'tailwind-merge';

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}
```

### 2.2 Button Component with CVA (`src/components/Button/Button.tsx`)
```tsx
import * as React from 'react';
import { Slot } from '@radix-ui/react-slot';
import { cva, type VariantProps } from 'class-variance-authority';
import { cn } from '../../lib/utils';

export const buttonVariants = cva(
  'inline-flex items-center justify-center rounded-lg text-sm font-semibold transition-colors focus-visible:outline-2 focus-visible:outline-offset-2 disabled:pointer-events-none disabled:opacity-50',
  {
    variants: {
      variant: {
        default: 'bg-blue-600 text-white hover:bg-blue-700 focus-visible:outline-blue-600',
        destructive: 'bg-rose-600 text-white hover:bg-rose-700 focus-visible:outline-rose-600',
        outline: 'border border-slate-300 bg-transparent text-slate-700 hover:bg-slate-100',
        ghost: 'text-slate-700 hover:bg-slate-100',
      },
      size: {
        sm: 'h-8 px-3 text-xs',
        md: 'h-10 px-4 text-sm',
        lg: 'h-12 px-6 text-base',
      },
    },
    defaultVariants: {
      variant: 'default',
      size: 'md',
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
      <Comp className={cn(buttonVariants({ variant, size, className }))} ref={ref} {...props} />
    );
  }
);
Button.displayName = 'Button';
```

### 2.3 Storybook Story (`src/components/Button/Button.stories.tsx`)
```tsx
import type { Meta, StoryObj } from '@storybook/react';
import { Button } from './Button';

const meta: Meta<typeof Button> = {
  title: 'Components/Button',
  component: Button,
  tags: ['autodocs'],
  argTypes: {
    variant: {
      control: 'select',
      options: ['default', 'destructive', 'outline', 'ghost'],
    },
    size: {
      control: 'select',
      options: ['sm', 'md', 'lg'],
    },
  },
};

export default meta;
type Story = StoryObj<typeof Button>;

export const Default: Story = {
  args: {
    children: 'Button',
    variant: 'default',
    size: 'md',
  },
};

export const Destructive: Story = {
  args: {
    children: 'Delete Account',
    variant: 'destructive',
  },
};
```

---

## 3. Testing Suite (`src/tests/Button.test.tsx`)

```tsx
import { render, screen } from '@testing-library/react';
import { Button } from '../components/Button/Button';
import { describe, it, expect } from 'vitest';

describe('Button Design Primitive', () => {
  it('renders default button element', () => {
    render(<Button>Click Me</Button>);
    const btn = screen.getByRole('button', { name: 'Click Me' });
    expect(btn).toBeInTheDocument();
    expect(btn).toHaveClass('bg-blue-600');
  });

  it('renders as anchor when asChild is used', () => {
    render(
      <Button asChild>
        <a href="/login">Login</a>
      </Button>
    );
    const link = screen.getByRole('link', { name: 'Login' });
    expect(link).toHaveAttribute('href', '/login');
  });
});
```

---

## 4. Build and Run Instructions

```bash
# Install dependencies
npm install @radix-ui/react-slot class-variance-authority clsx tailwind-merge

# Start Storybook
npm run storybook
```
