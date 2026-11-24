# Radix UI Primitives — Basic to Advanced

A comprehensive reference card for `@radix-ui/react-*` unstyled accessible primitives in React 19.

> **Target Version:** Radix Primitives v1.1+  
> **Philosophy:** Unstyled, accessible, composable UI building blocks  
> **Key Architecture:** WAI-ARIA compliance, focus trapping, portal mounting, the `asChild` Slot pattern  

---

## Level 0 — Concepts

- **Unstyled Primitives**: Radix components provide 100% of the accessibility, keyboard navigation, focus management, and ARIA attributes with 0% CSS styles. You style them with Tailwind CSS or CSS Modules.
- **The `asChild` Slot Pattern**: Instead of rendering wrapper `<div>` or `<button>` elements, passing `asChild` merges Radix event handlers and ARIA attributes directly onto your own child element using Radix's `<Slot>`.
- **Core Primitives Table**:

| Package | Primitives | Purpose |
| :--- | :--- | :--- |
| `@radix-ui/react-dialog` | `Root`, `Trigger`, `Portal`, `Overlay`, `Content`, `Title` | Accessible modal dialogs with focus trapping |
| `@radix-ui/react-dropdown-menu` | `Root`, `Trigger`, `Content`, `Item`, `Separator` | Contextual menus with keyboard typeahead |
| `@radix-ui/react-popover` | `Root`, `Trigger`, `Anchor`, `Content` | Floating tooltips and interactive popovers |
| `@radix-ui/react-tabs` | `Root`, `List`, `Trigger`, `Content` | Accessible tabbed panels with arrow key navigation |
| `@radix-ui/react-slot` | `Slot` | Merges props and behavior onto immediate children |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install @radix-ui/react-dialog @radix-ui/react-slot
```

### 1.2 Accessible Modal Dialog with `asChild`
```tsx
import React from 'react';
import * as Dialog from '@radix-ui/react-dialog';

export function AccessibleModal() {
  return (
    <Dialog.Root>
      {/* asChild merges click listener directly onto custom button */}
      <Dialog.Trigger asChild>
        <button className="px-4 py-2 bg-blue-600 text-white rounded font-medium hover:bg-blue-700">
          Open Settings
        </button>
      </Dialog.Trigger>

      <Dialog.Portal>
        {/* Semi-transparent Backdrop */}
        <Dialog.Overlay className="fixed inset-0 bg-black/50 backdrop-blur-sm z-50 data-[state=open]:animate-fade-in" />
        {/* Centered Modal Content with Focus Trap */}
        <Dialog.Content className="fixed top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-full max-w-md bg-white p-6 rounded-xl shadow-2xl z-50 focus:outline-none">
          <Dialog.Title className="text-lg font-bold text-gray-900">
            Account Preferences
          </Dialog.Title>
          <Dialog.Description className="text-sm text-gray-500 my-2">
            Update your profile and notification settings.
          </Dialog.Description>

          <div className="mt-6 flex justify-end gap-2">
            <Dialog.Close asChild>
              <button className="px-3 py-1.5 border rounded text-sm hover:bg-gray-50">
                Cancel
              </button>
            </Dialog.Close>
            <button className="px-3 py-1.5 bg-blue-600 text-white rounded text-sm">
              Save
            </button>
          </div>
        </Dialog.Content>
      </Dialog.Portal>
    </Dialog.Root>
  );
}
```

---

## Level 2 — Intermediate: Styling with Data Attributes

Radix components expose dynamic state via CSS `data-*` attributes:
- `data-state="open | closed"`
- `data-disabled`
- `data-highlighted`

```css
/* Styling Radix states directly in Tailwind or CSS */
.my-dialog-content[data-state='open'] {
  animation: scaleIn 200ms ease-out;
}
```

---

## Level 3 — Advanced: Building a Custom Slot Primitive

```tsx
import { Slot } from '@radix-ui/react-slot';
import React from 'react';

export function Button({ asChild, ...props }: { asChild?: boolean; [key: string]: any }) {
  const Comp = asChild ? Slot : 'button';
  return <Comp className="px-4 py-2 bg-blue-600 text-white rounded font-medium" {...props} />;
}

// When used asChild, it styles a React Router Link without adding a wrapper DOM element!
// <Button asChild><Link to="/dashboard">Go to Dashboard</Link></Button>
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Omitting `<Dialog.Title>`** | Fails accessibility compliance; screen readers cannot announce the dialog's purpose. | Always include `<Dialog.Title>` inside Content (or provide `aria-label`). |
| **Passing Multiple Children to `<Slot>` or `asChild`**| `<Slot>` expects exactly one React element child; crashes with `React.Children.only` error. | Ensure `asChild` wraps exactly one child element. |
| **Mounting Dialog Content Outside `<Dialog.Portal>`**| Modal is trapped inside parent CSS `overflow: hidden` or stacking context z-index clipping. | Always wrap overlays and content in `<Dialog.Portal>`. |
| **Suppressing Escape Key Handlers Unconditionally** | Traps keyboard users inside dialogs, violating WCAG 2.1.2. | Allow `Escape` to close dialogs unless unsaved changes confirmation is required. |
