# 09 · Styling and Assets

How your app looks and how its visual assets are delivered: CSS, Tailwind, shadcn/ui components, theming, images, fonts and static files.

> Verified against the Next.js 16.4 documentation. Tailwind notes cover v4 (current); shadcn/ui and `next-themes` details follow their own docs and can change between versions.

## The default stack

```text
Tailwind CSS (utilities)  +  CSS variables (tokens)  +  shadcn/ui (components you own)
        │                          │
   CSS Modules when needed    next-themes for light/dark
        │
next/image  +  next/font  +  public/ and imported assets
```

You do not need all of it. A reasonable minimum is Tailwind, `next/image` and `next/font`; add shadcn/ui and theming when the product needs a component system and dark mode.

## Reading order

| # | Note | Covers |
|---|---|---|
| 00 | [CSS](./00-css.md) | CSS Modules, global CSS, ordering, dev vs production, CSS-in-JS caveats |
| 01 | [Tailwind CSS](./01-tailwindcss.md) | v4 setup, `@theme`, dynamic class rules, `cn()`, v3 differences |
| 02 | [shadcn/ui](./02-shadcn-ui.md) | Copy-in components, `init` / `add`, `components.json`, customizing |
| 03 | [Theming](./03-theming.md) | Design tokens, dark mode, `next-themes`, avoiding the flash |
| 04 | [Images](./04-images.md) | `next/image`, remote patterns, `sizes`, `fill`, `preload`, formats |
| 05 | [Fonts](./05-fonts.md) | `next/font`, variable fonts, Tailwind variables, shared definitions |
| 06 | [Static Assets](./06-static-assets.md) | `public/`, caching, imports vs `public/`, metadata files |

## The rules to remember

1. **Tailwind for components, CSS Modules for the rest, one global file** imported in the root layout.
2. **Never assemble Tailwind class names from fragments**; write complete class names.
3. **Use semantic tokens, not raw colors,** so theming is a variable swap.
4. **`<Image>` needs dimensions (or `fill` in a sized parent) and an accurate `sizes`.** Only the LCP image is eager.
5. **Self-host fonts with `next/font`;** define each font once.
6. **Import images your components use;** keep `public/` for files that need stable URLs. Never use it for uploads.
7. **Verify CSS ordering with a production build.**

## Next

[10 · SEO and Metadata](../10-seo-and-metadata/README.md): the Metadata API, Open Graph images, sitemaps, `robots` and structured data.
