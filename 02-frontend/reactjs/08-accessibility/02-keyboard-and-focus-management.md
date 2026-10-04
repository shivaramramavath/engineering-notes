# Keyboard and Focus Management

Many people never use a mouse: those with motor disabilities, screen reader users, power users, and anyone with a broken trackpad or a full hand. If your app can't be operated with a keyboard, it can't be operated by them. **Focus** — which element currently receives keyboard input — is the thread that makes keyboard use possible. This file covers tab order, visible focus, expected keyboard behavior, composite widgets (roving tabindex), modals and focus traps, and the focus problems specific to single-page apps.

## Prerequisites

[`00-semantic-html.md`](./00-semantic-html.md), [`01-aria.md`](./01-aria.md), [`../03-hooks/04-useRef.md`](../03-hooks/04-useRef.md), and [`../03-hooks/02-useEffect.md`](../03-hooks/02-useEffect.md)

---

## 1. The basics of keyboard operation

| Key | Expected behavior |
|-----|-------------------|
| `Tab` / `Shift+Tab` | Move focus forward/backward through interactive elements |
| `Enter` | Activate links and buttons; submit forms |
| `Space` | Activate buttons; toggle checkboxes |
| `Escape` | Close dialogs, menus, popovers; cancel |
| Arrow keys | Move within a group (tabs, radios, menu items, listboxes, sliders) |
| `Home` / `End` | First/last item in a group |

**Native elements already behave this way.** A `<button>`, `<a href>`, `<input>`, `<select>`, `<summary>`, and `<textarea>` are focusable and operable out of the box. Custom elements aren't — one more reason to prefer native ones ([`00-semantic-html.md`](./00-semantic-html.md)).

**Quick test:** put your mouse away and operate your whole app with `Tab`, `Shift+Tab`, `Enter`, `Space`, `Escape`, and arrow keys. Every action should be reachable and every state visible.

---

## 2. Tab order and `tabindex`

The **tab order** follows the DOM order of focusable elements. Keep the DOM order the same as the visual order, and tab order takes care of itself.

| `tabindex` | Effect |
|------------|--------|
| *(none)* | Native interactive elements are in the tab order; everything else isn't |
| `tabIndex={0}` | Adds a non-native element to the tab order, in DOM order |
| `tabIndex={-1}` | Focusable **by script** (`element.focus()`) but not via Tab |
| `tabIndex={1+}` | **Never use** — positive values override the natural order and create chaos |

```tsx
// Focusable programmatically (e.g., move focus to a heading after navigation)
<h1 ref={headingRef} tabIndex={-1}>Settings</h1>
```

Layout tricks that **reorder visually** without reordering the DOM (`order`, `flex-direction: row-reverse`, grid placement, `position`) make Tab jump around unpredictably. Reorder the DOM instead.

Don't make things focusable that do nothing: a stop on a non-interactive element confuses everyone. If content must be scrollable by keyboard, a scroll container needs `tabIndex={0}` (and a name), because Tab can't reach it otherwise.

---

## 3. Visible focus

Users must **see** where focus is (WCAG 2.4.7, and in WCAG 2.2, focus mustn't be hidden behind sticky headers or banners). Never remove the outline without replacing it:

```css
/* ❌ */
*:focus { outline: none; }

/* ✅ Style it, using :focus-visible so mouse clicks don't show a ring */
:focus-visible {
  outline: 2px solid var(--focus-color, #2563eb);
  outline-offset: 2px;
}
```

- **`:focus-visible`** shows the ring for keyboard (and similar) focus but not for most mouse clicks. Use it instead of `:focus`.
- Give the indicator **enough contrast** (3:1 against adjacent colors) and thickness (2px is a good floor).
- Check that focus rings aren't **clipped** by `overflow: hidden` on a parent.
- In Tailwind: `focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-blue-600`, or `focus-visible:ring-2` ([`../07-styling/02-tailwindcss.md`](../07-styling/02-tailwindcss.md)).
- **Sticky headers can cover the focused element** as the browser scrolls it into view. Add `scroll-padding-top` equal to the header height to the `html` element.

---

## 4. Skip links

Keyboard users shouldn't Tab through the entire navigation on every page. A **skip link** is the first focusable element and jumps to the main content:

```tsx
<a href="#main" className="skip-link">Skip to main content</a>
...
<main id="main" tabIndex={-1}>…</main>
```

```css
.skip-link {
  position: absolute;
  left: 0;
  top: 0;
  transform: translateY(-100%);   /* hidden off-screen until focused */
}
.skip-link:focus { transform: translateY(0); }
```

`tabIndex={-1}` on `<main>` ensures focus actually moves there in all browsers when the link is activated. With client-side routing, check that the hash link works with your router, or move focus yourself.

---

## 5. Composite widgets: one tab stop, arrows inside

A toolbar with 10 buttons, a tab list, a menu, or a grid of cells would be tedious if every item were a tab stop. The pattern: the whole widget is **one** Tab stop, and **arrow keys** move within it.

### Roving tabindex

Only the **active** item has `tabIndex={0}`; all others have `tabIndex={-1}`. Arrow keys update which item is active and move focus:

```tsx
function Toolbar({ items }: { items: { id: string; label: string }[] }) {
  const [active, setActive] = useState(0);
  const refs = useRef<(HTMLButtonElement | null)[]>([]);

  function move(next: number) {
    const index = (next + items.length) % items.length;      // wrap around
    setActive(index);
    refs.current[index]?.focus();
  }

  function handleKeyDown(e: KeyboardEvent<HTMLDivElement>) {
    switch (e.key) {
      case "ArrowRight": e.preventDefault(); move(active + 1); break;
      case "ArrowLeft":  e.preventDefault(); move(active - 1); break;
      case "Home":       e.preventDefault(); move(0); break;
      case "End":        e.preventDefault(); move(items.length - 1); break;
    }
  }

  return (
    <div role="toolbar" aria-label="Text formatting" onKeyDown={handleKeyDown}>
      {items.map((item, i) => (
        <button
          key={item.id}
          type="button"
          ref={(el) => { refs.current[i] = el; }}
          tabIndex={i === active ? 0 : -1}
          onFocus={() => setActive(i)}
        >
          {item.label}
        </button>
      ))}
    </div>
  );
}
```

Key points:

- `onFocus` keeps `active` in sync if the user clicks an item.
- **`e.preventDefault()`** stops the page from scrolling on arrow keys.
- Arrow direction follows the widget's orientation (left/right for horizontal, up/down for vertical); support **Home/End**. Wrap-around is a design choice.
- Disabled items: skip them while moving.
- The exact keys come from the WAI-ARIA Authoring Practices (APG) for each widget type — follow them, because screen reader users expect them ([`01-aria.md`](./01-aria.md)).

### Alternative: `aria-activedescendant`

Instead of moving DOM focus, keep focus on a container (like a combobox input) and point `aria-activedescendant` at the "current" option's id. Used for comboboxes and listboxes where focus must stay in a text field. It's more intricate; let a library handle it.

**In practice:** use an accessible primitive library (Radix, React Aria, Headless UI), which implements these patterns correctly ([`../09-ui-components/00-shadcn-ui.md`](../09-ui-components/00-shadcn-ui.md)).

---

## 6. Modals and focus

A dialog has to do four things:

1. **Move focus into the dialog** when it opens (to its first control, its heading, or the dialog itself).
2. **Keep focus inside** while open (a "focus trap"): Tab and Shift+Tab cycle within it.
3. **Close on `Escape`**.
4. **Return focus** to the element that opened it when it closes.

Plus: the page behind must be **inert**, so screen readers and Tab can't reach it.

### The native `<dialog>` does most of it

```tsx
function Modal({ open, onClose, title, children }: ModalProps) {
  const ref = useRef<HTMLDialogElement>(null);

  useEffect(() => {
    const dialog = ref.current;
    if (!dialog) return;
    if (open && !dialog.open) dialog.showModal();     // traps focus, inerts the page, Esc closes
    if (!open && dialog.open) dialog.close();
  }, [open]);

  return (
    <dialog ref={ref} onClose={onClose} aria-labelledby="modal-title">
      <h2 id="modal-title">{title}</h2>
      {children}
      <button type="button" onClick={onClose}>Close</button>
    </dialog>
  );
}
```

`showModal()` provides the trap, `Escape`, the inert background, and **returns focus to the previously focused element** on close, in modern browsers. Use `onClose` (which also fires on Escape) to sync your React state. Styling the `::backdrop` and animating are possible but need extra care ([`../09-ui-components/02-dialogs-and-modals.md`](../09-ui-components/02-dialogs-and-modals.md)).

### Manual management (when you can't use `<dialog>`)

```tsx
const triggerRef = useRef<HTMLElement | null>(null);

function openDialog() {
  triggerRef.current = document.activeElement as HTMLElement;   // remember who opened it
  setOpen(true);
}

function closeDialog() {
  setOpen(false);
  triggerRef.current?.focus();                                  // return focus
}
```

Then trap focus inside (listen for `Tab` and wrap between the first and last focusable elements — easy to get wrong), set `role="dialog"` with `aria-modal="true"` and a name, and make the background `inert`. This is a lot to get right: **use a tested library** (Radix `Dialog`, React Aria, `focus-trap-react`) rather than writing it ([`../09-ui-components/02-dialogs-and-modals.md`](../09-ui-components/02-dialogs-and-modals.md)).

### The `inert` attribute

`inert` makes an element and its descendants unfocusable, unclickable, and absent from the accessibility tree — the right tool to disable the page behind a modal or offscreen drawer:

```tsx
<div id="app" inert={dialogOpen}>…</div>
```

React 19 supports `inert` as a boolean prop. In older versions, handle the attribute manually. Support is good in current browsers; check caniuse.com for yours.

### Non-modal popovers and menus

Dropdowns and tooltips needn't trap focus, but must close on `Escape`, return focus to their trigger, and (for menus) support arrow keys. The native **Popover API** (`popover` attribute) handles light dismiss and layering for simple cases.

---

## 7. Focus in single-page apps

In a traditional site, navigating to a new page resets focus to the top and announces the new page title. A client-side router **does neither**: the URL changes, content swaps, and focus stays on the link the user clicked (which may no longer exist), so keyboard users are stranded and screen reader users hear nothing.

Fix it on route changes:

```tsx
function RouteAnnouncer() {
  const { pathname } = useLocation();
  const headingRef = useRef<HTMLHeadingElement>(null);

  useEffect(() => {
    document.title = `${titleFor(pathname)} – My App`;
    headingRef.current?.focus();          // move focus to the new page's <h1> (tabIndex={-1})
  }, [pathname]);
  // …
}
```

Common approaches: **focus the new page's `<h1>`** (`tabIndex={-1}`), or focus the `<main>` container; also **update `document.title`** and consider announcing the navigation through a live region ([`03-screen-readers.md`](./03-screen-readers.md)). Don't move focus on the very first page load. Router support varies; check yours ([`../10-routing/03-navigation.md`](../10-routing/03-navigation.md)).

### Other places focus gets lost

- **After deleting an item**: the focused button vanishes and focus falls back to `<body>`. Move focus to the next item, the previous item, the list heading, or an "Add" button.
- **After adding an item**: focus the new item or its first input.
- **After closing a menu or inline editor**: return to the trigger.
- **After a failed form submit**: focus the first invalid field or an error summary ([`../06-forms/05-form-submission-and-errors.md`](../06-forms/05-form-submission-and-errors.md)).
- **Content that updates in place** (filters, search results): don't steal focus; announce the change instead.
- **Conditionally rendered controls**: if an element that has focus is removed from the DOM, focus is lost.

```tsx
// After deleting from a list
function handleDelete(id: string, index: number) {
  deleteItem(id);
  // wait for React to commit the new list, then move focus
  requestAnimationFrame(() => {
    (itemRefs.current[index] ?? itemRefs.current[index - 1] ?? addButtonRef.current)?.focus();
  });
}
```

(Using an effect keyed on the list length is also reasonable. Don't call `focus()` in the render body — it's a side effect ([`../02-state-and-rendering/03-rendering.md`](../02-state-and-rendering/03-rendering.md)).)

---

## 8. Programmatic focus tips

- Use a **ref** and call `focus()` in an event handler or effect, never during render ([`../03-hooks/04-useRef.md`](../03-hooks/04-useRef.md)).
- **Autofocus is risky.** The `autoFocus` prop on page load can disorient screen reader users and jump the page on mobile. Use it for dialogs and in-flow interactions where the user just asked for the thing, not on page load.
- `element.focus({ preventScroll: true })` avoids an unwanted scroll jump when you control scrolling yourself.
- **Hover isn't keyboard.** Anything that appears on hover (tooltips, menus) must also appear on focus, and be dismissible without moving focus (WCAG 1.4.13): Escape closes it, and the pointer can move onto the content.

---

## 9. Avoiding keyboard traps and bad shortcuts

- **No keyboard traps:** the user must always be able to leave a component with the keyboard (WCAG 2.1.2). Modals trap *intentionally* and release on close; embedded widgets (editors, maps, iframes) must offer an exit key.
- **Custom shortcuts:** single-character shortcuts (`s`, `/`) conflict with screen reader and voice-control commands. Offer a way to turn them off or remap them, or require a modifier, and don't trigger them while typing in inputs.
- **Don't override standard browser and OS shortcuts** (`Ctrl+F`, `Tab`, `F5`).
- **Drag-and-drop** needs a keyboard alternative (buttons to move items, a select to reorder). WCAG 2.2 requires a non-dragging alternative.
- **Pointer gestures** (swipe, pinch, path-based) need single-pointer alternatives.

---

## 10. Testing keyboard accessibility

1. **Tab through the page** from the top: is the order logical? Is every interactive element reachable? Is focus always visible?
2. **Operate every control** with Enter/Space/arrows/Escape.
3. **Open and close each overlay**: does focus enter, stay, and return correctly?
4. **Navigate between routes**: where does focus land?
5. **Zoom to 200%–400%** and repeat the checks.
6. Automate part of it: Testing Library's `userEvent.tab()`, `userEvent.keyboard("{Escape}")`, and `expect(element).toHaveFocus()` ([`../18-testing-and-debugging/02-component-testing-with-rtl.md`](../18-testing-and-debugging/02-component-testing-with-rtl.md)); Playwright for full flows ([`../18-testing-and-debugging/06-e2e-testing-playwright.md`](../18-testing-and-debugging/06-e2e-testing-playwright.md)).

---

## Common mistakes

- **Clickable `div`s with no `tabIndex`, role, or key handling** — unreachable by keyboard.
- **`outline: none` with no replacement** — keyboard users can't see focus.
- **Positive `tabindex`** values — scrambles the natural order.
- **Visual reordering with CSS** that doesn't match the DOM order.
- **Modals that don't move focus in, trap it, close on Escape, or restore it** — or a modal while the page behind stays reachable.
- **Every item in a composite widget being a tab stop** (a 30-item toolbar) — use a roving tabindex.
- **Forgetting `preventDefault()` on arrow keys**, so the page scrolls while navigating a widget.
- **No focus management after route changes, deletions, or closing popovers** — focus falls back to `<body>`.
- **Hover-only functionality** — unreachable by keyboard and touch.
- **Sticky headers covering the focused element.**
- **Auto-focusing on page load**, and calling `focus()` during render.
- **Drag-and-drop with no keyboard alternative.**
- **Hand-rolling focus traps** instead of using `<dialog>` or a tested library.

## Quick summary

- Everything operable by mouse must be operable by keyboard: Tab, Enter, Space, Escape, arrows, Home/End
- Prefer native elements; tab order follows DOM order; use `tabIndex` 0 or -1, never positive
- Always show focus with `:focus-visible` styling that has enough contrast; don't let sticky UI hide it
- Provide a skip link; make composite widgets one tab stop with a roving tabindex
- Modals: move focus in, trap, close on Escape, restore focus, inert background — `<dialog>` or a library does it for you
- In SPAs, manage focus and the document title on route changes, and after adds, deletes, and closes
- Test by actually using only the keyboard, then automate what you can

## Next

**[`03-screen-readers.md`](./03-screen-readers.md)** covers how screen readers present your UI, how to announce dynamic changes, and how to test with one.
