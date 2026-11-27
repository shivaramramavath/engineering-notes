# Performance Hints, Speculation Rules API, and Instant Navigations

| Metadata | Details |
| :--- | :--- |
| **Specification** | W3C Speculation Rules / W3C Priority Hints / W3C Resource Hints |
| **Prerequisites** | `05-head-metadata-and-seo/05-link-rel-values-and-resource-hints.md`, `05-head-metadata-and-seo/06-script-loading-strategies.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Newly Available (Speculation Rules API Baseline 2024 / Chrome 109+, Edge 109+, Safari 18+ preview) |
| **Browsers Tested** | Chrome 124+, Edge 124+, Safari 17.4+, Firefox Nightly |
| **Tested With** | Chrome DevTools Application > Speculative Loads panel, WebPageTest |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Basic List-Based Prerendering for Instant Navigations](#example-1-basic-list-based-prerendering-for-instant-navigations)
   - [Example 2: Document Rule Prerendering on Hover (`moderate` eagerness)](#example-2-document-rule-prerendering-on-hover-moderate-eagerness)
   - [Example 3: Comprehensive Multi-Tier Speculation Rules (Prefetch vs. Prerender)](#example-3-comprehensive-multi-tier-speculation-rules-prefetch-vs-prerender)
   - [Example 4: Fine-Tuned Resource Priority Signaling with `fetchpriority`](#example-4-fine-tuned-resource-priority-signaling-with-fetchpriority)
   - [Example 5: Inspecting Speculative Loads in Chrome DevTools](#example-5-inspecting-speculative-loads-in-chrome-devtools)
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

For decades, navigating between web pages carried an unavoidable delay: users clicked a link and waited while the browser resolved DNS, established a TLS connection, downloaded the HTML document, requested stylesheets, and executed JavaScript. Even the most highly optimized websites experienced a 200–800ms navigation pause.

The **Speculation Rules API** represents the holy grail of web performance engineering: **Instant, 0-millisecond page navigations**.

Rather than relying on legacy, inflexible resource hints (`<link rel="prefetch">` or deprecated `<link rel="prerender">`), the Speculation Rules API uses a declarative JSON configuration block (`<script type="speculationrules">`) to direct the browser engine to **speculatively prefetch or completely prerender** upcoming pages in a hidden background process. 

When the user finally clicks the link, the page is not loaded from the network—it is instantly swapped into the active viewport with **zero loading delay**, unlocking true instant app-like navigation across standard multi-page websites.

---

## 2. Core Concepts

### Prefetch vs. Prerender in Speculation Rules
- **`"source": "list"` or `"document"`**:
  - `list`: Explicitly lists exact URLs to speculate on.
  - `document`: Automatically discovers links matching CSS selectors or URL patterns present in the DOM.
- **`"action": "prefetch"`**:
  - Downloads the HTML document body and places it into the HTTP cache.
  - Does NOT execute JavaScript or parse subresources. Low memory footprint.
- **`"action": "prerender"`**:
  - **Full background page execution**. The browser boots an invisible background renderer process, downloads the HTML, constructs the DOM and CSSOM, fetches all images and stylesheets, and executes JavaScript.
  - Clicking the link activates the prerendered page instantaneously with 0ms LCP!

### Eagerness Levels
Controls *when* the browser triggers speculation:
1. **`immediate`**: Triggered immediately when the rule is parsed (ideal for predictable next-step links).
2. **`eager`**: Triggered with very high confidence, regardless of user interaction.
3. **`moderate`**: **The sweet spot for performance**. Triggered only when the user shows intent (hovering a cursor over a link for > 200ms or on `pointerdown`).
4. **`conservative`**: Triggered only on mouse down / touch start immediately preceding a click.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                  Speculation Rules Prerendering Lifecycle                              |
+-----------------------------------------------------------------------------------------+

  User views Article Page (https://blog.com/post-1)
       |
       v
  Browser parses: <script type="speculationrules">
       |
       +--> Rule: Prerender any link matching href="/post-2" on hover (moderate)
       |
  User hovers cursor over: <a href="/post-2">Next Article</a> (for > 200ms)
       |
       v
  [ Background Invisible Renderer Process Spawned ]
  - Fetches /post-2 over HTTP/3
  - Constructs DOM & CSSOM
  - Downloads hero.avif and main.css
  - Executes app.js in background context!
  - Pauses execution before paint or audio playback!
       |
       v
  User clicks the link!
       |
       v
  [ Instant Activation (0ms Latency) ]
  - Background renderer instantly swapped into foreground viewport!
  - LCP is achieved at 0ms!
  - Navigation is perceived as completely instantaneous!
```

---

## 4. Syntax & Usage

```html
<head>
  <meta charset="utf-8" />
  <title>Fast Documentation</title>

  <!-- Speculation Rules JSON Script Block -->
  <script type="speculationrules">
  {
    "prerender": [
      {
        "source": "list",
        "urls": ["/docs/getting-started", "/docs/installation"],
        "eagerness": "immediate"
      }
    ]
  }
  </script>
</head>
```

> **Cross-Browser & Baseline Status Box**
>
> - Speculation Rules API: **Baseline Newly Available (2024)** across Chrome 109+, Edge 109+, and under active implementation in Safari (Technology Preview 18+) and Firefox Nightly.
> - `fetchpriority` attribute: **Baseline Newly Available (2024)** across all major engines.

---

## 5. Practical Examples

### Example 1: Basic List-Based Prerendering for Instant Navigations
Preloading the next obvious step in a checkout or registration funnel with instant 0ms transitions.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Step 1: Account Setup | Acme Cloud</title>

  <!-- 
    List-based Prerendering:
    Tells the browser to prerender Step 2 in the background immediately!
  -->
  <script type="speculationrules">
  {
    "prerender": [
      {
        "source": "list",
        "urls": ["/signup/step-2"],
        "eagerness": "immediate"
      }
    ]
  }
  </script>
</head>
<body>
  <main>
    <h1>Create Your Account</h1>
    <form action="/signup/step-2" method="get">
      <label for="email">Work Email:</label>
      <input type="email" id="email" required />
      <button type="submit">Proceed to Step 2 &rarr;</button>
    </form>
  </main>
</body>
</html>
```

---

### Example 2: Document Rule Prerendering on Hover (`moderate` eagerness)
Automatically prerenders any blog article link hovered by a user without hardcoding specific URLs.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>Tech Blog Home</title>

  <!-- 
    Document Rules with Moderate Eagerness:
    - Scans the DOM for any <a> tag whose href starts with "/articles/"
    - Excludes URLs containing "/logout" or "/admin"
    - Initiates background prerender ONLY when user hovers for > 200ms
  -->
  <script type="speculationrules">
  {
    "prerender": [
      {
        "source": "document",
        "where": {
          "and": [
            { "href_matches": "/articles/*" },
            { "not": { "href_matches": "/articles/draft-*" } }
          ]
        },
        "eagerness": "moderate"
      }
    ]
  }
  </script>
</head>
<body>
  <nav>
    <ul>
      <li><a href="/articles/kubernetes-scaling">Kubernetes Scaling Guide</a></li>
      <li><a href="/articles/go-concurrency">Deep Dive into Go Channels</a></li>
    </ul>
  </nav>
</body>
</html>
```

---

### Example 3: Comprehensive Multi-Tier Speculation Rules (Prefetch vs. Prerender)
Prerendering high-probability links while prefetching lower-probability links to balance memory and server load.

```html
<script type="speculationrules">
{
  "prerender": [
    {
      "source": "document",
      "where": {
        "selector_matches": ".prerender-link"
      },
      "eagerness": "moderate"
    }
  ],
  "prefetch": [
    {
      "source": "document",
      "where": {
        "selector_matches": ".prefetch-link"
      },
      "eagerness": "conservative"
    }
  ]
}
</script>

<div class="article-footer">
  <!-- Prerendered on hover (0ms navigation!) -->
  <a href="/next-chapter" class="prerender-link">Read Next Chapter &rarr;</a>

  <!-- Prefetched on mouse down (lightweight resource save) -->
  <a href="/all-chapters" class="prefetch-link">Browse Full Index</a>
</div>
```

---

### Example 4: Fine-Tuned Resource Priority Signaling with `fetchpriority`
Demonstrating how `fetchpriority` coordinates network traffic between critical hero imagery and deferred scripts.

```html
<head>
  <!-- Highest priority preload: The critical LCP candidate -->
  <link 
    rel="preload" 
    as="image" 
    href="/hero-banner.avif" 
    type="image/avif" 
    fetchpriority="high" 
  />

  <!-- Low priority preload: An offscreen illustration needed later -->
  <link 
    rel="preload" 
    as="image" 
    href="/footer-illustration.webp" 
    type="image/webp" 
    fetchpriority="low" 
  />
</head>
<body>
  <!-- Hero Image receives High Network Priority -->
  <img 
    src="/hero-banner.avif" 
    alt="Corporate Campus" 
    width="1200" 
    height="600" 
    fetchpriority="high" 
    decoding="async" 
  />

  <!-- Tracking script explicitly downgraded to lowest network priority -->
  <script 
    async 
    src="/analytics.js" 
    fetchpriority="low"
  ></script>
</body>
```

---

### Example 5: Inspecting Speculative Loads in Chrome DevTools
How developers verify that speculation rules are running and inspect background prerender state.

```html
<script>
  // Programmatic detection of whether the current page was loaded via prerendering
  if (document.prerendering) {
    console.log('Page is currently prerendering in an invisible background tab!');
    
    // Defer heavy analytics or audio until the user actually activates the page!
    document.addEventListener('prerenderingchange', () => {
      console.log('User activated the page! Launching real-time trackers.');
      initRealtimeTelemetry();
    }, { once: true });
  } else {
    console.log('Standard page navigation.');
    initRealtimeTelemetry();
  }

  function initRealtimeTelemetry() {
    // Send analytics beacon
  }
</script>
```

---

## 6. Best Practices

- [ ] **Use `"eagerness": "moderate"` for document rules**: Initiates prerendering only when a user hovers over a link for > 200ms. This captures user intent while preventing wasted server bandwidth on un-clicked links.
- [ ] **Check `document.prerendering` before dispatching analytics**: Prevent inflating view counts or executing billable API calls while a page is merely prerendering in an invisible background tab.
- [ ] **Strictly exclude logout, destructive, and state-changing links**: Never prerender URLs that alter user state (e.g., `/logout`, `/cart/clear`, `/delete`). Use negative regex patterns in `"where"`.
- [ ] **Always declare `fetchpriority="high"` on the single LCP image**: Informs the network scheduler to stream the hero image ahead of non-critical CSS or scripts.
- [ ] **Limit `"immediate"` prerendering to 1 URL**: Background prerender processes consume physical RAM. Restrict immediate prerenders to high-probability sequential steps (e.g., Next button in an exam).
- [ ] **Pair with `Sec-Purpose: prefetch;prerender` header checks on the server**: Configure your backend to return lightweight representations or skip expensive database queries if the request is speculative.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Prerendering state-changing URLs (e.g., `/logout`) | **Critical** | User hovers over a link and is accidentally logged out or triggers a deletion! | Add negative rules: `{ "not": { "href_matches": "/logout*" } }`. |
| Tracking analytics during background prerender | **High** | Corrupts analytics with ghost pageviews for pages the user hovered but never clicked. | Listen to `prerenderingchange` before sending pageview beacons. |
| Setting `"eagerness": "immediate"` on dozens of links | **Critical** | Exhausts device memory; mobile browsers abort prerenders and throttle the site. | Use `"eagerness": "moderate"` with document rules. |
| Using legacy `<link rel="prerender">` | **Moderate** | Deprecated in modern browsers; operates merely as a prefetch without background execution. | Migrate to `<script type="speculationrules">`. |
| Adding `fetchpriority="high"` to every image | **High** | Squelches network prioritization; browser cannot discern true LCP asset. | Apply `fetchpriority="high"` to only ONE LCP image per page. |
| Syntax errors in the speculation JSON block | **High** | Browser silently discards the entire speculation rules block without error. | Validate JSON syntax strictly in DevTools > Speculative Loads. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Performance Transformation
- **Instant 0ms LCP**: When a user clicks a prerendered link, the Largest Contentful Paint (LCP) time drops to **0 milliseconds**, because the page was already fully rendered, painted, and composited in memory before the click occurred.

### Security Defenses
- **Background Audio Block**: Modern browser engines strictly prevent background prerendered pages from playing sound, requesting camera/microphone access, or launching popups until the user explicitly activates the page.
- **Cross-Origin Restrictions**: By default, the Speculation Rules API only permits prerendering of **same-origin** pages to prevent third-party tracking and privacy leaks.

---

## 9. Comparison / When to Use What

| Speculation Strategy | Memory Impact | Server Bandwidth | Navigation Speed | Best Suited For |
| :--- | :--- | :--- | :--- | :--- |
| **Prerender (`immediate`)** | High (Full renderer) | High | **Instant (0ms)** | Strict multi-step onboarding funnels, sequential wizards. |
| **Prerender (`moderate`)** | Moderate (Only on hover) | Low | **Instant (0ms)** | High-intent article links, primary navigation menu items. |
| **Prefetch (`document`)** | Very Low (HTML only) | Low | Fast (~100ms) | General catalog lists, search result suggestions. |
| **`<link rel="prefetch">`**| Very Low (HTML only) | Low | Fast (~100ms) | Simple single-page asset prefetching in legacy browsers. |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between <code>&lt;link rel="prefetch"&gt;</code> and the Speculation Rules API prerendering?</strong></summary>

- `<link rel="prefetch">` merely fetches the raw bytes of an HTML document or asset over the network and stores them in the browser's HTTP disk cache. When the user navigates, the browser must still parse the HTML, construct the DOM and CSSOM, fetch and decode images, and execute JavaScript.
- The **Speculation Rules API (`"action": "prerender"`)** spawns a complete, invisible background rendering process. It parses the document, downloads subresources, constructs the render tree, and executes JavaScript. When the user clicks the link, the page is activated instantly with **zero loading latency (0ms LCP)**.
</details>

<details>
<summary><strong>2. How does the browser prevent background prerendered pages from playing audio or triggering premature user actions?</strong></summary>

The browser places the prerendered document into a restricted background state. Audio output is muted, video playback is deferred, permission prompts (camera/microphone/geolocation) are held, and alert/confirm dialogs are blocked. When the user clicks the link and activates the page, the browser transitions the document out of the prerendering state, fires the `prerenderingchange` event, and enables full interactive capabilities.
</details>

<details>
<summary><strong>3. Why is `"eagerness": "moderate"` considered the optimal default for document-based speculation rules?</strong></summary>

`"immediate"` prerendering triggers as soon as the HTML is parsed, which wastes immense device RAM and cellular bandwidth on links the user may never click. `"moderate"` eagerness monitors user intent: it triggers the background prerender only when a desktop user hovers their mouse cursor over a link for more than 200 milliseconds, or when a mobile user begins a `pointerdown` gesture. This 200ms head start is typically all the browser needs to fetch and render the page before the click completes.
</details>

<details>
<summary><strong>4. How can analytics scripts differentiate between an invisible prerendered page and an active user view?</strong></summary>

By querying the `document.prerendering` boolean:
```javascript
if (document.prerendering) {
  document.addEventListener('prerenderingchange', () => {
    sendPageviewEvent();
  }, { once: true });
} else {
  sendPageviewEvent();
}
```
This ensures web analytics platforms record pageviews only when the human user actually views the page, eliminating false-positive "hover ghost" visits.
</details>

<details>
<summary><strong>5. What is the risk of using unconstrained wildcard document rules in speculation rules?</strong></summary>

If a document rule matches every hyperlink on a page (e.g., `{ "href_matches": "/*" }`), it may accidentally prerender destructive or state-changing URLs—such as `/logout`, `/cart/clear`, or `/profile/delete`. Prerendering these links in the background would inadvertently log out the user or mutate their account. Developers must use negative selectors (`{ "not": { "href_matches": "/logout*" } }`) to protect state-changing routes.
</details>

<details>
<summary><strong>6. What HTTP request header does the browser send when issuing a speculative request?</strong></summary>

The browser sends the `Sec-Purpose` header:
- For prefetch: `Sec-Purpose: prefetch`
- For prerender: `Sec-Purpose: prefetch;prerender`
Web servers can inspect this header to return lightweight templates or bypass expensive backend operations until the page is formally activated.
</details>

<details>
<summary><strong>7. How does <code>fetchpriority="high"</code> differ from <code>&lt;link rel="preload"&gt;</code>?</strong></summary>

- `<link rel="preload">` is a **declarative discovery mechanism**: it tells the browser to start downloading an asset right now, even if it hasn't encountered the referencing element in the DOM yet.
- `fetchpriority="high"` is a **relative priority hint**: it works on existing elements (`<img>`, `<script>`, `<link>`) to instruct the network scheduler to prioritize this specific resource over other competing resources of the same type.
</details>

<details>
<summary><strong>8. Where can developers inspect and debug active speculation rules in Chrome DevTools?</strong></summary>

In Chrome DevTools, open the **Application** panel and navigate to the **Speculative Loads** section in the sidebar. It displays:
1. All parsed speculation rules.
2. The current status of speculative URLs (Ready, Running, Discarded, Blocked).
3. Detailed explanations if a prerender was canceled (e.g., Memory Limit Exceeded, Disallowed Header, HTTP 404).
</details>

---

## 11. Practice Exercises

### Exercise 1: Single-URL Prerender Rule (Easy)
- **Task**: Author an HTML `<script type="speculationrules">` block that immediately prerenders the `/checkout/payment` URL.

<details>
<summary>View Solution</summary>

```html
<script type="speculationrules">
{
  "prerender": [
    {
      "source": "list",
      "urls": ["/checkout/payment"],
      "eagerness": "immediate"
    }
  ]
}
</script>
```
</details>

---

### Exercise 2: Hover-Based Document Rule for Blog Articles (Medium)
- **Task**: Create a speculation rule that automatically prerenders all links starting with `/blog/` with `moderate` eagerness, while explicitly excluding any link starting with `/blog/admin/`.

<details>
<summary>View Solution</summary>

```html
<script type="speculationrules">
{
  "prerender": [
    {
      "source": "document",
      "where": {
        "and": [
          { "href_matches": "/blog/*" },
          { "not": { "href_matches": "/blog/admin/*" } }
        ]
      },
      "eagerness": "moderate"
    }
  ]
}
</script>
```
</details>

---

### Exercise 3: Advanced Two-Tier Speculation & Analytics Defense (Hard)
- **Task**: 
  1. Write a speculation rule that prerenders any anchor with class `.instant-page` on hover (`moderate`), and prefetches all other anchors matching `/products/*` on mouse down (`conservative`).
  2. Write an accompanying JavaScript snippet that waits until the page is fully activated before sending an analytics tracking beacon.

<details>
<summary>View Solution</summary>

**HTML Speculation Rules:**
```html
<script type="speculationrules">
{
  "prerender": [
    {
      "source": "document",
      "where": {
        "selector_matches": ".instant-page"
      },
      "eagerness": "moderate"
    }
  ],
  "prefetch": [
    {
      "source": "document",
      "where": {
        "href_matches": "/products/*"
      },
      "eagerness": "conservative"
    }
  ]
}
</script>
```

**JavaScript Analytics Protection:**
```html
<script>
  function trackPageView() {
    console.log('Sending Google Analytics Pageview Beacon now!');
    // navigator.sendBeacon('/api/analytics', JSON.stringify({ page: window.location.pathname }));
  }

  // Check if page is currently running in background prerender mode
  if (document.prerendering) {
    document.addEventListener('prerenderingchange', () => {
      trackPageView();
    }, { once: true });
  } else {
    trackPageView();
  }
</script>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                 SPECULATION RULES CHEAT SHEET                                   |
+-------------------------------------------------------------------------------------------------+
| Property / Directive     | Value / Options                         | Purpose                    |
+-------------------------------------------------------------------------------------------------+
| <script type="...">      | type="speculationrules"                 | Declarative JSON block     |
| action                   | "prerender" (0ms) vs "prefetch" (cache) | Target execution model     |
| source                   | "list" (explicit URLs) vs "document"    | Link targeting strategy    |
| eagerness: "immediate"   | Prepares on page load                   | High-probability funnels   |
| eagerness: "moderate"    | Prepares on hover > 200ms / pointerdown | Optimal default for web    |
| where                    | href_matches, selector_matches, and/not | Filtering URL patterns     |
| document.prerendering    | Boolean flag in JavaScript              | Defers analytics / audio   |
| DevTools Debugger        | Application > Speculative Loads         | Live status & abort audits |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [W3C Speculation Rules Specification](https://wicg.github.io/nav-speculation/speculation-rules.html)
- [web.dev: Prerender pages in Chrome for instant page navigations](https://web.dev/articles/speculation-rules)
- [MDN Web Docs: Speculation Rules API](https://developer.mozilla.org/en-US/docs/Web/API/Speculation_Rules_API)
- [Chrome for Developers: Debugging Speculative Loads](https://developer.chrome.com/docs/devtools/application/speculative-loads)

---

[← Previous: Content Security Policy in Meta](10-content-security-policy-meta.md) | [Section Index](README.md) | [Next: Module 06 Accessibility Architecture →](../06-accessibility/README.md)
