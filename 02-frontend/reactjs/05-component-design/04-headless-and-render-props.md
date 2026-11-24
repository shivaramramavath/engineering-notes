# Headless UI Architecture and Render Props in React

- **Topic**: `05-component-design`
- **React version**: React 19+ (Evolution from React 16.8+)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Architectural design pattern separating logic/accessibility from presentation; compatible with React Compiler.

---

## 1. Concept Overview

When building design systems, one of the biggest architectural traps is tightly coupling **behavior** (keyboard navigation, focus trapping, state transitions, ARIA attributes) to **styling** (CSS classes, HTML elements, DOM hierarchy).

If a `<Dropdown>` component bakes in fixed Tailwind classes and a rigid `<div>` structure:
- Consumers cannot easily restyle it for dark mode, mobile bottom sheets, or custom layouts.
- Reusing the complex keyboard navigation in another part of the application requires duplicating logic or fighting CSS specificity.

**Headless UI Architecture** completely decouples logic, state, and accessibility from markup and visual presentation. The headless component or hook manages:
- State transitions (open/closed, selected items, active indexes).
- Keyboard handlers (Arrow keys, Escape, Enter, Space, Home, End).
- W3C WAI-ARIA roles, states, and properties (`aria-expanded`, `aria-controls`, `role="listbox"`).
- Focus management (trapping, restoring focus).

The consumer has 100% control over the rendered JSX, HTML tags, and CSS classes.

```
Traditional Coupled Component:
┌────────────────────────────────────────────────────────┐
│ <Select>                                               │
│  State + ARIA + Hardcoded Tailwind Classes + Fixed DOM │
└────────────────────────────────────────────────────────┘
(Inflexible: cannot change markup or restyle without overrides)

Headless Component Architecture:
┌────────────────────────────────────────────────────────┐
│ Headless Engine (Hook / Render Prop / Slot)            │
│ • State machines (isOpen, selectedItem)                │
│ • Keyboard interaction (ArrowDown, Escape)             │
│ • ARIA attributes (aria-activedescendant)             │
└──────────────────────────┬─────────────────────────────┘
                           │ Injects state & prop getters
                           ▼
┌────────────────────────────────────────────────────────┐
│ Consumer UI (100% Custom Presentation)                 │
│ • Render any HTML tag (div, ul, dialog, bottom sheet)  │
│ • Any styling solution (Tailwind v4, CSS Modules, etc) │
└────────────────────────────────────────────────────────┘
```

### When to Use
- Building enterprise design systems (e.g. Radix UI, Headless UI, React Aria, TanStack Table).
- Complex interactive widgets: Comboboxes, Multi-selects, Date Pickers, Modals, Sliders, Accordions.
- Multi-brand white-label applications where 5 different client themes share identical core logic.

### When NOT to Use
- Simple presentational components: Buttons, Avatars, Badges, Dividers (these have no complex state or accessibility state machines).
- One-off application widgets where decoupled flexibility adds unnecessary indirection.

---

## 2. Key Mechanics & Architecture

### The Prop Getters Pattern
Headless components frequently expose **prop getters**—functions that return an object of attributes and event handlers to spread onto consumer JSX elements (e.g., `getToggleProps()`, `getItemProps()`).

Prop getters solve a critical problem: **handler composition**. If the consumer wants their own `onClick` handler on an element while the headless engine also needs an `onClick` handler, a prop getter composes both handlers safely:

```ts
function callAll(...fns: (((...args: any[]) => void) | undefined)[]) {
  return (...args: any[]) => {
    fns.forEach((fn) => fn && fn(...args));
  };
}

function getToggleProps(customProps = {}) {
  return {
    ...customProps,
    'aria-expanded': isOpen,
    'aria-haspopup': true,
    onClick: callAll(customProps.onClick, handleInternalToggle),
  };
}
```

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Primary Vehicle** | Custom Hooks (modern) or Render Props / Slots (structural) |
| **Styling Coupling** | 0% (zero CSS or markup opinions) |
| **Accessibility Enforcement** | High: bakes in WAI-ARIA 1.2 keyboard and screen reader contracts |
| **DOM Ownership** | Consumer renders all HTML tags |
| **Prop Getters** | Composes consumer event handlers safely via functional piping |
| **React Compiler Impact** | Hook outputs are memoized cleanly by the compiler |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
Headless Toggle hook with prop getters.

```tsx
import React, { useState, useCallback } from 'react';

export function useToggle(initial = false) {
  const [on, setOn] = useState(initial);

  const toggle = useCallback(() => setOn((prev) => !prev), []);
  const set = useCallback((value: boolean) => setOn(value), []);

  const getTogglerProps = (props: React.ButtonHTMLAttributes<HTMLButtonElement> = {}) => ({
    ...props,
    'aria-pressed': on,
    type: 'button' as const,
    onClick: (e: React.MouseEvent<HTMLButtonElement>) => {
      props.onClick?.(e);
      if (!e.defaultPrevented) {
        toggle();
      }
    },
  });

  return { on, toggle, set, getTogglerProps };
}

// Consumer Component: 100% freedom in styling!
export function CustomSwitch() {
  const { on, getTogglerProps } = useToggle();

  return (
    <button
      {...getTogglerProps({
        className: `w-12 h-6 flex items-center rounded-full p-1 transition-colors ${
          on ? 'bg-indigo-600' : 'bg-zinc-300'
        }`,
      })}
    >
      <span
        className={`bg-white w-4 h-4 rounded-full shadow-md transform transition-transform ${
          on ? 'translate-x-6' : 'translate-x-0'
        }`}
      />
    </button>
  );
}
```
Output:
```
// Clicking button smoothly translates indicator; HTML contains aria-pressed="true"/"false".
```

### Example 2: Production / Real-World Integration
A Headless Accordion with keyboard navigation (Arrow Up/Down, Home/End) and strict WAI-ARIA Disclosure pattern.

```tsx
import React, { useState, useId, useCallback } from 'react';

export function useAccordion({ totalItems }: { totalItems: number }) {
  const [openIndex, setOpenIndex] = useState<number | null>(null);
  const baseId = useId();

  const toggleIndex = useCallback((idx: number) => {
    setOpenIndex((prev) => (prev === idx ? null : idx));
  }, []);

  const getTriggerProps = (index: number) => {
    const headerId = `${baseId}-header-${index}`;
    const panelId = `${baseId}-panel-${index}`;
    const isOpen = openIndex === index;

    return {
      id: headerId,
      'aria-expanded': isOpen,
      'aria-controls': panelId,
      onClick: () => toggleIndex(index),
      onKeyDown: (e: React.KeyboardEvent) => {
        if (e.key === 'ArrowDown') {
          e.preventDefault();
          const next = (index + 1) % totalItems;
          document.getElementById(`${baseId}-header-${next}`)?.focus();
        } else if (e.key === 'ArrowUp') {
          e.preventDefault();
          const prev = (index - 1 + totalItems) % totalItems;
          document.getElementById(`${baseId}-header-${prev}`)?.focus();
        }
      },
    };
  };

  const getPanelProps = (index: number) => {
    const headerId = `${baseId}-header-${index}`;
    const panelId = `${baseId}-panel-${index}`;
    const isOpen = openIndex === index;

    return {
      id: panelId,
      role: 'region',
      'aria-labelledby': headerId,
      hidden: !isOpen,
    };
  };

  return { openIndex, getTriggerProps, getPanelProps };
}

// Consumer Implementation:
export function FaqAccordion({ items }: { items: { q: string; a: string }[] }) {
  const { openIndex, getTriggerProps, getPanelProps } = useAccordion({ totalItems: items.length });

  return (
    <div className="max-w-md divide-y border rounded-lg">
      {items.map((item, idx) => (
        <div key={idx} className="p-2">
          <button
            {...getTriggerProps(idx)}
            className="w-full flex justify-between font-medium text-sm text-left py-2"
          >
            {item.q}
            <span>{openIndex === idx ? '▲' : '▼'}</span>
          </button>
          <div {...getPanelProps(idx)} className="text-xs text-zinc-600 pb-2">
            {item.a}
          </div>
        </div>
      ))}
    </div>
  );
}
```
Output:
```
// Renders accessible accordion with full keyboard arrow navigation between headers.
```

### Example 3: Edge Case / Classic Render Props Fallback for Layout Slots
Using render props to expose internal measurement dimensions to child layouts.

```tsx
import React, { useRef, useState, useLayoutEffect, type ReactNode } from 'react';

interface Size {
  width: number;
  height: number;
}

interface MeasureProps {
  children: (size: Size) => ReactNode;
}

export function Measure({ children }: MeasureProps) {
  const ref = useRef<HTMLDivElement>(null);
  const [size, setSize] = useState<Size>({ width: 0, height: 0 });

  useLayoutEffect(() => {
    if (ref.current) {
      const rect = ref.current.getBoundingClientRect();
      setSize({ width: Math.round(rect.width), height: Math.round(rect.height) });
    }
  }, []);

  return (
    <div ref={ref} className="w-full">
      {children(size)}
    </div>
  );
}

// Consumer usage:
export function AdaptiveChart() {
  return (
    <Measure>
      {({ width }) => (
        <div className="p-4 border rounded">
          {width < 400 ? (
            <div className="text-xs text-amber-600">Mobile Compact View ({width}px)</div>
          ) : (
            <div className="text-sm text-emerald-600">Full Desktop SVG Chart ({width}px)</div>
          )}
        </div>
      )}
    </Measure>
  );
}
```
Output:
```
// Dynamically switches view based on rendered container width measured synchronously.
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Overwriting consumer event handlers in prop getters instead of composing them.

```tsx
// ❌ WRONG: Consumer onClick is overwritten and lost!
function badGetButtonProps(consumerProps: any = {}) {
  return {
    ...consumerProps,
    onClick: () => {
      // ⚠️ BUG: If consumer passed onClick={() => logAnalytics()}, it is completely erased!
      internalHandler();
    },
  };
}

// ✅ REMEDIATION: Pipe both handlers using callAll!
function callAll<T extends (...args: any[]) => void>(...fns: (T | undefined)[]) {
  return (...args: Parameters<T>) => {
    fns.forEach((fn) => fn?.(...args));
  };
}

function goodGetButtonProps(consumerProps: any = {}) {
  return {
    ...consumerProps,
    onClick: callAll(consumerProps.onClick, internalHandler),
  };
}
```
Output:
```
// Both consumer analytics and internal state transitions execute reliably.
```

### Example 5: Idiomatic Modern React 19 Pattern
Headless Combobox with React 19 Actions and `useId`.

```tsx
import React, { useState, useId } from 'react';

export function useCombobox<T extends string>({
  items,
  onSelect,
}: {
  items: T[];
  onSelect: (item: T) => void;
}) {
  const [query, setQuery] = useState('');
  const [isOpen, setIsOpen] = useState(false);
  const [highlightedIndex, setHighlightedIndex] = useState(0);
  const baseId = useId();

  const filteredItems = items.filter((item) =>
    item.toLowerCase().includes(query.toLowerCase())
  );

  const getInputProps = () => ({
    id: `${baseId}-input`,
    role: 'combobox',
    'aria-autocomplete': 'list' as const,
    'aria-expanded': isOpen,
    'aria-controls': `${baseId}-menu`,
    'aria-activedescendant': isOpen ? `${baseId}-item-${highlightedIndex}` : undefined,
    value: query,
    onChange: (e: React.ChangeEvent<HTMLInputElement>) => {
      setQuery(e.target.value);
      setIsOpen(true);
      setHighlightedIndex(0);
    },
    onFocus: () => setIsOpen(true),
    onKeyDown: (e: React.KeyboardEvent) => {
      if (e.key === 'ArrowDown') {
        e.preventDefault();
        setHighlightedIndex((idx) => Math.min(idx + 1, filteredItems.length - 1));
      } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        setHighlightedIndex((idx) => Math.max(idx - 1, 0));
      } else if (e.key === 'Enter' && isOpen && filteredItems[highlightedIndex]) {
        e.preventDefault();
        onSelect(filteredItems[highlightedIndex]);
        setQuery(filteredItems[highlightedIndex]);
        setIsOpen(false);
      } else if (e.key === 'Escape') {
        setIsOpen(false);
      }
    },
  });

  const getMenuProps = () => ({
    id: `${baseId}-menu`,
    role: 'listbox',
    hidden: !isOpen,
  });

  const getItemProps = (item: T, index: number) => ({
    id: `${baseId}-item-${index}`,
    role: 'option',
    'aria-selected': index === highlightedIndex,
    onClick: () => {
      onSelect(item);
      setQuery(item);
      setIsOpen(false);
    },
  });

  return {
    isOpen,
    filteredItems,
    highlightedIndex,
    getInputProps,
    getMenuProps,
    getItemProps,
  };
}
```
Output:
```
// Provides complete accessible combobox state machine with keyboard selection.
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Hardcoding CSS classes inside headless hooks** | Defeats the "headless" principle and forces consumers to override styles. | Return pure ARIA and state props; leave styling to consumer. |
| **Overwriting consumer event handlers** | Spreading consumer props before the hook handler replaces consumer callbacks. | Use `callAll` helper to compose consumer and internal handlers. |
| **Omitting ARIA roles and states** | Makes the headless component inaccessible to screen readers. | Always bake in WCAG 2.2 compliant ARIA attributes. |
| **Creating DOM nodes inside headless hooks** | Hooks should not generate DOM nodes directly; components handle JSX. | Return objects containing props, state, and handlers. |
| **Using render props where a custom hook suffices** | Render props create nesting callback pyramids ("wrapper hell"). | Prefer custom hooks (`useHook`) for state, reserving render props for slots. |
| **Neglecting keyboard interactions** | A headless component without arrow key / Esc / Enter handlers is incomplete. | Implement full W3C APG keyboard interaction patterns. |

---

## 5. React Internals: Beneath the Abstraction

Headless architecture thrives in React because of the separation between **Virtual DOM reconciler logic** and **Host DOM elements**:
1. When a hook returns a prop getter, it simply returns a plain JavaScript object `{ id, role, onClick, 'aria-expanded': true }`.
2. React's JSX transform treats `{...getProps()}` as ordinary props spread onto the host JSX element.
3. React Fiber reconciliation receives the composed handlers and assigns them to the host fiber's `memoizedProps`.
4. Event delegation (`SyntheticEvent`) routes events through the composed handler pipeline seamlessly.

---

## 6. Performance, Bundling & React Compiler

- **Bundle Efficiency**: Headless hooks contain zero CSS, SVGs, or bulky markup, making them extremely lightweight.
- **Tree-Shaking**: Only the hooks you import are included in your bundle.
- **React Compiler**: The React Compiler memoizes prop getter calls and returned objects, eliminating the need for manual `useCallback` inside simple headless hooks.

---

## 7. Accessibility (a11y) & Usability

Headless architecture is the gold standard for accessible web engineering:
- **ARIA Verification**: Ensures `aria-expanded`, `aria-haspopup`, and `aria-controls` are always synchronized with state.
- **Focus Management**: Automatically sets `tabIndex={0}` or `-1` for roaming tabindex patterns in toolbars and menus.
- **Screen Reader Announcements**: Guarantees that active descendant updates are exposed via `aria-activedescendant`.

---

## 8. Testing & Verification Patterns

Testing headless hooks using `@testing-library/react-hooks` or inline test harnesses:

```tsx
import { render, screen, fireEvent } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import React from 'react';
import { useToggle } from './useToggle';

function TestToggleConsumer() {
  const { on, getTogglerProps } = useToggle();
  return (
    <button {...getTogglerProps({ 'data-testid': 'toggle-btn' })}>
      {on ? 'ON' : 'OFF'}
    </button>
  );
}

describe('useToggle Headless Hook', () => {
  it('toggles state and updates aria-pressed correctly', () => {
    render(<TestToggleConsumer />);
    const btn = screen.getByTestId('toggle-btn');

    expect(btn).toHaveTextContent('OFF');
    expect(btn).toHaveAttribute('aria-pressed', 'false');

    fireEvent.click(btn);

    expect(btn).toHaveTextContent('ON');
    expect(btn).toHaveAttribute('aria-pressed', 'true');
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 16.8 / 17** | Custom hooks replaced legacy Render Props and HOCs as the primary headless mechanism. | Migrate render-prop components (like Downshift) to hooks. |
| **React 18** | Introduced `useId` and `useSyncExternalStore` for robust headless SSR and store subscriptions. | Use `useId` for headless ARIA linkages. |
| **React 19** | Direct ref props and Actions integrate cleanly with headless prop getters. | Pass `ref` directly into prop getters. |

---

## 10. Security & Hardening

1. **Safe Event Composition**: In `callAll`, ensure errors thrown in a consumer handler do not corrupt internal state cleanup.
2. **Preventing Prototype Pollution**: When deep-merging consumer props in prop getters, avoid recursive merging of `__proto__` or `constructor`.

---

## 11. Real-World Case Study

### Radix UI vs Headless UI vs React Aria

The three dominant headless libraries in modern React:
- **Radix UI**: Uses composite compound components with the `asChild` Slot pattern. Provides great developer ergonomics and automatic focus trapping.
- **Tailwind Headless UI**: Provides headless components (`<Menu>`, `<Listbox>`, `<Dialog>`) tightly optimized for Tailwind CSS transition classes.
- **Adobe React Aria**: Provides hooks (`useButton`, `useSelect`, `useDialog`) with the most rigorous internationalization, accessibility, and platform compliance in the industry.

---

## 12. Interview Q&A

<details>
<summary>1. What is the difference between a headless UI component and a standard styled component?</summary>

A standard styled component bundles visual styles (CSS classes, colors, spacing) directly with component markup and logic.
A headless UI component provides only the state logic, keyboard event handling, and ARIA accessibility attributes, rendering zero styles or giving complete control of the DOM markup to the consumer via custom hooks, prop getters, or slots.
</details>

<details>
<summary>2. What is the "Prop Getters" pattern, and why is it preferred over raw prop objects?</summary>

A prop getter is a function (e.g. `getButtonProps(customProps)`) that returns an object of props. It is preferred over returning a static object because it allows the consumer to pass custom props (such as their own `onClick` or `className`), and the prop getter automatically merges and chains event handlers (`callAll`) so neither handler is lost.
</details>

<details>
<summary>3. Why did custom hooks largely replace render props for headless state in React?</summary>

Render props required wrapping components in nested JSX functions (`<Consumer>{({ data }) => ...}</Consumer>`), leading to "wrapper hell" and deep indentation pyramids. Custom hooks allow extracting headless state and logic linearly at the top level of the component body without creating additional DOM wrappers or React fiber nodes.
</details>

<details>
<summary>4. When are render props still useful in modern React?</summary>

Render props are still useful for **structural layout slots** where a parent component controls the rendering iteration or measurement geometry (e.g., virtualized lists where each row is rendered on demand, or container measurement components).
</details>

<details>
<summary>5. How do you implement keyboard navigation in a headless dropdown?</summary>

In the input or trigger's `onKeyDown` handler:
- `ArrowDown` increments `highlightedIndex` (wrapping around or clamping).
- `ArrowUp` decrements `highlightedIndex`.
- `Enter` or `Space` selects the currently highlighted item.
- `Escape` closes the menu and restores focus to the trigger.
- `Home` and `End` jump to the first and last items.
</details>

<details>
<summary>6. How does `useId` improve headless UI architecture?</summary>

Headless components rely on linking DOM nodes for accessibility (`aria-controls`, `aria-labelledby`, `aria-describedby`). Using `useId` inside headless hooks guarantees that generated element IDs match between server-side rendering and client hydration without ID collisions.
</details>

<details>
<summary>7. How do you prevent event default behavior in composed handlers?</summary>

In your `callAll` utility, inspect `event.defaultPrevented`. If the consumer's custom handler calls `e.preventDefault()`, the headless internal handler checks this flag and skips its default action.
</details>

<details>
<summary>8. Does headless UI add runtime overhead to React apps?</summary>

No. Headless hooks execute standard lightweight JavaScript state updates and return plain objects. Because there is no runtime CSS-in-JS parsing or extra DOM nodes, headless architectures are typically faster and smaller than heavyweight styled UI libraries.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Headless Disclosure / Expander Hook</summary>

**Specification**: Build a `useDisclosure` hook returning `isOpen`, `open()`, `close()`, `toggle()`, and `getDisclosureProps()`.

```tsx
import { useState, useCallback, useId } from 'react';

export function useDisclosure(initialState = false) {
  const [isOpen, setIsOpen] = useState(initialState);
  const id = useId();

  const toggle = useCallback(() => setIsOpen((prev) => !prev), []);

  const getTriggerProps = () => ({
    'aria-expanded': isOpen,
    'aria-controls': `${id}-panel`,
    onClick: toggle,
  });

  const getPanelProps = () => ({
    id: `${id}-panel`,
    hidden: !isOpen,
  });

  return { isOpen, toggle, getTriggerProps, getPanelProps };
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Headless Stepper Hook</summary>

**Specification**: Build a `useStepper` hook that manages `step`, `next()`, `prev()`, and disables bounds.

```tsx
import { useState, useCallback } from 'react';

export function useStepper({ maxSteps }: { maxSteps: number }) {
  const [step, setStep] = useState(0);

  const next = useCallback(() => setStep((s) => Math.min(s + 1, maxSteps - 1)), [maxSteps]);
  const prev = useCallback(() => setStep((s) => Math.max(s - 1, 0)), []);

  return {
    step,
    isFirst: step === 0,
    isLast: step === maxSteps - 1,
    next,
    prev,
    getStepProps: (idx: number) => ({
      'aria-current': idx === step ? ('step' as const) : undefined,
    }),
  };
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Headless Tabs with Roaming TabIndex</summary>

**Specification**: Create a `useTabs` hook implementing the W3C Tabs pattern with keyboard arrow switching and `tabIndex={0 | -1}`.

```tsx
import { useState, useId } from 'react';

export function useTabs(tabCount: number) {
  const [activeTab, setActiveTab] = useState(0);
  const baseId = useId();

  const getTabProps = (index: number) => ({
    id: `${baseId}-tab-${index}`,
    role: 'tab' as const,
    'aria-selected': activeTab === index,
    'aria-controls': `${baseId}-panel-${index}`,
    tabIndex: activeTab === index ? 0 : -1,
    onClick: () => setActiveTab(index),
    onKeyDown: (e: React.KeyboardEvent) => {
      if (e.key === 'ArrowRight') {
        const next = (index + 1) % tabCount;
        setActiveTab(next);
        document.getElementById(`${baseId}-tab-${next}`)?.focus();
      } else if (e.key === 'ArrowLeft') {
        const prev = (index - 1 + tabCount) % tabCount;
        setActiveTab(prev);
        document.getElementById(`${baseId}-tab-${prev}`)?.focus();
      }
    },
  });

  const getPanelProps = (index: number) => ({
    id: `${baseId}-panel-${index}`,
    role: 'tabpanel' as const,
    'aria-labelledby': `${baseId}-tab-${index}`,
    hidden: activeTab !== index,
  });

  return { activeTab, getTabProps, getPanelProps };
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Headless Tooltip with Hover & Focus Delays</summary>

**Specification**: Implement a `useTooltip` hook that manages open/close timing on pointer hover and focus with accessible ARIA tags.

```tsx
import { useState, useRef, useId } from 'react';

export function useTooltip(delayMs = 300) {
  const [visible, setVisible] = useState(false);
  const timerRef = useRef<any>(null);
  const id = useId();

  const show = () => {
    timerRef.current = setTimeout(() => setVisible(true), delayMs);
  };

  const hide = () => {
    clearTimeout(timerRef.current);
    setVisible(false);
  };

  const getTriggerProps = () => ({
    'aria-describedby': visible ? id : undefined,
    onMouseEnter: show,
    onMouseLeave: hide,
    onFocus: show,
    onBlur: hide,
  });

  const getTooltipProps = () => ({
    id,
    role: 'tooltip' as const,
    hidden: !visible,
  });

  return { visible, getTriggerProps, getTooltipProps };
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Headless Focus Trap for Modal Dialogs</summary>

**Specification**: Implement a `useFocusTrap` hook that traps Tab and Shift+Tab key navigation strictly within an active container DOM element.

```tsx
import { useRef, useEffect } from 'react';

export function useFocusTrap(active: boolean) {
  const containerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!active || !containerRef.current) return;

    const el = containerRef.current;
    const focusable = el.querySelectorAll<HTMLElement>(
      'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])'
    );
    const first = focusable[0];
    const last = focusable[focusable.length - 1];

    first?.focus();

    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key !== 'Tab') return;

      if (e.shiftKey) {
        if (document.activeElement === first) {
          e.preventDefault();
          last?.focus();
        }
      } else {
        if (document.activeElement === last) {
          e.preventDefault();
          first?.focus();
        }
      }
    };

    el.addEventListener('keydown', handleKeyDown);
    return () => el.removeEventListener('keydown', handleKeyDown);
  }, [active]);

  return containerRef;
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Complete Headless Multi-Select Dropdown with Chip Removal</summary>

**Specification**: Build a complete `useMultiSelect` hook that tracks selected items, exposes item tag removal props, and filters search queries.

```tsx
import { useState, useId } from 'react';

export function useMultiSelect<T extends { id: string; label: string }>({ items }: { items: T[] }) {
  const [selected, setSelected] = useState<T[]>([]);
  const [query, setQuery] = useState('');
  const [isOpen, setIsOpen] = useState(false);
  const id = useId();

  const toggleItem = (item: T) => {
    setSelected((prev) =>
      prev.some((i) => i.id === item.id) ? prev.filter((i) => i.id !== item.id) : [...prev, item]
    );
  };

  const removeItem = (itemId: string) => {
    setSelected((prev) => prev.filter((i) => i.id !== itemId));
  };

  const filteredItems = items.filter((item) =>
    item.label.toLowerCase().includes(query.toLowerCase())
  );

  return {
    selected,
    isOpen,
    filteredItems,
    toggleItem,
    removeItem,
    getContainerProps: () => ({ id: `${id}-container` }),
    getInputProps: () => ({
      value: query,
      onChange: (e: React.ChangeEvent<HTMLInputElement>) => {
        setQuery(e.target.value);
        setIsOpen(true);
      },
      onFocus: () => setIsOpen(true),
    }),
    getItemProps: (item: T) => ({
      role: 'option' as const,
      'aria-selected': selected.some((i) => i.id === item.id),
      onClick: () => toggleItem(item),
    }),
    getRemoveButtonProps: (itemId: string) => ({
      type: 'button' as const,
      'aria-label': 'Remove item',
      onClick: (e: React.MouseEvent) => {
        e.stopPropagation();
        removeItem(itemId);
      },
    }),
  };
}
```
</details>
