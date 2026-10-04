# Screen Readers

A screen reader converts the interface into speech or braille. Blind and low-vision users rely on it, and so do many people with cognitive or learning disabilities. To build for them, you need a rough mental model of how they work, a few techniques for announcing dynamic changes, and the habit of actually **testing with one**. This file covers all three.

## Prerequisites

[`00-semantic-html.md`](./00-semantic-html.md), [`01-aria.md`](./01-aria.md), and [`02-keyboard-and-focus-management.md`](./02-keyboard-and-focus-management.md)

---

## 1. How screen readers work

A screen reader doesn't read the screen; it reads the **accessibility tree** built from your DOM ([`00-semantic-html.md`](./00-semantic-html.md)). For each node it announces the **name**, **role**, and **state**, for example: "Save, button", "Email, edit text, required, invalid entry", "Main menu, navigation landmark".

Users navigate in two broad ways:

| Mode | How | Used for |
|------|-----|----------|
| **Browse / reading mode** | Arrow keys read line by line; shortcut keys jump by **headings, landmarks, links, form fields, tables, lists, buttons** | Reading content and scanning a page |
| **Focus / forms mode** | Keys go directly to the page (typing in fields, widget keyboard handling) | Interacting with inputs and widgets |

Consequences for you:

- **Headings and landmarks are navigation.** Many users jump by heading or landmark first, so a meaningful outline matters ([`00-semantic-html.md`](./00-semantic-html.md)).
- **Links and buttons are listed out of context**, so "Read more" ×10 is useless; make names self-explanatory.
- **Reading order = DOM order**, regardless of visual layout. CSS can't reorder what the reader says.
- **Not everything is announced when it changes.** Updating text in place usually produces silence unless you use a live region or move focus.
- **Custom widgets can switch the reader into focus mode** (via roles like `application`, `textbox`, `combobox`), changing how keys behave; use correct roles and don't invent behavior.

### Common screen reader and browser pairings

| Screen reader | Platform | Typically paired with | Cost |
|---------------|----------|-----------------------|------|
| **NVDA** | Windows | Firefox, Chrome | Free |
| **JAWS** | Windows | Chrome, Edge | Paid |
| **VoiceOver** | macOS, iOS | Safari | Built in |
| **TalkBack** | Android | Chrome | Built in |
| **Narrator** | Windows | Edge | Built in |

Behavior differs across pairings. Don't assume that passing in one means passing in all; test at least one desktop and one mobile pairing for important flows.

---

## 2. Visually hidden text

Sometimes you need text for screen readers that sighted users don't need to see (a label on an icon button, extra context for a link). Don't use `display: none` or `visibility: hidden` — those hide content from screen readers too. Use a **visually hidden** (a.k.a. `sr-only`) technique:

```css
.sr-only {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip-path: inset(50%);
  white-space: nowrap;
  border: 0;
}
```

```tsx
<button type="button">
  <TrashIcon aria-hidden="true" />
  <span className="sr-only">Delete invoice 1042</span>
</button>
```

Tailwind ships this as `sr-only` ([`../07-styling/02-tailwindcss.md`](../07-styling/02-tailwindcss.md)). Use it sparingly: first ask whether the information should simply be **visible** for everyone.

---

## 3. Live regions: announcing changes

When content changes **without focus moving** — a "Saved" message, a search result count, a form error, a cart total — screen reader users won't know unless you tell them. A **live region** is an element whose changes are announced automatically.

### The three ways

| Technique | Behavior |
|-----------|----------|
| `role="status"` (implicit `aria-live="polite"`) | Announces **when the user is idle**; for non-urgent updates ("3 results", "Saved") |
| `role="alert"` (implicit `aria-live="assertive"`) | Announces **immediately**, interrupting; for urgent errors only |
| `aria-live="polite" \| "assertive"` | Generic live region when no role fits |

```tsx
function SearchResults({ count }: { count: number }) {
  return (
    <>
      <p role="status">{count} results found</p>
      {/* results list */}
    </>
  );
}
```

### The rule that causes most bugs

> **A live region must exist in the DOM *before* its content changes.**

Screen readers watch registered regions. If you **insert** a brand-new `<div role="status">Saved</div>` already containing its text, many readers won't announce it. Instead, render the **empty container once** and then change its text:

```tsx
// ✅ Container always rendered; only the message changes
<div role="status" aria-live="polite">
  {message}   {/* "" → "Profile saved" */}
</div>

// ❌ Region is created together with its text — often silent
{message && <div role="status">{message}</div>}
```

(`role="alert"` is more forgiving: many readers announce alerts that are inserted dynamically. But don't rely on that for non-urgent messages.)

### A reusable announcer

An app-wide announcer avoids scattering regions around:

```tsx
const AnnouncerContext = createContext<(message: string) => void>(() => {});

export function AnnouncerProvider({ children }: { children: ReactNode }) {
  const [message, setMessage] = useState("");

  const announce = useCallback((text: string) => {
    setMessage("");                                      // reset so repeats are announced
    setTimeout(() => setMessage(text), 50);
  }, []);

  return (
    <AnnouncerContext value={announce}>
      {children}
      <div role="status" aria-live="polite" className="sr-only">{message}</div>
    </AnnouncerContext>
  );
}

export const useAnnounce = () => useContext(AnnouncerContext);
```

```tsx
const announce = useAnnounce();
async function save() {
  await api.save(data);
  announce("Changes saved");
}
```

Resetting then re-setting the text makes identical consecutive messages announce again. Libraries and toast components often include this behavior ([`../09-ui-components/08-toasts-and-notifications.md`](../09-ui-components/08-toasts-and-notifications.md)).

### Guidelines for live regions

- **Be concise.** Announce the *outcome* ("Item added to cart, 3 items"), not a paragraph.
- **Use `polite` by default**; reserve `assertive`/`alert` for errors that must interrupt.
- **Don't announce everything.** Constant announcements (every keystroke, every tick of a timer) are noise. Throttle them.
- **Don't put large, frequently changing content in a live region.**
- Don't also move focus **and** announce the same thing; the reader will say it twice.
- `aria-atomic="true"` makes the whole region be read on each change (instead of only the changed part); `aria-relevant` rarely needs touching.
- Respect **timing**: auto-dismissing toasts vanish before some users hear them; keep important messages around or allow them to be reviewed.

---

## 4. Loading and busy states

```tsx
<section aria-busy={isLoading} aria-labelledby="orders-title">
  <h2 id="orders-title">Orders</h2>
  {isLoading ? <Spinner /> : <OrderList orders={orders} />}
</section>

{isLoading && <p role="status">Loading orders…</p>}
```

- Give spinners and skeletons a **text equivalent**: a visually hidden "Loading…" in a status region, not just an animated graphic.
- Announce when loading finishes if the content appears elsewhere on the page ("12 orders loaded").
- `aria-busy` support varies; treat it as a hint, not a guarantee. Don't use `aria-busy` to hide content permanently.
- For progress bars use `<progress>` or `role="progressbar"` with `aria-valuenow`/`aria-valuemin`/`aria-valuemax` ([`../06-forms/06-file-upload.md`](../06-forms/06-file-upload.md)).

---

## 5. Announcing route changes in SPAs

A client-side route change produces no page load, so nothing is announced. Combine:

1. **Update `document.title`.**
2. **Move focus** to the new page's `<h1>` (or `<main>`), which makes the screen reader read it ([`02-keyboard-and-focus-management.md`](./02-keyboard-and-focus-management.md)).
3. Or, as a fallback, announce "Navigated to Settings" in a polite live region.

Choose **one** of focus movement or announcement per navigation to avoid duplicates. See also [`../10-routing/03-navigation.md`](../10-routing/03-navigation.md).

---

## 6. Content that reads well

- **Meaningful text alternatives** for images ([`00-semantic-html.md`](./00-semantic-html.md)); decorative graphics use `alt=""` or `aria-hidden`.
- **Icons**: give the control a name; hide the icon.
- **Link and button text** makes sense alone; add context if needed with visually hidden text or `aria-label` that **includes** the visible text ("Read more about pricing").
- **Avoid character-based decoration** read aloud: "★★★★☆" may read "black star black star…"; provide "4 out of 5 stars" instead. Same for emoji-heavy labels and ASCII art.
- **Abbreviations and numbers**: spell out or use `<abbr>`; format dates and numbers in a readable way (`<time dateTime>`).
- **Tables**: headers and captions ([`00-semantic-html.md`](./00-semantic-html.md)); don't use tables for layout.
- **Lists**: real lists announce their length.
- **Set `lang`** on the page and on inline foreign-language phrases.
- **Plain language and clear headings** help every reader, including screen reader users scanning the page.
- **Don't rely on position or color alone** ("the button on the right", "the red field"): describe by name.

---

## 7. Testing with a screen reader

Automated tools can't tell you whether the experience *makes sense*. Spend 15–30 minutes with a screen reader on your key flows.

### Setup

- **macOS VoiceOver (with Safari):** `Cmd+F5` toggles it. The modifier ("VO") is `Ctrl+Option`. Use the **Rotor** (`VO+U`) to list headings, landmarks, links, and form controls.
- **NVDA (Windows, free; with Firefox or Chrome):** `Insert` or `Caps Lock` is the NVDA key; `H` jumps by heading, `D` by landmark, `F` by form field, `K` by link, `B` by button; the **Elements list** (`NVDA+F7`) lists them.
- **iOS VoiceOver / Android TalkBack** for mobile flows: swipe to move, double-tap to activate.

Tip: also turn on the **speech viewer** (NVDA) or **caption panel** (VoiceOver) to read what's being said while you test.

### What to check

1. **Page load:** is the title announced and sensible?
2. **Navigation by headings, landmarks, links, and form fields:** is the outline logical, and does each item make sense alone?
3. **Every control:** does it announce a clear name, role, and state (checked, expanded, invalid)?
4. **Forms:** are labels, hints, and required status announced? Do errors get announced, and does focus move sensibly on failure?
5. **Dynamic updates:** are results, errors, and confirmations announced exactly once?
6. **Overlays and menus:** focus enters, stays, and returns; the background is unreachable; Escape works.
7. **Route changes:** announced or focused?
8. **Images, icons, charts, and tables:** meaningful or silent as intended?
9. **Close your eyes** (or turn off the display) and try to complete the task.

### Tips

- Test **the real task**, not a random walk: sign up, search, check out.
- Learn a handful of commands; you don't need to be an expert user, just competent enough to find problems. Better still, watch or test with **actual screen reader users** through usability testing.
- Behavior differs across screen reader + browser combinations — a quirk in one isn't always your bug, but a failure across several is.
- Use the **browser DevTools accessibility panel** to see the computed name, role, and states when something sounds wrong ([`01-aria.md`](./01-aria.md)).
- Don't treat "it reads something" as success; the output must be **accurate, concise, and in a sensible order**.

---

## Common mistakes

- **Using `display: none` for screen-reader-only text** — hides it from everyone; use a visually-hidden class.
- **Inserting a live region together with its message** — often silent; render the container first, then update it.
- **`role="alert"`/`aria-live="assertive"` for everything** — constant interruptions; use `polite`/`status` by default.
- **Announcing too much** (every keystroke or timer tick) — noise that users learn to ignore.
- **No text equivalent for spinners, icons, or charts.**
- **Moving focus *and* announcing the same message** — read twice.
- **Toasts that disappear in seconds** — missed by slower readers and not reviewable.
- **Vague links and buttons** ("click here", "more") that fail out of context.
- **Decorative characters and emoji read aloud** as noise.
- **Relying on visual order or color words** ("see the blue button on the right").
- **Never testing with a real screen reader** — automated checks catch only a fraction of issues.
- **Assuming one screen reader/browser combination represents all.**

## Quick summary

- Screen readers read the accessibility tree: name, role, state, in DOM order; users navigate by headings, landmarks, links, and form fields
- Hide visually, not from assistive tech, with an `sr-only` class; hide decoratives with `aria-hidden` or `alt=""`
- Live regions announce changes without moving focus: `role="status"` (polite) by default, `role="alert"` (assertive) for urgent errors
- Render the live region container **before** changing its text; keep announcements short and infrequent; avoid doubles with focus changes
- Give loading states a text alternative and announce completion; handle SPA route changes with focus and title
- Test with NVDA or VoiceOver on real flows, and with real users when possible

## Next

**[`04-accessibility-checklist.md`](./04-accessibility-checklist.md)** pulls everything together into a practical WCAG-based checklist, with the tooling to automate what can be automated.
