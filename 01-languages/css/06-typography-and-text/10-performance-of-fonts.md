# Font Performance & Zero-CLS Metrics

| Metadata | Details |
| :--- | :--- |
| **Topic** | Font Loading Architecture, Zero-CLS Metric Overrides (`size-adjust`, `ascent-override`), Subsetting (`pyftsubset`), Preload & Caching Strategies |
| **Spec Level** | CSS Fonts Module Level 5 / CSS Fonts Module Level 4 |
| **Baseline Status** | Widely Available (`ascent-override`, `descent-override`, `line-gap-override`, `size-adjust` Baseline 2021 across Chrome 87+, Safari 15.4+, Firefox 89+) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Lighthouse 11, WebPageTest, Chromium LayoutNG, Capsize / Next.js Font Engine |
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

Web typography is frequently the primary bottleneck for Google Core Web Vitals scores:
- **Largest Contentful Paint (LCP)**: If the largest visible element above the fold is an `<h1>` heading or hero banner, delays in downloading, decoding, or rasterizing the web font directly delay LCP.
- **Cumulative Layout Shift (CLS)**: When a web font loads via `font-display: swap`, differences in character widths (advance widths), ascent, and descent between the local fallback font (e.g., Arial or Times New Roman) and the custom web font cause text lines to wrap differently and page elements to violently jump.

Historically, eliminating CLS required complex JavaScript font-loading observers that dynamically swapped CSS classes. Modern CSS solves this natively at the engine level through **Font Metrics Overrides**:
- `size-adjust`
- `ascent-override`
- `descent-override`
- `line-gap-override`

By mathematically scaling and shifting the fallback font to match the exact geometric silhouette of the incoming web font, developers can achieve **zero visual layout shift ($CLS = 0.00$)** even on high-latency mobile connections.

---

## 2. Core Concepts

### The Four CSS Font Metric Descriptors
Declared inside `@font-face`, these descriptors override the native OpenType/TrueType metric headers embedded in local system fonts:

1. **`size-adjust`**: Scales all glyphs and advance widths in the font by a uniform percentage without altering computed `font-size`.
   - Example: `size-adjust: 104.5%;` expands a narrow fallback font so its average character width matches a wider web font.
2. **`ascent-override`**: Sets the height of the font above the baseline.
   - Example: `ascent-override: 92%;`.
3. **`descent-override`**: Sets the depth of descenders below the baseline.
   - Example: `descent-override: 24%;`.
4. **`line-gap-override`**: Overrides the internal font-designer leading recommendation.
   - Example: `line-gap-override: 0%;`.

### Glyph Subsetting
A standard OpenType font file often contains thousands of glyphs supporting hundreds of worldwide languages, historical ligatures, mathematical symbols, and dingbats, weighing 150 KB to 500 KB.
- **Subsetting**: Stripping away every character code point not needed by your application.
- Subsetting a Latin-focused font file reduces payload size from **120 KB down to 12–18 KB**, an **85% reduction in transfer payload**.

### Self-Hosting vs Third-Party CDNs
- Modern web performance engineering mandates **self-hosting fonts** on your primary domain or edge CDN.
- Third-party CDNs (like Google Fonts) cannot share cache across different origins in modern browsers (due to cross-site cache partitioning introduced in Chrome, Safari, and Firefox). Loading fonts from external origins forces extra DNS lookups, TLS negotiations, and render-blocking round trips.

---

## 3. How It Works Internally

```
Anatomy of Font Swap Layout Shift (CLS) vs Zero-CLS Matching

1. Unmatched Fallback (Arial -> Inter):
   [Arial Fallback] : "The Distributed Cloud Architecture" (Wraps to 2 lines, height: 64px)
                              |
                     [Web Font Downloads]
                              v
   [Inter Web Font] : "The Distributed Cloud Architecture" (Wraps to 3 lines, height: 96px)
   ===> Result: Surrounding content pushed down by 32px! (HIGH CLS!)

2. Tuned Fallback via size-adjust and ascent-override:
   [Tuned Arial]    : "The Distributed Cloud Architecture" (Wraps to 3 lines, height: 96px)
                              |
                     [Web Font Downloads]
                              v
   [Inter Web Font] : "The Distributed Cloud Architecture" (Identical 96px footprint!)
   ===> Result: Zero movement of surrounding elements! (CLS = 0.00!)
```

### Mathematical Metric Alignment Formula
To calculate `size-adjust`:
$$\text{size-adjust} = \frac{\text{Web Font Average Character Width}}{\text{Fallback Font Average Character Width}} \times 100\%$$
Similarly, `ascent-override` and `descent-override` are scaled relative to the newly adjusted em-square.

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`size-adjust` & Metric Overrides**: Baseline 2021 (Chrome 87+, Safari 15.4+, Firefox 89+).
- **WOFF2**: Baseline Widely Available.
- **Preload `<link as="font">`**: Baseline Widely Available.

```css
/* 1. Register the Web Font */
@font-face {
  font-family: "BrandSans";
  font-style: normal;
  font-weight: 400;
  font-display: swap;
  src: url("/fonts/brand-sans.woff2") format("woff2");
}

/* 2. Override Fallback System Metrics (Zero-CLS Arial Fallback) */
@font-face {
  font-family: "BrandSans-Fallback";
  src: local("Arial");
  size-adjust: 105.2%;
  ascent-override: 94%;
  descent-override: 26%;
  line-gap-override: 0%;
}

/* 3. Declare Font Stack */
body {
  font-family: "BrandSans", "BrandSans-Fallback", sans-serif;
}
```

---

## 5. Practical Examples

### Example 1: Zero-CLS Fallback Matching for Inter & Arial
```css
/* Target: Inter Web Font */
@font-face {
  font-family: "Inter";
  font-style: normal;
  font-weight: 400;
  font-display: swap;
  src: url("/fonts/inter-regular.woff2") format("woff2");
}

/* Fallback: Perfectly Tuned Arial to match Inter's exact geometry */
@font-face {
  font-family: "Inter-Fallback";
  src: local("Arial");
  size-adjust: 107.5%;
  ascent-override: 90%;
  descent-override: 22.5%;
  line-gap-override: 0%;
}

/* Hero Heading: When Inter downloads, zero layout shift occurs */
.hero-title {
  font-family: "Inter", "Inter-Fallback", sans-serif;
  font-size: clamp(2rem, 5vw, 4rem);
  font-weight: 700;
  line-height: 1.15;
}
```

### Example 2: Command-Line Glyph Subsetting via `pyftsubset`
```bash
# Subsetting a 250 KB font to a 14 KB Latin subset using fonttools (Python)
pyftsubset Inter-Bold.ttf \
  --unicodes="U+0020-007F,U+00A0-00FF,U+2018-201D,U+2026" \
  --flavor=woff2 \
  --output-file=inter-bold.subset.woff2 \
  --layout-features="kern,liga" \
  --desubroutinize
```
*Result: Transferred byte size drops from 240 KB to 13.8 KB.*

### Example 3: Production High-Performance HTML Preload Configuration
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>High Performance Typography</title>

  <!-- Preload strictly the 1 WOFF2 font needed for the above-the-fold hero -->
  <link rel="preload"
        href="/assets/fonts/inter-bold.subset.woff2"
        as="font"
        type="font/woff2"
        crossorigin>

  <!-- Critical Inlined Fallback Stylesheet -->
  <style>
    @font-face {
      font-family: "Inter";
      font-weight: 700;
      font-display: swap;
      src: url("/assets/fonts/inter-bold.subset.woff2") format("woff2");
    }
    @font-face {
      font-family: "Inter-Fallback";
      src: local("Arial");
      size-adjust: 107.5%;
      ascent-override: 90%;
      descent-override: 22.5%;
    }
    h1 {
      font-family: "Inter", "Inter-Fallback", sans-serif;
      font-size: 3rem;
      font-weight: 700;
    }
  </style>
</head>
<body>
  <h1>Accelerating Global Delivery</h1>
</body>
</html>
```

### Example 4: HTTP Caching Header Configuration (Nginx / Cloudflare)
```nginx
# Nginx Configuration: Immutable static font caching
location ~* \.(woff2)$ {
    expires 1y;
    add_header Cache-Control "public, max-age=31536000, immutable";
    add_header Access-Control-Allow-Origin "*";
    types { font/woff2 woff2; }
}
```

### Example 5: Multi-Range Subsetting for Multilingual Publishing
```css
/* Split massive typeface into targeted geographic subsets */

/* 1. Basic Latin (14 KB) - Downloaded immediately */
@font-face {
  font-family: "GlobalSerif";
  src: url("/fonts/serif-latin.woff2") format("woff2");
  font-display: swap;
  unicode-range: U+0000-00FF;
}

/* 2. Cyrillic (18 KB) - Downloaded ONLY if Russian/Ukrainian text appears */
@font-face {
  font-family: "GlobalSerif";
  src: url("/fonts/serif-cyrillic.woff2") format("woff2");
  font-display: swap;
  unicode-range: U+0400-04FF;
}

/* 3. Mathematical Symbols & Greek (12 KB) - Downloaded ONLY on formula pages */
@font-face {
  font-family: "GlobalSerif";
  src: url("/fonts/serif-math.woff2") format("woff2");
  font-display: swap;
  unicode-range: U+0370-03FF, U+2200-22FF;
}
```

---

## 6. Best Practices

1. **Serve Fonts Strictly as WOFF2**: WOFF2 uses Brotli compression, yielding 30%–50% smaller binaries than WOFF1. Modern browser support is 100%.
2. **Always Pair `font-display: swap` with Metric Overrides**: If you enable `swap`, you must provide a tuned fallback font with `size-adjust` to eliminate the layout shift when swapping occurs.
3. **Preload Maximum 1–2 Critical Above-the-Fold Fonts**: Over-preloading creates network contention that delays critical CSS and JavaScript execution.
4. **Self-Host Fonts on Same Origin or CDN**: Cross-site cache partitioning renders third-party font hosting services obsolete for performance.
5. **Set `Cache-Control: public, max-age=31536000, immutable`**: Web fonts are static assets; pair long-lived caching with content hashes in the filename.
6. **Subset Glyphs to Target Language**: Never ship full 5,000-glyph multi-lingual fonts to an English-only application. Use `pyftsubset` to purge unused code points.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **`font-display: swap` without fallback metric overrides** | Swapping in web font shifts all surrounding page layout, inflating CLS scores. | Use `size-adjust` and `ascent-override` on local fallback. |
| **Omitting `crossorigin` on `<link rel="preload" as="font">`** | Font spec mandates anonymous CORS. Browser fetches file twice! | Always add `crossorigin` to font preloads. |
| **Using external Google Fonts stylesheets in production** | Adds 2 external render-blocking network origins, increasing LCP latency. | Download WOFF2 files and self-host locally. |
| **Preloading 8 different font weights** | Chokes initial HTTP bandwidth and delays First Contentful Paint. | Preload strictly 1 (or at most 2) critical fonts. |
| **Forgetting `format("woff2")` in `@font-face`** | Browser downloads the font before verifying format support. | Explicitly annotate `src: url(...) format("woff2");`. |
| **Hosting fonts without CORS headers (`Access-Control-Allow-Origin`)** | Web fonts fetched across CDN subdomains fail to load due to CORS restrictions. | Set `Access-Control-Allow-Origin: *;` on font assets. |

---

## 8. Performance, Accessibility & Browser Support

### Lighthouse & Core Web Vitals Impact
- **LCP (Largest Contentful Paint)**: Reducing font binary size from 120 KB to 14 KB and preloading the file typically improves LCP by 300ms–800ms on mobile connections.
- **CLS (Cumulative Layout Shift)**: Implementing `size-adjust` and `ascent-override` reduces font-swap layout shift from $0.15$ down to **$0.00$**.

---

## 9. Comparison / When to Use What

| Strategy | Performance Overhead | Visual Shift (CLS) | Implementation Effort | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **System Font Stack** | **Zero (0 ms, 0 KB)** | **0.00** | Zero | Dashboards, dev tools, high-speed apps |
| **`font-display: optional`** | Fast (100ms or fallback) | **0.00** | Low | E-commerce, news publications |
| **`font-display: swap` + Overrides** | Fast (Immediate render) | **0.00** (Tuned) | Medium | Brand-sensitive applications |
| **`font-display: block`** | Slow (Up to 3s blank) | High | Low | Logos, icon fonts (not recommended) |

---

## 10. Interview Questions

<details>
<summary>1. How do CSS Font Metrics Overrides (<code>size-adjust</code>, <code>ascent-override</code>, <code>descent-override</code>) eliminate Cumulative Layout Shift (CLS)?</summary>

When a browser renders text using a fallback system font (such as Arial) while a web font is downloading, the fallback font has its own inherent character advance widths and vertical em-box metrics. When the web font finishes downloading and swaps in via `font-display: swap`, differences in character widths cause words to reflow to different lines, and differences in line-height cause blocks to expand or contract, causing page elements below to jump (CLS).

Font metric overrides allow developers to alter the local system font inside a dedicated `@font-face` declaration:
- `size-adjust`: Scales the glyph advance widths of the system font to match the web font.
- `ascent-override` & `descent-override`: Aligns the baseline and line-box height to match the web font.
When the web font finishes downloading and swaps in, both fonts occupy the exact same spatial footprint, resulting in a CLS of 0.
</details>

<details>
<summary>2. Why does third-party font hosting (e.g., standard Google Fonts) fail to benefit from browser caching across different websites today?</summary>

Historically, if a user visited Website A that used Google Fonts' Roboto, and then visited Website B that also used Roboto, the browser would serve the font from the shared HTTP cache without making a network request.

To prevent cross-site user tracking and timing attacks, modern browser engines (Chromium, WebKit, Gecko) implemented **Cross-Site Cache Partitioning** (HTTP Cache Partitioning). The browser partitions its HTTP cache using a double key: `(Top-Level Origin, Asset URL)`. 
As a result, an asset cached by `site-a.com` can **never** be accessed by `site-b.com`. Self-hosting fonts removes third-party DNS lookups and TLS handshakes while offering the exact same caching behavior.
</details>

<details>
<summary>3. Why is glyph subsetting so effective at reducing font file sizes?</summary>

Most commercial and open-source font files are engineered to be universally applicable: they bundle thousands of glyphs covering Latin, Cyrillic, Greek, Vietnamese, mathematical operators, box-drawing characters, and stylistic alternates. 

An English-language marketing website typically requires fewer than 100 unique characters (A–Z, a–z, 0–9, basic punctuation). Subsetting tools strip out the unused glyph vectors and their associated OpenType kerning tables, reducing file sizes from 150 KB+ down to 12–15 KB.
</details>

<details>
<summary>4. What happens if you preload a font but omit the <code>crossorigin</code> attribute?</summary>

The browser will download the font file twice. Per W3C CSS Fonts specification, web fonts must be fetched using anonymous CORS mode. If `crossorigin` is omitted on `<link rel="preload" as="font">`, the preload request is sent with `no-cors` mode. When the CSS engine subsequently parses the `@font-face` rule, it initiates a CORS request. Because the request modes do not match, the browser cannot reuse the preloaded asset and issues a second HTTP request.
</details>

<details>
<summary>5. What are the key differences between WOFF and WOFF2 container formats?</summary>

- **WOFF (Web Open Font Format 1.0)**: Uses standard zlib / gzip compression on raw TrueType/OpenType tables.
- **WOFF2 (Web Open Font Format 2.0)**: Employs **Brotli compression** along with custom preprocessing transforms that re-order table data for maximum entropy compression. WOFF2 files are typically **30% to 50% smaller** than equivalent WOFF files.
</details>

<details>
<summary>6. How does <code>font-display: optional</code> achieve zero CLS without requiring manual metric overrides?</summary>

`font-display: optional` gives the font an extremely brief block window (approximately 100ms) and **zero swap window**. If the font is already in disk cache or downloads within 100ms, it renders immediately. 

If it does not load within 100ms, the browser renders the fallback font and **cancels the visual swap entirely for that page load**. Because the fallback font is never replaced on that page view, no layout shift can occur ($CLS = 0.00$). The browser finishes downloading the font in the background and caches it for subsequent page views.
</details>

<details>
<summary>7. What HTTP caching header directives should be configured on font binaries?</summary>

Font binaries are static immutable assets. When using content-hashed filenames (e.g., `inter.8f4b2a.woff2`), servers should send:
```http
Cache-Control: public, max-age=31536000, immutable
```
This instructs browsers to cache the asset for a full year and bypass HTTP revalidation requests (304 Not Modified) even when the user reloads the page.
</details>

<details>
<summary>8. How do tools like Capsize and Next.js Font Engine automate font metric overrides?</summary>

These tools read the binary metadata header tables (`OS/2`, `hhea`, `head`) of the target web font and the target system fallback font (e.g., Arial). They extract the exact `unitsPerEm`, `sTypoAscender`, `sTypoDescender`, and character advance widths, and mathematically calculate the precise `size-adjust`, `ascent-override`, and `descent-override` percentages, emitting a ready-to-use `@font-face` CSS rule.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Zero-CLS Fallback Font Rule
**Goal**: Write a tuned fallback `@font-face` for Georgia with `size-adjust: 102%`, `ascent-override: 95%`, and apply it to an editorial headline.

<details>
<summary>View Solution</summary>

```css
@font-face {
  font-family: "CustomEditorialSerif";
  font-style: normal;
  font-weight: 700;
  font-display: swap;
  src: url("/fonts/editorial-bold.woff2") format("woff2");
}

@font-face {
  font-family: "TunedGeorgia";
  src: local("Georgia");
  size-adjust: 102%;
  ascent-override: 95%;
  descent-override: 25%;
  line-gap-override: 0%;
}

h1.zero-cls-headline {
  font-family: "CustomEditorialSerif", "TunedGeorgia", serif;
  font-size: 3rem;
  line-height: 1.2;
}
```
</details>

### Exercise 2: Configure Preload and Resource Hints
**Goal**: Write the optimal HTML `<head>` tags to preload a critical hero font and establish early connection to an edge CDN.

<details>
<summary>View Solution</summary>

```html
<link rel="preconnect" href="https://cdn.example.com" crossorigin>

<link rel="preload"
      href="https://cdn.example.com/fonts/inter-bold.woff2"
      as="font"
      type="font/woff2"
      crossorigin>
```
</details>

### Exercise 3: Multi-Language Unicode-Range Division
**Goal**: Write two `@font-face` rules that load Latin characters in file A and Greek characters in file B under the same `font-family`.

<details>
<summary>View Solution</summary>

```css
@font-face {
  font-family: "UniversalSans";
  src: url("/fonts/universal-latin.woff2") format("woff2");
  font-display: swap;
  unicode-range: U+0000-00FF;
}

@font-face {
  font-family: "UniversalSans";
  src: url("/fonts/universal-greek.woff2") format("woff2");
  font-display: swap;
  unicode-range: U+0370-03FF;
}
```
</details>

### Exercise 4: Zero-CLS Responsive Typography Stack
**Goal**: Combine fluid `clamp()` sizing with metric-adjusted fallbacks for a hero display title.

<details>
<summary>View Solution</summary>

```css
.responsive-hero {
  font-family: "CustomEditorialSerif", "TunedGeorgia", serif;
  font-size: clamp(2.25rem, 6vw, 4.5rem);
  font-weight: 700;
  line-height: 1.1;
  letter-spacing: -0.03em;
}
```
</details>

### Exercise 5: Test Font Loading Events via CSS Font Loading API
**Goal**: Write a JavaScript snippet verifying that a font has successfully rendered and log its status.

<details>
<summary>View Solution</summary>

```javascript
document.fonts.ready.then(() => {
  console.log("All active web fonts have finished downloading and rasterizing!");
});

// Check specific font status
if (document.fonts.check("1em CustomEditorialSerif")) {
  console.log("CustomEditorialSerif is loaded and active!");
}
```
</details>

### Exercise 6: Server Nginx Cache Configuration
**Goal**: Write an Nginx configuration block enforcing immutable caching and permissive CORS for WOFF2 files.

<details>
<summary>View Solution</summary>

```nginx
location ~* \.woff2$ {
    expires 365d;
    add_header Cache-Control "public, max-age=31536000, immutable";
    add_header Access-Control-Allow-Origin "*";
    add_header Timing-Allow-Origin "*";
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Descriptor | Syntax Example | Role in Performance |
| :--- | :--- | :--- |
| `size-adjust` | `size-adjust: 105%;` | Scales fallback glyph advance widths to eliminate horizontal CLS |
| `ascent-override` | `ascent-override: 90%;` | Matches fallback ascent to web font to eliminate vertical CLS |
| `descent-override` | `descent-override: 22%;` | Matches fallback descent to web font |
| `line-gap-override`| `line-gap-override: 0%;` | Strips internal leading differences between fonts |
| `unicode-range` | `unicode-range: U+0000-00FF;` | Downloads font only if matching characters exist on page |
| Font Preload | `<link rel="preload" as="font" crossorigin>` | Triggers download before DOM/CSSOM render tree construction |
| `font-display: optional`| `font-display: optional;` | 100ms render window; zero layout shift guaranteed |

---

## 13. Further Reading
- [W3C CSS Fonts Module Level 5 - Font Metrics Overrides](https://www.w3.org/TR/css-fonts-5/#font-metrics-overrides)
- [MDN Web Docs: size-adjust](https://developer.mozilla.org/en-US/docs/Web/CSS/@font-face/size-adjust)
- [web.dev: Improved font fallbacks with size-adjust and metric overrides](https://web.dev/articles/css-size-adjust)
- [Capsize: Sizing and zero-CLS typography tooling](https://seek-oss.github.io/capsize/)
- [Fonttools pyftsubset documentation](https://fonttools.readthedocs.io/en/latest/subset/)

---

[Previous: 09-icon-fonts-vs-svg.md](09-icon-fonts-vs-svg.md) | [Module README](README.md) | [Next Module: 07-visual-styling](file:///e:/Learning/engineering-notes/01-languages/css/07-visual-styling/README.md)
