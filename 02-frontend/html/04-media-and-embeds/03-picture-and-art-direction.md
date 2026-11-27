# The Picture Element, Art Direction, and Format Negotiation

| Metadata | Details |
| :--- | :--- |
| **Specification** | WHATWG HTML Living Standard (§4.8.3 The `picture` element) |
| **Prerequisites** | `04-media-and-embeds/02-responsive-images-in-depth.md`, `01-fundamentals/06-images-and-alt-text.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`<picture>`, `<source media>`, `<source type>`) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, Chrome DevTools Network/Rendering, axe-core 4.9 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Classic Format Negotiation (AVIF &rarr; WebP &rarr; JPEG Fallback)](#example-1-classic-format-negotiation-avif--webp--jpeg-fallback)
   - [Example 2: Art Direction for Mobile vs. Desktop Viewports](#example-2-art-direction-for-mobile-vs-desktop-viewports)
   - [Example 3: Dark Mode and Light Mode Image Switching with `prefers-color-scheme`](#example-3-dark-mode-and-light-mode-image-switching-with-prefers-color-scheme)
   - [Example 4: Enterprise Production Pattern (Format Switching + Resolution Switching Combined)](#example-4-enterprise-production-pattern-format-switching--resolution-switching-combined)
   - [Example 5: Print vs. Screen Dynamic Media Swapping](#example-5-print-vs-screen-dynamic-media-swapping)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility, SEO & Security Notes](#8-performance-accessibility-seo--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

While the `srcset` and `sizes` attributes solve **resolution switching** (serving different pixel densities of the same image), they cannot solve two other critical web media challenges:
1. **Art Direction**: Adapting image composition or cropping across viewports. A wide panoramic shot looks stunning on a 27-inch desktop monitor, but shrinking that entire landscape onto a 375px mobile phone reduces human subjects to unrecognizable specks. Mobile requires a tight, vertical crop focusing on the subject.
2. **Format Negotiation (Progressive Enhancement)**: Serving next-generation codecs like AVIF (which offers up to 50% better compression than JPEG) or WebP, while gracefully falling back to universal JPEG or PNG for legacy browsers that lack modern decoder support.

The HTML `<picture>` element solves both problems natively. Acting as an intelligent decision wrapper around a mandatory inner `<img>` element, `<picture>` inspects child `<source>` tags against media queries and MIME types, swapping the image source seamlessly while retaining complete layout, accessibility, and styling hooks.

---

## 2. Core Concepts

### Architecture: The `<picture>` Wrapper vs. Rendered `<img>`
A critical architectural concept in HTML is that the `<picture>` element is an **invisible wrapper**. It does not render a visual box by itself, nor does it carry an accessible name.
- The inner `<img>` element is what **actually renders** to the screen.
- All styles (`border`, `border-radius`, `box-shadow`, `aspect-ratio`), event listeners (`onload`, `onerror`), and accessibility attributes (`alt`, `aria-labelledby`) **must** be applied to the inner `<img>`, not the outer `<picture>`.
- The `<picture>` element merely acts as an algorithm engine that feeds the optimal URL source into the inner `<img>.currentSrc`.

### `<source>` Attributes inside `<picture>`
Child `<source>` elements accept:
- **`media`**: A CSS media query (e.g., `media="(min-width: 1024px)"`, `media="(orientation: portrait)"`, or `media="(prefers-color-scheme: dark)"`).
- **`type`**: The MIME type of the resource (e.g., `type="image/avif"`, `type="image/webp"`). The browser checks whether its internal decoder supports this MIME type.
- **`srcset`**: The candidate image URL or list of candidates with `w` or `x` descriptors.
- **`sizes`**: Display slot calculations (used when `<source>` specifies width `w` descriptors).
- **`width` and `height`**: HTML Living Standard allows specifying intrinsic dimensions on `<source>` elements to preserve differing aspect ratios during art direction.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                      <picture> Source Selection Algorithm                               |
+-----------------------------------------------------------------------------------------+

  HTML Parser / Preload Scanner encounters <picture>
       |
       v
  Iterate through child <source> elements in document tree order:
       |
       +--> Check 1: Does <source> have a `media` attribute?
       |      - If YES: Does the media query match the current viewport/device?
       |        - NO  -> SKIP to next <source>
       |        - YES -> Proceed to Check 2
       |
       +--> Check 2: Does <source> have a `type` attribute?
       |      - If YES: Does browser engine support this image MIME type?
       |        - NO  -> SKIP to next <source>
       |        - YES -> Proceed to Selection!
       |
       v
  MATCH FOUND!
  Take the matched <source>'s `srcset` and feed it into the inner <img>!
  (All remaining <source> elements are ignored)
       |
       v
  FALLBACK:
  If NO <source> elements match (or none are supported),
  the browser executes the standard `src` / `srcset` of the inner <img>.
```

---

## 4. Syntax & Usage

```html
<picture>
  <!-- 1. High-efficiency AVIF format for desktop -->
  <source 
    media="(min-width: 1024px)" 
    srcset="/images/banner-desktop.avif" 
    type="image/avif" 
  />
  <!-- 2. Universal WebP format for desktop -->
  <source 
    media="(min-width: 1024px)" 
    srcset="/images/banner-desktop.webp" 
    type="image/webp" 
  />
  <!-- 3. Mobile portrait crop in AVIF -->
  <source 
    srcset="/images/banner-mobile.avif" 
    type="image/avif" 
  />
  <!-- 4. Mandatory inner <img> element providing fallback, alt text, and sizing -->
  <img 
    src="/images/banner-mobile.jpg" 
    alt="Architectural designer sketching blueprints at a sunlit wooden desk" 
    width="800" 
    height="1000" 
    loading="lazy" 
    decoding="async" 
  />
</picture>
```

> **Cross-Browser & Baseline Status Box**
>
> - `<picture>`, `<source media>`, `<source type>`: **Baseline Widely Available** across all major browsers since 2015.
> - AVIF image support (`image/avif`): **Baseline Widely Available** (Chrome 85+, Firefox 93+, Safari 16.0+, Edge 121+).
> - WebP image support (`image/webp`): **Baseline Widely Available** (Universal across all modern engines).

---

## 5. Practical Examples

### Example 1: Classic Format Negotiation (AVIF &rarr; WebP &rarr; JPEG Fallback)
Serving the highest compression format supported by the user's browser with progressive degradation.

```html
<picture class="product-visual">
  <!-- Preferred: AVIF (~50% smaller than JPEG with superior color fidelity) -->
  <source srcset="/products/watch-gold.avif" type="image/avif" />

  <!-- Modern Alternative: WebP (~30% smaller than JPEG) -->
  <source srcset="/products/watch-gold.webp" type="image/webp" />

  <!-- Legacy Universal Fallback: Optimized Baseline JPEG -->
  <img 
    src="/products/watch-gold.jpg" 
    alt="Chronograph luxury wrist watch with gold bezel and black leather strap" 
    width="800" 
    height="800" 
    loading="lazy" 
    decoding="async" 
    class="responsive-card-img"
  />
</picture>

<style>
  .responsive-card-img {
    width: 100%;
    max-width: 400px;
    height: auto;
    aspect-ratio: 1 / 1;
    object-fit: cover;
    border-radius: 8px;
  }
</style>
```

---

### Example 2: Art Direction for Mobile vs. Desktop Viewports
Cropping from an ultra-wide 21:9 cinematic banner on desktop to a tight 4:5 vertical portrait on mobile so the human subject remains prominent.

```html
<figure class="hero-feature">
  <picture>
    <!-- Desktop: Wide landscape crop (21:9 ratio) -->
    <source 
      media="(min-width: 1024px)" 
      srcset="/hero/campaign-wide.webp" 
      width="1920" 
      height="820" 
    />

    <!-- Tablet: Moderate landscape crop (16:9 ratio) -->
    <source 
      media="(min-width: 640px)" 
      srcset="/hero/campaign-medium.webp" 
      width="1024" 
      height="576" 
    />

    <!-- Mobile default: Tight portrait crop (4:5 ratio) -->
    <img 
      src="/hero/campaign-portrait.webp" 
      alt="Athlete sprinting across urban bridge during morning sunrise" 
      width="640" 
      height="800" 
      fetchpriority="high"
      decoding="async" 
      class="hero-art-directed"
    />
  </picture>
  <figcaption>Spring 2026 High-Performance Footwear Campaign</figcaption>
</figure>

<style>
  .hero-art-directed {
    width: 100%;
    height: auto;
    display: block;
    object-fit: cover;
  }
</style>
```

---

### Example 3: Dark Mode and Light Mode Image Switching with `prefers-color-scheme`
Automatically swapping technical architectural diagrams or UI mockups when the user toggles dark mode at the OS level.

```html
<picture class="system-diagram">
  <!-- Dark mode diagram with dark slate background and glowing neon vectors -->
  <source 
    media="(prefers-color-scheme: dark)" 
    srcset="/diagrams/microservices-dark.svg" 
  />

  <!-- Light mode diagram with white background and crisp contrast -->
  <img 
    src="/diagrams/microservices-light.svg" 
    alt="System architecture diagram illustrating API gateway routing requests to auth, billing, and notification microservices" 
    width="960" 
    height="540" 
    loading="lazy" 
    class="diagram-asset"
  />
</picture>

<style>
  .diagram-asset {
    width: 100%;
    max-width: 800px;
    height: auto;
    border: 1px solid #ddd;
    border-radius: 6px;
    padding: 1rem;
    background-color: var(--card-bg, #fff);
  }
</style>
```

---

### Example 4: Enterprise Production Pattern (Format Switching + Resolution Switching Combined)
The ultimate production image pattern: combining multi-codec format negotiation (AVIF + WebP) with multi-density fluid responsive sizing (`srcset` + `sizes`).

```html
<picture class="article-hero">
  <!-- Desktop & Tablet AVIF -->
  <source 
    type="image/avif" 
    media="(min-width: 768px)"
    srcset="
      /content/tech-400.avif   400w,
      /content/tech-800.avif   800w,
      /content/tech-1400.avif 1400w
    "
    sizes="(max-width: 1200px) 70vw, 900px"
  />

  <!-- Desktop & Tablet WebP Fallback -->
  <source 
    type="image/webp" 
    media="(min-width: 768px)"
    srcset="
      /content/tech-400.webp   400w,
      /content/tech-800.webp   800w,
      /content/tech-1400.webp 1400w
    "
    sizes="(max-width: 1200px) 70vw, 900px"
  />

  <!-- Mobile Cropped AVIF (Vertical orientation) -->
  <source 
    type="image/avif"
    srcset="
      /content/tech-mobile-360.avif 360w,
      /content/tech-mobile-720.avif 720w
    "
    sizes="100vw"
  />

  <!-- Base <img> with standard WebP/JPEG resolution switching -->
  <img 
    src="/content/tech-mobile-720.jpg" 
    srcset="
      /content/tech-mobile-360.jpg 360w,
      /content/tech-mobile-720.jpg 720w
    "
    sizes="100vw"
    alt="Server racks glowing with blue LED indicators inside modern green data center" 
    width="900" 
    height="600" 
    loading="lazy" 
    decoding="async" 
    class="featured-hero-img"
  />
</picture>
```

---

### Example 5: Print vs. Screen Dynamic Media Swapping
Serving an ink-saving monochrome high-contrast line illustration when the document is printed.

```html
<picture class="report-chart">
  <!-- High-resolution monochrome version optimized for ink and paper -->
  <source 
    media="print" 
    srcset="/reports/q4-chart-print.png" 
  />

  <!-- Full-color interactive screen version -->
  <img 
    src="/reports/q4-chart-screen.png" 
    alt="Bar chart showing 34% quarterly revenue growth across enterprise subscriptions" 
    width="1000" 
    height="500" 
    loading="lazy" 
  />
</picture>
```

---

## 6. Best Practices

- [ ] **Always include a standard `<img>` as the last child of `<picture>`**: The browser will render absolutely nothing if the inner `<img>` is omitted.
- [ ] **Put `alt` attributes exclusively on the `<img>` tag**: `<source>` elements do not support or expose `alt` text. Putting `alt` on `<picture>` or `<source>` violates accessibility standards and is completely ignored by screen readers.
- [ ] **Apply CSS classes and inline styles to the `<img>`**: Do not style `<picture>` expecting styles to cascade visually; apply width, height, borders, and shadows to the rendered `<img>`.
- [ ] **Order `<source>` elements strictly by specificity**:
  - Put narrower media queries (e.g., `(min-width: 1280px)`) **before** wider ones (e.g., `(min-width: 768px)`).
  - Put advanced formats (`type="image/avif"`) **before** intermediate formats (`type="image/webp"`).
- [ ] **Provide intrinsic `width` and `height` on `<source>` when aspect ratios change**: Modern browsers use attributes on matching `<source>` tags to recompute CSS aspect ratio dynamically during art direction.
- [ ] **Reserve `<picture>` for Art Direction or Format Negotiation**: If you only need resolution switching (same image at different resolutions), stick to standard `<img srcset="..." sizes="...">`. `<picture>` adds unnecessary DOM complexity if composition does not change.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Omitting the inner `<img>` tag inside `<picture>` | **Critical** | Zero images render to the screen; element is completely broken. | Always include a valid `<img src="..." alt="...">` inside `<picture>`. |
| Placing `alt` attribute on `<picture>` or `<source>` | **Critical** | Screen readers announce the image as unlabeled or decorative; accessibility failure. | Always place `alt` directly on the inner `<img>`. |
| Ordering format types from oldest to newest | **High** | Browser encounters `type="image/jpeg"` first, immediately selects it, and skips AVIF/WebP. | Place modern formats first: AVIF &rarr; WebP &rarr; JPEG. |
| Ordering `min-width` media queries in ascending order | **High** | Lower `min-width` queries match first, blocking larger desktop crops from ever executing. | Order `min-width` queries from largest to smallest (e.g., 1200px &rarr; 800px). |
| Applying CSS styling to `<picture>` instead of `<img>` | **Moderate** | Image ignores borders, border-radius, or aspect-ratio declarations. | Target `picture img` or apply class directly to `<img>`. |
| Using `<picture>` when basic `<img srcset>` is sufficient | **Low** | Unnecessary HTML verbosity without any art direction or codec benefits. | Use standard `<img>` with `srcset` and `sizes` for pure resolution switching. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Web Platform Performance
- **Codec Efficiency**: AVIF achieves 20% to 50% better compression efficiency compared to WebP and JPEG at identical perceptual quality (SSIM/VMAF). Using `<picture>` format negotiation provides dramatic byte savings without stranding users on older Safari or mobile browsers.
- **Speculative Parsing**: The browser's preload scanner understands `<picture>`, `<source media>`, and `<source type>`, fetching the optimal candidate during the initial HTML stream before stylesheet evaluation.

### Accessibility (WCAG 2.2 AA)
- Screen readers treat `<picture>` as a non-semantic container; they navigate directly to the inner `<img>` and speak its `alt` text. Ensure the `alt` description remains accurate across all art-directed crops.

---

## 9. Comparison / When to Use What

| Requirement | Preferred Technique | Why? |
| :--- | :--- | :--- |
| **Serve 1x, 2x, 3x for Retina** | `<img srcset="... 1x, ... 2x">` | Simplest syntax; browser handles density matching automatically. |
| **Fluid image resizing for layout** | `<img srcset="...w" sizes="...">` | Optimal for performance; zero unnecessary DOM wrappers. |
| **Serve AVIF with WebP / JPEG fallback** | `<picture>` with `<source type="...">` | Browser inspects internal codec support before fetching. |
| **Different crops on mobile vs desktop** | `<picture>` with `<source media="...">` | Required because different aspect ratios alter visual composition. |
| **Dark mode vs Light mode images** | `<picture>` with `(prefers-color-scheme)` | Declarative image swapping without JavaScript event listeners. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is an inner <code>&lt;img&gt;</code> element strictly required inside a <code>&lt;picture&gt;</code> element?</strong></summary>

The `<picture>` element is not a visual rendering element; it is an invisible container that provides alternate source options. The inner `<img>` element is what the browser engine actually instantiates in the render tree, styles with CSS, binds event listeners to, and exposes to assistive technologies via the accessibility tree. Without an inner `<img>`, nothing will be displayed on screen.
</details>

<details>
<summary><strong>2. What is the fundamental difference between "Art Direction" and "Resolution Switching"?</strong></summary>

- **Resolution Switching** involves serving the exact same image content with the same aspect ratio and cropping, but at different physical pixel resolutions (e.g., 400px vs 1200px) to match display sizes or device pixel ratios. Handled via `<img srcset sizes>`.
- **Art Direction** involves altering the visual composition, aspect ratio, or framing of the image depending on viewport dimensions (e.g., showing a tight vertical crop of a person's face on mobile, and a wide panoramic landscape on desktop). Handled via `<picture>` and `<source media>`.
</details>

<details>
<summary><strong>3. How should <code>media="(min-width: ...)"</code> conditions be ordered inside a <code>&lt;picture&gt;</code> element?</strong></summary>

They must be ordered from **largest breakpoint to smallest** (descending order). The browser evaluates `<source>` tags from top to bottom and chooses the *first* one whose media query evaluates to true. If `media="(min-width: 500px)"` is placed above `media="(min-width: 1200px)"`, a 1400px screen will match the 500px query first and never evaluate the 1200px source.
</details>

<details>
<summary><strong>4. Can you specify different intrinsic <code>width</code> and <code>height</code> attributes on child <code>&lt;source&gt;</code> tags?</strong></summary>

Yes. Under the modern WHATWG HTML specification, `<source>` elements inside `<picture>` accept `width` and `height` attributes. This allows developers doing art direction across different aspect ratios (e.g., 16:9 on desktop, 1:1 on tablet, 4:5 on mobile) to communicate the exact intrinsic dimensions of each source, allowing the browser to recompute the CSS aspect ratio dynamically and prevent Cumulative Layout Shift (CLS).
</details>

<details>
<summary><strong>5. Why is placing the <code>alt</code> attribute on a <code>&lt;source&gt;</code> element an accessibility error?</strong></summary>

The `<source>` element does not represent a visual object or accessibility node in the DOM. Screen readers ignore `<source>` elements entirely and query only the rendered `<img>` element for accessible names. Putting `alt` on `<source>` is invalid HTML and leaves the rendered image unlabeled in the accessibility tree.
</details>

<details>
<summary><strong>6. How does browser format negotiation work with <code>&lt;source type="..."&gt;</code>?</strong></summary>

When the browser parses `<source type="image/avif">`, it calls its internal image decoding registry to verify if it has an active decoder for that MIME type. If supported, it accepts that source. If unsupported, it skips to the next child `<source>` until it finds a supported format or falls back to the default `<img>`.
</details>

<details>
<summary><strong>7. How can <code>&lt;picture&gt;</code> be used to switch images for system dark mode without JavaScript?</strong></summary>

By specifying the `prefers-color-scheme` CSS media feature inside the `<source media="...">` attribute:
```html
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="diagram-dark.svg">
  <img src="diagram-light.svg" alt="...">
</picture>
```
When the user's operating system toggles dark mode, the browser automatically switches the displayed image asset.
</details>

<details>
<summary><strong>8. When should you NOT use the <code>&lt;picture&gt;</code> element?</strong></summary>

You should not use `<picture>` when your only goal is serving different pixel resolutions of the same cropped image across different screen sizes. Using standard `<img srcset="..." sizes="...">` is cleaner, more performant, and avoids redundant HTML wrapper markup.
</details>

---

## 11. Practice Exercises

### Exercise 1: Modern Format Negotiation (Easy)
- **Task**: Author a `<picture>` element that offers AVIF and WebP formats, falling back to a standard JPEG image with accessible alt text and explicit dimensions of `600x400`.

<details>
<summary>View Solution</summary>

```html
<picture>
  <source srcset="/photos/forest.avif" type="image/avif" />
  <source srcset="/photos/forest.webp" type="image/webp" />
  <img 
    src="/photos/forest.jpg" 
    alt="Sunlight filtering through redwood pine trees in misty morning fog" 
    width="600" 
    height="400" 
    loading="lazy" 
    decoding="async" 
  />
</picture>
```
</details>

---

### Exercise 2: Responsive Art Direction Across 3 Breakpoints (Medium)
- **Task**: Create an art-directed picture element:
  - Wide landscape crop (`1920x800`) for desktop screens `1024px` and above.
  - Square crop (`800x800`) for tablet screens between `640px` and `1023px`.
  - Portrait crop (`400x600`) for mobile screens below `640px`.

<details>
<summary>View Solution</summary>

```html
<picture>
  <!-- Desktop: min-width 1024px -->
  <source 
    media="(min-width: 1024px)" 
    srcset="/banners/hero-desktop.webp" 
    width="1920" 
    height="800" 
  />

  <!-- Tablet: min-width 640px -->
  <source 
    media="(min-width: 640px)" 
    srcset="/banners/hero-tablet.webp" 
    width="800" 
    height="800" 
  />

  <!-- Mobile Fallback -->
  <img 
    src="/banners/hero-mobile.webp" 
    alt="Graphic designer working on a tablet in an open creative studio" 
    width="400" 
    height="600" 
    loading="lazy" 
    decoding="async" 
    style="width: 100%; height: auto;" 
  />
</picture>
```
</details>

---

### Exercise 3: Full Format and Art Direction Combination (Hard)
- **Task**: Build an enterprise hero component that provides:
  1. Desktop screens (`>= 1024px`): AVIF with WebP fallback in landscape (`1600x600`).
  2. Mobile screens (`< 1024px`): AVIF with WebP fallback in vertical portrait (`600x800`).
  3. Legacy fallback: Default JPEG on `<img>`.
  4. Ensure correct ordering of media queries and MIME types.

<details>
<summary>View Solution</summary>

```html
<picture class="enterprise-hero">
  <!-- Desktop AVIF -->
  <source 
    media="(min-width: 1024px)" 
    srcset="/hero/desktop.avif" 
    type="image/avif" 
    width="1600" 
    height="600" 
  />

  <!-- Desktop WebP -->
  <source 
    media="(min-width: 1024px)" 
    srcset="/hero/desktop.webp" 
    type="image/webp" 
    width="1600" 
    height="600" 
  />

  <!-- Mobile AVIF -->
  <source 
    srcset="/hero/mobile.avif" 
    type="image/avif" 
    width="600" 
    height="800" 
  />

  <!-- Mobile WebP -->
  <source 
    srcset="/hero/mobile.webp" 
    type="image/webp" 
    width="600" 
    height="800" 
  />

  <!-- Universal Legacy Fallback -->
  <img 
    src="/hero/fallback.jpg" 
    alt="Software engineering team collaborating in front of multi-screen monitoring station" 
    width="1600" 
    height="600" 
    fetchpriority="high"
    decoding="async" 
    class="hero-img"
  />
</picture>

<style>
  .hero-img {
    width: 100%;
    height: auto;
    object-fit: cover;
    display: block;
  }
</style>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                 <picture> ELEMENT CHEAT SHEET                                   |
+-------------------------------------------------------------------------------------------------+
| Syntax / Attribute       | Behavior / Rule                                                      |
+-------------------------------------------------------------------------------------------------+
| <picture>                | Invisible wrapper; executes selection algorithm                      |
| <source media="...">     | Media query filter (order from largest min-width to smallest)       |
| <source type="...">      | MIME format filter (order: AVIF -> WebP -> JPEG/PNG)                |
| <img src="..." alt="...">| MANDATORY final child; renders pixels, holds alt, receives styles    |
| width / height on source | Modern Baseline: Preserves aspect ratio during art direction         |
| prefers-color-scheme     | Allows declarative Dark Mode / Light Mode image swapping             |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [WHATWG HTML Living Standard: The `picture` element](https://html.spec.whatwg.org/multipage/embedded-content.html#the-picture-element)
- [MDN Web Docs: `<picture>`: The Picture element](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/picture)
- [web.dev: Art direction with the picture element](https://web.dev/learn/design/picture-element)
- [Smashing Magazine: Responsive Images Done Right with `<picture>`](https://www.smashingmagazine.com/)

---

[← Previous: Responsive Images in Depth](02-responsive-images-in-depth.md) | [Section Index](README.md) | [Next: iFrames & Sandboxing →](04-iframes-and-sandboxing.md)
