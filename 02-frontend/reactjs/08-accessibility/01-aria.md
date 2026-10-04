# ARIA

ARIA (Accessible Rich Internet Applications) is a set of HTML attributes that **adds information to the accessibility tree**: roles ("this is a tab"), states ("expanded", "selected"), and properties ("labelled by that heading"). It exists for the gaps where HTML has no native element — custom tabs, comboboxes, live updates. But ARIA is easy to misuse, and **no ARIA is better than bad ARIA**: it changes what assistive technology *announces*, not how anything *behaves*.

## Prerequisites

[`00-semantic-html.md`](./00-semantic-html.md)

---

## The rules of ARIA

1. **Don't use ARIA if a native element does the job.** Use `<button>`, not `<div role="button">`.
2. **Don't change native semantics** unless you really must. `<h2 role="tab">` throws away the heading; wrap it or restructure instead.
3. **All interactive ARIA controls must be usable with the keyboard.** A role doesn't add focusability or key handling — you must (see [`02-keyboard-and-focus-management.md`](./02-keyboard-and-focus-management.md)).
4. **Don't hide focusable elements** (`aria-hidden="true"` or `role="presentation"` on something focusable) — users land on an element that doesn't exist for their screen reader.
5. **Every interactive element needs an accessible name.**

**ARIA changes the accessibility tree only.** `role="button"` makes a screen reader say "button" but does not make the element focusable, respond to Enter or Space, or look like a button. `aria-expanded="true"` states that something is open; **your code** must open it.

---

## Roles, states, and properties

| Kind | What it does | Examples |
|------|--------------|----------|
| **Role** | Says *what an element is* | `role="dialog"`, `tab`, `tablist`, `tabpanel`, `menu`, `menuitem`, `alert`, `status`, `switch`, `combobox`, `listbox` |
| **Property** | Describes relationships and characteristics (changes rarely) | `aria-label`, `aria-labelledby`, `aria-describedby`, `aria-controls`, `aria-haspopup`, `aria-required` |
| **State** | Describes current condition (changes with interaction) | `aria-expanded`, `aria-selected`, `aria-checked`, `aria-pressed`, `aria-current`, `aria-invalid`, `aria-busy`, `aria-hidden`, `aria-disabled` |

In JSX, ARIA attributes keep their **hyphenated lowercase** names:

```tsx
<button aria-expanded={open} aria-controls="menu-panel">Menu</button>
```

Boolean-like ARIA attributes take string values (`"true"`/`"false"`); React converts `true`/`false` booleans to those strings for `aria-*` attributes, so `aria-expanded={open}` works.

---

## Accessible names: the most important concept

Every interactive element (and many structural ones) needs an **accessible name**: the text a screen reader announces. The browser computes it from, in rough priority order:

1. `aria-labelledby` (points to other elements by id)
2. `aria-label` (a string)
3. Native sources: `<label>`, button/link text content, `alt`, `<caption>`, `<legend>`, `title`
4. `title` / `placeholder` (last resort; unreliable)

### Naming icon-only buttons

```tsx
// ❌ A screen reader says "button" with no name
<button onClick={close}><XIcon /></button>

// ✅ aria-label on the button (and hide the decorative icon)
<button onClick={close} aria-label="Close dialog">
  <XIcon aria-hidden="true" />
</button>

// ✅ or visually hidden text
<button onClick={close}>
  <XIcon aria-hidden="true" />
  <span className="sr-only">Close dialog</span>
</button>
```

Visible text is better than `aria-label` where possible: it's translated by page-translation tools, and it works for voice-control users who say what they see.

### `aria-label` vs `aria-labelledby`

```tsx
// aria-label: a string
<nav aria-label="Primary">…</nav>

// aria-labelledby: reuse existing visible text (preferred when a heading exists)
<section aria-labelledby="billing-title">
  <h2 id="billing-title">Billing</h2>
</section>
```

Use `aria-labelledby` when the label is already on screen, so it stays in sync. Generate ids with `useId` ([`00-semantic-html.md`](./00-semantic-html.md)).

**Caveat:** `aria-label` is **not announced on generic elements** like a plain `<div>` or `<span>` (they have no role). Use it on interactive elements, landmarks, or elements with an appropriate role.

### "Label in Name"

If a control has visible text, its accessible name must **include that text** (WCAG 2.5.3), so voice users who say "click Search" can activate `aria-label="Search the site"`, but not `aria-label="Find"`.

---

## Descriptions: `aria-describedby`

Names say what something is; **descriptions** add supporting detail (hints, errors):

```tsx
const id = useId();
const hintId = `${id}-hint`;
const errorId = `${id}-error`;

<label htmlFor={id}>Password</label>
<input
  id={id}
  type="password"
  aria-describedby={error ? `${hintId} ${errorId}` : hintId}
  aria-invalid={Boolean(error)}
/>
<p id={hintId}>At least 8 characters.</p>
{error && <p id={errorId} role="alert">{error}</p>}
```

`aria-describedby` accepts **multiple space-separated ids**. The user hears the label, the control type, then the description. Details in [`../06-forms/01-form-validation.md`](../06-forms/01-form-validation.md).

---

## Common states and what they mean

| Attribute | Use on | Meaning |
|-----------|--------|---------|
| `aria-expanded` | Buttons that show/hide content | Whether the controlled content is open |
| `aria-controls` | The same button | Which element it controls (support varies; still useful) |
| `aria-pressed` | Toggle buttons | Pressed/not pressed (not for buttons that change label) |
| `aria-selected` | Tabs, options, grid cells | The current selection in a composite widget |
| `aria-checked` | Custom checkboxes, switches, radios | Checked state (prefer native `<input type="checkbox">`) |
| `aria-current` | Links in nav, steps | `"page"`, `"step"`, `"true"`: the current item |
| `aria-invalid` | Inputs | The value is invalid |
| `aria-required` | Inputs | Required (native `required` also works) |
| `aria-disabled` | Controls | Disabled but still focusable/discoverable |
| `aria-busy` | Regions being updated | Content is loading |
| `aria-haspopup` | Triggers | A menu, listbox, dialog, etc. will appear |
| `aria-modal` | Dialogs | Content outside is inert (also needs real focus management) |
| `aria-live` | Regions | Announce changes (see [`03-screen-readers.md`](./03-screen-readers.md)) |
| `aria-hidden` | Anything | Remove from the accessibility tree |

### A disclosure (show/hide) done right

```tsx
function Disclosure({ title, children }: { title: string; children: ReactNode }) {
  const [open, setOpen] = useState(false);
  const panelId = useId();

  return (
    <div>
      <button
        type="button"
        aria-expanded={open}
        aria-controls={panelId}
        onClick={() => setOpen((o) => !o)}
      >
        {title}
      </button>
      <div id={panelId} hidden={!open}>{children}</div>
    </div>
  );
}
```

A native `<details><summary>` does the same with no JavaScript and no ARIA — prefer it when it fits. The `hidden` attribute removes the panel from the accessibility tree when closed; the button's `aria-expanded` communicates state.

### Toggle button vs switch

- **`aria-pressed`** — a button whose label stays the same while its state changes ("Bold", "Mute"). Don't also change the label to "Unmute".
- **`role="switch"` + `aria-checked`** — an on/off setting ("Dark mode").

### Current page in navigation

```tsx
<a href="/settings" aria-current={isActive ? "page" : undefined}>Settings</a>
```

React Router's `NavLink` sets `aria-current="page"` automatically.

---

## `aria-hidden` and hiding content

| Technique | Visible | In accessibility tree | Focusable |
|-----------|:-------:|:---------------------:|:---------:|
| `display: none` / `hidden` / `visibility: hidden` | ❌ | ❌ | ❌ |
| `aria-hidden="true"` | ✅ | ❌ | **still focusable** ⚠ |
| `sr-only` (visually hidden CSS) | ❌ | ✅ | ✅ (if focusable) |
| `inert` attribute | ✅ | ❌ | ❌ |

- Use `aria-hidden="true"` for **decorative** content (icons next to text) — never on an element that contains focusable children or that has meaning.
- To hide a **region** (like the page behind a modal), use **`inert`** — it removes content from the accessibility tree *and* the tab order, which `aria-hidden` alone does not ([`02-keyboard-and-focus-management.md`](./02-keyboard-and-focus-management.md)). In React 19, `inert` works as a boolean prop; earlier React versions need `inert=""` handling. Check your version.

---

## Widget roles: when you really do need them

For widgets with no native equivalent, ARIA gives you roles, but **you** must implement the behavior from the **WAI-ARIA Authoring Practices Guide (APG)** — which specifies roles, required attributes, and the keyboard interaction for each pattern (tabs, menu button, combobox, dialog, tree view, etc.).

A tablist needs, at minimum: `role="tablist"`, `role="tab"` with `aria-selected` and `aria-controls`, `role="tabpanel"` with `aria-labelledby`, a roving tabindex, and arrow-key/Home/End handling. [`../05-component-design/02-compound-components.md`](../05-component-design/02-compound-components.md) builds one; the keyboard half is in [`02-keyboard-and-focus-management.md`](./02-keyboard-and-focus-management.md).

**Practical advice:** don't hand-write complex widgets. Use an accessible primitive library — **Radix UI, React Aria, Headless UI, Ariakit** — which implement APG patterns and have been tested with assistive technology across browsers ([`../09-ui-components/00-shadcn-ui.md`](../09-ui-components/00-shadcn-ui.md)). Your job is then to provide good labels, content, and styling.

### Roles to avoid reaching for

- `role="menu"`/`menuitem` are for **application-style menus** (like a desktop File menu) with arrow-key navigation — **not** for site navigation. A site's nav is a `<nav>` with a list of links.
- `role="button"` on a `div` — use `<button>`.
- `role="link"` on a `span` — use `<a href>`.
- `role="application"` — changes how screen readers handle keys; almost always harmful.

---

## Live regions (preview)

`role="status"`, `role="alert"`, and `aria-live` tell screen readers to announce content changes without moving focus. They have timing rules that cause common bugs (the region must exist **before** its content changes). Covered in [`03-screen-readers.md`](./03-screen-readers.md).

---

## Tooling for ARIA correctness

- **`eslint-plugin-jsx-a11y`** flags invalid roles, missing labels, `aria-*` typos, interactive roles without keyboard handlers, and more at write time ([`../00-setup/05-typescript-and-linting-setup.md`](../00-setup/05-typescript-and-linting-setup.md)).
- **axe** (browser extension, or in tests) catches invalid ARIA usage, missing names, and bad references ([`04-accessibility-checklist.md`](./04-accessibility-checklist.md)).
- **Browser DevTools → Accessibility panel** shows the computed role, name, description, and states of the selected element — the fastest way to see what assistive technology sees.
- **Testing Library queries by role** (`getByRole("button", { name: "Save" })`) double as accessibility checks: if the role or name is wrong, the test can't find the element ([`../18-testing-and-debugging/02-component-testing-with-rtl.md`](../18-testing-and-debugging/02-component-testing-with-rtl.md)).

None of these replace actually testing with a keyboard and a screen reader.

---

## Common mistakes

- **ARIA on top of an element that already has the semantics** (`<button role="button">`) — redundant; and `role` overrides can *remove* native behavior.
- **`role="button"` on a `div` without keyboard support** — not focusable, no Enter/Space handling.
- **Icon-only controls with no accessible name.**
- **`aria-label` on a plain `div`/`span`** — ignored without a role.
- **Using `aria-label` where visible text would work** — breaks translation and voice control (label-in-name).
- **`aria-hidden="true"` on focusable elements** (or ancestors of them) — focus lands on something invisible to screen readers.
- **Leaving `aria-expanded` unchanged** (or toggling it while the content doesn't change).
- **`aria-controls`/`aria-labelledby`/`aria-describedby` pointing to ids that don't exist** (or duplicate ids).
- **`role="menu"` for site navigation** — use `<nav>` and links.
- **Using ARIA states as styling hooks only** while forgetting to keep them in sync with real state.
- **Inventing ARIA attributes or misspelling them** — silently ignored; lint for it.
- **Trusting automated checks alone** — a passing linter says nothing about whether the experience makes sense.

## Quick summary

- ARIA adds semantics to the accessibility tree; it never adds behavior, focus, or visuals
- First rule: use the native element; second: don't override its semantics
- Everything interactive needs a name: native text, `aria-labelledby`, or `aria-label` (prefer visible text; names must include visible labels)
- `aria-describedby` for hints and errors; `aria-expanded`, `aria-pressed`, `aria-selected`, `aria-current`, `aria-invalid` for state — and keep them in sync
- `aria-hidden` removes from the tree but not from the tab order; use `hidden` or `inert` to truly hide
- Custom widgets follow the APG patterns for roles *and* keyboard behavior; prefer a primitive library
- Verify with lint, axe, DevTools' accessibility panel, and real assistive technology

## Next

**[`02-keyboard-and-focus-management.md`](./02-keyboard-and-focus-management.md)** covers the behavior half of accessibility: keyboard interaction and managing where focus goes.
