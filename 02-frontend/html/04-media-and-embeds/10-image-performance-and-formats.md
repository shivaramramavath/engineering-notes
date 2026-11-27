# Image Performance, Modern Codecs, and Core Web Vitals Optimization

| Metadata | Details |
| :--- | :--- |
| **Specification** | WHATWG HTML Living Standard (§4.8.4 The `img` element) / Web Platform Baseline |
| **Prerequisites** | `04-media-and-embeds/02-responsive-images-in-depth.md`, `04-media-and-embeds/03-picture-and-art-direction.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`loading="lazy"`, `decoding="async"`, `fetchpriority="high"`, AVIF, WebP) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | Chrome DevTools Lighthouse, WebPageTest, axe-core 4.9 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: The Ultimate LCP Hero Image Implementation](#example-1-the-ultimate-lcp-hero-image-implementation)
   - [Example 2: Below-the-Fold Lazy-Loaded Product Gallery](#example-2-below-the-fold-lazy-loaded-product-gallery)
   - [Example 3: Next-Gen Codec Negotiation (AVIF &rarr; WebP &rarr; JPEG)](#example-3-next-gen-codec-negotiation-avif--webp--jpeg)
   - [Example 4: Zero Cumulative Layout Shift (CLS) Card Component](#example-4-zero-cumulative-layout-shift-cls-card-component)
   - [Example 5: Client Hints Responsive Image Optimization](#example-5-client-hints-responsive-image-optimization)
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

Images account for over 60% of total transferred page weight across the global web. Unoptimized images are the leading cause of slow page loads, bloated cellular bandwidth usage, poor Core Web Vitals scores, and lost user conversions:
- **Largest Contentful Paint (LCP)**: In the vast majority of web pages, the LCP candidate is an above-the-fold hero image or featured banner. Bloated file sizes or delayed downloads directly harm search engine ranking.
- **Cumulative Layout Shift (CLS)**: Images loaded without declared dimensions force the browser to recalculate layout geometry after downloading, causing visible content jumps that frustrate users.
- **Main-Thread CPU Block**: Large images require intensive CPU decompression cycles, delaying time to interactive.

Modern HTML provides powerful built-in performance primitives: native lazy loading (`loading="lazy"`), asynchronous decoding (`decoding="async"`), priority signaling (`fetchpriority="high"`), and next-generation codecs (AVIF, WebP). 

Mastering image performance architecture enables developers to deliver visually stunning, high-resolution media while keeping transfer sizes minimal and Core Web Vitals scores green.

---

## 2. Core Concepts

### Modern Image Codec Comparison

| Format | Compression vs. JPEG | Transparency (Alpha)? | Animation? | Color Depth (HDR)? | Baseline Browser Status |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **AVIF** | **50% smaller** | Yes (Lossy & Lossless) | Yes (AVIS) | 10-bit / 12-bit HDR | **Widely Available** (Chrome 85+, Firefox 93+, Safari 16+) |
| **WebP** | **30% smaller** | Yes (Lossy & Lossless) | Yes | 8-bit SDR | **Widely Available** (100% universal support) |
| **JPEG** | Baseline (0%) | No | No | 8-bit SDR | Universal Legacy Fallback |
| **PNG** | Larger (Lossless only) | Yes (Lossless) | Rare (APNG) | 8-bit / 16-bit | Diagrams, sharp screenshots |
| **SVG** | Infinitely Scalable | Yes | Yes (CSS/SMIL) | Vector curves | Logos, UI icons, flat illustrations |

### Native Loading & Priority Attributes
1. **`loading="lazy"` vs `loading="eager"`**:
   - `lazy`: Defers network fetching until the image reaches a calculated distance threshold from the visible viewport.
   - `eager`: Fetches the image immediately upon parsing, regardless of where it sits on the page (default).
2. **`fetchpriority="high"` vs `fetchpriority="low"`**:
   - Informs the browser's network resource scheduler of the relative priority of the image.
   - `fetchpriority="high"` should be reserved **exclusively** for the LCP hero image above the fold.
3. **`decoding="async"` vs `decoding="sync"`**:
   - `async`: Decodes the compressed image data off the browser's main UI thread, preventing dropped frames and scrolling jank.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                  Browser Image Fetching & Rendering Pipeline                            |
+-----------------------------------------------------------------------------------------+

  1. Speculative Preload Scanner encounters <img>
       |
       +--> Inspects attributes:
            - Is fetchpriority="high"? -> Elevate HTTP/2 stream priority to Highest!
            - Is loading="lazy"?        -> Pause! Check if element is near viewport.
       |
       v
  2. Network Transmission (CDN & Codec):
     - AVIF/WebP payload streams into browser cache.
       |
       v
  3. Layout Allocation (Zero-CLS Phase):
     - Browser calculates: aspect-ratio = width / height (e.g., 16 / 9)
     - Reserves empty placeholder box in Layout Tree BEFORE pixels arrive!
     - CLS = 0.000!
       |
       v
  4. Decompression & Paint (decoding="async"):
     - Worker thread decompresses AVIF/JPEG byte stream into uncompressed RGBA bitmap.
     - Compositor layer pushes rasterized pixels to GPU frame buffer.
```

---

## 4. Syntax & Usage

```html
<!-- The Optimal LCP Hero Image -->
<link rel="preload" as="image" href="/hero.avif" fetchpriority="high" type="image/avif" />

<img 
  src="/hero.jpg" 
  srcset="/hero.avif 1x, /hero-2x.avif 2x" 
  alt="Modern solar-powered data center nestled in green hills" 
  width="1920" 
  height="1080" 
  fetchpriority="high" 
  decoding="async" 
  class="hero-img"
/>
```

> **Cross-Browser & Baseline Status Box**
>
> - `loading="lazy"`: **Baseline Widely Available** across all major engines since 2020.
> - `decoding="async"`: **Baseline Widely Available** since 2018.
> - `fetchpriority`: **Baseline Newly Available (2024)** across Chrome 101+, Safari 17.2+, Firefox 132+, Edge 101+.
> - AVIF format: **Baseline Widely Available**.

---

## 5. Practical Examples

### Example 1: The Ultimate LCP Hero Image Implementation
The industry gold standard pattern for optimizing the Largest Contentful Paint (LCP) element: preloading the resource, elevating network fetch priority, decoding asynchronously, and forbidding lazy loading.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Acme Global Aerospace</title>

  <!-- 
    Preload Scanner Acceleration:
    Tells the browser to start downloading the high-efficiency AVIF LCP image
    at the earliest possible millisecond in the head pipeline!
  -->
  <link 
    rel="preload" 
    as="image" 
    href="/assets/spacecraft-1200.avif" 
    type="image/avif" 
    fetchpriority="high" 
  />
</head>
<body>
  <main>
    <section class="hero-container">
      <!-- 
        LCP HERO IMAGE RULES:
        1. NEVER use loading="lazy" on the LCP image!
        2. Set fetchpriority="high"
        3. Set decoding="async"
        4. Supply explicit width & height to prevent CLS
      -->
      <picture>
        <source srcset="/assets/spacecraft-1200.avif" type="image/avif" />
        <source srcset="/assets/spacecraft-1200.webp" type="image/webp" />
        <img 
          src="/assets/spacecraft-1200.jpg" 
          alt="Orbital research vessel docking with international space laboratory" 
          width="1200" 
          height="675" 
          fetchpriority="high" 
          decoding="async" 
          class="hero-visual"
        />
      </picture>
      <h1>Pioneering Orbital Exploration</h1>
    </section>
  </main>

  <style>
    .hero-container {
      width: 100%;
      max-width: 1200px;
      margin: 0 auto;
    }
    .hero-visual {
      width: 100%;
      height: auto;
      aspect-ratio: 16 / 9;
      display: block;
      object-fit: cover;
    }
  </style>
</body>
</html>
```

---

### Example 2: Below-the-Fold Lazy-Loaded Product Gallery
Conserving mobile bandwidth by ensuring off-screen images download only when approaching the viewport.

```html
<section class="product-gallery">
  <h2>Recommended Gear</h2>

  <div class="gallery-grid">
    <article class="product-card">
      <!-- 
        BELOW-THE-FOLD RULES:
        1. loading="lazy"
        2. decoding="async"
        3. width & height declared for aspect ratio
      -->
      <img 
        src="/products/tent-600.webp" 
        srcset="/products/tent-300.webp 300w, /products/tent-600.webp 600w"
        sizes="(max-width: 600px) 100vw, 300px"
        alt="Four-person waterproof all-weather camping tent in orange" 
        width="600" 
        height="450" 
        loading="lazy" 
        decoding="async" 
        class="card-thumb"
      />
      <h3>Alpine Explorer 4P</h3>
      <p class="price">$349.00</p>
    </article>
  </div>
</section>

<style>
  .card-thumb {
    width: 100%;
    height: auto;
    aspect-ratio: 4 / 3;
    object-fit: cover;
    border-radius: 6px;
  }
</style>
```

---

### Example 3: Next-Gen Codec Negotiation (AVIF &rarr; WebP &rarr; JPEG)
Serving the highest compression format supported by the user's browser, saving over 60% in file weight.

```html
<picture>
  <!-- 1. AVIF: 45 KB (Superb quality, highest compression) -->
  <source srcset="/photos/mountain.avif" type="image/avif" />

  <!-- 2. WebP: 68 KB (Widely supported modern format) -->
  <source srcset="/photos/mountain.webp" type="image/webp" />

  <!-- 3. JPEG: 140 KB (Legacy universal baseline) -->
  <img 
    src="/photos/mountain.jpg" 
    alt="Snow-capped alpine peaks reflecting in calm glacial lake" 
    width="800" 
    height="500" 
    loading="lazy" 
    decoding="async" 
  />
</picture>
```

---

### Example 4: Zero Cumulative Layout Shift (CLS) Card Component
Preventing layout jank and jarring visual jumps by reserving exact vertical layout space before the image downloads.

```html
<article class="news-card">
  <!-- 
    The browser calculates aspect-ratio: 800 / 450 = 16:9.
    Even before a single byte of photo.webp downloads,
    the browser reserves the exact height in the document flow!
  -->
  <div class="card-media">
    <img 
      src="/news/quantum-800.webp" 
      alt="Quantum computing processor glowing in ultra-cold dilution refrigerator" 
      width="800" 
      height="450" 
      loading="lazy" 
      decoding="async" 
      class="news-img"
    />
  </div>
  <div class="card-body">
    <h3>Breakthrough in 100-Qubit Quantum Coherence</h3>
    <p>Researchers sustain record-breaking coherence times under commercial conditions.</p>
  </div>
</article>

<style>
  .news-card {
    max-width: 400px;
    border: 1px solid #e2e8f0;
    border-radius: 8px;
    overflow: hidden;
  }
  .news-img {
    width: 100%;
    height: auto;
    display: block;
  }
</style>
```

---

### Example 5: Client Hints Responsive Image Optimization
How modern CDNs (Cloudflare, Cloudinary, Fastly) dynamically deliver optimized images based on browser Client Hints headers.

```html
<!-- In HTML Document Head -->
<meta http-equiv="Accept-CH" content="DPR, Width, Viewport-Width" />

<!-- 
  When the browser requests this image, it sends HTTP request headers:
  DPR: 2.0
  Width: 360
  Viewport-Width: 412
  Accept: image/avif,image/webp,image/apng,*/*
  
  The intelligent image CDN automatically transcodes and returns 
  a 720px wide AVIF image on the fly!
-->
<img 
  src="/cdn/image-optimizer?src=hero.jpg" 
  sizes="(max-width: 600px) 100vw, 800px" 
  alt="Corporate headquarters skyline" 
  width="800" 
  height="450" 
  loading="lazy" 
  decoding="async" 
/>
```

---

## 6. Best Practices

- [ ] **NEVER put `loading="lazy"` on above-the-fold or LCP images**: Lazy-loading an LCP image delays its download until the main layout pass completes, severely hurting Core Web Vitals.
- [ ] **Always add `fetchpriority="high"` to the single primary LCP image**: Informs the browser network scheduler to prioritize this image over external scripts or non-critical fonts.
- [ ] **Always provide `width` and `height` attributes on every `<img>`**: Enables the browser engine to compute the intrinsic aspect ratio immediately, locking in a Cumulative Layout Shift (CLS) score of 0.
- [ ] **Use `decoding="async"` across all non-critical images**: Moves image byte decompression off the browser's main thread to prevent UI stuttering and scroll drops.
- [ ] **Adopt AVIF as the primary modern format with WebP/JPEG fallbacks**: AVIF delivers dramatic byte savings (often 20% smaller than WebP and 50% smaller than JPEG).
- [ ] **Preload the LCP hero image in `<head>`**: Pair `<link rel="preload" as="image" ...>` with `fetchpriority="high"` for mission-critical hero banners.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Applying `loading="lazy"` to the LCP hero image | **Critical** | Image download is delayed by hundreds of milliseconds; destroys LCP score. | Remove `loading="lazy"`; add `fetchpriority="high"`. |
| Omitting `width` and `height` attributes | **High** | Browser cannot calculate aspect ratio; triggers layout shift (high CLS). | Specify intrinsic `width` and `height` on `<img>`. |
| Adding `fetchpriority="high"` to multiple images | **Moderate** | Dilutes network scheduling priority; browser cannot determine which resource matters most. | Apply `fetchpriority="high"` to only ONE LCP image per page. |
| Serving oversized desktop assets to mobile phones | **High** | Wastes megabytes of mobile cellular data; increases memory overhead and crashes tabs. | Use `srcset` and `sizes` for resolution switching. |
| Using lossy formats (JPEG/AVIF) for text screenshots or line icons | **Moderate** | Compression artifacts blur text and sharp borders. | Use PNG or SVG for logos, text screenshots, and icons. |
| Forgetting fallback `<img>` inside `<picture>` | **Critical** | Nothing renders to the screen in any browser. | Always include a valid `<img src="..." alt="...">` inside `<picture>`. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Core Web Vitals Optimization Checklist
- **LCP (< 2.5 seconds)**:
  - Serve image in AVIF or WebP.
  - Preload image in document `<head>` with `fetchpriority="high"`.
  - Ensure image is hosted on the same origin or an HTTP/3 CDN with `preconnect`.
- **CLS (< 0.1)**:
  - Supply explicit `width` and `height` attributes matching intrinsic aspect ratio.
  - Set CSS `aspect-ratio` on image wrappers.

---

## 9. Comparison / When to Use What

| Format / Strategy | Compression Ratio | Main Benefit | When to Use |
| :--- | :--- | :--- | :--- |
| **AVIF** | Very High (50% vs JPEG) | Superior compression, HDR support | Preferred modern format for photos and hero banners. |
| **WebP** | High (30% vs JPEG) | 100% universal browser support | Secondary fallback format for all web imagery. |
| **SVG** | Vector | Lossless infinite scaling, tiny size | Logos, icons, interactive charts. |
| **PNG** | Lossless | Sharp crisp edges, no compression fuzz | Screenshots with small text, transparent graphics. |
| **JPEG** | Moderate | Universal compatibility | Ultimate legacy fallback format. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is applying <code>loading="lazy"</code> to an above-the-fold hero image considered a critical performance anti-pattern?</strong></summary>

The browser's speculative preload scanner starts downloading standard images immediately while parsing raw HTML. When an image has `loading="lazy"`, the browser intentionally defers network fetching until the main parser constructs the DOM and executes layout calculations to verify where the image sits relative to the viewport. For an above-the-fold image, this unnecessary delay pushes back the start of the image download by hundreds of milliseconds, directly degrading the Largest Contentful Paint (LCP) metric.
</details>

<details>
<summary><strong>2. How do the <code>width</code> and <code>height</code> attributes prevent Cumulative Layout Shift (CLS) on responsive images?</strong></summary>

Modern browser rendering engines use the HTML `width` and `height` attributes to compute the element's default CSS aspect ratio (`aspect-ratio: width / height`) before any image bytes are downloaded. When responsive CSS (such as `width: 100%; height: auto;`) is applied, the browser reserves the exact vertical height in the document layout, preventing layout reflows and content jumps when the image finally renders.
</details>

<details>
<summary><strong>3. What does <code>fetchpriority="high"</code> do, and how does it interact with the browser's resource scheduler?</strong></summary>

`fetchpriority="high"` signals to the browser engine that a resource is of critical visual importance. In the browser's HTTP/2 or HTTP/3 multiplexed network pipeline, resources normally have default priorities (e.g., stylesheets are highest, scripts are high, images are low). Elevating an image to `fetchpriority="high"` instructs the network scheduler to request the image payload concurrently with or immediately after critical CSS, drastically improving LCP.
</details>

<details>
<summary><strong>4. What is the difference between <code>decoding="async"</code> and <code>decoding="sync"</code>?</strong></summary>

- `decoding="sync"`: The browser decompresses the image byte stream on the main JavaScript/UI thread. For large, high-resolution images, this can block user interactions and cause frame rate drops.
- `decoding="async"`: The browser decodes the image off the main thread on a background worker thread, only painting the rasterized pixels when ready without interfering with main-thread execution or scrolling smoothness.
</details>

<details>
<summary><strong>5. Why is AVIF generally superior to WebP for photographic content?</strong></summary>

AVIF is derived from the modern AV1 video codec keyframe architecture. It utilizes advanced intra-frame prediction algorithms, directional transforms, and chroma subsampling enhancements that allow it to achieve 20% to 50% smaller file sizes than WebP at identical perceptual visual quality (measured via SSIM/VMAF), especially in complex gradients, dark shadows, and fine textures.
</details>

<details>
<summary><strong>6. When should you choose PNG over WebP or AVIF?</strong></summary>

PNG remains valuable for technical screenshots containing sharp typography, pixel art, or diagrams where lossy compression artifacts would make text blurry or unreadable, and where SVG cannot be used. However, lossless WebP and lossless AVIF often achieve 20-30% smaller file sizes than PNG even for lossless data.
</details>

<details>
<summary><strong>7. How does <code>&lt;link rel="preload" as="image"&gt;</code> work alongside responsive images?</strong></summary>

You can preload responsive images by supplying `imagesrcset` and `imagesizes` attributes on the `<link>` element:
```html
<link rel="preload" as="image" imagesrcset="hero-400.webp 400w, hero-800.webp 800w" imagesizes="100vw" />
```
This enables the browser preload scanner to calculate the exact candidate file to fetch before reaching the `<body>` markup.
</details>

<details>
<summary><strong>8. What are Client Hints, and how do they automate responsive image delivery?</strong></summary>

Client Hints are HTTP request headers (such as `DPR`, `Width`, and `Viewport-Width`) sent by the browser to the web server or CDN. They inform the edge server of the client's exact screen pixel ratio and layout slot dimensions, allowing an image transformation CDN to resize, transcode, and compress the image automatically without needing verbose client-side `srcset` markup.
</details>

---

## 11. Practice Exercises

### Exercise 1: Optimized LCP Hero Element (Easy)
- **Task**: Author an HTML `<head>` and `<body>` snippet that preloads and renders an LCP hero banner with `fetchpriority="high"`, `decoding="async"`, intrinsic dimensions (`1920x1080`), and no lazy loading.

<details>
<summary>View Solution</summary>

```html
<head>
  <link rel="preload" as="image" href="/hero.webp" fetchpriority="high" type="image/webp" />
</head>
<body>
  <img 
    src="/hero.webp" 
    alt="Futuristic electric sports car parked on an open mountain road" 
    width="1920" 
    height="1080" 
    fetchpriority="high" 
    decoding="async" 
    style="width: 100%; height: auto; display: block;" 
  />
</body>
```
</details>

---

### Exercise 2: Responsive Card with Zero Layout Shift (Medium)
- **Task**: Create an article card with an image that has intrinsic dimensions of `800x600`. Use `srcset` with candidates for 400w and 800w, `sizes="100vw"`, `loading="lazy"`, and CSS ensuring an aspect ratio of 4:3.

<details>
<summary>View Solution</summary>

```html
<article class="feature-card">
  <img 
    src="/articles/ai-800.webp" 
    srcset="/articles/ai-400.webp 400w, /articles/ai-800.webp 800w" 
    sizes="(max-width: 600px) 100vw, 400px" 
    alt="Robotic arm assembling complex circuit board" 
    width="800" 
    height="600" 
    loading="lazy" 
    decoding="async" 
    class="card-img" 
  />
  <h3>Next-Gen Robotics</h3>
</article>

<style>
  .card-img {
    width: 100%;
    height: auto;
    aspect-ratio: 4 / 3;
    object-fit: cover;
    display: block;
  }
</style>
```
</details>

---

### Exercise 3: Complete Performance Audit & Refactoring (Hard)
- **Task**: Refactor the following poor-performing hero section to achieve maximum Core Web Vitals optimization (targeting LCP < 1.2s and CLS = 0):
  ```html
  <!-- Poor performing original -->
  <section class="banner">
    <img src="huge-banner.jpg" loading="lazy" style="width: 100%;">
  </section>
  ```

<details>
<summary>View Solution</summary>

**Refactored High-Performance Code:**

```html
<!-- In <head> -->
<link 
  rel="preload" 
  as="image" 
  href="/banner-1200.avif" 
  type="image/avif" 
  fetchpriority="high" 
/>

<!-- In <body> -->
<section class="banner">
  <picture>
    <!-- Modern AVIF format (50% compression savings) -->
    <source 
      type="image/avif" 
      srcset="/banner-600.avif 600w, /banner-1200.avif 1200w" 
      sizes="(max-width: 768px) 100vw, 1200px" 
    />
    <!-- WebP fallback -->
    <source 
      type="image/webp" 
      srcset="/banner-600.webp 600w, /banner-1200.webp 1200w" 
      sizes="(max-width: 768px) 100vw, 1200px" 
    />
    <!-- Baseline fallback with intrinsic dimensions and high priority -->
    <img 
      src="/banner-1200.jpg" 
      alt="Solar farm array operating in the desert under bright blue sky" 
      width="1200" 
      height="600" 
      fetchpriority="high" 
      decoding="async" 
      class="responsive-banner" 
    />
  </picture>
</section>

<style>
  .responsive-banner {
    width: 100%;
    height: auto;
    aspect-ratio: 2 / 1;
    display: block;
    object-fit: cover;
  }
</style>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                               IMAGE PERFORMANCE CHEAT SHEET                                     |
+-------------------------------------------------------------------------------------------------+
| Attribute / Feature      | Performance Impact                      | Best Practice              |
+-------------------------------------------------------------------------------------------------+
| fetchpriority="high"     | Elevates network stream scheduling priority| Use ONLY on LCP Hero image |
| loading="lazy"           | Defers download until near viewport     | Use on ALL below-fold imgs |
| decoding="async"         | Decompresses bitmap off the main thread | Apply to all non-blocking  |
| width / height           | Calculates default CSS aspect ratio     | MANDATORY on all <img>     |
| <picture> with AVIF      | Slashes payload by up to 50% vs JPEG    | Primary modern delivery    |
| <link rel="preload">     | Starts network fetch during HTML head   | Accelerates LCP start time |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [web.dev: Optimize Largest Contentful Paint (LCP)](https://web.dev/articles/optimize-lcp)
- [web.dev: Cumulative Layout Shift (CLS) Guide](https://web.dev/articles/cls)
- [MDN Web Docs: Priority Hints (`fetchpriority`)](https://developer.mozilla.org/en-US/docs/Web/API/HTMLImageElement/fetchPriority)
- [Cloudinary: Modern Image Formats (AVIF vs WebP vs JPEG XL)](https://cloudinary.com/guides/image-formats)

---

[← Previous: Media Accessibility](09-media-accessibility.md) | [Section Index](README.md) | [Next: Modern Media APIs →](11-modern-media-apis.md)
