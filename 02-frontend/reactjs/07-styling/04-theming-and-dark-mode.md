# Theming and Dark Mode

A **theme** is a set of design decisions — colors, spacing, typography, radii — expressed as **tokens** that components reference instead of hard-coded values. Swap the token values and the whole UI changes. Dark mode is the most common theme switch, and it's easy to get *almost* right: the hard parts are respecting the system preference, remembering the user's choice, and avoiding a flash of the wrong theme on page load. This file covers all of it.

## Prerequisites

[`00-css-essentials.md`](./00-css-essentials.md) (custom properties), [`02-tailwindcss.md`](./02-tailwindcss.md) (if you use Tailwind), [`../03-hooks/05-useContext.md`](../03-hooks/05-useContext.md), and [`../03-hooks/02-useEffect.md`](../03-hooks/02-useEffect.md)

---

## 1. Design tokens

A **design token** is a named design decision (`--color-surface`, `--space-4`, `--radius-md`) rather than a raw value (`#ffffff`, `16px`). Components use tokens; themes define them.

### Two layers

| Layer | Example | Purpose |
|-------|---------|---------|
| **Primitive** (raw palette) | `--blue-600: #2563eb`, `--gray-900: #111827` | The available values; never used directly in components |
| **Semantic** (role-based) | `--color-primary`, `--color-surface`, `--color-text-muted`, `--color-border` | What the value is *for*; components reference these |

```css
:root {
  /* primitives */
  --gray-50: #f9fafb;
  --gray-900: #111827;
  --blue-600: #2563eb;
  --blue-400: #60a5fa;

  /* semantic (light) */
  --background: var(--gray-50);
  --foreground: var(--gray-900);
  --primary: var(--blue-600);
  --primary-foreground: #ffffff;
  --border: #e5e7eb;
  --muted-foreground: #6b7280;
}
```

**Semantic names are what make theming work**: `--background` can be light in one theme and dark in another, while a component that uses `background: var(--background)` never changes. Don't name tokens after colors (`--white`), because in a dark theme `--white` is no longer white.

Token categories to define: color, spacing, font families and sizes, radii, shadows, z-index scale, durations.

---

## 2. Switching themes with CSS variables

Override the semantic tokens under a selector that identifies the theme — an attribute or class on the `<html>` element:

```css
:root {
  color-scheme: light;
  --background: #ffffff;
  --foreground: #111827;
  --border: #e5e7eb;
  --primary: #2563eb;
}

:root[data-theme="dark"] {
  color-scheme: dark;
  --background: #0b1220;
  --foreground: #e5e7eb;
  --border: #1f2937;
  --primary: #60a5fa;
}

body {
  background: var(--background);
  color: var(--foreground);
}
.card { border: 1px solid var(--border); }
```

- **`color-scheme: dark`** tells the browser to render native controls, scrollbars, and default colors for form elements in dark. Don't forget it.
- Because custom properties cascade, you can also theme a **subtree** (`<section data-theme="dark">`), e.g., a dark footer.
- Components stay theme-agnostic: they reference tokens only.

---

## 3. Light, dark, and system

Users expect three choices: **Light**, **Dark**, **System** (follow the OS). Model it as three stored values and one resolved value:

| Concept | Values |
|---------|--------|
| **Preference** (stored) | `"light"` \| `"dark"` \| `"system"` |
| **Resolved theme** (applied) | `"light"` \| `"dark"` |

```
resolved = preference === "system" ? (prefersDark ? "dark" : "light") : preference
```

Detect the system setting with `matchMedia`:

```ts
const query = window.matchMedia("(prefers-color-scheme: dark)");
const prefersDark = query.matches;
query.addEventListener("change", (e) => { /* update when the OS changes while the app is open */ });
```

---

## 4. Avoiding the flash of the wrong theme

If theme logic runs inside React, the page first renders with the default theme, then React mounts and switches — users with dark mode see a **white flash on every load**. Fix it by applying the theme **before first paint** with a tiny inline script in `index.html`:

```html
<!-- index.html, inside <head>, before the stylesheet and the app script -->
<script>
  (function () {
    try {
      var stored = localStorage.getItem("theme");          // "light" | "dark" | "system" | null
      var pref = stored || "system";
      var dark = pref === "dark" ||
        (pref === "system" && window.matchMedia("(prefers-color-scheme: dark)").matches);
      document.documentElement.dataset.theme = dark ? "dark" : "light";
      document.documentElement.style.colorScheme = dark ? "dark" : "light";
    } catch (e) {}
  })();
</script>
```

It runs synchronously before the body is painted, so the very first frame is already correct. Keep it small, dependency-free, and wrapped in `try/catch` (storage can throw in private modes). With a server-rendering framework, set the theme through a cookie so the server can render the right class, and use the framework's recommended no-flash approach.

If you have a **Content Security Policy**, an inline script needs a nonce or hash ([`../19-production/05-security.md`](../19-production/05-security.md)).

---

## 5. The React side

A provider holds the preference, applies the resolved theme, persists changes, and follows the OS in "system" mode:

```tsx
import { createContext, useContext, useEffect, useMemo, useState, type ReactNode } from "react";

type Preference = "light" | "dark" | "system";
type Resolved = "light" | "dark";

type ThemeContextValue = {
  preference: Preference;
  resolved: Resolved;
  setPreference: (p: Preference) => void;
};

const ThemeContext = createContext<ThemeContextValue | null>(null);
const STORAGE_KEY = "theme";

function getSystemTheme(): Resolved {
  return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
}

function readPreference(): Preference {
  try {
    const stored = localStorage.getItem(STORAGE_KEY);
    return stored === "light" || stored === "dark" || stored === "system" ? stored : "system";
  } catch {
    return "system";
  }
}

export function ThemeProvider({ children }: { children: ReactNode }) {
  const [preference, setPreferenceState] = useState<Preference>(readPreference);
  const [system, setSystem] = useState<Resolved>(getSystemTheme);

  // Follow OS changes
  useEffect(() => {
    const query = window.matchMedia("(prefers-color-scheme: dark)");
    const onChange = () => setSystem(query.matches ? "dark" : "light");
    query.addEventListener("change", onChange);
    return () => query.removeEventListener("change", onChange);
  }, []);

  const resolved: Resolved = preference === "system" ? system : preference;   // derived, not state

  // Apply to the document
  useEffect(() => {
    document.documentElement.dataset.theme = resolved;
    document.documentElement.style.colorScheme = resolved;
  }, [resolved]);

  const value = useMemo<ThemeContextValue>(
    () => ({
      preference,
      resolved,
      setPreference: (p) => {
        setPreferenceState(p);
        try { localStorage.setItem(STORAGE_KEY, p); } catch { /* ignore */ }
      },
    }),
    [preference, resolved]
  );

  return <ThemeContext value={value}>{children}</ThemeContext>;
}

export function useTheme(): ThemeContextValue {
  const ctx = useContext(ThemeContext);
  if (!ctx) throw new Error("useTheme must be used within a ThemeProvider");
  return ctx;
}
```

The toggle:

```tsx
function ThemeToggle() {
  const { preference, setPreference } = useTheme();
  return (
    <fieldset>
      <legend className="sr-only">Theme</legend>
      {(["light", "dark", "system"] as const).map((p) => (
        <label key={p}>
          <input
            type="radio"
            name="theme"
            value={p}
            checked={preference === p}
            onChange={() => setPreference(p)}
          />
          {p}
        </label>
      ))}
    </fieldset>
  );
}
```

Design notes:

- **`resolved` is derived** from `preference` and `system`; it isn't stored ([`../02-state-and-rendering/02-state-structure-and-lifting.md`](../02-state-and-rendering/02-state-structure-and-lifting.md)).
- The effect applying the attribute **synchronizes React state with an external system (the DOM)**, which is a legitimate effect ([`../03-hooks/02-useEffect.md`](../03-hooks/02-useEffect.md)).
- The inline script and the provider must **agree** on storage key and attribute name.
- Memoize the context value so consumers don't re-render needlessly ([`../13-state-management/01-context-patterns-and-performance.md`](../13-state-management/01-context-patterns-and-performance.md)).
- A mature library (`next-themes` works outside Next.js too) handles edge cases such as cross-tab sync; the code above shows what it does.
- Prefer a plain accessible control (a radio group, or a `<select>`). A single icon button that cycles three states needs a clear accessible name and current state.

---

## 6. Tailwind and dark mode

Tailwind's `dark:` variant applies utilities when dark mode is active. By default it follows the **system** preference (`prefers-color-scheme`). To drive it from **your toggle** (the `data-theme` attribute above, or a `.dark` class), configure the variant to match your selector. In Tailwind v4 this is done in CSS:

```css
@import "tailwindcss";

@custom-variant dark (&:where([data-theme=dark], [data-theme=dark] *));
```

(In v3, set `darkMode: "class"` or `"selector"` in `tailwind.config.js`. Check the docs for the exact option in your version.)

Two ways to use it:

**1. Explicit `dark:` utilities** — fine for small projects:

```tsx
<div className="bg-white text-gray-900 dark:bg-gray-900 dark:text-gray-100">…</div>
```

**2. Semantic tokens mapped to Tailwind** — scales much better, and is what shadcn/ui does. Define the theme once in CSS, then components use a single class and don't need `dark:` at all:

```css
:root {
  --background: oklch(1 0 0);
  --foreground: oklch(0.145 0 0);
  --primary: oklch(0.205 0 0);
  --border: oklch(0.922 0 0);
}
[data-theme="dark"] {
  --background: oklch(0.145 0 0);
  --foreground: oklch(0.985 0 0);
  --primary: oklch(0.922 0 0);
  --border: oklch(1 0 0 / 10%);
}

@theme inline {
  --color-background: var(--background);
  --color-foreground: var(--foreground);
  --color-primary: var(--primary);
  --color-border: var(--border);
}
```

```tsx
<div className="bg-background text-foreground border border-border">…</div>   {/* adapts automatically */}
```

Prefer approach 2 for apps: components don't carry theme logic, adding a theme means editing one CSS block, and design tokens stay the single source of truth. See [`../09-ui-components/00-shadcn-ui.md`](../09-ui-components/00-shadcn-ui.md) and [`../09-ui-components/09-design-system.md`](../09-ui-components/09-design-system.md). (The `oklch()` color format is supported in modern browsers; hex or `hsl()` also work.)

---

## 7. Designing a good dark theme

Dark mode isn't an inverted light mode:

- **Don't use pure black (`#000`) on pure white text** — harsh and causes halation; use dark grays and slightly off-white text.
- **Elevation is lighter, not shadowed.** In light themes depth comes from shadows; in dark themes, raised surfaces are slightly *lighter*.
- **Reduce saturation** of bright brand colors; they vibrate on dark backgrounds.
- **Verify contrast** for text and UI components in **both** themes: at least 4.5:1 for normal text, 3:1 for large text and UI boundaries ([`../08-accessibility/04-accessibility-checklist.md`](../08-accessibility/04-accessibility-checklist.md)). Check the *muted* text and *disabled* states too; they are the usual failures.
- **Images and illustrations**: dim bright images slightly, provide transparent or dark variants of logos, and make SVG icons use `currentColor`.
- **Shadows and borders** often need different values per theme.
- **Charts and status colors** must stay distinguishable in both themes and not rely on color alone.

---

## 8. Other theme dimensions

The same mechanism handles more than dark mode:

- **Brand or white-label themes** (`data-brand="acme"`).
- **Density** (compact vs comfortable spacing) via spacing tokens.
- **High-contrast mode** (`@media (prefers-contrast: more)` or a user setting).
- **Per-section themes** (a dark hero on a light page).
- **Forced colors** (Windows High Contrast): test with `@media (forced-colors: active)`; avoid relying on background images for meaning.

Switching a theme should only change **token values**, never component code.

---

## 9. Transitions and performance

- Don't animate every property when switching themes: `* { transition: all 0.3s }` causes jank and flashes on load. If you want a fade, add a transition class **after** mount, only for background and color, and respect `prefers-reduced-motion`.
- Changing a few custom properties on `<html>` is cheap; the browser recalculates styles once.
- Avoid theme toggles that re-render the entire React tree through context when CSS variables suffice: the CSS already changed. Only components that need the *value* in JavaScript (a chart library's colors, `resolved` for an `<img>` source) should read `useTheme`.

---

## 10. Testing

- Verify the **no-flash** behavior with throttled CPU and a hard reload in each preference.
- Test **system mode** by toggling the OS or emulating `prefers-color-scheme` in DevTools (Rendering → Emulate CSS media feature).
- Test with storage disabled or blocked; the app must fall back to `"system"`.
- Check contrast in both themes with an automated tool plus manual review.
- Snapshot or visual tests per theme catch token regressions ([`../18-testing-and-debugging/05-integration-testing.md`](../18-testing-and-debugging/05-integration-testing.md)).

---

## Common mistakes

- **Applying the theme in a React effect only** — causes a flash of the wrong theme on every load; use the inline head script.
- **Naming tokens after colors** (`--white`) instead of roles (`--background`).
- **Forgetting `color-scheme`** — native inputs, scrollbars, and autofill stay light in dark mode.
- **Hard-coding colors in components** — they won't respond to the theme.
- **Storing the resolved theme instead of the preference** — "system" mode can't track OS changes.
- **Not handling blocked storage** (`localStorage` throws) — crash on load in private modes.
- **Assuming Tailwind's `dark:` follows your toggle** — it follows the OS until you configure the variant.
- **Inverting colors mechanically** — poor contrast, harsh whites, oversaturated accents.
- **Not testing contrast in both themes**, especially muted and disabled text.
- **Animating `all` during theme change** — janky and flashes.
- **Putting the theme in React state when CSS variables are enough** — unnecessary re-renders.
- **An inline script that violates your CSP** — add a nonce or hash.

## Quick summary

- Define **semantic design tokens** (roles, not colors) as CSS custom properties; components use only tokens
- A theme is a different set of token values under `[data-theme="dark"]` (or `.dark`), plus `color-scheme`
- Model **preference** (`light | dark | system`) separately from the **resolved** theme; derive the latter
- Prevent the flash with a small inline script in `<head>` that sets the theme before first paint
- The React provider syncs state with the DOM attribute, `localStorage`, and the OS media query
- In Tailwind, configure the `dark` variant to match your selector, and prefer semantic tokens over scattered `dark:` utilities
- Design dark mode deliberately and verify contrast in every theme

## Next

**[`05-styling-approaches-compared.md`](./05-styling-approaches-compared.md)** compares the styling options side by side and gives a decision guide, plus the `cn` helper for merging classes.
