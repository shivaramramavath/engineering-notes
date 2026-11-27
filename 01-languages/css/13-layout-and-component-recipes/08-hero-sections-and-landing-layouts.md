# Hero Sections and Landing Layouts in Modern CSS

| Topic | Module | Level | Baseline Status | Browsers Tested | Date | Prerequisites |
|---|---|---|---|---|---|---|
| Hero Sections & Landing Layouts | 13: Layout & Component Recipes | Intermediate / Advanced | Widely Available | Chrome 120+, Firefox 120+, Safari 17.4+ | 2026-03-31 | Grid, Flexbox, Fluid Typography, Gradients |

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
9. [Comparison: Hero Layout Architectures](#9-comparison-hero-layout-architectures)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

The hero section is the premier visual and cognitive entry point of a product website or landing page. It immediately communicates the value proposition, establishes brand authority, presents compelling media, and drives the primary conversion action (Call To Action).

Historically, hero sections suffered from recurring implementation problems:
- **Disjointed Breakpoints**: Authors wrote dozen-breakpoint media query ladders (`@media (min-width: 768px)`, `@media (min-width: 992px)`, `@media (min-width: 1200px)`) to resize headings and re-adjust paddings, resulting in jarring text reflows as viewports scaled.
- **Mobile Viewport Cutoff**: Using `height: 100vh` clipped primary CTA buttons behind mobile Safari and Chrome address bars.
- **Image Shift and CLS**: Full-bleed hero visuals and decorative graphics caused massive Cumulative Layout Shift (CLS) as high-resolution assets downloaded asynchronously.
- **Text Illegibility over Media**: Placing text directly over photography or video resulted in unreadable headlines when user environments varied or images loaded with different contrasts.

Modern CSS resolves these challenges natively through:
- **Mathematical Fluidity**: `clamp()` functions interpolate typography and spacing seamlessly between mobile and 4K displays without arbitrary media query steps.
- **Resilient Viewport Sizing**: `min-height: 100svh` and `100dvh` guarantee that conversion buttons remain visible above the mobile fold.
- **CSS Grid Alignments**: Two-column split heroes adapt from stacked single-column mobile arrangements to asymmetric desktop matrices with zero layout shift.

---

## 2. Core Concepts

### 1. The Anatomy of an High-Conversion Hero
A production hero section typically comprises four structural zones:
1. **The Value Ribbon**: Badge pill highlighting new features, announcements, or funding rounds.
2. **The Editorial Block**: Fluid H1 headline, explanatory lead paragraph, and conversion cluster (primary button, secondary video trigger, or inline email capture).
3. **The Visual Showcase**: Interactive product mockup, 3D canvas, code editor window, or media preview.
4. **The Social Proof Strip**: Client logo ribbon, customer ratings, or metric milestones.

```
+-------------------------------------------------------------------+
|  [Badge: Introducing v3.0 →]                                      |
|                                                                   |
|  Build Resilient Systems               +------------------------+ |
|  at Internet Scale                     | Interactive Code       | |
|                                        | Editor / Product UI    | |
|  Deploy microservices in milliseconds  | Mockup                 | |
|  with zero infrastructure overhead.    |                        | |
|                                        +------------------------+ |
|  [Get Started Free] [Book Demo]                                   |
+-------------------------------------------------------------------+
|  Trusted by 10,000+ teams: [Logo A] [Logo B] [Logo C] [Logo D]    |
+-------------------------------------------------------------------+
```

### 2. Fluid Clamping vs Step Discontinuity
Instead of jumping from `font-size: 28px` to `48px` at an arbitrary `768px` media query breakpoint, fluid typography uses:
```css
font-size: clamp(2.25rem, 1.25rem + 4vw, 4.5rem);
```
This scales smoothly across every intermediate screen resolution, eliminating jagged text wraps and preserving optimal line lengths (*measure*).

### 3. Contrast Defense with Layered Scrims
When overlaying text on imagery or video, raw transparent overlays (`rgba(0, 0, 0, 0.4)`) often wash out light text near high-contrast specular highlights. Layered linear gradients with cubic-bezier easing curves produce smooth, photorealistic contrast scrims that preserve WCAG AAA compliance.

---

## 3. How It Works Internally

During hero layout execution, browser engines optimize layout and compositing:

```
[Viewport: 1280x800]
         │
         ▼
[Compute Hero Container: min-height: 100dvh; display: grid; align-items: center]
         │
         ├── Col 1: minmax(0, 1.1fr) -> Editorial Block
         │     ├── Calculate fluid clamp(2.25rem, 1.25rem + 4vw, 4.5rem) -> Resolves to 56px
         │     └── Sizing resolves without text overflow
         │
         └── Col 2: minmax(0, 0.9fr) -> Visual Showcase (aspect-ratio: 16 / 10)
               ├── Reserves geometric space BEFORE image/video finishes downloading
               └── Eliminates Cumulative Layout Shift (CLS = 0)
```

By specifying explicit `aspect-ratio` tokens on the visual media container, the browser reserves the exact box dimensions during the initial HTML parser pass, guaranteeing zero layout shift once media loads.

---

## 4. Syntax & Usage

### Baseline Browser Support
| Feature | Baseline Status | Chrome / Edge | Firefox | Safari |
|---|---|---|---|---|
| CSS `clamp()` Function | Widely Available | 79+ | 75+ | 13.1+ |
| Dynamic Viewport Units (`dvh`, `svh`) | Widely Available | 108+ | 101+ | 15.4+ |
| CSS `aspect-ratio` | Widely Available | 88+ | 89+ | 15.0+ |
| CSS Grid Two-Column Split | Widely Available | 57+ | 52+ | 10.1+ |

### Core Split Hero Scaffolding
```css
.hero-section {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
  align-items: center;
  gap: clamp(2rem, 5vw, 5rem);
  min-height: min(100dvh, 850px);
  padding: clamp(2rem, 6vw, 6rem) clamp(1rem, 5vw, 4rem);
  max-width: 1400px;
  margin: 0 auto;
}

@media (max-width: 900px) {
  .hero-section {
    grid-template-columns: 1fr;
    min-height: auto;
    text-align: center;
  }
}
```

---

## 5. Practical Examples

### Example 1: Asymmetric Split Hero with Fluid Typography and Code Mockup
```html
<section class="hero-split">
  <div class="hero-editorial">
    <div class="pill-badge">
      <span class="pulse"></span> Engine v4.0 is live →
    </div>
    <h1 class="hero-title">Deploy Serverless Workflows in Seconds</h1>
    <p class="hero-lead">
      Automate distributed workflows across your cloud infrastructure with type-safe triggers, zero cold-starts, and instant rollback observability.
    </p>
    <div class="cta-cluster">
      <a href="#signup" class="btn btn-primary">Start Deploying Free</a>
      <a href="#demo" class="btn btn-secondary">Interactive Playground</a>
    </div>
  </div>

  <div class="hero-visual">
    <div class="code-window">
      <div class="window-bar">
        <span class="dot red"></span>
        <span class="dot yellow"></span>
        <span class="dot green"></span>
        <span class="window-title">workflow.ts</span>
      </div>
      <pre><code>import { serve } from "@engine/core";

export default serve(async (req) => {
  const result = await processPipeline(req);
  return Response.json({ status: 200, result });
});</code></pre>
    </div>
  </div>
</section>
```

```css
.hero-split {
  display: grid;
  grid-template-columns: 1.15fr 0.85fr;
  align-items: center;
  gap: clamp(2rem, 4vw, 4rem);
  min-height: 85dvh;
  max-width: 1280px;
  margin: 0 auto;
  padding: 3rem 1.5rem;
}

.hero-editorial {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 1.5rem;
}

.pill-badge {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  padding: 0.35rem 0.875rem;
  background: #eff6ff;
  border: 1px solid #bfdbfe;
  border-radius: 9999px;
  font-size: 0.875rem;
  font-weight: 500;
  color: #1d4ed8;
}

.pulse {
  width: 8px;
  height: 8px;
  background: #2563eb;
  border-radius: 50%;
  box-shadow: 0 0 0 0 rgba(37, 99, 235, 0.7);
  animation: pulse-ring 2s infinite;
}

@keyframes pulse-ring {
  0% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(37, 99, 235, 0.7); }
  70% { transform: scale(1); box-shadow: 0 0 0 8px rgba(37, 99, 235, 0); }
  100% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(37, 99, 235, 0); }
}

.hero-title {
  font-size: clamp(2.25rem, 1.25rem + 3.5vw, 4.25rem);
  font-weight: 800;
  line-height: 1.1;
  letter-spacing: -0.03em;
  color: #0f172a;
  margin: 0;
}

.hero-lead {
  font-size: clamp(1rem, 0.9rem + 0.5vw, 1.25rem);
  line-height: 1.6;
  color: #475569;
  max-width: 58ch;
  margin: 0;
}

.cta-cluster {
  display: flex;
  gap: 1rem;
  flex-wrap: wrap;
  width: 100%;
}

.btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 0.75rem 1.5rem;
  border-radius: 8px;
  font-weight: 600;
  text-decoration: none;
  font-size: 1rem;
  transition: all 150ms ease;
}

.btn-primary {
  background: #2563eb;
  color: #ffffff;
  border: 1px solid #1d4ed8;
}

.btn-primary:hover {
  background: #1d4ed8;
}

.btn-secondary {
  background: #ffffff;
  color: #334155;
  border: 1px solid #cbd5e1;
}

.btn-secondary:hover {
  background: #f8fafc;
}

/* Code Mockup Card */
.hero-visual {
  width: 100%;
  min-width: 0;
}

.code-window {
  background: #0f172a;
  border-radius: 12px;
  border: 1px solid #334155;
  box-shadow: 0 20px 40px rgba(0, 0, 0, 0.3);
  overflow: hidden;
}

.window-bar {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 0.75rem 1rem;
  background: #1e293b;
  border-bottom: 1px solid #334155;
}

.dot {
  width: 10px;
  height: 10px;
  border-radius: 50%;
}

.dot.red { background: #ef4444; }
.dot.yellow { background: #eab308; }
.dot.green { background: #22c55e; }

.window-title {
  margin-left: auto;
  font-size: 0.75rem;
  color: #94a3b8;
  font-family: monospace;
}

.code-window pre {
  margin: 0;
  padding: 1.25rem;
  overflow-x: auto;
  color: #e2e8f0;
  font-family: monospace;
  font-size: 0.875rem;
  line-height: 1.5;
}

@media (max-width: 900px) {
  .hero-split {
    grid-template-columns: 1fr;
    text-align: center;
  }
  .hero-editorial {
    align-items: center;
  }
  .cta-cluster {
    justify-content: center;
  }
}
```

---

### Example 2: Centered Full-Viewport Hero with Mesh Gradient Background
```html
<section class="hero-centered">
  <div class="mesh-backdrop"></div>
  <div class="hero-container">
    <h1 class="gradient-headline">Scale Your Architecture Beyond Limits</h1>
    <p class="hero-subtext">
      The universal distributed runtime that brings edge latency, multi-region database failover, and serverless compute together.
    </p>
    <div class="email-capture-form">
      <input type="email" placeholder="Enter your work email..." aria-label="Work email">
      <button type="submit">Request Access</button>
    </div>
    <span class="micro-copy">No credit card required. Free tier includes 100k requests/mo.</span>
  </div>
</section>
```

```css
.hero-centered {
  position: relative;
  min-height: 90dvh;
  display: flex;
  align-items: center;
  justify-content: center;
  overflow: hidden;
  padding: 4rem 1.5rem;
  text-align: center;
  background: #030712;
  color: #ffffff;
}

/* Subtle modern radial mesh gradient */
.mesh-backdrop {
  position: absolute;
  inset: 0;
  background-image: 
    radial-gradient(circle at 50% 20%, rgba(99, 102, 241, 0.25) 0%, transparent 60%),
    radial-gradient(circle at 80% 80%, rgba(236, 72, 153, 0.15) 0%, transparent 50%),
    radial-gradient(circle at 20% 70%, rgba(14, 165, 233, 0.15) 0%, transparent 50%);
  pointer-events: none;
}

.hero-container {
  position: relative;
  max-width: 840px;
  z-index: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 1.5rem;
}

.gradient-headline {
  font-size: clamp(2.5rem, 1.5rem + 4vw, 5rem);
  font-weight: 800;
  line-height: 1.05;
  letter-spacing: -0.03em;
  background: linear-gradient(180deg, #ffffff 30%, #94a3b8 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  margin: 0;
}

.hero-subtext {
  font-size: clamp(1.125rem, 1rem + 0.4vw, 1.35rem);
  line-height: 1.6;
  color: #94a3b8;
  max-width: 62ch;
  margin: 0;
}

/* Inline Form Cluster */
.email-capture-form {
  display: flex;
  width: 100%;
  max-width: 480px;
  background: rgba(255, 255, 255, 0.05);
  border: 1px solid rgba(255, 255, 255, 0.15);
  backdrop-filter: blur(8px);
  border-radius: 9999px;
  padding: 0.35rem;
  gap: 0.5rem;
}

.email-capture-form input {
  flex: 1;
  background: transparent;
  border: none;
  padding: 0.6rem 1rem;
  color: #ffffff;
  font-size: 1rem;
  outline: none;
}

.email-capture-form input:focus-visible {
  outline: 2px solid #3b82f6;
  border-radius: 9999px;
}

.email-capture-form input::placeholder {
  color: #64748b;
}

.email-capture-form button {
  background: #3b82f6;
  color: #ffffff;
  border: none;
  padding: 0.6rem 1.25rem;
  border-radius: 9999px;
  font-weight: 600;
  cursor: pointer;
  white-space: nowrap;
  transition: background 150ms ease;
}

.email-capture-form button:hover {
  background: #2563eb;
}

.micro-copy {
  font-size: 0.8125rem;
  color: #64748b;
}

@media (max-width: 600px) {
  .email-capture-form {
    flex-direction: column;
    border-radius: 12px;
    background: transparent;
    border: none;
    padding: 0;
  }
  .email-capture-form input {
    background: rgba(255, 255, 255, 0.08);
    border: 1px solid rgba(255, 255, 255, 0.2);
    border-radius: 8px;
    padding: 0.75rem 1rem;
  }
  .email-capture-form button {
    border-radius: 8px;
    padding: 0.75rem;
  }
}
```

---

### Example 3: Full-Bleed Video Background with Accessible Contrast Scrim
```html
<section class="hero-video-banner">
  <video class="bg-video" autoplay loop muted playsinline poster="/assets/hero-poster.webp">
    <source src="/assets/hero-loop.mp4" type="video/mp4">
  </video>
  
  <div class="video-scrim"></div>
  
  <div class="video-hero-content">
    <h1>Autonomous Edge Networks</h1>
    <p>Real-time routing intelligence across 300+ global edge datacenters.</p>
    <a href="#explore" class="btn btn-primary">Discover the Network</a>
  </div>
</section>
```

```css
.hero-video-banner {
  position: relative;
  min-height: 80dvh;
  display: flex;
  align-items: center;
  justify-content: center;
  overflow: hidden;
  color: #ffffff;
  text-align: center;
}

.bg-video {
  position: absolute;
  top: 50%;
  left: 50%;
  min-width: 100%;
  min-height: 100%;
  width: auto;
  height: auto;
  transform: translate(-50%, -50%);
  object-fit: cover;
  z-index: 0;
}

/* Layered Photometric Contrast Scrim */
.video-scrim {
  position: absolute;
  inset: 0;
  background: linear-gradient(
    180deg,
    rgba(15, 23, 42, 0.8) 0%,
    rgba(15, 23, 42, 0.5) 50%,
    rgba(15, 23, 42, 0.9) 100%
  );
  z-index: 1;
}

.video-hero-content {
  position: relative;
  z-index: 2;
  max-width: 720px;
  padding: 2rem 1rem;
}

.video-hero-content h1 {
  font-size: clamp(2.5rem, 1.5rem + 3.5vw, 4.5rem);
  margin-bottom: 1rem;
  text-shadow: 0 2px 10px rgba(0, 0, 0, 0.5);
}

.video-hero-content p {
  font-size: 1.25rem;
  color: #e2e8f0;
  margin-bottom: 2rem;
  text-shadow: 0 1px 4px rgba(0, 0, 0, 0.5);
}
```

---

### Example 4: Social Proof Marquee Strip with Infinite Pure CSS Animation
```html
<section class="social-proof-strip">
  <p class="proof-label">Trusted by high-velocity engineering teams globally</p>
  
  <div class="marquee-track" aria-hidden="true">
    <div class="marquee-group">
      <span>HyperScale</span>
      <span>CloudFrontier</span>
      <span>OmniBase</span>
      <span>VeloceNet</span>
      <span>TerraStack</span>
    </div>
    <!-- Duplicate group for infinite loop continuity -->
    <div class="marquee-group">
      <span>HyperScale</span>
      <span>CloudFrontier</span>
      <span>OmniBase</span>
      <span>VeloceNet</span>
      <span>TerraStack</span>
    </div>
  </div>
</section>
```

```css
.social-proof-strip {
  padding: 3rem 0;
  background: #ffffff;
  border-top: 1px solid #f1f5f9;
  border-bottom: 1px solid #f1f5f9;
  overflow: hidden;
  text-align: center;
}

.proof-label {
  font-size: 0.875rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  color: #94a3b8;
  margin-bottom: 1.75rem;
}

.marquee-track {
  display: flex;
  width: max-content;
  gap: 3rem;
  mask-image: linear-gradient(
    to right,
    transparent 0%,
    black 15%,
    black 85%,
    transparent 100%
  );
  -webkit-mask-image: linear-gradient(
    to right,
    transparent 0%,
    black 15%,
    black 85%,
    transparent 100%
  );
}

.marquee-group {
  display: flex;
  align-items: center;
  gap: 3rem;
  animation: marquee-scroll 25s linear infinite;
}

.marquee-group span {
  font-size: 1.25rem;
  font-weight: 700;
  color: #64748b;
  letter-spacing: -0.02em;
}

@keyframes marquee-scroll {
  from {
    transform: translateX(0);
  }
  to {
    transform: translateX(calc(-100% - 3rem));
  }
}

@media (prefers-reduced-motion: reduce) {
  .marquee-group {
    animation-play-state: paused;
  }
}
```

---

### Example 5: Bento Showcase Grid below Split Hero
```html
<div class="bento-hero-grid">
  <div class="bento-tile bento-main">
    <h3>Instant Global Deployment</h3>
    <p>Ship binaries to 40 regions in under 3 seconds.</p>
  </div>
  <div class="bento-tile">
    <h3>99.999% SLA</h3>
    <p>Zero downtime failover.</p>
  </div>
  <div class="bento-tile">
    <h3>Zero Trust</h3>
    <p>Hardware security modules.</p>
  </div>
</div>
```

```css
.bento-hero-grid {
  display: grid;
  grid-template-columns: 2fr 1fr;
  grid-template-rows: repeat(2, 160px);
  gap: 1.25rem;
  max-width: 1200px;
  margin: 2rem auto;
  padding: 0 1.5rem;
}

.bento-tile {
  background: #f8fafc;
  border: 1px solid #e2e8f0;
  border-radius: 12px;
  padding: 1.5rem;
  display: flex;
  flex-direction: column;
  justify-content: flex-end;
}

.bento-main {
  grid-row: span 2;
  background: linear-gradient(135deg, #1e293b 0%, #0f172a 100%);
  color: #ffffff;
  border-color: #334155;
}

.bento-tile h3 {
  margin: 0 0 0.5rem 0;
  font-size: 1.25rem;
}

.bento-tile p {
  margin: 0;
  color: #64748b;
  font-size: 0.95rem;
}

.bento-main p {
  color: #94a3b8;
}

@media (max-width: 768px) {
  .bento-hero-grid {
    grid-template-columns: 1fr;
    grid-template-rows: auto;
  }
  .bento-main {
    grid-row: span 1;
    min-height: 200px;
  }
}
```

---

## 6. Best Practices

1. **Maintain Optimal Headline Measure**: Cap heading line lengths at 20–25 characters and paragraphs at 50–65 characters (`max-width: 60ch`). Excessively wide headings impede rapid eye scanning.
2. **Always Use `aspect-ratio` on Visual Mockups**: Reserve layout volume for hero screenshots, videos, or 3D canvases to prevent Cumulative Layout Shift (CLS) when network assets load.
3. **Use Dynamic Viewport Units (`100dvh` or `100svh`)**: Ensure the primary CTA remains above the fold on mobile devices regardless of dynamic browser URL bars.
4. **Implement Photometric Scrims for Text-over-Media**: Never put raw text directly on dynamic photography or video without a multi-stop gradient scrim to guarantee WCAG 4.5:1 contrast across all frames.
5. **Honor `prefers-reduced-motion` on Marquees and Video**: Provide paused states or static fallbacks for continuous marquee animations and background video playback.
6. **Ensure Touch-Friendly CTAs**: Mobile conversion buttons must meet the 48x48px touch target threshold with ample finger tap spacing (`gap: 1rem`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom | Correct Architecture |
|---|---|---|---|
| **Fold Clipping on Mobile** | Using `height: 100vh` on hero container | Mobile Safari address bar covers CTA buttons | Use `min-height: 100dvh` or `100svh` |
| **Severe CLS on Image Load** | Visual mockup lacks declared width/height or `aspect-ratio` | Page jumps abruptly down when the 800KB mockup downloads | Apply `aspect-ratio: 16 / 9; width: 100%;` |
| **Jagged Breakpoint Shifts** | Hard-coding font sizes in multiple `@media` steps | Text abruptly pops between sizes while resizing browser | Implement fluid `clamp(2rem, 1rem + 3vw, 4.5rem)` |
| **Illegible Headline on Video** | Light text over un-scrimmed bright video frames | Headline is unreadable when video transitions to bright scene | Add multi-stop linear gradient `.video-scrim` overlay |
| **Infinite Marquee Jank** | Animating marquee via `margin-left` or `left` | Animation stutters and drops frames during scrolling | Animate `transform: translateX()` exclusively |
| **Single Column Overflow** | Unconstrained flex child without `min-width: 0` | Wide code snippet forces horizontal scroll on mobile | Assign `min-width: 0` to editorial and code containers |

---

## 8. Performance, Accessibility & Browser Support

### Performance Safeguards
- **LCP (Largest Contentful Paint) Prioritization**: If the hero image is the LCP candidate, preload it in HTML `<link rel="preload" as="image" href="...">` and mark the `<img>` with `fetchpriority="high"`. Never lazy-load the hero image (`loading="eager"`).
- **Video Background Optimization**: Mute background videos (`muted`), include `playsinline`, and provide a compressed WebP `poster` image to display while media buffers.

### Accessibility Matrix
- **Semantic Hierarchy**: The primary hero heading MUST be an `<h1>`. Only one `<h1>` should exist per landing page.
- **Skip Links**: Include a skip navigation link so keyboard users can bypass top navigation directly to the hero CTA.
- **Video Fallbacks**: Background videos must be decorative only (`aria-hidden="true"` or paired with full textual descriptions).

---

## 9. Comparison: Hero Layout Architectures

| Criteria | Split Grid Hero | Centered Single-Col Hero | Full-Bleed Video Hero | Bento Grid Hero |
|---|---|---|---|---|
| **Primary Strength** | Product demo + value copy | High-concept brand authority | Emotional / atmospheric impact | Feature-rich SaaS capabilities |
| **Mobile Adaptability** | Stacks Col 1 over Col 2 | Natural column shrink | Crops video edges via `cover` | Re-orders grid into vertical list |
| **LCP Performance** | Fast (Static code/mockup) | Fastest (Text-driven) | Slowest (Requires video buffering) | Fast (Modular lightweight cards) |
| **Conversion Focus** | Direct (Lead + CTA + Demo) | High (Single focal point) | Moderate (Visual immersion) | High (Multiple targeted value points) |
| **Baseline Support** | Widely Available | Widely Available | Widely Available | Widely Available |

---

## 10. Interview Questions

<details>
<summary><strong>1. How do you construct a fluid typography equation using `clamp()` and how does it prevent media query bloat?</strong></summary>

**Answer:**
A fluid clamp equation follows the syntax: `font-size: clamp(MIN, VAL, MAX)`.
- `MIN`: The minimum font size on small mobile screens (e.g., `2rem`).
- `MAX`: The maximum font size on large desktop screens (e.g., `4.5rem`).
- `VAL`: The dynamic rate of scaling, combining a fixed base with viewport units (e.g., `1rem + 3.5vw`).

When the viewport is 400px wide, `1rem (16px) + 3.5vw (14px) = 30px` (clamped to `2rem` / 32px). As the viewport expands, font size increases smoothly without sudden jumps, eliminating the need for 5+ media queries.
</details>

<details>
<summary><strong>2. Why should hero images NEVER have `loading="lazy"` applied?</strong></summary>

**Answer:**
Hero images are almost universally the **Largest Contentful Paint (LCP)** element on a landing page. When `loading="lazy"` is applied, the browser delays downloading the image until layout is calculated and the viewport position is verified. This adds hundreds of milliseconds of delay to the LCP metric, penalizing Core Web Vitals scores. Hero images should always have `loading="eager"` and `fetchpriority="high"`.
</details>

<details>
<summary><strong>3. How does `aspect-ratio` prevent Cumulative Layout Shift (CLS) in hero sections?</strong></summary>

**Answer:**
When an image or iframe loads asynchronously without dimensions, the browser initially renders it with a height of 0px. When the asset finishes downloading, the browser calculates its intrinsic dimensions and abruptly pushes all content below it down the page, causing high CLS.

By declaring `aspect-ratio: 16 / 9; width: 100%;`, the browser reserves the exact proportional box height during initial layout construction before any image bytes have downloaded, ensuring 0 layout shift.
</details>

<details>
<summary><strong>4. How do you create an infinite smooth marquee logo scroll in pure CSS without JavaScript?</strong></summary>

**Answer:**
1. Create a container with `overflow: hidden` and `display: flex`.
2. Inside the container, render two identical groups of logos side-by-side (`<div class="group">...</div>`).
3. Animate each group with `@keyframes scroll { from { transform: translateX(0); } to { transform: translateX(-100%); } }`.
4. Set `animation: scroll 20s linear infinite`. As group 1 translates off-screen, group 2 seamlessly occupies its space, producing a perpetual loop without gaps or JavaScript timer overhead.
</details>

<details>
<summary><strong>5. What is the difference between `100dvh` and `100svh` when designing hero sections for mobile?</strong></summary>

**Answer:**
- `100svh` (Small Viewport Height): Assumes dynamic mobile browser interface bars (URL bar, navigation tray) are fully expanded. Designing with `min-height: 100svh` guarantees that all hero content and CTA buttons fit on-screen without requiring scrolling even before the user touches the display.
- `100dvh` (Dynamic Viewport Height): Dynamically recalculates as browser toolbars collapse during scroll. If used on a fixed hero, content can resize as the user begins to scroll.
For static hero landing folds, `100svh` or `min(100dvh, 800px)` provides the safest layout stability.
</details>

<details>
<summary><strong>6. How do you achieve accessible text contrast over arbitrary background images using CSS gradients?</strong></summary>

**Answer:**
By overlaying a multi-stop linear gradient scrim between the image and the text:
```css
.hero-scrim {
  background: linear-gradient(
    to bottom,
    rgba(0, 0, 0, 0.7) 0%,
    rgba(0, 0, 0, 0.4) 40%,
    rgba(0, 0, 0, 0.8) 100%
  );
}
```
This guarantees that regardless of whether the underlying photo contains white clouds, bright reflections, or dark shadows, the calculated contrast ratio between the text and background meets the WCAG AA minimum of 4.5:1.
</details>

<details>
<summary><strong>7. How does CSS `text-wrap: balance` improve landing page headlines?</strong></summary>

**Answer:**
`text-wrap: balance` instructs the browser layout engine to distribute text evenly across lines, preventing single-word typographic "widows" or awkward ragged lines. This produces visually symmetrical, publication-quality headlines automatically across every screen width without requiring manual `<br>` tags.
</details>

<details>
<summary><strong>8. How should high contrast mode (`forced-colors: active`) be handled on gradient text headlines?</strong></summary>

**Answer:**
Gradient text is created using `-webkit-background-clip: text` and `-webkit-text-fill-color: transparent`. Under `forced-colors: active`, transparent text will cause headlines to completely disappear.

To resolve this:
```css
@media (forced-colors: active) {
  .gradient-headline {
    -webkit-text-fill-color: CanvasText;
    background: none;
    color: CanvasText;
  }
}
```
This restores visible OS system text colors in high-contrast accessibility modes.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Fluid Hero Headline with `text-wrap: balance`
**Task**: Style an H1 headline that scales smoothly from 32px on mobile to 64px on desktop, preventing typographic widows using modern CSS.
<details>
<summary>View Verified Solution</summary>

```css
.hero-h1 {
  font-size: clamp(2rem, 1rem + 3vw, 4rem);
  font-weight: 800;
  line-height: 1.1;
  letter-spacing: -0.02em;
  text-wrap: balance;
  max-width: 20ch;
  color: #0f172a;
}
```
</details>

---

### Exercise 2: Responsive Split-to-Stack Two-Column Hero
**Task**: Create a two-column hero layout where the text is on the left and the image is on the right. When the screen drops below 840px, stack them with the image appearing *below* the text.
<details>
<summary>View Verified Solution</summary>

```css
.hero-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  align-items: center;
  gap: 3rem;
  max-width: 1200px;
  margin: 0 auto;
  padding: 3rem 1.5rem;
}

@media (max-width: 840px) {
  .hero-grid {
    grid-template-columns: 1fr;
    text-align: center;
  }
}
```
</details>

---

### Exercise 3: Zero-CLS Video Background Container
**Task**: Style a video container that spans 100% width, maintains a 16:9 aspect ratio, and clips the video neatly using `object-fit: cover`.
<details>
<summary>View Verified Solution</summary>

```css
.video-container {
  width: 100%;
  aspect-ratio: 16 / 9;
  position: relative;
  overflow: hidden;
  border-radius: 12px;
  background-color: #0f172a; /* Placeholder color before video starts */
}

.video-container video {
  width: 100%;
  height: 100%;
  object-fit: cover;
  display: block;
}
```
</details>

---

### Exercise 4: Glassmorphism Hero Metric Card
**Task**: Create a floating metric card overlaid on a colorful hero background with a frosted glass backdrop filter.
<details>
<summary>View Verified Solution</summary>

```css
.metric-glass-card {
  background: rgba(255, 255, 255, 0.1);
  border: 1px solid rgba(255, 255, 255, 0.2);
  backdrop-filter: blur(12px);
  -webkit-backdrop-filter: blur(12px);
  border-radius: 12px;
  padding: 1.5rem;
  color: #ffffff;
  box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.2);
}
```
</details>

---

### Exercise 5: Accessible Single-Line Newsletter Bar
**Task**: Build an inline subscription form where the input stretches to fill space and the submit button does not shrink, collapsing vertically on mobile screens.
<details>
<summary>View Verified Solution</summary>

```html
<form class="subscribe-bar">
  <input type="email" placeholder="you@domain.com" required aria-label="Email Address">
  <button type="submit">Get Started</button>
</form>
```

```css
.subscribe-bar {
  display: flex;
  gap: 0.5rem;
  width: 100%;
  max-width: 440px;
}

.subscribe-bar input {
  flex: 1;
  min-width: 0;
  padding: 0.75rem 1rem;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  font-size: 1rem;
}

.subscribe-bar button {
  flex-shrink: 0;
  padding: 0.75rem 1.5rem;
  background: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
}

@media (max-width: 480px) {
  .subscribe-bar {
    flex-direction: column;
  }
}
```
</details>

---

### Exercise 6: Subtle Mesh Glow Animation
**Task**: Animate a glowing colored radial gradient orb in the background of a dark-mode hero section.
<details>
<summary>View Verified Solution</summary>

```css
.glowing-orb {
  position: absolute;
  width: 500px;
  height: 500px;
  background: radial-gradient(circle, rgba(99, 102, 241, 0.3) 0%, transparent 70%);
  border-radius: 50%;
  filter: blur(60px);
  pointer-events: none;
  animation: float-glow 8s ease-in-out infinite alternate;
}

@keyframes float-glow {
  0% { transform: translate(-20px, -20px) scale(0.9); }
  100% { transform: translate(30px, 40px) scale(1.1); }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+------------------------------------------------------------------------------------+
|                       HERO & LANDING LAYOUTS CHEAT SHEET                           |
+------------------------------------------------------------------------------------+
| 1. Fluid Typography:                                                               |
|    font-size: clamp(2rem, 1rem + 3.5vw, 4.5rem); text-wrap: balance;              |
+------------------------------------------------------------------------------------+
| 2. Zero-CLS Aspect Ratio:                                                          |
|    aspect-ratio: 16 / 9; width: 100%; object-fit: cover;                           |
+------------------------------------------------------------------------------------+
| 3. Mobile Fold Protection:                                                         |
|    min-height: min(100dvh, 850px); /* or min-height: 100svh; */                   |
+------------------------------------------------------------------------------------+
| 4. Contrast Scrim Overlay:                                                         |
|    background: linear-gradient(180deg, rgba(0,0,0,0.8), rgba(0,0,0,0.4));          |
+------------------------------------------------------------------------------------+
| 5. Continuous CSS Marquee:                                                         |
|    @keyframes scroll { from { transform: translateX(0); }                          |
|                        to { transform: translateX(-100%); } }                      |
+------------------------------------------------------------------------------------+
| 6. Squeeze-Proof Inline Form:                                                      |
|    input { flex: 1; min-width: 0; } button { flex-shrink: 0; }                     |
+------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [W3C CSS Values and Units Module Level 4: `clamp()`](https://www.w3.org/TR/css-values-4/#funcdef-clamp)
- [MDN Web Docs: CSS `text-wrap`](https://developer.mozilla.org/en-US/docs/Web/CSS/text-wrap)
- [web.dev: Optimize Largest Contentful Paint (LCP)](https://web.dev/articles/optimize-lcp)
- [web.dev: Designing Accessible Video Backgrounds](https://web.dev/articles/accessible-video-backgrounds)
- [MDN Web Docs: CSS `aspect-ratio`](https://developer.mozilla.org/en-US/docs/Web/CSS/aspect-ratio)

---

## Navigation
- **Previous**: [Modals, Tooltips, and Popovers](file:///e:/Learning/engineering-notes/01-languages/css/13-layout-and-component-recipes/07-modals-tooltips-and-popovers.md)
- **Next**: [Forms and Form Layouts](file:///e:/Learning/engineering-notes/01-languages/css/13-layout-and-component-recipes/09-forms-layouts.md)
