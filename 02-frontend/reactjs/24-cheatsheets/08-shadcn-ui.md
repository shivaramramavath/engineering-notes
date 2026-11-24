# shadcn/ui & Radix UI Cheatsheet

> Quick-reference guide for Radix UI primitives, `asChild` composition, CVA variants, and keyboard-accessible UI patterns.

---

## 1. Class Variance Authority (CVA) Variant Syntax

```ts
import { cva, type VariantProps } from 'class-variance-authority';

export const badgeVariants = cva(
  'inline-flex items-center rounded-full px-2.5 py-0.5 text-xs font-semibold transition-colors',
  {
    variants: {
      variant: {
        default: 'bg-slate-900 text-white hover:bg-slate-800',
        secondary: 'bg-slate-100 text-slate-900 hover:bg-slate-200',
        destructive: 'bg-rose-500 text-white hover:bg-rose-600',
        outline: 'border border-slate-200 text-slate-800',
      },
    },
    defaultVariants: {
      variant: 'default',
    },
  }
);
```

---

## 2. Accessible Dialog (Modal) with Radix UI

```tsx
import * as Dialog from '@radix-ui/react-dialog';

export function ConfirmModal({ isOpen, onClose, onConfirm }: ModalProps) {
  return (
    <Dialog.Root open={isOpen} onOpenChange={onClose}>
      <Dialog.Portal>
        <Dialog.Overlay className="fixed inset-0 bg-black/50 backdrop-blur-xs" />
        <Dialog.Content className="fixed left-1/2 top-1/2 -translate-x-1/2 -translate-y-1/2 rounded-xl bg-white p-6 shadow-2xl">
          <Dialog.Title className="text-lg font-bold">Confirm Action</Dialog.Title>
          <Dialog.Description className="mt-2 text-sm text-slate-600">
            Are you sure you want to proceed? This action cannot be undone.
          </Dialog.Description>
          <div className="mt-6 flex justify-end gap-3">
            <Dialog.Close asChild>
              <button className="btn-secondary">Cancel</button>
            </Dialog.Close>
            <button onClick={onConfirm} className="btn-primary">Confirm</button>
          </div>
        </Dialog.Content>
      </Dialog.Portal>
    </Dialog.Root>
  );
}
```
