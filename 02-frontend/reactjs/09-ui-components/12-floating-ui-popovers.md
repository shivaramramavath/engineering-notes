# Floating UI: High-Precision Positioning for Tooltips, Popovers & Menus

- **Topic**: `09-ui-components`
- **React version**: React 19+ (`@floating-ui/react` v0.26+)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0, `@floating-ui/react` 0.26.28
- **Bundle / compiler notes**: Evolution of Popper.js; lightweight (~3.5KB) positioning and interaction engine; powers Radix Popovers and Shadcn Tooltips.

---

## 1. Concept Overview

Positioning floating UI elements (tooltips, popovers, dropdown menus, hover cards, comboboxes) relative to an anchor element is deceptively complex:
- If a button sits near the bottom edge of the viewport, an opening menu must **flip** above the button to stay visible.
- If a user scrolls the page or resizes the browser, the floating menu must **track** the reference element in real time without lag.
- If a menu threatens to overflow the screen horizontally, it must **shift** inward along the viewport axis.
- Anchored arrows pointing from the floating box to the trigger must accurately track diagonal boundaries.
- Context menus must anchor to dynamic **virtual coordinates** (mouse click X/Y) rather than a static DOM node.

**Floating UI** (formerly Popper.js) is the industry-standard positioning engine for the web.

Key Architectural Advantages:
- **Modular Middleware Pipeline**: Calculations run through a composable pipeline (`offset()`, `flip()`, `shift()`, `arrow()`, `size()`).
- **Interaction Hooks**: `@floating-ui/react` provides complete interaction management (`useHover`, `useFocus`, `useDismiss`, `useRole`, `useId`).
- **Zero Layout Shifts (`autoUpdate`)**: Real-time position tracking across parent scroll containers, window resizes, and dynamic element mutations.
- **Virtual Reference Support**: Position tooltips against mouse cursors or highlighted text selections effortlessly.

```
Floating UI Positioning Pipeline:
[ Reference Anchor Element ]
             │
             ├──► 1. `offset(8)` (Adds 8px gap)
             ├──► 2. `flip()` (Flips from bottom -> top if hitting screen edge)
             ├──► 3. `shift({ padding: 5 })` (Shifts left/right to stay in view)
             └──► 4. `arrow()` (Computes exact X/Y for pointing triangle)
             │
             ▼
Computed Coordinates: { x: 142px, y: 388px, placement: "top" }
Applied via style={floatingStyles} directly to Floating Element
```

### When to Use
- Tooltips, popovers, contextual dropdowns, hover cards, flyout sub-menus.
- Custom context menus anchored to mouse right-click coordinates.
- Rich-text editor floating formatting toolbars anchored to highlighted text selections.

### When NOT to Use
- Full-screen modal dialogs centered in the viewport (use standard CSS `fixed inset-0` or `<dialog>`).
- Static sidebar drawers docked to a window edge.

---

## 2. Key Mechanics & Architecture

### Core Building Blocks in `@floating-ui/react`
1. **`useFloating()`**: The primary positioning hook. Computes `{ x, y, strategy, refs, floatingStyles, middlewareData }`.
2. **`autoUpdate`**: Function passed to `whileElementsMounted` ensuring position updates continuously during scrolls and resizes.
3. **Middleware**:
   - `offset(px)`: Displaces floating element from reference.
   - `flip()`: Inverts placement (e.g. `'bottom'` to `'top'`) upon viewport boundary collision.
   - `shift()`: Shifts floating element along cross-axis to keep it on screen.
   - `arrow({ element })`: Positions an arrow element along the reference edge.
4. **Interactions (`useInteractions`)**:
   - `useHover`: Triggers on pointer hover with delay options.
   - `useFocus`: Triggers on keyboard focus.
   - `useDismiss`: Closes on `Escape` key or click-outside.
   - `useRole`: Applies proper ARIA roles (`role="tooltip"` or `role="dialog"`).

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Positioning Engine** | GPU-accelerated CSS `transform` or `top`/`left` coords |
| **Collision Detection** | Automated boundary checks via `flip()` and `shift()` |
| **Tracking Mode** | `autoUpdate` registers resize and scroll listeners automatically |
| **Portals Support** | Works with `<FloatingPortal>` to escape `overflow: hidden` containers |
| **Bundle Footprint** | ~3.5KB gzipped core |
| **React 19 Compatibility** | Full compatibility with React 19 native refs and compiler |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern — Accessible Hover Tooltip
A complete accessible tooltip with hover delay, keyboard focus trigger, and escape dismiss.

```tsx
import React, { useState } from 'react';
import {
  useFloating,
  autoUpdate,
  offset,
  flip,
  shift,
  useHover,
  useFocus,
  useDismiss,
  useRole,
  useInteractions,
  FloatingPortal,
} from '@floating-ui/react';

export function AccessibleTooltip({
  text,
  children,
}: {
  text: string;
  children: React.ReactNode;
}) {
  const [isOpen, setIsOpen] = useState(false);

  const { refs, floatingStyles, context } = useFloating({
    open: isOpen,
    onOpenChange: setIsOpen,
    placement: 'top',
    whileElementsMounted: autoUpdate,
    middleware: [offset(6), flip(), shift({ padding: 8 })],
  });

  const hover = useHover(context, { delay: { open: 200, close: 100 } });
  const focus = useFocus(context);
  const dismiss = useDismiss(context);
  const role = useRole(context, { role: 'tooltip' });

  const { getReferenceProps, getFloatingProps } = useInteractions([
    hover,
    focus,
    dismiss,
    role,
  ]);

  return (
    <>
      {/* Anchor Element */}
      <span ref={refs.setReference} {...getReferenceProps()} className="inline-block">
        {children}
      </span>

      {/* Floating Tooltip in Portal */}
      <FloatingPortal>
        {isOpen && (
          <div
            ref={refs.setFloating}
            style={floatingStyles}
            {...getFloatingProps()}
            className="z-50 bg-zinc-900 text-white text-[11px] font-medium px-2.5 py-1 rounded shadow-lg pointer-events-none animate-fade-in"
          >
            {text}
          </div>
        )}
      </FloatingPortal>
    </>
  );
}

// Consumer Usage:
export function TooltipDemo() {
  return (
    <div className="p-8">
      <AccessibleTooltip text="Export project as high-res PNG">
        <button className="px-3 py-1.5 bg-zinc-100 hover:bg-zinc-200 text-zinc-800 rounded text-xs font-semibold">
          Export Image
        </button>
      </AccessibleTooltip>
    </div>
  );
}
```
Output:
```
// Hovering button displays tooltip above button; if near top edge, flips below automatically.
```

### Example 2: Production / Real-World Integration — Popover with Floating Arrow
Interactive popover with click-outside dismiss and dynamic arrow alignment.

```tsx
import React, { useState, useRef } from 'react';
import {
  useFloating,
  autoUpdate,
  offset,
  flip,
  shift,
  arrow,
  useClick,
  useDismiss,
  useRole,
  useInteractions,
  FloatingPortal,
  FloatingArrow,
} from '@floating-ui/react';

export function FilterPopover() {
  const [isOpen, setIsOpen] = useState(false);
  const arrowRef = useRef<SVGSVGElement>(null);

  const { refs, floatingStyles, context } = useFloating({
    open: isOpen,
    onOpenChange: setIsOpen,
    placement: 'bottom-start',
    whileElementsMounted: autoUpdate,
    middleware: [
      offset(10),
      flip(),
      shift({ padding: 12 }),
      arrow({ element: arrowRef }),
    ],
  });

  const click = useClick(context);
  const dismiss = useDismiss(context);
  const role = useRole(context, { role: 'dialog' });

  const { getReferenceProps, getFloatingProps } = useInteractions([click, dismiss, role]);

  return (
    <div className="p-4">
      <button
        ref={refs.setReference}
        {...getReferenceProps()}
        className="px-3 py-1.5 bg-indigo-600 text-white rounded-md text-xs font-semibold"
      >
        Filter Results ▼
      </button>

      <FloatingPortal>
        {isOpen && (
          <div
            ref={refs.setFloating}
            style={floatingStyles}
            {...getFloatingProps()}
            className="z-50 bg-white border border-zinc-200 rounded-xl shadow-2xl p-4 w-64 text-xs"
          >
            {/* SVG Arrow automatically tracks anchor border */}
            <FloatingArrow
              ref={arrowRef}
              context={context}
              className="fill-white [&>path]:stroke-zinc-200"
            />

            <h4 className="font-bold mb-2">Filter Parameters</h4>
            <div className="space-y-2">
              <label className="flex items-center gap-2">
                <input type="checkbox" defaultChecked />
                <span>Active Users Only</span>
              </label>
              <label className="flex items-center gap-2">
                <input type="checkbox" />
                <span>Verified Emails</span>
              </label>
            </div>
            <button
              onClick={() => setIsOpen(false)}
              className="mt-3 w-full bg-zinc-900 text-white py-1 rounded text-xs font-medium"
            >
              Apply Filter
            </button>
          </div>
        )}
      </FloatingPortal>
    </div>
  );
}
```
Output:
```
// Popover opens below trigger; SVG arrow points directly to button center.
```

### Example 3: Edge Case / Virtual Positioning (Context Menu at Mouse X/Y)
Opening a context menu anchored to the user's right-click mouse coordinates using virtual references.

```tsx
import React, { useState } from 'react';
import {
  useFloating,
  offset,
  flip,
  shift,
  useDismiss,
  useRole,
  useInteractions,
  FloatingPortal,
} from '@floating-ui/react';

export function RightClickContextMenu() {
  const [isOpen, setIsOpen] = useState(false);

  const { refs, floatingStyles, context } = useFloating({
    open: isOpen,
    onOpenChange: setIsOpen,
    placement: 'bottom-start',
    middleware: [offset(4), flip(), shift({ padding: 8 })],
  });

  const dismiss = useDismiss(context);
  const role = useRole(context, { role: 'menu' });
  const { getFloatingProps } = useInteractions([dismiss, role]);

  function handleContextMenu(e: React.MouseEvent) {
    e.preventDefault();

    // Virtual Element representing mouse cursor point:
    refs.setPositionReference({
      getBoundingClientRect() {
        return {
          width: 0,
          height: 0,
          x: e.clientX,
          y: e.clientY,
          top: e.clientY,
          left: e.clientX,
          right: e.clientX,
          bottom: e.clientY,
        };
      },
    });

    setIsOpen(true);
  }

  return (
    <div
      onContextMenu={handleContextMenu}
      className="h-48 border-2 border-dashed rounded-xl flex items-center justify-center bg-zinc-50 select-none cursor-context-menu"
    >
      <span className="text-xs text-zinc-500">Right click anywhere inside this box</span>

      <FloatingPortal>
        {isOpen && (
          <div
            ref={refs.setFloating}
            style={floatingStyles}
            {...getFloatingProps()}
            className="z-50 bg-white border rounded-lg shadow-xl p-1 min-w-[140px] text-xs"
          >
            <div className="p-2 hover:bg-zinc-100 rounded cursor-pointer" onClick={() => setIsOpen(false)}>
              Inspect File
            </div>
            <div className="p-2 hover:bg-zinc-100 rounded cursor-pointer" onClick={() => setIsOpen(false)}>
              Download Copy
            </div>
            <div className="p-2 hover:bg-red-50 text-red-600 rounded cursor-pointer" onClick={() => setIsOpen(false)}>
              Delete Record
            </div>
          </div>
        )}
      </FloatingPortal>
    </div>
  );
}
```
Output:
```
// Right clicking anywhere inside boundary anchors context menu directly under mouse pointer.
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Forgetting `FloatingPortal`, causing popovers to be clipped by parent `overflow: hidden`.

```tsx
// ❌ WRONG: Rendering floating element inside an overflow-hidden card!
export function ClippedPopover() {
  return (
    <div className="overflow-hidden border rounded-xl h-24">
      <button ref={refs.setReference}>Open Menu</button>
      {/* ⚠️ BUG: The floating menu will be cut off and clipped by parent overflow: hidden! */}
      {isOpen && (
        <div ref={refs.setFloating} style={floatingStyles}>
          Menu items (Clipped!)
        </div>
      )}
    </div>
  );
}

// ✅ REMEDIATION: Always wrap floating elements with <FloatingPortal>!
export function FixedPopover() {
  return (
    <div className="overflow-hidden border rounded-xl h-24">
      <button ref={refs.setReference}>Open Menu</button>
      <FloatingPortal>
        {/* Safe: Rendered in document.body outside all overflow clipping boundaries! */}
        {isOpen && (
          <div ref={refs.setFloating} style={floatingStyles}>
            Menu items (Visible!)
          </div>
        )}
      </FloatingPortal>
    </div>
  );
}
```

### Example 5: Idiomatic Modern React 19 Pattern
Floating Rich Text Toolbar positioned over selected browser text range.

```tsx
import React, { useState, useEffect } from 'react';
import { useFloating, offset, flip, shift, FloatingPortal } from '@floating-ui/react';

export function SelectionToolbar() {
  const [isOpen, setIsOpen] = useState(false);

  const { refs, floatingStyles } = useFloating({
    open: isOpen,
    onOpenChange: setIsOpen,
    placement: 'top',
    middleware: [offset(8), flip(), shift({ padding: 8 })],
  });

  useEffect(() => {
    function handleSelectionChange() {
      const selection = window.getSelection();
      if (!selection || selection.isCollapsed || !selection.rangeCount) {
        setIsOpen(false);
        return;
      }

      const range = selection.getRangeAt(0);

      // Anchor floating toolbar to highlighted text range!
      refs.setPositionReference({
        getBoundingClientRect() {
          return range.getBoundingClientRect();
        },
      });

      setIsOpen(true);
    }

    document.addEventListener('selectionchange', handleSelectionChange);
    return () => document.removeEventListener('selectionchange', handleSelectionChange);
  }, [refs]);

  return (
    <FloatingPortal>
      {isOpen && (
        <div
          ref={refs.setFloating}
          style={floatingStyles}
          className="z-50 bg-zinc-900 text-white rounded-lg shadow-xl px-2 py-1 flex gap-2 text-xs font-bold"
        >
          <button onClick={() => document.execCommand('bold')}>B</button>
          <button onClick={() => document.execCommand('italic')}>I</button>
          <button onClick={() => document.execCommand('underline')}>U</button>
        </div>
      )}
    </FloatingPortal>
  );
}
```
Output:
```
// Highlighting any text on the page displays floating formatting toolbar directly above selection.
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Omitting `whileElementsMounted: autoUpdate`** | Floating element fails to reposition when page scrolls. | Pass `whileElementsMounted: autoUpdate` into `useFloating`. |
| **Rendering floating elements without `FloatingPortal`** | Popovers get clipped by parent `overflow: hidden` or `z-index` stacks. | Wrap floating output in `<FloatingPortal>`. |
| **Hardcoding `placement` without `flip()` middleware** | Menu goes offscreen when anchor is close to window border. | Add `flip()` to middleware pipeline. |
| **Omitting `shift()` middleware** | Long dropdowns overflow the right or left edge of the viewport. | Add `shift({ padding: 8 })`. |
| **Mutating `floatingStyles` manually** | Overwriting `position: absolute` or transform math breaks tracking. | Spread `style={floatingStyles}` and merge extra styles carefully. |
| **Missing `useDismiss` hook** | Users cannot close the popover by pressing Escape or clicking outside. | Include `useDismiss(context)` in `useInteractions`. |

---

## 5. React Internals: Beneath the Abstraction

Inside Floating UI:
1. `useFloating` attaches an internal ResizeObserver and scroll listener via `autoUpdate`.
2. When the pipeline runs:
   - Floating UI queries `reference.getBoundingClientRect()` and `floating.getBoundingClientRect()`.
   - It computes base `x` and `y` offsets based on `placement` (`'top'`, `'bottom'`, etc.).
   - It sequentially passes the coordinates through each middleware (`offset`, `flip`, `shift`, `arrow`).
   - If collision is detected with viewport boundaries (checked via `computePosition`), `flip` alters the placement.
3. The final result is returned as `floatingStyles` containing `position: 'fixed'` or `'absolute'` with optimized transform translation coordinates.

---

## 6. Performance, Bundling & React Compiler

- **Bundle Footprint**: ~3.5KB gzipped core.
- **Scroll Throttling**: Updates are batched inside `requestAnimationFrame`, eliminating scroll stutter.
- **React Compiler**: The React Compiler memoizes interaction prop getters (`getReferenceProps`, `getFloatingProps`), ensuring stable bindings.

---

## 7. Accessibility (a11y) & Usability

- **ARIA Role Assignment**: `useRole` automatically sets `role="tooltip"`, `role="dialog"`, or `role="menu"`.
- **Keyboard Traversal**: `useDismiss` automatically wires the `Escape` key to close the floating element and restores focus to the reference trigger.

---

## 8. Testing & Verification Patterns

Testing Floating UI popovers with Vitest and `@testing-library/react`:

```tsx
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { describe, it, expect } from 'vitest';
import React from 'react';
import { FilterPopover } from './FilterPopover';

describe('FilterPopover with Floating UI', () => {
  it('opens popover on click and closes on escape', async () => {
    const user = userEvent.setup();
    render(<FilterPopover />);

    const button = screen.getByRole('button', { name: /filter results/i });
    await user.click(button);

    // Popover is open
    expect(screen.getByRole('dialog')).toBeInTheDocument();
    expect(screen.getByText(/filter parameters/i)).toBeInTheDocument();

    // Escape closes popover
    await user.keyboard('{Escape}');
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **Popper.js v2** | Legacy DOM-based positioning library. | Cumbersome React wrappers; heavy bundle. |
| **Floating UI v0.10+** | Rewrite of Popper.js; modular middleware, interaction hooks. | Official replacement for Popper. |
| **React 19** | Full compatibility with React 19 native refs and compiler. | Used internally in Radix UI / shadcn/ui. |

---

## 10. Security & Hardening

1. **Virtual Element Sanitation**: When positioning context menus against user selection ranges, ensure text content is not executed as unescaped HTML.
2. **Clickjacking in Portals**: Ensure `FloatingPortal` containers have appropriate `z-index` boundaries so malicious elements cannot overlay invisible click traps.

---

## 11. Real-World Case Study

### Notion / Slack Floating Formatting Toolbars

Rich document editors like Notion and Slack require a formatting toolbar that floats directly above selected text:
- When a user highlights text with the mouse, Floating UI sets a virtual reference to the DOM `Range`.
- If the text is near the top of the browser window, Floating UI flips the toolbar to appear *below* the selected line.
- The entire interaction runs in a 3KB bundle with zero frame lag.

---

## 12. Interview Q&A

<details>
<summary>1. What is Floating UI, and why did it replace Popper.js?</summary>

Floating UI is the complete rewrite and successor of Popper.js. It features a modular middleware architecture, a much smaller bundle footprint (~3.5KB vs ~10KB), framework-agnostic core calculations, and dedicated React interaction hooks (`useHover`, `useClick`, `useDismiss`, `useRole`) that manage complete accessible floating element lifecycles.
</details>

<details>
<summary>2. Why is `whileElementsMounted: autoUpdate` required in `useFloating`?</summary>

Without `autoUpdate`, Floating UI computes the floating element's position only once when it mounts. If the user scrolls the page, resizes the window, or content changes dynamically, the floating element will become detached from its anchor. `autoUpdate` registers efficient listeners that recalculate coordinates on every animation frame during layout changes.
</details>

<details>
<summary>3. What is the difference between `flip()` and `shift()` middleware?</summary>

- **`flip()`**: Changes the placement to the opposite side (e.g. flips `'top'` to `'bottom'`) when there is insufficient space along the primary axis.
- **`shift()`**: Keeps the floating element on screen by sliding it along the cross-axis (e.g. sliding left or right) when it overflows viewport boundaries.
</details>

<details>
<summary>4. What is a "Virtual Element" in Floating UI?</summary>

A virtual element is an object that implements `getBoundingClientRect()` without being backed by a static DOM node. This allows positioning floating elements against dynamic coordinates, such as mouse pointer positions (for context menus) or highlighted text ranges (for formatting toolbars).
</details>

<details>
<summary>5. Why should floating elements be rendered inside a `<FloatingPortal>`?</summary>

If a floating element is rendered inside a parent container with `overflow: hidden`, `overflow: scroll`, or a restrictive CSS `z-index` stacking context, the popover will be clipped or hidden behind sibling elements. `<FloatingPortal>` teleports the floating element to `document.body`, escaping all clipping contexts.
</details>

<details>
<summary>6. How does the `arrow()` middleware work?</summary>

`arrow({ element: arrowRef })` inspects the reference element and floating element geometry, computing the exact `x` or `y` coordinates for an arrow indicator so it stays centered against the reference element while clamping within the floating element's border radius.
</details>

<details>
<summary>7. How do you implement hover delay in Floating UI?</summary>

Using the `useHover` hook options:
```ts
const hover = useHover(context, { delay: { open: 300, close: 150 } });
```
This prevents tooltips from flickering when the user quickly moves the cursor across multiple elements.
</details>

<details>
<summary>8. Does Floating UI work with React Server Components (RSC)?</summary>

The floating component requires client-side event listeners and DOM measurements, so it must be declared in a Client Component (`'use client'`). Its content, however, can render server-rendered children seamlessly.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Basic Top-Aligned Tooltip</summary>

**Specification**: Build a simple tooltip that appears above a button on hover.

```tsx
import React, { useState } from 'react';
import { useFloating, offset, useHover, useInteractions } from '@floating-ui/react';

export function SimpleTooltip({ text, children }: { text: string; children: React.ReactNode }) {
  const [open, setOpen] = useState(false);
  const { refs, floatingStyles, context } = useFloating({ open, onOpenChange: setOpen, middleware: [offset(4)] });
  const hover = useHover(context);
  const { getReferenceProps, getFloatingProps } = useInteractions([hover]);

  return (
    <>
      <span ref={refs.setReference} {...getReferenceProps()}>{children}</span>
      {open && <div ref={refs.setFloating} style={floatingStyles} {...getFloatingProps()} className="bg-black text-white text-xs p-1 rounded">{text}</div>}
    </>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Click-Activated Dropdown Menu</summary>

**Specification**: Build a menu that opens on click and closes when clicking outside.

```tsx
import React, { useState } from 'react';
import { useFloating, useClick, useDismiss, useInteractions } from '@floating-ui/react';

export function ClickMenu() {
  const [open, setOpen] = useState(false);
  const { refs, floatingStyles, context } = useFloating({ open, onOpenChange: setOpen });
  const click = useClick(context);
  const dismiss = useDismiss(context);
  const { getReferenceProps, getFloatingProps } = useInteractions([click, dismiss]);

  return (
    <div>
      <button ref={refs.setReference} {...getReferenceProps()} className="border p-1 text-xs">Menu ▼</button>
      {open && <div ref={refs.setFloating} style={floatingStyles} {...getFloatingProps()} className="border bg-white p-2 text-xs shadow">Menu Body</div>}
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Popover with SVG Arrow Pointing to Anchor</summary>

**Specification**: Implement an arrow that dynamically rotates and points toward the trigger.

```tsx
import React, { useRef, useState } from 'react';
import { useFloating, arrow, FloatingArrow, offset } from '@floating-ui/react';

export function ArrowPopover() {
  const [open, setOpen] = useState(true);
  const arrowRef = useRef(null);
  const { refs, floatingStyles, context } = useFloating({
    open,
    middleware: [offset(8), arrow({ element: arrowRef })],
  });

  return (
    <div>
      <button ref={refs.setReference}>Anchor</button>
      {open && (
        <div ref={refs.setFloating} style={floatingStyles} className="bg-white border p-3 rounded">
          <FloatingArrow ref={arrowRef} context={context} className="fill-white" />
          Content
        </div>
      )}
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Custom Cursor Follower Tooltip</summary>

**Specification**: Floating tooltip that tracks the exact mouse coordinates across a canvas.

```tsx
import React, { useState } from 'react';
import { useFloating, offset } from '@floating-ui/react';

export function CursorTracker() {
  const { refs, floatingStyles } = useFloating({ placement: 'right-start', middleware: [offset(10)] });

  const onMouseMove = (e: React.MouseEvent) => {
    refs.setPositionReference({
      getBoundingClientRect: () => ({ width: 0, height: 0, x: e.clientX, y: e.clientY, top: e.clientY, left: e.clientX, right: e.clientX, bottom: e.clientY }),
    });
  };

  return (
    <div onMouseMove={onMouseMove} className="h-32 border p-2 text-xs">
      Move cursor here
      <div ref={refs.setFloating} style={floatingStyles} className="bg-black text-white p-1 text-[10px] rounded pointer-events-none">Coordinates Follower</div>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Cascading Nested Sub-Menus</summary>

**Specification**: Multi-level flyout menu where hovering an item opens a secondary sub-menu flipped along the right edge.

```tsx
// Using placement: 'right-start' with nested FloatingPortals and flip({ fallbackPlacements: ['left-start'] })
```
</details>

<details>
<summary>Exercise 6 (Hard): Highlighted Text Selection Annotator</summary>

**Specification**: Floating popover that opens above user text selection with a "Comment" button.

```tsx
// Using document.addEventListener('selectionchange') with range.getBoundingClientRect() virtual reference.
```
</details>
