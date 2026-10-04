# Component API Design

A component's props are its public API. A good API makes correct usage obvious and incorrect usage hard; a bad one grows a new boolean for every edge case until nobody remembers what combinations work. This file covers **when to build a reusable component at all**, how to design its props, and how to support both controlled and uncontrolled use.

## Prerequisites

[`../01-fundamentals/02-components-and-props.md`](../01-fundamentals/02-components-and-props.md), [`../04-typescript-with-react/00-typing-components-and-props.md`](../04-typescript-with-react/00-typing-components-and-props.md), and [`../02-state-and-rendering/02-state-structure-and-lifting.md`](../02-state-and-rendering/02-state-structure-and-lifting.md) (controlled vs uncontrolled basics)

---

## 1. Should this be reusable yet?

Abstraction has a cost: every shared component couples its callers together, and a wrong abstraction is harder to fix than duplication.

Practical rules:

- **Duplicate once, abstract on the third use** ("rule of three"). Two similar pieces may differ in ways you can't predict yet.
- **Abstract what's the same, not what looks similar.** Two forms that share markup but have different validation, data, and behavior are different components.
- **Start specific, generalize on evidence.** Build `UserCard`, then extract `Card` when real variations appear.
- **If a "reusable" component keeps gaining props for one caller's needs**, the abstraction is wrong. Split it, or switch to composition (below).

Good candidates for reuse: buttons, inputs, modals, layout shells, tabs — things with **stable behavior** and **many callers**.

---

## 2. Design the API from the caller's side

Write the usage code *first*, then implement it:

```tsx
// What do I want to write?
<Button variant="danger" size="sm" onClick={handleDelete}>Delete</Button>

// Not:
<Button isDanger isSmall isNotRounded hasIcon iconPosition="left" onClick={handleDelete} />
```

Questions to ask:

- Is the common case a one-liner?
- Can someone use it correctly without reading the implementation?
- What happens with the wrong combination — a compile error, or a silent bug?

---

## 3. Keep the prop list small and meaningful

| Guideline | Why |
|-----------|-----|
| Few required props, sensible defaults for the rest | Common usage stays short |
| Name props for **what they mean**, not how they're implemented | `variant="danger"`, not `redBackground` |
| One prop, one concern | Avoid props that interact in surprising ways |
| Prefer **string-literal unions** to booleans for anything with more than two states | `size: "sm" \| "md" \| "lg"`, not `isSmall` and `isLarge` |
| Boolean props default to `false` and read naturally | `disabled`, `loading`, `fullWidth` — never `isNotDisabled` |
| More than ~6–8 props is a smell | Often several components in one |

### Replace boolean explosions with variants

```tsx
// ❌ 2⁴ = 16 combinations, several of them nonsense
type ButtonProps = {
  isPrimary?: boolean;
  isDanger?: boolean;
  isSmall?: boolean;
  isLarge?: boolean;
};

// ✅ Each prop has exactly the valid values
type ButtonProps = {
  variant?: "primary" | "secondary" | "danger";
  size?: "sm" | "md" | "lg";
};
```

Implementing variants cleanly (class maps, `cva`): [`../09-ui-components/01-component-variants.md`](../09-ui-components/01-component-variants.md).

### Props that depend on each other

When one prop only makes sense with another, encode it in the type with a discriminated union rather than documenting it ([`../04-typescript-with-react/05-advanced-typing-patterns.md`](../04-typescript-with-react/05-advanced-typing-patterns.md)):

```tsx
type AlertProps =
  | { dismissible?: false }
  | { dismissible: true; onDismiss: () => void };
```

### Pass values, not DOM events

Expose `onChange: (value: string) => void` rather than the raw event unless the caller truly needs it ([`../04-typescript-with-react/01-typing-events.md`](../04-typescript-with-react/01-typing-events.md)).

---

## 4. Prefer composition over configuration

When a component needs different **content or structure** in different places, don't add props for each part — accept elements:

```tsx
// ❌ Configuration: every new need adds a prop
<Dialog
  title="Delete item"
  titleIcon={<TrashIcon />}
  description="This cannot be undone."
  showCloseButton
  confirmLabel="Delete"
  cancelLabel="Cancel"
  onConfirm={...}
  footerAlign="right"
/>

// ✅ Composition: structure is the caller's call
<Dialog>
  <Dialog.Header>
    <TrashIcon /> Delete item
  </Dialog.Header>
  <Dialog.Body>This cannot be undone.</Dialog.Body>
  <Dialog.Footer>
    <Button variant="ghost">Cancel</Button>
    <Button variant="danger" onClick={onConfirm}>Delete</Button>
  </Dialog.Footer>
</Dialog>
```

The composed API has fewer props, no decision about footer alignment or labels baked in, and never needs changing to accommodate a new layout. Techniques are in [`01-composition-patterns.md`](./01-composition-patterns.md) and [`02-compound-components.md`](./02-compound-components.md).

**Rule of thumb:** configure for *simple values* (size, variant, disabled); compose for *content and structure*.

---

## 5. Forward the rest: native props, `className`, `ref`

A reusable UI component should behave like the element it wraps. Accept the element's native props, merge `className`, and let callers set `id`, `aria-*`, `data-*`, and event handlers:

```tsx
import type { ComponentProps } from "react";
import { cn } from "@/lib/cn";   // class-merging helper, e.g. clsx + tailwind-merge

type ButtonProps = ComponentProps<"button"> & {
  variant?: "primary" | "secondary" | "danger";
  size?: "sm" | "md" | "lg";
};

function Button({
  variant = "primary",
  size = "md",
  className,
  type = "button",     // safe default: don't accidentally submit forms
  ...rest
}: ButtonProps) {
  return (
    <button
      type={type}
      className={cn("btn", `btn-${variant}`, `btn-${size}`, className)}
      {...rest}
    />
  );
}
```

Details:

- **`...rest` last** so callers can override behavior intentionally. Put the props you *don't* want overridden after it.
- **Merge `className`**, don't replace it ([`../07-styling/05-styling-approaches-compared.md`](../07-styling/05-styling-approaches-compared.md)).
- **`ref` is an ordinary prop in React 19**, and `ComponentProps<"button">` includes it. Earlier versions need `forwardRef` ([`../16-advanced-react/01-refs-and-imperative-handles.md`](../16-advanced-react/01-refs-and-imperative-handles.md)).
- Default `type="button"`: a `<button>` inside a form submits it unless told otherwise.

Don't spread unknown props onto custom (non-DOM) components blindly, and avoid passing your own non-DOM props down to a DOM element, which produces React warnings about unknown attributes.

---

## 6. Controlled and uncontrolled component APIs

Interactive components (inputs, tabs, accordions, dialogs, selects) often need **both** modes:

| Mode | Who owns the state | Props |
|------|--------------------|-------|
| **Uncontrolled** | The component | `defaultValue` (initial) + optional `onChange` for notifications |
| **Controlled** | The parent | `value` + `onChange` |

Offering both lets simple usage stay simple while advanced usage can drive the component. The same convention as native `<input>` makes the API familiar. Naming convention: `value` / `defaultValue` / `onValueChange` (or `open` / `defaultOpen` / `onOpenChange`).

### A reusable hook

Write the dual-mode logic once and reuse it across components:

```tsx
import { useState, useCallback } from "react";

type Options<T> = {
  value?: T;
  defaultValue: T;
  onChange?: (value: T) => void;
};

export function useControllableState<T>({
  value,
  defaultValue,
  onChange,
}: Options<T>) {
  const [internal, setInternal] = useState<T>(defaultValue);
  const isControlled = value !== undefined;
  const current = isControlled ? (value as T) : internal;

  const setValue = useCallback(
    (next: T) => {
      if (!isControlled) setInternal(next);
      onChange?.(next);
    },
    [isControlled, onChange]
  );

  return [current, setValue] as const;
}
```

```tsx
type ToggleProps = {
  pressed?: boolean;
  defaultPressed?: boolean;
  onPressedChange?: (pressed: boolean) => void;
  children: ReactNode;
};

function Toggle({ pressed, defaultPressed = false, onPressedChange, children }: ToggleProps) {
  const [isPressed, setPressed] = useControllableState({
    value: pressed,
    defaultValue: defaultPressed,
    onChange: onPressedChange,
  });

  return (
    <button aria-pressed={isPressed} onClick={() => setPressed(!isPressed)}>
      {children}
    </button>
  );
}

<Toggle defaultPressed>Bold</Toggle>                                // uncontrolled
<Toggle pressed={isBold} onPressedChange={setIsBold}>Bold</Toggle>  // controlled
```

Rules:

- **Don't switch modes during a component's life.** Going from `value={undefined}` to a defined value (or back) is the same bug as the native input warning; document it, and in development warn.
- `undefined` means "uncontrolled", so a controlled value that legitimately needs "empty" should use `null` or `""`.
- Always call `onChange`, even when uncontrolled — callers may want to observe changes.

Controlled vs uncontrolled *form inputs* in depth: [`../06-forms/00-controlled-and-uncontrolled-inputs.md`](../06-forms/00-controlled-and-uncontrolled-inputs.md). State-ownership background: [`../02-state-and-rendering/02-state-structure-and-lifting.md`](../02-state-and-rendering/02-state-structure-and-lifting.md).

---

## 7. Build in accessibility

An API that makes the accessible path the default is better than documentation asking people to remember:

- Render the right semantic element by default (`<button>`, not `<div role="button">`).
- Require what accessibility requires: an `aria-label` when there's no visible text, an `alt` for images, an association between label and input.
- Manage focus where users expect it (dialogs trap and restore focus).
- Make the keyboard behavior part of the component, not left to callers.

A type can enforce this: for an icon-only button, require `aria-label` when `children` is absent. See [`../08-accessibility/`](../08-accessibility/README.md).

---

## 8. Defaults, errors, and evolution

- **Safe defaults.** The unconfigured component should do the harmless, expected thing.
- **Fail loudly in development.** Throw or `console.error` for impossible usage (a compound child outside its parent) with a helpful message.
- **Avoid breaking changes.** Adding an optional prop is safe; renaming or changing a default isn't. Deprecate first (keep the old prop, warn, remove later).
- **Don't expose internals** (state setters, internal ids, DOM nodes) unless you want to support them forever.
- **Document by example.** A Storybook story or a short usage block per variant ([`../09-ui-components/09-design-system.md`](../09-ui-components/09-design-system.md)).

---

## 9. Signs of a bad component API

| Smell | Likely fix |
|-------|-----------|
| Many boolean props | Variants or composition |
| Props that are mutually exclusive but both optional | Discriminated union, or split components |
| A prop that takes a config object of JSX (`columns={[{ render: … }]}`) used for everything | Accept children/composition |
| Callers pass `className` hacks to undo internal styling | Expose the right variants or slots |
| `if (props.mode === …)` branches covering unrelated behavior | Two components |
| Props passed through several layers unchanged | Composition or context |
| Same component, three unrelated uses with prop overrides | The abstraction is wrong; duplicate or split |

---

## Common mistakes

- **Abstracting too early** — premature shared components that every caller bends.
- **Boolean flags for variations** — combinatorial explosion; use unions.
- **Configuring content through props** (`titleIcon`, `footerAlign`…) instead of accepting elements.
- **Not forwarding native props, `className`, and `ref`** — callers must fork the component for basic needs.
- **Overriding `className` instead of merging** — breaks caller styling.
- **Switching a component between controlled and uncontrolled** — causes lost state and warnings.
- **Omitting `onChange` in uncontrolled mode** — callers can't observe changes.
- **Defaulting `<button>` type to `submit`** — accidental form submissions.
- **Leaking DOM events or internals into the public API.**

## Quick summary

- Abstract on the third repeat, and only what's genuinely the same
- Design the usage first; keep props few, named for meaning, with safe defaults
- Variants over booleans; unions or composition for dependent options
- Configure simple values with props; compose content and structure
- Forward native props, merge `className`, and expose `ref`
- Support `value`/`defaultValue`/`onValueChange` with one shared hook
- Make the accessible path the default and fail loudly on misuse

## Next

**[`01-composition-patterns.md`](./01-composition-patterns.md)** covers the techniques behind "compose, don't configure": slots, headless components, and polymorphism.
