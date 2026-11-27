# Open Graph Protocol, Twitter Cards, and Social Media Previews

| Metadata | Details |
| :--- | :--- |
| **Specification** | The Open Graph Protocol (ogp.me) / X Developer Platform (Twitter Cards) |
| **Prerequisites** | `05-head-metadata-and-seo/03-title-description-and-canonical.md`, `01-fundamentals/06-images-and-alt-text.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`og:*`, `twitter:*` metadata tags) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | Facebook Sharing Debugger, LinkedIn Post Inspector, Twitter Card Validator |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: The Canonical Enterprise Social Sharing Meta Block](#example-1-the-canonical-enterprise-social-sharing-meta-block)
   - [Example 2: Deep Technical Blog Post with Author and Publishing Dates](#example-2-deep-technical-blog-post-with-author-and-publishing-dates)
   - [Example 3: E-Commerce Product Social Card with Currency and Pricing](#example-3-e-commerce-product-social-card-with-currency-and-pricing)
   - [Example 4: Messaging App Optimization (Slack, Discord, WhatsApp, iMessage)](#example-4-messaging-app-optimization-slack-discord-whatsapp-imessage)
   - [Example 5: Video and Audio Media Cards (`twitter:player` & `og:video`)](#example-5-video-and-audio-media-cards-twitterplayer--ogvideo)
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

When users share a link on social media platforms (X/Twitter, LinkedIn, Facebook, Pinterest) or messaging applications (Slack, Discord, WhatsApp, Telegram, Apple iMessage), the platform does not simply render a bare URL. Instead, a background crawler visits the webpage, extracts metadata, and renders a rich visual widget known as a **Social Card** or **Link Preview**.

Without explicit social metadata, scrapers attempt to guess: they often pull low-resolution advertising banners, extract random navigation menu text, or display broken grey placeholder boxes. This destroys professional credibility and drastically reduces viral click-through rates.

By implementing the **Open Graph Protocol (`og:*`)** and **Twitter Cards (`twitter:*`)**, developers exert total control over how their content is visually packaged across the entire global internet—defining high-resolution imagery (1200x630), compelling titles, structured descriptions, and accessible alt text.

---

## 2. Core Concepts

### The Open Graph Protocol (`og:*`)
Originally created by Facebook and standardized at `ogp.me`, Open Graph is now the universal lingua franca for social metadata, supported by Facebook, LinkedIn, Slack, Discord, WhatsApp, Telegram, and Pinterest.
- **`og:title`**: The headline of your object as it should appear within the graph.
- **`og:type`**: The type of object (e.g., `website`, `article`, `product`, `profile`, `video.movie`).
- **`og:image`**: The absolute URL to the image that represents your content.
- **`og:url`**: The canonical URL for the object.
- **`og:description`**: A one to two sentence description of your content.
- **`og:site_name`**: The overall website brand name (e.g., "GitHub" or "The New York Times").
- **`og:locale`**: The language and territory (e.g., `en_US`, `fr_FR`).

### The 1.91:1 Golden Ratio for Social Images
To prevent awkward cropping across feeds:
- **Optimal Dimensions**: **1200 x 630 pixels** (Aspect Ratio 1.91:1).
- **Minimum Dimensions**: 600 x 315 pixels.
- **Image File Size**: Keep under **5 MB** (under **300 KB** is ideal for mobile chat apps like WhatsApp).
- **Accessibility**: Always declare `og:image:alt` to provide equivalent descriptive text for blind social media users.

### Twitter Cards (`twitter:*`)
X (formerly Twitter) recognizes Open Graph tags as fallbacks, but offers proprietary tags for card presentation:
- **`twitter:card`**: The card template:
  - `summary_large_image`: Full-width banner image above the title (recommended for almost all editorial and marketing content).
  - `summary`: Compact square thumbnail to the left of the title (ideal for company homepages or simple tools).
  - `player`: Embedded interactive video/audio stream.
  - `app`: Mobile app install card with direct store links.
- **`twitter:site`**: The `@username` for the website or publishing organization.
- **`twitter:creator`**: The `@username` for the individual content author.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                  Social Crawler Scrape and Cache Lifecycle                              |
+-----------------------------------------------------------------------------------------+

  User pastes URL into Slack / Twitter / LinkedIn / WhatsApp
                          |
                          v
  Social Platform Server dispatches HTTP Scraper Bot:
  (User-Agent: "facebookexternalhit/1.1", "Twitterbot/1.0", "Slackbot-LinkExpanding")
                          |
                          v
  Scraper requests HTML document <head>:
       |
       +--> Parses <meta property="og:title" content="...">
       +--> Parses <meta property="og:image" content="https://example.com/banner.jpg">
       +--> Parses <meta name="twitter:card" content="summary_large_image">
                          |
                          v
  Platform Cache Engine:
  - Downloads banner.jpg to platform CDN
  - Caches social card metadata in key-value store for 7 to 30 days!
                          |
                          v
  Renders Interactive Rich Visual Card in User's Feed or Chat!
```

---

## 4. Syntax & Usage

```html
<head>
  <!-- Standard SEO Baseline -->
  <title>Building Resilient Microservices with Go | CloudCraft</title>
  <meta name="description" content="A comprehensive architectural guide to building distributed, fault-tolerant microservices." />
  <link rel="canonical" href="https://cloudcraft.io/go-microservices" />

  <!-- Open Graph Protocol (Facebook, LinkedIn, Slack, Discord) -->
  <meta property="og:site_name" content="CloudCraft Engineering" />
  <meta property="og:type" content="article" />
  <meta property="og:url" content="https://cloudcraft.io/go-microservices" />
  <meta property="og:title" content="Building Resilient Microservices with Go" />
  <meta property="og:description" content="A comprehensive architectural guide to building distributed, fault-tolerant microservices." />
  <meta property="og:image" content="https://cloudcraft.io/images/og-go-microservices.jpg" />
  <meta property="og:image:width" content="1200" />
  <meta property="og:image:height" content="630" />
  <meta property="og:image:alt" content="Conceptual diagram of distributed Go microservices communicating via gRPC" />
  <meta property="og:locale" content="en_US" />

  <!-- Twitter Cards (X) -->
  <meta name="twitter:card" content="summary_large_image" />
  <meta name="twitter:site" content="@CloudCraftOrg" />
  <meta name="twitter:creator" content="@AlexRiveraDev" />
  <meta name="twitter:title" content="Building Resilient Microservices with Go" />
  <meta name="twitter:description" content="A comprehensive architectural guide to building distributed, fault-tolerant microservices." />
  <meta name="twitter:image" content="https://cloudcraft.io/images/og-go-microservices.jpg" />
  <meta name="twitter:image:alt" content="Conceptual diagram of distributed Go microservices communicating via gRPC" />
</head>
```

> **Cross-Browser & Baseline Status Box**
>
> - Open Graph Protocol: **Baseline Widely Available** across all social platforms, messaging clients, and search aggregators since 2010.
> - Twitter Cards: **Baseline Widely Available** (X automatically falls back to `og:*` if `twitter:*` tags are omitted).

---

## 5. Practical Examples

### Example 1: The Canonical Enterprise Social Sharing Meta Block
A modular, production-tested metadata block suitable for company homepages or marketing portals.

```html
<!DOCTYPE html>
<html lang="en" prefix="og: https://ogp.me/ns#">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Enterprise Cloud Analytics | DataSphere</title>

  <!-- Canonical URL -->
  <link rel="canonical" href="https://datasphere.io" />

  <!-- Open Graph Core -->
  <meta property="og:type" content="website" />
  <meta property="og:site_name" content="DataSphere Analytics" />
  <meta property="og:url" content="https://datasphere.io" />
  <meta property="og:title" content="Enterprise Cloud Analytics | DataSphere" />
  <meta property="og:description" content="Transform Petabyte-scale telemetric logs into actionable real-time dashboard intelligence." />
  
  <!-- High-Resolution Image (1200x630) -->
  <meta property="og:image" content="https://datasphere.io/assets/social/og-homepage-1200x630.jpg" />
  <meta property="og:image:secure_url" content="https://datasphere.io/assets/social/og-homepage-1200x630.jpg" />
  <meta property="og:image:type" content="image/jpeg" />
  <meta property="og:image:width" content="1200" />
  <meta property="og:image:height" content="630" />
  <meta property="og:image:alt" content="DataSphere real-time telemetry dashboard glowing on dual monitors" />

  <!-- Twitter Card -->
  <meta name="twitter:card" content="summary_large_image" />
  <meta name="twitter:site" content="@DataSphereHQ" />
</head>
<body>
  <h1>DataSphere Enterprise Platform</h1>
</body>
</html>
```

---

### Example 2: Deep Technical Blog Post with Author and Publishing Dates
Using `og:type="article"` with extended metadata properties to expose authorship and publication timestamps.

```html
<head>
  <title>Hardening Linux Servers with eBPF | KernelCraft</title>
  
  <!-- Open Graph Article Sub-type -->
  <meta property="og:type" content="article" />
  <meta property="og:site_name" content="KernelCraft Blog" />
  <meta property="og:url" content="https://kernelcraft.org/ebpf-hardening" />
  <meta property="og:title" content="Hardening Linux Servers with eBPF" />
  <meta property="og:description" content="Deploying real-time kernel observability and syscall filtering with modern eBPF programs." />
  <meta property="og:image" content="https://kernelcraft.org/images/ebpf-guide.jpg" />
  <meta property="og:image:alt" content="eBPF architecture diagram showing bytecode verified inside the Linux kernel" />

  <!-- Extended Article Metadata (ISO-8601 Timestamps) -->
  <meta property="article:published_time" content="2026-03-15T08:00:00Z" />
  <meta property="article:modified_time" content="2026-03-18T14:30:00Z" />
  <meta property="article:author" content="https://kernelcraft.org/authors/elena-vance" />
  <meta property="article:section" content="Systems Engineering" />
  <meta property="article:tag" content="Linux" />
  <meta property="article:tag" content="Security" />
  <meta property="article:tag" content="eBPF" />

  <!-- Twitter Card -->
  <meta name="twitter:card" content="summary_large_image" />
  <meta name="twitter:creator" content="@ElenaVanceSec" />
</head>
```

---

### Example 3: E-Commerce Product Social Card with Currency and Pricing
Using Open Graph product extensions so platforms like Pinterest and Facebook can display live product prices.

```html
<head>
  <title>Ergonomic Mechanical Keyboard - Dark Slate | KeyWorks</title>

  <meta property="og:type" content="og:product" />
  <meta property="og:site_name" content="KeyWorks Studio" />
  <meta property="og:url" content="https://keyworks.com/products/slate-split" />
  <meta property="og:title" content="Ergonomic Split Mechanical Keyboard" />
  <meta property="og:description" content="Custom CNC aluminum chassis with hot-swappable tactile switches and per-key RGB backlighting." />
  <meta property="og:image" content="https://keyworks.com/images/slate-split-1200x630.jpg" />
  <meta property="og:image:alt" content="Two halves of an ergonomic split mechanical keyboard with dark slate keycaps" />

  <!-- Open Graph Commerce Extensions -->
  <meta property="product:price:amount" content="249.00" />
  <meta property="product:price:currency" content="USD" />
  <meta property="product:availability" content="in stock" />

  <meta name="twitter:card" content="summary_large_image" />
</head>
```

---

### Example 4: Messaging App Optimization (Slack, Discord, WhatsApp, iMessage)
Optimizing metadata specifically for instant messaging preview scrapers that often truncate large cards or require lightweight thumbnails.

```html
<!-- 
  WHATSAPP & iMESSAGE SECRETS:
  1. WhatsApp ignores images larger than 300KB! Keep image payload under 300KB.
  2. WhatsApp prefers a 1:1 square ratio for link previews inside chat bubbles.
  3. Include a fallback square thumbnail image alongside the standard 1.91:1 banner!
-->
<head>
  <title>QuickSync Cloud File Transfer</title>
  
  <meta property="og:title" content="Send Files Instantly with QuickSync" />
  <meta property="og:description" content="End-to-end encrypted file transfers up to 10GB. No account required." />
  <meta property="og:url" content="https://quicksync.io" />

  <!-- Primary Large Banner for Slack, Discord, Twitter, LinkedIn -->
  <meta property="og:image" content="https://quicksync.io/og-large-1200x630.jpg" />
  <meta property="og:image:width" content="1200" />
  <meta property="og:image:height" content="630" />

  <!-- Square Thumbnail Fallback for WhatsApp & Mobile Messages -->
  <meta property="og:image" content="https://quicksync.io/og-square-400x400.jpg" />
  <meta property="og:image:width" content="400" />
  <meta property="og:image:height" content="400" />
  
  <!-- Discord Embed Accent Color -->
  <meta name="theme-color" content="#5865F2" />
</head>
```

---

### Example 5: Video and Audio Media Cards (`twitter:player` & `og:video`)
Enabling embedded media playback directly inside social feeds without forcing the user to leave the platform.

```html
<head>
  <title>Keynote 2026: Future of the Web | TechConf</title>

  <meta property="og:type" content="video.other" />
  <meta property="og:title" content="Keynote 2026: Future of the Web" />
  <meta property="og:url" content="https://techconf.org/keynote" />
  <meta property="og:image" content="https://techconf.org/poster-1200.jpg" />

  <!-- Open Graph Video Player -->
  <meta property="og:video" content="https://techconf.org/embed/keynote" />
  <meta property="og:video:type" content="text/html" />
  <meta property="og:video:width" content="1280" />
  <meta property="og:video:height" content="720" />

  <!-- Twitter Player Card -->
  <meta name="twitter:card" content="player" />
  <meta name="twitter:player" content="https://techconf.org/embed/keynote" />
  <meta name="twitter:player:width" content="1280" />
  <meta name="twitter:player:height" content="720" />
  <meta name="twitter:image" content="https://techconf.org/poster-1200.jpg" />
</head>
```

---

## 6. Best Practices

- [ ] **Always use absolute URLs for `og:image` and `og:url`**: Scraper bots do not possess a browser window context and cannot resolve relative paths like `/images/og.jpg`.
- [ ] **Design social images at 1200 x 630 pixels**: Matches the standard 1.91:1 ratio across X, Facebook, LinkedIn, and Slack, ensuring sharp rendering on Retina displays.
- [ ] **Declare `og:image:width` and `og:image:height`**: Explicit dimensions allow Facebook and LinkedIn crawlers to render the preview card synchronously upon the first user share without making a secondary image head request.
- [ ] **Always provide `og:image:alt`**: Blind and low-vision users on X and LinkedIn rely on image alt tags announced by screen readers.
- [ ] **Keep images under 300 KB for mobile messaging compatibility**: WhatsApp and Telegram frequently fail to display previews if the target image exceeds 300 KB.
- [ ] **Test with official platform debuggers**: Always clear scraper caches using the Facebook Sharing Debugger and LinkedIn Post Inspector when updating metadata.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Using relative URLs for `og:image` (`/og.jpg`) | **Critical** | Social scrapers cannot resolve the path; preview renders with no image. | Always provide full absolute URL: `https://example.com/og.jpg`. |
| Image exceeds 5 MB (or 300 KB on WhatsApp) | **High** | Scraper times out or aborts download; link preview displays as plain text. | Compress image using WebP or optimized JPEG. |
| Missing `og:image:alt` | **Moderate** | Screen reader users receive no description of the shared image content. | Add `<meta property="og:image:alt" content="...">`. |
| Blocked by `robots.txt` | **Critical** | Social bots (`Twitterbot`, `facebookexternalhit`) are blocked from reading `<head>`. | Allow social bot user agents in `robots.txt`. |
| Placing critical text at the extreme edges of social images | **Moderate** | Different platforms crop edges irregularly, cutting off titles or logos. | Keep all text centered within the safe 1000x520 middle zone. |
| Expecting immediate updates after changing HTML | **Low** | Platforms cache scraped cards for weeks; changes are not visible immediately. | Force cache refresh using the platform's developer debugger tool. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- Social cards are an extension of your web presence. Declaring `og:image:alt` ensures that when your card is shared on platforms that support accessible link previews (such as X/Twitter and LinkedIn), blind users hear an accurate description of the visual graphic.

### Security
- **Open Redirect Protection**: Ensure the URL declared in `og:url` matches the canonical URL. Do not allow attackers to inject malicious redirect destinations into social sharing parameters.

---

## 9. Comparison / When to Use What

| Platform / Protocol | Primary Meta Prefix | Default Large Format | Fallback Behavior |
| :--- | :--- | :--- | :--- |
| **Open Graph (Meta)** | `property="og:*"` | 1200 x 630 px (1.91:1) | Lingua franca; read by almost all platforms. |
| **Twitter Cards (X)** | `name="twitter:*"` | `summary_large_image` | Automatically falls back to `og:*` if omitted. |
| **LinkedIn** | `property="og:*"` | 1200 x 627 px | Strict adherence to standard Open Graph. |
| **Slack / Discord** | `property="og:*"` | 1200 x 630 px | Uses `og:image` and `theme-color` for border accents. |
| **WhatsApp** | `property="og:*"` | Square 1:1 thumbnail | Strictly requires image weight < 300 KB. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why must <code>og:image</code> always be specified as an absolute URL rather than a relative URL?</strong></summary>

Social media scrapers (such as `facebookexternalhit` or `Twitterbot`) are backend server applications, not interactive client web browsers. When they fetch and parse your HTML document, they do not execute client-side JavaScript or resolve relative paths against a document location. If you specify `content="/images/og.png"`, the scraper does not know the intended scheme or domain, causing image fetching to fail completely.
</details>

<details>
<summary><strong>2. What is the standard dimension and aspect ratio for high-resolution Open Graph images?</strong></summary>

The universal industry standard is **1200 x 630 pixels**, which corresponds to an aspect ratio of approximately **1.91:1**. This ensures that previews display as full-width hero cards on high-DPR Retina mobile screens and desktop feeds across X, Facebook, LinkedIn, Slack, and Discord without vertical cropping or letterbox black bars.
</details>

<details>
<summary><strong>3. How does X (Twitter) handle social card metadata when <code>twitter:*</code> tags are omitted?</strong></summary>

The X/Twitter card parser is engineered to fall back gracefully to standard Open Graph tags if dedicated `twitter:*` tags are absent. For example, if `twitter:title` and `twitter:image` are omitted, X will read `og:title` and `og:image`. However, you should still explicitly declare `<meta name="twitter:card" content="summary_large_image">`, because X defaults to a small thumbnail (`summary`) unless `summary_large_image` is explicitly specified.
</details>

<details>
<summary><strong>4. Why do link previews sometimes fail to display an image when shared on WhatsApp?</strong></summary>

WhatsApp has strict mobile resource constraints: its scraper bot will ignore any image file that exceeds **300 KB** in file size. Additionally, WhatsApp prefers square (1:1) aspect ratio images for in-chat message bubble thumbnails. If an image is 2 MB or ultra-wide, WhatsApp may omit the image entirely and display only the page title.
</details>

<details>
<summary><strong>5. Why is declaring explicit <code>og:image:width</code> and <code>og:image:height</code> tags beneficial?</strong></summary>

When an image is shared for the very first time on Facebook or LinkedIn, the platform scraper downloads the HTML. If width and height are not declared, the platform must make an asynchronous secondary HTTP request to download the image file and inspect its binary header before it can render the card in the feed. Declaring width and height allows the platform to calculate layout geometry and render the rich preview card immediately on the first share.
</details>

<details>
<summary><strong>6. What is the purpose of the <code>og:image:alt</code> attribute?</strong></summary>

`og:image:alt` provides alternative text describing the content and purpose of the social preview image for blind and low-vision users who use screen readers on social networks like X and LinkedIn, satisfying modern accessibility expectations for shared digital assets.
</details>

<details>
<summary><strong>7. How do you invalidate a cached social preview card after updating metadata on a live webpage?</strong></summary>

Social platforms aggressively cache scraped metadata on edge CDNs for weeks. To force a platform to re-scrape and update its cache, you must submit the URL to official developer debugger tools:
- Facebook: **Sharing Debugger** ("Scrape Again" button).
- LinkedIn: **Post Inspector**.
- X (Twitter): **Card Validator** or by appending a dummy query parameter (e.g., `?v=2`).
</details>

<details>
<summary><strong>8. What is the difference between <code>property="og:..."</code> and <code>name="twitter:..."</code>?</strong></summary>

- The Open Graph protocol uses the XML RDFa attribute syntax: `<meta property="og:...">`.
- Twitter Cards and standard HTML metadata use the standard HTML name attribute: `<meta name="twitter:...">`.
Modern parsers are forgiving, but using `property` for OG and `name` for Twitter conforms strictly to the respective official specifications.
</details>

---

## 11. Practice Exercises

### Exercise 1: Basic Blog Article Social Metadata (Easy)
- **Task**: Author an HTML `<head>` snippet for an article titled "Mastering TypeScript Generics" with Open Graph title, type, url, image (1200x630), and large Twitter card.

<details>
<summary>View Solution</summary>

```html
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Mastering TypeScript Generics | TSWeekly</title>

  <!-- Open Graph -->
  <meta property="og:type" content="article" />
  <meta property="og:site_name" content="TSWeekly" />
  <meta property="og:url" content="https://tsweekly.dev/posts/generics" />
  <meta property="og:title" content="Mastering TypeScript Generics" />
  <meta property="og:description" content="A comprehensive tutorial on generic constraints, conditional types, and utility types." />
  <meta property="og:image" content="https://tsweekly.dev/images/og-generics-1200x630.jpg" />
  <meta property="og:image:width" content="1200" />
  <meta property="og:image:height" content="630" />
  <meta property="og:image:alt" content="TypeScript code snippet showing generic constraint syntax" />

  <!-- Twitter Card -->
  <meta name="twitter:card" content="summary_large_image" />
  <meta name="twitter:site" content="@TSWeekly" />
</head>
```
</details>

---

### Exercise 2: Dual Image Fallback for Web and WhatsApp (Medium)
- **Task**: Create an Open Graph block that provides a standard 1200x630 banner for desktop feeds, and a secondary square (400x400) image under 300KB for mobile chat apps.

<details>
<summary>View Solution</summary>

```html
<head>
  <meta property="og:title" content="Join Our Live Developer Hackathon" />
  <meta property="og:description" content="Build AI applications and compete for $50k in prizes. Registration open now." />
  <meta property="og:url" content="https://hackathon.example.com" />

  <!-- 1. Primary Large Card Image (1200x630) -->
  <meta property="og:image" content="https://hackathon.example.com/social-banner-1200x630.jpg" />
  <meta property="og:image:width" content="1200" />
  <meta property="og:image:height" content="630" />
  <meta property="og:image:alt" content="Hackathon banner with neon code graphics" />

  <!-- 2. Square Thumbnail Fallback (WhatsApp / iMessage) -->
  <meta property="og:image" content="https://hackathon.example.com/social-thumb-400x400.jpg" />
  <meta property="og:image:width" content="400" />
  <meta property="og:image:height" content="400" />
</head>
```
</details>

---

### Exercise 3: Social Card Audit and Repair (Hard)
- **Task**: An engineering team complains that their links on LinkedIn and X appear as broken blank text boxes with no images. Audit the following markup and fix all 3 critical bugs:
  ```html
  <head>
    <title>Enterprise Security Guide</title>
    <meta name="twitter:card" content="photo">
    <meta property="og:title" content="Enterprise Security Guide">
    <meta property="og:image" content="/assets/og-image.png">
    <meta property="og:url" content="https://sec.example.com/guide">
  </head>
  ```

<details>
<summary>View Solution</summary>

**Bugs Identified:**
1. **Relative URL on `og:image` (`/assets/og-image.png`)**: Scrapers fail to resolve relative paths. Must be an absolute URL (`https://sec.example.com/assets/og-image.png`).
2. **Obsolete Twitter Card Type (`photo`)**: `twitter:card="photo"` was deprecated by X years ago. Must be `summary_large_image` or `summary`.
3. **Missing `og:description`**: Without a description, social platforms display raw page headers or random navigation links.

**Corrected Production Code:**
```html
<head>
  <title>Enterprise Security Guide | SecCorp</title>

  <!-- Open Graph -->
  <meta property="og:type" content="article" />
  <meta property="og:url" content="https://sec.example.com/guide" />
  <meta property="og:title" content="Enterprise Security Guide | SecCorp" />
  <meta property="og:description" content="Best practices for zero-trust architecture, identity federation, and threat modeling." />
  <meta property="og:image" content="https://sec.example.com/assets/og-image.png" />
  <meta property="og:image:width" content="1200" />
  <meta property="og:image:height" content="630" />
  <meta property="og:image:alt" content="Shield icon overlaid on global network telemetry" />

  <!-- Twitter Card -->
  <meta name="twitter:card" content="summary_large_image" />
</head>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                 SOCIAL CARDS QUICK REFERENCE                                    |
+-------------------------------------------------------------------------------------------------+
| Property / Tag           | Value / Recommendation                  | Function                   |
+-------------------------------------------------------------------------------------------------+
| og:title                 | Page title (Max 60 chars)               | Headline in card           |
| og:description           | Summary snippet (120-155 chars)         | Description text           |
| og:image                 | ABSOLUTE URL ONLY (1200 x 630 px)       | Main visual preview banner |
| og:image:alt             | Accessible textual description          | Screen reader support      |
| og:image:width / height  | 1200 / 630                              | Enables instant rendering  |
| og:url                   | Canonical URL                           | Target link on card click  |
| twitter:card             | "summary_large_image"                   | Full-width photo card on X |
| twitter:site             | @CompanyHandle                          | Publisher attribution      |
| WhatsApp requirement     | File size strictly < 300 KB             | Prevents preview failure   |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [The Open Graph Protocol Specification](https://ogp.me/)
- [X (Twitter) Developer Documentation: Cards Markup Reference](https://developer.x.com/en/docs/x-for-websites/cards/overview/markup-reference)
- [Facebook Sharing Debugger](https://developers.facebook.com/tools/debug/)
- [LinkedIn Post Inspector](https://www.linkedin.com/post-inspector/)

---

[← Previous: Title, Description & Canonical](03-title-description-and-canonical.md) | [Section Index](README.md) | [Next: Link Rel Values & Resource Hints →](05-link-rel-values-and-resource-hints.md)
