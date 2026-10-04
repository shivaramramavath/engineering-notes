# Semantic HTML

The most effective accessibility technique is also the simplest: **use the right HTML element for the job**. Native elements come with roles, names, keyboard behavior, focus handling, and screen reader support built in. A `<button>` is focusable, activates with Enter and Space, announces itself as a button, and works with voice control — for free. A `<div onClick>` has none of that until you rebuild it by hand, and you'll rarely rebuild all of it correctly.

## Prerequisites

[`../01-fundamentals/01-jsx.md`](../01-fundamentals/01-jsx.md) and [`../01-fundamentals/06-events.md`](../01-fundamentals/06-events.md)

---

## Why semantics matter

Browsers turn your HTML into an **accessibility tree**: a simplified structure of roles ("button", "heading level 2", "navigation"), names ("Save", "Main menu"), and states ("expanded", "checked"). Assistive technology reads that tree, not your pixels. Semantic HTML gives it accurate information; a pile of `div`s and `span`s gives it nothing.

```tsx
// ❌ Looks like a button; the accessibility tree sees plain text
<div className="btn" onClick={save}>Save</div>

// ✅ Role, focus, keyboard activation, and name — all built in
<button type="button" onClick={save}>Save</button>
```

---

## 1. Buttons vs links

The rule: **links navigate; buttons act.**

| Use | When | Example |
|-----|------|---------|
| `<a href>` (or your router's `Link`) | Going to a new URL or location | "View pricing", "Read more" |
| `<button>` | Doing something on this page | "Save", "Open menu", "Delete" |

```tsx
<a href="/pricing">Pricing</a>
<Link to="/pricing">Pricing</Link>       {/* React Router renders a real <a> */}

<button type="button" onClick={openMenu}>Menu</button>
<button type="submit">Create account</button>
```

Common errors:

- **`<a>` without `href`** isn't focusable or a link; **`<a href="#" onClick>`** used as a button misleads users and breaks Back/forward behavior. Use a `<button>`.
- **A `<button>` that navigates** loses link behaviors (open in new tab, copy address, middle-click). Use a link.
- **Default `type`.** A `<button>` inside a `<form>` defaults to `submit`. Write `type="button"` unless it should submit.
- Don't nest interactive elements (a button inside a link).

---

## 2. Headings: the page outline

Headings let screen reader users scan and jump through a page (they often navigate by heading list). Use them for **structure**, not size:

```tsx
<h1>Order history</h1>
  <h2>Pending</h2>
  <h2>Completed</h2>
    <h3>March 2025</h3>
```

- **One `<h1>` per page** (the page's topic); don't skip levels (`h2` → `h4`).
- **Choose the level by structure, then style with CSS.** Don't use `<h3>` because it "looks right", and don't use a `<div className="big-bold">` because you don't want default styles.
- In components, the correct heading level depends on **where the component is used**. Let callers choose:

```tsx
type CardProps = { title: string; headingLevel?: 2 | 3 | 4; children: ReactNode };

function Card({ title, headingLevel = 3, children }: CardProps) {
  const Heading = `h${headingLevel}` as const;
  return (
    <section>
      <Heading>{title}</Heading>
      {children}
    </section>
  );
}
```

---

## 3. Landmarks: regions of the page

Landmark elements let users jump straight to the main content or navigation:

| Element | Landmark role | Use |
|---------|---------------|-----|
| `<header>` | `banner` (when a direct child of `body`) | Site header |
| `<nav>` | `navigation` | Major navigation blocks |
| `<main>` | `main` | The page's primary content — **exactly one** |
| `<aside>` | `complementary` | Related side content |
| `<footer>` | `contentinfo` (when a direct child of `body`) | Site footer |
| `<section>` + a name | `region` | Meaningful section (only a landmark if it has an accessible name) |
| `<form>` + a name | `form` | Important forms |
| `<search>` | `search` | Search area |

```tsx
function Layout({ children }: { children: ReactNode }) {
  return (
    <>
      <a href="#main" className="skip-link">Skip to main content</a>
      <header>
        <nav aria-label="Primary">…</nav>
      </header>
      <main id="main" tabIndex={-1}>{children}</main>
      <footer>…</footer>
    </>
  );
}
```

- When there are **several** landmarks of the same type, give each a **distinct name** (`<nav aria-label="Primary">`, `<nav aria-label="Footer">`).
- Don't wrap everything in landmarks; use them for the main page regions.
- The **skip link** (first focusable element) lets keyboard users bypass repeated navigation ([`02-keyboard-and-focus-management.md`](./02-keyboard-and-focus-management.md)).

---

## 4. Lists, tables, and other structure

**Lists.** Use `<ul>`/`<ol>`/`<li>` for lists of items. Screen readers announce "list, 5 items", which helps people understand grouping. Menus, breadcrumbs, and card grids are often lists:

```tsx
<ul>
  {items.map((item) => <li key={item.id}>{item.name}</li>)}
</ul>
```

**Tables are for tabular data**, never for layout. Make relationships explicit:

```tsx
<table>
  <caption>Quarterly revenue</caption>
  <thead>
    <tr>
      <th scope="col">Quarter</th>
      <th scope="col">Revenue</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <th scope="row">Q1</th>
      <td>$1.2M</td>
    </tr>
  </tbody>
</table>
```

`<caption>` names the table; `<th scope>` ties data cells to their headers. For sortable, interactive grids see [`../09-ui-components/07-data-tables.md`](../09-ui-components/07-data-tables.md).

**Other useful native elements:**

| Element | Provides |
|---------|----------|
| `<details>` / `<summary>` | A disclosure widget with keyboard and state handled by the browser |
| `<dialog>` | A modal with focus trapping, Escape to close, and inert background via `showModal()` |
| `<fieldset>` + `<legend>` | Grouping related controls (radio groups, address blocks) with a group name |
| `<time dateTime>` | Machine-readable dates |
| `<figure>` + `<figcaption>` | An image or chart with a caption |
| `<progress>` / `<meter>` | Progress and gauge values |
| `<abbr title>` | Abbreviations |

Prefer these over custom widgets when they meet your needs — see [`01-aria.md`](./01-aria.md) for when they don't.

---

## 5. Forms

Every input needs a **label**; this is the single most common form accessibility failure.

```tsx
// Explicit association
<label htmlFor="email">Email</label>
<input id="email" type="email" autoComplete="email" />

// Implicit association
<label>
  Email
  <input type="email" autoComplete="email" />
</label>
```

In React the attribute is **`htmlFor`**, and ids must be unique per page — generate them with `useId` for reusable components:

```tsx
function TextField({ label, ...props }: { label: string } & ComponentProps<"input">) {
  const id = useId();
  return (
    <div>
      <label htmlFor={id}>{label}</label>
      <input id={id} {...props} />
    </div>
  );
}
```

Rules:

- **A placeholder is not a label.** It disappears on input, usually has poor contrast, and isn't reliably announced.
- Use the right **`type`** (`email`, `tel`, `number`, `date`, `password`) and **`autoComplete`** values (`name`, `email`, `current-password`), which help everyone and are required by WCAG for common fields.
- Group related inputs: radio buttons and checkbox sets belong in a `<fieldset>` with a `<legend>`.
- Connect hints and errors with `aria-describedby`, and mark invalid fields with `aria-invalid` ([`01-aria.md`](./01-aria.md), [`../06-forms/01-form-validation.md`](../06-forms/01-form-validation.md)).
- Mark required fields visibly and with `required` (or `aria-required`).
- Use a real `<button type="submit">`, so Enter submits the form.

---

## 6. Images and media

```tsx
<img src="chart.png" alt="Sales grew 40% from January to June" />    {/* informative */}
<img src="divider.svg" alt="" />                                     {/* decorative: empty alt */}
```

- **Every `<img>` needs an `alt` attribute.** Describe the *purpose or content* that matters in context, not "image of…".
- **Decorative** images get `alt=""` so screen readers skip them. Never omit the attribute (readers may announce the file name).
- **Icon-only buttons** need a name on the button, not on the icon ([`01-aria.md`](./01-aria.md)).
- Complex images (charts) need a longer text alternative nearby, or a data table.
- **Video** needs captions; **audio** needs a transcript. Don't autoplay with sound.
- Don't put text in images; if you must, repeat it in `alt`.

---

## 7. Page-level basics

- **Language:** `<html lang="en">` lets screen readers choose the right pronunciation. Mark inline language changes: `<span lang="fr">…</span>`. (The Vite template sets `lang="en"`; keep it correct.)
- **Page title:** every page/route needs a unique, descriptive `<title>` — it's the first thing a screen reader announces. In a single-page app you must update it on navigation ([`../10-routing/03-navigation.md`](../10-routing/03-navigation.md)):

```tsx
useEffect(() => {
  document.title = `${pageName} – My App`;
}, [pageName]);
```

React 19 can also render `<title>` directly in components and hoist it to `<head>`. Check the docs for your version.
- **Zoom and reflow:** don't disable pinch zoom (`user-scalable=no`) ([`../07-styling/03-responsive-design.md`](../07-styling/03-responsive-design.md)).
- **Link text** should make sense out of context: "Read the pricing guide", not "Click here" or repeated "Read more".

---

## 8. React-specific notes

- **Fragments (`<>…</>`) add no DOM**, so they're safe for structure — and wrapper `<div>`s inside `<ul>`, `<tr>`, or `<dl>` break the semantics. Use a fragment (with a key where needed).
- **Component names aren't elements.** `<Button>` may render a `<div>`; check what actually reaches the DOM in DevTools (Elements panel and the Accessibility tab).
- **`div` soup in design-system components** is a common source of lost semantics; make base components render the correct element by default, and allow an `as` prop only with care ([`../05-component-design/01-composition-patterns.md`](../05-component-design/01-composition-patterns.md)).
- **Client-side routing breaks default browser behavior**: no page load means no focus reset and no screen reader announcement. Handle it yourself ([`02-keyboard-and-focus-management.md`](./02-keyboard-and-focus-management.md)).
- **Conditional rendering changes the accessibility tree.** Content removed from the DOM disappears for assistive tech; content hidden with `display: none` or `hidden` is also removed from the tree, while content hidden only visually (`sr-only`, opacity) remains available.

---

## When native elements aren't enough

Some widgets have no native equivalent (tabs, comboboxes, menus with arrow-key navigation, tree views). Then you need ARIA roles plus keyboard handling and focus management — or, better, an accessible primitive library such as Radix, React Aria, or Headless UI ([`../09-ui-components/00-shadcn-ui.md`](../09-ui-components/00-shadcn-ui.md)). See [`01-aria.md`](./01-aria.md).

---

## Common mistakes

- **Clickable `div`/`span` instead of `<button>`/`<a>`** — no keyboard, role, or focus.
- **Links used as buttons, and buttons as links** — wrong semantics and lost browser behavior.
- **Skipping heading levels or choosing headings by font size.**
- **Missing `<main>`**, or several `<main>` elements; multiple same-type landmarks without distinct names.
- **Inputs without labels**, or placeholder-only "labels".
- **Missing `alt`**, or noisy `alt` text ("image of…"); decorative images without `alt=""`.
- **Layout tables**, and tables without headers or captions.
- **Not updating `document.title`** on route changes.
- **Wrapper `div`s inside lists and tables**, breaking required structure.
- **Forgetting `type="button"`** on non-submit buttons inside forms.
- **Vague link text** ("click here", "read more") repeated many times.
- **Wrong or missing `lang`** on the document.

## Quick summary

- Semantic elements give role, name, keyboard behavior, and focus for free; prefer them to `div` plus ARIA
- Links navigate, buttons act; set `type="button"` when not submitting
- One `<h1>`, ordered headings, chosen for structure; landmarks (`header`, `nav`, `main`, `footer`) with unique names when repeated
- Use real lists, tables with `<th scope>` and `<caption>`, and native `<details>`/`<dialog>` when they fit
- Label every input (`htmlFor`/`useId`); group with `fieldset`/`legend`; right `type` and `autoComplete`
- `alt` on every image (empty for decoration); correct `lang`; unique page `<title>` updated on navigation
- Reach for ARIA or a primitive library only when no native element does the job

## Next

**[`01-aria.md`](./01-aria.md)** covers ARIA — what it adds, what it doesn't, and the rules that keep it from doing more harm than good.
