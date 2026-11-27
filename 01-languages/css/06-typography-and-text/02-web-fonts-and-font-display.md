# Web Fonts & Font Display

| Metadata | Details |
| :--- | :--- |
| **Topic** | `@font-face` Rule, Font File Formats (WOFF2/WOFF), `font-display` Policies, FOIT vs FOUT, Preloading & Unicode Ranges |
| **Spec Level** | CSS Fonts Module Level 4 / W3C WOFF File Format 2.0 |
| **Baseline Status** | Widely Available (`@font-face` Baseline 2015; `font-display` Baseline 2018; WOFF2 Baseline 2018) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Lighthouse 11, WebPageTest, Chromium LayoutNG, Safari WebKit |
| **Module** | [06-typography-and-text](README.md) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility & Browser Support](#8-performance-accessibility--browser-support)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Web fonts liberate web design from the limited set of pre-installed operating system typefaces, allowing brands to render distinctive typography consistently across devices. However, because custom font binaries must be downloaded over the network before glyphs can be rasterized, web fonts represent one of the most common causes of poor Core Web Vitals (specifically Largest Contentful Paint [LCP] and Cumulative Layout Shift [CLS]).

When a browser renders text that requires an external web font:
- **FOIT (Flash of Invisible Text)**: The browser hides the text, displaying blank space until the font finishes downloading (or until a timeout expires).
- **FOUT (Flash of Unstyled Text)**: The browser immediately displays a fallback font (such as Arial or Times), and abruptly swaps in the web font once it downloads, often causing visible layout shifting.

The `@font-face` rule and its `font-display` descriptor provide web engineers with direct, programmatic control over the font loading lifecycle, enabling instant rendering, graceful fallbacks, and zero layout shift.

---

## 2. Core Concepts

### The `@font-face` Rule
The `@font-face` at-rule registers a custom typeface with the browser font cache. It binds an arbitrary font family identifier (`font-family: "CustomFont"`) to one or more remote or local font resource locations:
- `src`: Points to font resources using `url()` and specifies their container format via `format()`.
- `local()`: Queries the user's local operating system font library first; if the font is already installed locally, no network request is made.
- `font-weight` & `font-style`: Declare the specific weight/style range this binary file satisfies.
- `font-display`: Dictates how the browser handles text rendering while the font file is downloading.
- `unicode-range`: Defines the specific set of Unicode code points supported by this font file.

### Modern Font Formats: WOFF2 is King
- **WOFF2 (Web Open Font Format 2.0)**: Baseline Widely Available (supported by all modern browsers). Uses Brotli compression, yielding files that are 30%–50% smaller than WOFF. In modern web engineering, **WOFF2 is the only format required**.
- **WOFF (Web Open Font Format 1.0)**: Legacy gzip-compressed format.
- **TTF / OTF**: Raw, uncompressed font binaries; substantially larger file sizes.
- **EOT / SVG Fonts**: Obsolete, deprecated legacy formats (IE8, early iOS Safari).

### The Five `font-display` Strategies
The `font-display` descriptor partitions the font loading lifecycle into three consecutive periods:
1. **Block Period**: Text styled with this font is rendered invisibly (FOIT).
2. **Swap Period**: Text is rendered using a fallback font; if the web font finishes downloading during this period, it swaps in (FOUT).
3. **Failure Period**: The font load is aborted; the fallback font persists permanently.

| `font-display` Value | Block Period | Swap Period | Failure Period | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **`swap`** | Extremely short (0ms) | Infinite | After timeout | Critical body text where immediate readability is paramount |
| **`optional`** | Extremely short (100ms) | **0ms (None)** | Immediate | Best performance & zero CLS. Uses fallback on slow networks |
| **`block`** | Brief (3s typical) | Infinite | After timeout | Icon fonts or branding logos where fallback text looks broken |
| **`fallback`** | Extremely short (100ms) | Short (3s typical) | After 3s | Balance between quick render and avoiding late layout shifts |
| **`auto`** | Browser default (usually behaves like `block`) | Varies | Varies | Not recommended (unpredictable cross-browser behavior) |

---

## 3. How It Works Internally

```
Timeline of font-display: swap vs font-display: optional

1. font-display: swap
Time:  0ms -------------------- 400ms (Network Finish) ----------------->
Text:  [Fallback Font (FOUT)]   [Web Font Swaps In -> CLS Risk]

2. font-display: optional (Slow 3G Network)
Time:  0ms ---- 100ms (Timeout) ----------------- 1200ms (Network Finish) ->
Text:  [Blank]  [Fallback Font]                   [Cached for NEXT visit;
                                                   NO visual layout swap!]

3. font-display: block
Time:  0ms -------------------- 3000ms (FOIT Timeout) ------------------>
Text:  [Invisible Blank Space]  [Fallback Font Swaps In]
```

### Font Preloading and Crossorigin
By default, browsers do not initiate font downloads when the CSS stylesheet is parsed. The browser engine only requests a font file after:
1. The DOM tree is constructed.
2. The CSSOM tree is constructed.
3. The Render Tree matches an actual DOM node with a CSS rule referencing that `font-family`.

If a heading appears halfway down the page, font fetching may be delayed by several seconds. Preloading triggers the network request during initial HTML parsing:
```html
<link rel="preload" href="/fonts/inter-bold.woff2" as="font" type="font/woff2" crossorigin>
```
> **Critical Requirement**: Font requests strictly require the `crossorigin` attribute (or `crossorigin="anonymous"`), even when loading fonts from the exact same origin. Fonts are loaded via anonymous CORS per W3C specification; omitting `crossorigin` causes the browser to download the font file twice.

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`@font-face`**: Baseline Widely Available.
- **`font-display`**: Baseline Widely Available (Chrome 60+, Safari 11.1+, Firefox 58+).
- **WOFF2**: Baseline Widely Available (Chrome 36+, Safari 12+, Firefox 39+, Edge 14+).

```css
/* Production @font-face Registration */
@font-face {
  font-family: "Inter";
  font-style: normal;
  font-weight: 400;
  font-display: swap;
  src: local("Inter Regular"),
       local("Inter-Regular"),
       url("/fonts/inter-regular.woff2") format("woff2");
  unicode-range: U+0000-00FF, U+0131, U+0152-0153, U+02BB-02BC, U+02C6, U+02DA, U+02DC;
}

@font-face {
  font-family: "Inter";
  font-style: normal;
  font-weight: 700;
  font-display: swap;
  src: local("Inter Bold"),
       local("Inter-Bold"),
       url("/fonts/inter-bold.woff2") format("woff2");
  unicode-range: U+0000-00FF, U+0131, U+0152-0153;
}
```

---

## 5. Practical Examples

### Example 1: High-Performance Self-Hosted Web Font with `swap`
```css
/* Self-hosted Inter font with fallback and swap strategy */
@font-face {
  font-family: "CustomSans";
  font-style: normal;
  font-weight: 400;
  font-display: swap;
  src: url("/assets/fonts/custom-sans-400.woff2") format("woff2");
}

@font-face {
  font-family: "CustomSans";
  font-style: normal;
  font-weight: 700;
  font-display: swap;
  src: url("/assets/fonts/custom-sans-700.woff2") format("woff2");
}

body {
  font-family: "CustomSans", system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  font-size: 1rem;
  line-height: 1.5;
  color: #1e293b;
}
```

### Example 2: Zero-CLS Typography with `font-display: optional`
```css
/* Best performance pattern for e-commerce and editorial sites:
   If the font cannot download within 100ms, use system fallback;
   cache the font in background for immediate display on next page view. */
@font-face {
  font-family: "EditorialSerif";
  font-style: normal;
  font-weight: 400 700;
  font-display: optional;
  src: url("/fonts/editorial-serif.woff2") format("woff2");
}

.article-content {
  font-family: "EditorialSerif", Georgia, Cambria, "Times New Roman", serif;
  font-size: 1.125rem;
  line-height: 1.7;
}
```

### Example 3: Subsetting Unicode Ranges for Multi-Script Sites
```css
/* Download Latin glyphs in primary bundle (18 KB) */
@font-face {
  font-family: "GlobalFont";
  font-style: normal;
  font-weight: 400;
  font-display: swap;
  src: url("/fonts/global-latin.woff2") format("woff2");
  unicode-range: U+0000-00FF, U+0131, U+0152-0153;
}

/* Download Cyrillic glyphs ONLY if the page contains Cyrillic characters */
@font-face {
  font-family: "GlobalFont";
  font-style: normal;
  font-weight: 400;
  font-display: swap;
  src: url("/fonts/global-cyrillic.woff2") format("woff2");
  unicode-range: U+0400-045F, U+0490-0491, U+04B0-04B1, U+2116;
}
```

### Example 4: Preloading the Critical Web Font in HTML
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Preload Optimization</title>

  <!-- Preload the exact WOFF2 file used in the above-the-fold hero heading -->
  <link rel="preload"
        href="/fonts/inter-bold.woff2"
        as="font"
        type="font/woff2"
        crossorigin>

  <link rel="stylesheet" href="/styles.css">
</head>
<body>
  <h1 class="hero-title">High Speed Web Typography</h1>
</body>
</html>
```

### Example 5: Mitigating Font Layout Shift with `size-adjust`
```css
/* Match fallback system font metrics to the custom web font to eliminate CLS */
@font-face {
  font-family: "FallbackArial";
  src: local("Arial");
  size-adjust: 104.5%;
  ascent-override: 95%;
  descent-override: 25%;
  line-gap-override: 0%;
}

.heading-matched {
  font-family: "CustomSans", "FallbackArial", sans-serif;
}
```

---

## 6. Best Practices

1. **Serve Exclusively WOFF2**: Avoid bundling legacy TTF, OTF, or EOT files unless explicitly supporting Internet Explorer 11. WOFF2 provides maximum compression and 100% modern browser coverage.
2. **Always Include `crossorigin` on Font Preloads**: Even for first-party fonts hosted on the exact same domain, omitting `crossorigin` causes duplicate network downloads.
3. **Limit Preloads to 1 or 2 Critical Above-the-Fold Fonts**: Preloading 6 different font weights chokes network bandwidth and degrades First Contentful Paint (FCP). Only preload the regular body font and primary hero heading font.
4. **Prefer `font-display: swap` for Content-Heavy Sites**: Ensure users can begin reading article text immediately on mobile networks without staring at blank whitespace.
5. **Consider `font-display: optional` for Repeat-Visitor Apps**: Eliminates FOUT and CLS entirely. First visit uses system font while web font caches silently; subsequent visits render with instant custom typography.
6. **Self-Host Web Fonts**: Avoid third-party render-blocking CDN requests (like Google Fonts) which require extra DNS lookup, TCP handshake, and TLS negotiation hops.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **Omitting `crossorigin` on `<link rel="preload" as="font">`** | Font spec requires anonymous CORS. Missing `crossorigin` causes the browser to download the file twice. | Always specify `crossorigin` on font preloads. |
| **Preloading every font weight and style** | Saturates initial HTTP/2 network streams, delaying critical CSS and JS assets. | Preload at most 1–2 critical fonts used above the fold. |
| **Using `font-display: block` on body text** | Causes Flash of Invisible Text (FOIT) on slow 3G networks, leaving users staring at blank pages for 3s. | Use `font-display: swap` or `optional` for body text. |
| **Declaring different `font-family` names for weights** | Breaks semantic typography (`font-weight: 700` won't map correctly). | Use the **same** `font-family` name across `@font-face` rules; vary `font-weight`. |
| **Forgetting `format("woff2")` in `src`** | Browser downloads the binary without checking format compatibility. | Explicitly annotate `src: url(...) format("woff2");`. |
| **Hosting web fonts without `Cache-Control: immutable`** | Forces browsers to revalidate font assets on subsequent page navigations. | Set HTTP header `Cache-Control: public, max-age=31536000, immutable`. |

---

## 8. Performance, Accessibility & Browser Support

### Core Web Vitals Impact
- **Largest Contentful Paint (LCP)**: If the largest visible element is a text heading, delaying font delivery via FOIT directly degrades LCP. Preloading and `font-display: swap` accelerate LCP.
- **Cumulative Layout Shift (CLS)**: When a web font replaces a fallback font, differences in character widths (advance widths) and line heights cause surrounding text and buttons to reflow. Tuning fallback metrics via `size-adjust` eliminates this shift.

### Immutable Caching
Font files are static binaries that rarely change. Production servers should serve font files with:
```http
Cache-Control: public, max-age=31536000, immutable
```
Pair this with content-hashed filenames (`inter.a8f29e.woff2`).

---

## 9. Comparison / When to Use What

| Strategy | Initial Render | Font Swap Event | CLS Impact | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **`font-display: swap`** | Instant (Fallback) | Yes, when font downloads | Moderate (can cause reflow) | Blogs, docs, e-commerce content |
| **`font-display: optional`** | Instant (Fallback or Font) | No swap if missed timeout | **Zero (0.00)** | Web apps, dashboards, performance-critical sites |
| **`font-display: block`** | Delayed (Up to 3s blank) | Yes, when font downloads | High | Icon fonts, logos, custom symbol fonts |
| **`font-display: fallback`** | Short blank (100ms) | Swaps only if within 3s | Low | General UI balance |

---

## 10. Interview Questions

<details>
<summary>1. Why must the <code>crossorigin</code> attribute be included on font preloads even when the font is hosted on the same origin?</summary>

The W3C CSS Fonts specification requires that web fonts must be fetched using the Anonymous Cross-Origin Resource Sharing (CORS) mode, regardless of whether the URL is same-origin or cross-origin. When the browser parser processes `<link rel="preload" as="font" href="...">`, it sets the fetch request mode according to the presence of the `crossorigin` attribute. 

If the `crossorigin` attribute is omitted, the preload request is sent with `mode: "no-cors"`. Later, when the CSS engine encounters `@font-face` and initiates the actual font fetch, it initiates a request with `mode: "cors"`. Because the request modes do not match, the browser cannot reuse the preloaded response in the HTTP cache and issues a **second network request** for the exact same font file.
</details>

<details>
<summary>2. Explain the exact three lifecycle phases of the <code>font-display</code> descriptor.</summary>

The three phases are:
1. **Font Block Period**: The browser renders the element using an invisible fallback font. Text is not visible to the user (FOIT). If the font loads during this period, it is rendered immediately.
2. **Font Swap Period**: The browser renders the element using the fallback font. If the web font finishes downloading during this period, the browser swaps out the fallback and repaints with the web font (FOUT).
3. **Font Failure Period**: If the web font has not loaded when the swap period expires, the browser marks the font load as failed and permanently uses the fallback font for that page session.
</details>

<details>
<summary>3. Why is <code>font-display: optional</code> considered the premier choice for zero-CLS web font engineering?</summary>

Under `font-display: optional`, the browser gives the font a tiny block period (approximately 100ms) and a **0ms swap period**. If the font is already in the browser disk cache or loads within 100ms (fast fiber/broadband), the custom font renders immediately. 

If the network is slow (mobile 3G/4G), the browser immediately renders the fallback font and **aborts the swap**. The page layout never shifts, resulting in a CLS of 0. Meanwhile, the browser continues downloading the font in the background and places it in the cache, ensuring that the custom font renders instantly on subsequent page navigations.
</details>

<details>
<summary>4. What is the purpose of the <code>unicode-range</code> descriptor in <code>@font-face</code>?</summary>

`unicode-range` defines the specific subset of Unicode characters supported by that `@font-face` declaration. It enables **smart conditional downloading**: the browser will not download the font file unless the HTML document actually contains at least one character matching that Unicode range. 

For example, a site can split a massive typeface into a 15 KB Latin subset and a 200 KB East Asian or Cyrillic subset; users browsing English pages will never download the Cyrillic font file.
</details>

<details>
<summary>5. How does self-hosting fonts compare to using third-party services like Google Fonts or Adobe Fonts?</summary>

Self-hosting fonts is significantly superior for modern web performance:
1. **Connection Overhead**: Google Fonts requires two external origins: `fonts.googleapis.com` (for CSS) and `fonts.gstatic.com` (for font binaries), incurring multiple DNS lookups, TCP handshakes, and TLS negotiations.
2. **Render Blocking**: The stylesheet link to Google Fonts is render-blocking by default.
3. **Privacy Compliance**: Passing user IP addresses to third-party font servers can raise GDPR compliance concerns in European jurisdictions.
4. Self-hosting with HTTP/2 or HTTP/3 multiplexing allows font assets to be delivered across the existing connection alongside HTML, CSS, and JS.
</details>

<details>
<summary>6. How do you register multiple weights and styles under a single <code>font-family</code> name?</summary>

Declare multiple `@font-face` blocks sharing the exact same `font-family` name, but specify different `font-weight` and `font-style` values:
```css
@font-face {
  font-family: "MySans";
  font-weight: 400;
  font-style: normal;
  src: url("/fonts/mysans-regular.woff2") format("woff2");
}
@font-face {
  font-family: "MySans";
  font-weight: 700;
  font-style: normal;
  src: url("/fonts/mysans-bold.woff2") format("woff2");
}
```
This ensures standard semantic markup (`<strong>`, `<b>`, `font-weight: bold`) activates the bold binary without requiring separate class names.
</details>

<details>
<summary>7. What is the `local()` source declaration in `@font-face` and what are its pros and cons?</summary>

`local("Font Name")` directs the browser to check whether the font is already installed locally in the user's operating system before making a network request. 
- **Pro**: Instant rendering with 0 network bytes if the user already has the font installed (e.g., designers or OS defaults).
- **Con**: If the locally installed font version has different metrics or kerning tables than the web version, visual discrepancies can occur between different users.
</details>

<details>
<summary>8. How does the browser determine whether a font download is needed when parsing CSS?</summary>

The browser uses lazy font evaluation. Merely declaring `@font-face` in a stylesheet does not trigger a download. The browser engine only requests the font binary if an element in the Render Tree actually references the `font-family` and contains matching characters that fall within the font's `unicode-range`. Hidden elements (`display: none`) do not trigger font downloads.
</details>

---

## 11. Practice Exercises

### Exercise 1: Write a Production `@font-face` Declaration
**Goal**: Register the "Geist" font for weights 400 and 700 using WOFF2 format, swap strategy, and proper format declarations.

<details>
<summary>View Solution</summary>

```css
@font-face {
  font-family: "Geist";
  font-style: normal;
  font-weight: 400;
  font-display: swap;
  src: url("/fonts/geist-regular.woff2") format("woff2");
}

@font-face {
  font-family: "Geist";
  font-style: normal;
  font-weight: 700;
  font-display: swap;
  src: url("/fonts/geist-bold.woff2") format("woff2");
}

body {
  font-family: "Geist", system-ui, sans-serif;
}
```
</details>

### Exercise 2: Configure Optimal HTML Font Preload
**Goal**: Write the `<link>` tag in the HTML head to preload a critical hero heading font without triggering duplicate downloads.

<details>
<summary>View Solution</summary>

```html
<link rel="preload"
      href="/fonts/geist-bold.woff2"
      as="font"
      type="font/woff2"
      crossorigin>
```
</details>

### Exercise 3: Implement Zero-CLS with `font-display: optional`
**Goal**: Implement a typography configuration that completely eliminates layout shifts by utilizing `optional` display behavior.

<details>
<summary>View Solution</summary>

```css
@font-face {
  font-family: "NewsSerif";
  font-style: normal;
  font-weight: 400;
  font-display: optional;
  src: url("/fonts/news-serif.woff2") format("woff2");
}

article p {
  font-family: "NewsSerif", Georgia, serif;
  font-size: 1.125rem;
  line-height: 1.6;
}
```
</details>

### Exercise 4: Configure Currency Symbol Subsetting
**Goal**: Create an isolated `@font-face` that loads a specialized tabular currency font ONLY for currency symbols (`$`, `€`, `£`, `¥`).

<details>
<summary>View Solution</summary>

```css
@font-face {
  font-family: "CurrencyFont";
  font-style: normal;
  font-weight: 600;
  font-display: swap;
  src: url("/fonts/currency-tabular.woff2") format("woff2");
  /* Unicode code points for $, £, ¥, € */
  unicode-range: U+0024, U+00A3, U+00A5, U+20AC;
}

.financial-display {
  font-family: "CurrencyFont", "Inter", sans-serif;
}
```
</details>

### Exercise 5: Fallback Font Metrics Override
**Goal**: Use `size-adjust` to expand a system fallback font to eliminate layout shift during web font swap.

<details>
<summary>View Solution</summary>

```css
@font-face {
  font-family: "CustomHeadingFont";
  src: url("/fonts/custom-heading.woff2") format("woff2");
  font-display: swap;
}

@font-face {
  font-family: "MatchedArial";
  src: local("Arial");
  size-adjust: 105%;
  ascent-override: 92%;
  descent-override: 24%;
}

h1 {
  font-family: "CustomHeadingFont", "MatchedArial", sans-serif;
}
```
</details>

### Exercise 6: Variable Font Single-File Registration
**Goal**: Register a single variable WOFF2 font file supporting weights from 100 to 900 in one `@font-face` declaration.

<details>
<summary>View Solution</summary>

```css
@font-face {
  font-family: "InterVariable";
  font-style: normal;
  font-weight: 100 900;
  font-display: swap;
  src: url("/fonts/inter-variable.woff2") format("woff2-variations");
}

.title {
  font-family: "InterVariable", sans-serif;
  font-weight: 780;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Parameter | Syntax / Example | Purpose |
| :--- | :--- | :--- |
| `src` format | `src: url(...) format("woff2");` | Specifies WOFF2 modern font compression |
| `font-display: swap` | `font-display: swap;` | Immediate fallback; swaps in when downloaded |
| `font-display: optional`| `font-display: optional;` | Zero CLS; 100ms window or fallback remains |
| `local()` | `src: local("Arial"), url(...);` | Avoids download if font is installed locally |
| `unicode-range` | `unicode-range: U+0000-00FF;` | Downloads font only if characters match range |
| Font Preload | `<link rel="preload" as="font" crossorigin>` | Initiates fetch before Render Tree construction |

---

## 13. Further Reading
- [W3C CSS Fonts Module Level 4 - @font-face](https://www.w3.org/TR/css-fonts-4/#font-face-rule)
- [MDN Web Docs: @font-face](https://developer.mozilla.org/en-US/docs/Web/CSS/@font-face)
- [MDN Web Docs: font-display](https://developer.mozilla.org/en-US/docs/Web/CSS/@font-face/font-display)
- [web.dev: Best practices for fonts](https://web.dev/articles/font-best-practices)
- [web.dev: Preload web fonts to improve loading speed](https://web.dev/articles/codelab-preload-web-fonts)

---

[Previous: 01-font-properties.md](01-font-properties.md) | [Module README](README.md) | [Next: 03-variable-fonts.md](03-variable-fonts.md)
