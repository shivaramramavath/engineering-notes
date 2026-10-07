# Fonts

Web fonts commonly cause two problems: privacy/performance costs from third-party requests, and layout shift when the font swaps in. `next/font` fixes both: it downloads fonts at build time, **self-hosts** them with your other static assets, and adjusts a fallback font's metrics so text barely moves when the real font loads.

> Verified against the Next.js 16.4 docs (Font Optimization guide and `next/font` API reference).

## What you get

- **No requests to Google** from the visitor's browser. CSS and font files are fetched at build time and served from your own domain.
- **No layout shift**: an automatic fallback font is sized to match the real one (`adjustFontFallback`).
- **Subsetting and preloading** per route.
- **Works for any font file**, via `next/font/local`.
- No package to install; it ships with Next.js.

## Google fonts

```tsx
// app/layout.tsx
import { Inter } from "next/font/google";

const inter = Inter({
  subsets: ["latin"],
  display: "swap",
});

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" className={inter.className}>
      <body>{children}</body>
    </html>
  );
}
```

Rules:

- **Call the font function at module scope** (top level of the file), not inside a component.
- **Prefer variable fonts.** If the font is variable you do not specify `weight`. If it is **not** variable, `weight` is required.
- **Names with spaces use underscores**: `Roboto Mono` is imported as `Roboto_Mono`.
- **Specify `subsets`** (for example `["latin"]`). Without it, with preload on, you get a warning.
- Fonts are **scoped to where you apply them**. Put it on the root layout's `<html>` to apply app-wide.

Non-variable font with several weights and styles:

```tsx
const roboto = Roboto({
  weight: ["400", "700"],
  style: ["normal", "italic"],
  subsets: ["latin"],
  display: "swap",
});
```

## Local fonts

```tsx
import localFont from "next/font/local";

const brand = localFont({
  src: "./fonts/Brand.woff2",   // relative to THIS file
  display: "swap",
});
```

Several files for one family:

```tsx
const roboto = localFont({
  src: [
    { path: "./Roboto-Regular.woff2",    weight: "400", style: "normal" },
    { path: "./Roboto-Italic.woff2",     weight: "400", style: "italic" },
    { path: "./Roboto-Bold.woff2",       weight: "700", style: "normal" },
    { path: "./Roboto-BoldItalic.woff2", weight: "700", style: "italic" },
  ],
});
```

`src` paths resolve relative to the file where you call `localFont`. Files can live anywhere in the project (colocated in `app/`, in `public/`, or a `styles/` folder). Use `.woff2` where possible; it is the smallest.

## Options

| Option | Applies to | Notes |
|---|---|---|
| `src` | local | Required. String or array of `{ path, weight?, style? }` |
| `weight` | both | Required unless the font is variable. String, range (`"100 900"`), or array (Google only) |
| `style` | both | `"normal"`, `"italic"`; array allowed for Google |
| `subsets` | google | Subsets to preload, such as `["latin"]` |
| `axes` | google | Extra variable axes beyond weight (for example `["slnt"]`) |
| `display` | both | `font-display`; default `"swap"` |
| `preload` | both | Default `true` |
| `fallback` | both | Fallback stack, for example `["system-ui", "arial"]` |
| `adjustFontFallback` | both | Google: boolean (default `true`). Local: `"Arial"` (default), `"Times New Roman"`, or `false` |
| `variable` | both | Name of a CSS variable to declare |
| `declarations` | local | Extra `@font-face` descriptors |

`display: "swap"` shows fallback text immediately, then swaps; with the metric-adjusted fallback the swap is nearly invisible.

## Three ways to apply a font

```tsx
<p className={inter.className}>Hello</p>        {/* class that sets font-family */}
<p style={inter.style}>Hello</p>                {/* inline style object with fontFamily */}
<main className={inter.variable}>…</main>        {/* declares the CSS variable; use var(--font-inter) */}
```

`className` and `style` are read-only values returned by the font loader.

## Multiple fonts

Keep fonts conservative; each is another download.

### With CSS variables (best with Tailwind)

```tsx
// app/layout.tsx
import { Inter, Roboto_Mono } from "next/font/google";

const inter = Inter({ subsets: ["latin"], variable: "--font-inter", display: "swap" });
const mono = Roboto_Mono({ subsets: ["latin"], variable: "--font-roboto-mono", display: "swap" });

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" className={`${inter.variable} ${mono.variable} antialiased`}>
      <body>{children}</body>
    </html>
  );
}
```

```css
/* app/globals.css  (Tailwind v4) */
@import "tailwindcss";

@theme inline {
  --font-sans: var(--font-inter);
  --font-mono: var(--font-roboto-mono);
}
```

Now `font-sans` and `font-mono` utilities use your fonts. `@theme inline` is needed because the value is another variable that only exists at runtime.

In **Tailwind v3**, map them in `tailwind.config.js`:

```js
theme: { extend: { fontFamily: { sans: ["var(--font-inter)"], mono: ["var(--font-roboto-mono)"] } } }
```

Without Tailwind, use the variable in plain CSS: `h1 { font-family: var(--font-roboto-mono); }`.

### With a shared fonts file

Every call to a font function creates **one hosted instance**. If you need the same font in several places, define it once and import the object:

```ts
// app/fonts.ts
import { Inter, Roboto_Mono } from "next/font/google";

export const inter = Inter({ subsets: ["latin"], display: "swap" });
export const robotoMono = Roboto_Mono({ subsets: ["latin"], display: "swap" });
```

```tsx
import { robotoMono } from "@/app/fonts";

<h1 className={robotoMono.className}>Title</h1>
```

Calling `Inter()` again in another file loads a second copy. Define once, import everywhere.

## Preloading behavior

A font is preloaded only on routes where it is used, according to where you call and apply it:

| Where the font is used | Preloaded on |
|---|---|
| A single page | That page's route |
| A layout | All routes wrapped by that layout |
| The root layout | All routes |

So a decorative font used on one marketing page does not slow the rest of the app. `preload: false` turns this off for a font that is not needed above the fold. The option applies to **every file** of that font call (they share one `@font-face` group).

## Icon fonts and third-party CSS

`next/font` is for text fonts. For icons, prefer inline SVG or an icon library of SVG components over an icon font; they are smaller and need no extra request.

## Performance guidance

- Use **one or two families**. Each weight/style you add is extra bytes.
- Prefer **variable fonts**: one file covers many weights.
- Limit **subsets** to what you need.
- Use `display: "swap"` (default) unless you have a reason to hide text.
- Keep `adjustFontFallback` on to stay near zero layout shift.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| Build error: "font function must be called at module scope" | Called inside a component | Move the call to the top level of the file |
| Warning about missing subsets | `subsets` omitted | Add `subsets: ["latin"]` |
| "Missing weight" error | Non-variable font without `weight` | Provide `weight`, or use a variable font |
| Tailwind `font-sans` does not change | Variable not mapped, or class not applied | Apply `font.variable` to `<html>`; add `@theme inline` mapping |
| Font does not load in one section | Applied class not on an ancestor of that content | Put `className`/`variable` on a parent element |
| Local font 404 or build error | Wrong relative `src` path | Path is relative to the file calling `localFont` |
| Fonts duplicated in the network panel | Font function called in several files | Use a shared fonts file |
| Layout shift on font swap | `adjustFontFallback` turned off, or no `fallback` | Leave defaults on |
| Font works in dev only | Network access to Google blocked at build time | Allow the build to fetch fonts, or self-host with `next/font/local` |

## Common mistakes

| Mistake | Fix |
|---|---|
| Linking Google Fonts with a `<link>` tag | Use `next/font/google` |
| Instantiating the same font in many files | One definition, import it |
| Loading many weights of a non-variable font | Use a variable font or trim weights |
| Forgetting `@theme inline` for font variables (Tailwind v4) | Add the mapping |
| Applying the font only to a component, expecting it app-wide | Put it on the root layout `<html>` |
| Importing multiple-word fonts with spaces | Use underscores (`Roboto_Mono`) |
| Using an icon font for a few icons | Use SVG |

## Quick Summary

- `next/font` self-hosts fonts at build time, removes third-party requests and reduces layout shift.
- Call the font function at module scope; set `subsets`; omit `weight` only for variable fonts.
- Apply with `className`, `style`, or a CSS `variable` (best with Tailwind via `@theme inline`).
- Define each font once in a shared file and import it.
- Preloading follows where the font is used; the root layout means every route.

## Next

- [Static Assets](./06-static-assets.md)
- [Tailwind CSS](./01-tailwindcss.md)
- [Images](./04-images.md)
