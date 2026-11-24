# Accessibility (a11y) — Senior Interview Questions & Deep Dives

> WAI-ARIA authoring practices, focus management, roving tabindex, keyboard navigation patterns, and automated accessibility auditing.

---

## Question 1: How do you implement the "Roving TabIndex" pattern for accessible composite widgets like menus, tabs, and toolbars?

### The Accessibility Problem
If every tab or menu item has `tabindex="0"`, a user pressing <kbd>Tab</kbd> must cycle through 20 items before escaping the component. WAI-ARIA specifies that the entire composite widget should have **only one active tab stop**, with arrow keys navigating within the group.

### The Roving TabIndex Pattern
1. Only the active or focused item has `tabIndex={0}`.
2. All other inactive siblings have `tabIndex={-1}`.
3. Left/Right or Up/Down arrow key listeners shift focus, updating the active index:

```tsx
export function TabList({ tabs, activeIndex, onChange }: TabListProps) {
  const handleKeyDown = (e: React.KeyboardEvent, index: number) => {
    let nextIndex = index;
    if (e.key === 'ArrowRight') nextIndex = (index + 1) % tabs.length;
    if (e.key === 'ArrowLeft') nextIndex = (index - 1 + tabs.length) % tabs.length;
    if (nextIndex !== index) {
      e.preventDefault();
      onChange(nextIndex);
    }
  };

  return (
    <div role="tablist">
      {tabs.map((tab, idx) => (
        <button
          key={tab.id}
          role="tab"
          tabIndex={activeIndex === idx ? 0 : -1}
          aria-selected={activeIndex === idx}
          onKeyDown={(e) => handleKeyDown(e, idx)}
        >
          {tab.label}
        </button>
      ))}
    </div>
  );
}
```

---

## Question 2: What are ARIA live regions, and when should you use `polite` versus `assertive`?

### The Concept
Screen readers only announce content changes when focused on an element. **ARIA live regions** (`aria-live`) announce dynamic DOM additions (e.g. toast notifications, form error banners) without requiring user focus to leave the active input.

### Modes
- `aria-live="polite"`: Screen reader waits until the user finishes typing or speaking before announcing the update (e.g., cart count increment, background search result count).
- `aria-live="assertive"`: Immediately interrupts the user's current speech (reserved for critical security warnings, session timeouts, or payment failure alerts).
