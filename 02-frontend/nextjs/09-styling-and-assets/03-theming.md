# Theming

Theming means one set of components that can look different: light and dark mode, brand colors, radius, density. The robust approach is **semantic CSS variables** (design tokens) that components reference, with a small amount of JavaScript to choose which set is active.

> The Tailwind and shadcn/ui parts follow their current docs; `next-themes` setup follows the shadcn/ui dark-mode guide for Next.js. Names such as token lists vary slightly between versions.

## The model

```text
tokens (CSS variables)   →   Tailwind utilities map to tokens   →   components use utilities
--background, --primary      bg-background, text-primary              <Card className="bg-card">
        │
   swapped by a class on <html>  (.dark)  or by prefers-color-scheme
```

Components never say "white" or "gray-900"; they say `background` and `foreground`. Changing the theme changes the variables, not the components.

## Tokens in CSS

```css
/* app/globals.css */
@import "tailwindcss";

@custom-variant dark (&:where(.dark, .dark *));   /* dark: applies under a .dark class */

:root {
  --background: oklch(1 0 0);
  --foreground: oklch(0.145 0 0);
  --card: oklch(1 0 0);
  --primary: oklch(0.205 0 0);
  --primary-foreground: oklch(0.985 0 0);
  --muted-foreground: oklch(0.556 0 0);
  --border: oklch(0.922 0 0);
  --radius: 0.625rem;
}

.dark {
  --background: oklch(0.145 0 0);
  --foreground: oklch(0.985 0 0);
  --card: oklch(0.205 0 0);
  --primary: oklch(0.922 0 0);
  --primary-foreground: oklch(0.205 0 0);
  --muted-foreground: oklch(0.708 0 0);
  --border: oklch(1 0 0 / 10%);
}

@theme inline {
  --color-background: var(--background);
  --color-foreground: var(--foreground);
  --color-card: var(--card);
  --color-primary: var(--primary);
  --color-primary-foreground: var(--primary-foreground);
  --color-muted-foreground: var(--muted-foreground);
  --color-border: var(--border);
  --radius-lg: var(--radius);
}

@layer base {
  body {
    @apply bg-background text-foreground;
  }
}
```

- `:root` holds light values; `.dark` overrides them.
- `@theme inline` maps each variable to a Tailwind color so `bg-background`, `text-primary` and `border-border` exist and **follow the variable at runtime**. (Without `inline`, Tailwind would copy the value at build time and dark mode would not switch.)
- `@custom-variant dark` makes the `dark:` prefix respond to a `.dark` class. Without it, Tailwind v4's `dark:` follows the OS setting only.
- shadcn/ui generates this structure for you with `init`; the tokens above mirror its naming. Values are examples; pick your own palette.

## Three ways to choose the active theme

| Strategy | How | Pros | Cons |
|---|---|---|---|
| **OS only** | `@media (prefers-color-scheme: dark)` | Zero JavaScript | No user toggle |
| **Class + JavaScript** (`next-themes`) | `.dark` on `<html>`, stored in `localStorage` | System / light / dark toggle, no flash | A small client component |
| **Cookie + server render** | Read the theme cookie in the root layout, set the class on `<html>` | No client script, no flash, server knows the theme | Reading `cookies()` makes the route dynamic; needs a Server Action to change it |

Most apps use `next-themes`.

## Setup with `next-themes`

```bash
npm install next-themes
```

```tsx
// components/theme-provider.tsx
"use client";

import { ThemeProvider as NextThemesProvider } from "next-themes";

export function ThemeProvider({ children, ...props }: React.ComponentProps<typeof NextThemesProvider>) {
  return <NextThemesProvider {...props}>{children}</NextThemesProvider>;
}
```

```tsx
// app/layout.tsx
import { ThemeProvider } from "@/components/theme-provider";
import "./globals.css";

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" suppressHydrationWarning>
      <body>
        <ThemeProvider
          attribute="class"
          defaultTheme="system"
          enableSystem
          disableTransitionOnChange
        >
          {children}
        </ThemeProvider>
      </body>
    </html>
  );
}
```

What each piece does:

- `attribute="class"` toggles the `dark` class on `<html>`, matching the CSS above.
- `defaultTheme="system"` + `enableSystem` follows the OS until the user chooses.
- `disableTransitionOnChange` prevents every element animating its colors during the switch.
- `suppressHydrationWarning` on `<html>` is required: the library sets the class before React hydrates, so the server and client markup differ on that one element on purpose. It applies only to that element, not its children.
- `next-themes` injects a tiny inline script that applies the saved or system theme **before first paint**, which is how it avoids the white flash.

### A toggle

The toggle must be a **Client Component** and must not render theme-dependent output until mounted, because the server does not know the theme:

```tsx
// components/theme-toggle.tsx
"use client";

import { useEffect, useState } from "react";
import { useTheme } from "next-themes";

export function ThemeToggle() {
  const { theme, setTheme } = useTheme();
  const [mounted, setMounted] = useState(false);
  useEffect(() => setMounted(true), []);

  if (!mounted) return <button aria-label="Toggle theme" disabled className="size-9" />; // same size, no theme-dependent content

  return (
    <select aria-label="Theme" value={theme} onChange={(e) => setTheme(e.target.value)}>
      <option value="system">System</option>
      <option value="light">Light</option>
      <option value="dark">Dark</option>
    </select>
  );
}
```

`theme` is the user's *choice* (may be `"system"`); `resolvedTheme` is what is actually applied (`"light"` or `"dark"`). Use `resolvedTheme` when you need to know the displayed mode, for example to pick a logo.

## Theme-aware images and assets

Avoid reading the theme in JavaScript where CSS can do it:

```tsx
<>
  <Image className="dark:hidden" src="/logo-light.svg" alt="Acme" width={120} height={32} />
  <Image className="hidden dark:block" src="/logo-dark.svg" alt="Acme" width={120} height={32} />
</>
```

Both images are in the HTML; CSS shows one. This works in Server Components and avoids a flash. Note that both are requested by the browser unless lazily loaded; for large images use `loading="lazy"` carefully, since the hidden one still may not load until shown.

## Brand themes and other axes

Variables make extra themes cheap. Scope overrides to an attribute or class:

```css
[data-brand="green"] {
  --primary: oklch(0.62 0.17 150);
  --primary-foreground: oklch(0.99 0 0);
}
```

```tsx
<html lang="en" data-brand="green">
```

Combine with `.dark` freely, since each sets different variables. `next-themes` can manage another attribute with multiple `themes` if you need a switcher.

## Respecting the OS and the browser UI

```css
html {
  color-scheme: light dark;   /* scrollbars, form controls, and default colors follow the theme */
}
```

Set `color-scheme` per theme when you control it (`.dark { color-scheme: dark; }`) so native controls match. For the browser toolbar color, use the `viewport` export's `themeColor` with media queries.

## Server-rendered theme (no client library)

For apps that already read cookies:

```tsx
// app/layout.tsx
import { cookies } from "next/headers";

export default async function RootLayout({ children }: { children: React.ReactNode }) {
  const theme = (await cookies()).get("theme")?.value === "dark" ? "dark" : "light";
  return (
    <html lang="en" className={theme}>
      <body>{children}</body>
    </html>
  );
}
```

```ts
// app/actions/theme.ts
"use server";

import { cookies } from "next/headers";

export async function setTheme(theme: "light" | "dark") {
  (await cookies()).set("theme", theme, { path: "/", maxAge: 60 * 60 * 24 * 365, sameSite: "lax" });
}
```

Setting a cookie in a Server Action re-renders the page so the new class applies. The cost: reading `cookies()` in the root layout makes every route request-time unless handled with Suspense under Cache Components. There is also no "system" option without extra client code. For most apps `next-themes` is simpler.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| Flash of the wrong theme | Theme applied after paint | Use `next-themes` (inline script), or the cookie approach |
| Hydration warning on `<html>` | Missing `suppressHydrationWarning` | Add it to `<html>` |
| `dark:` classes do nothing when toggling | Tailwind v4 `dark:` follows the OS by default | Add `@custom-variant dark (...)` |
| Colors do not change in dark mode | Tokens mapped with `@theme` instead of `@theme inline` | Use `@theme inline` for variable references |
| Toggle shows the wrong icon on load | Rendering theme-dependent UI before mount | Render a placeholder until mounted |
| Everything animates when switching | Transitions on colors | `disableTransitionOnChange` |
| Native inputs stay light | `color-scheme` not set | Set `color-scheme` per theme |
| Theme resets on reload | Storage blocked or not configured | Check `localStorage` access and the `storageKey` |

## Common mistakes

| Mistake | Fix |
|---|---|
| Hard-coding `bg-white` / `text-gray-900` in components | Use semantic tokens (`bg-background`, `text-foreground`) |
| Provider placed in a Server Component file without `"use client"` | Wrap `next-themes` in a client component |
| Reading `useTheme()` and rendering immediately | Wait for `mounted`, or use CSS variants |
| Forgetting `suppressHydrationWarning` | Add it to `<html>` only |
| Maintaining parallel light/dark class lists everywhere | Put differences in tokens, not in every component |
| Using `theme` to detect dark mode | Use `resolvedTheme` (since `theme` can be `"system"`) |
| Low contrast in one mode | Check both themes against accessibility contrast guidelines |

## Quick Summary

- Theme with semantic CSS variables; components use `bg-background`-style utilities, never raw colors.
- `:root` holds light values, `.dark` overrides; `@theme inline` maps variables to Tailwind colors; `@custom-variant dark` ties `dark:` to the class.
- `next-themes` toggles the class, remembers the choice, follows the OS, and avoids the flash.
- Put `suppressHydrationWarning` on `<html>`; render theme-dependent UI only after mount (or use CSS).
- A cookie-driven server theme is possible but makes the layout request-time.

## Next

- [Images](./04-images.md)
- [Tailwind CSS](./01-tailwindcss.md)
- [shadcn/ui](./02-shadcn-ui.md)
