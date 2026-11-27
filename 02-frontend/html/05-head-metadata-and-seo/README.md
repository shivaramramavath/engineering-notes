# Module 05: Document Head, Metadata, Social Cards, and SEO

## Overview
Module 05 explores the `<head>` of an HTML document—the mission control center of web architecture. It covers resource loading order, metadata configuration, mobile viewports, Open Graph and Twitter Card social previews, modern resource hints (`preload`, `prefetch`, `preconnect`, `dns-prefetch`), script loading execution models (`defer`, `async`, `type="module"`), structured data (Schema.org / JSON-LD), internationalization with `hreflang`, Progressive Web App (PWA) web manifests, client-side Content Security Policy via `<meta>`, and the latest Speculation Rules API.

---

## Learning Objectives
By the end of this module, you will be able to:
- Structure an optimized, parser-friendly `<head>` section following the optimal head tag ordering rules.
- Configure mobile viewports (`meta viewport`), character encodings (`meta charset`), and dark mode theme adaptations (`color-scheme`, `theme-color`).
- Craft search-optimized `<title>`, `<meta name="description">`, and canonical URL links (`rel="canonical"`) to eliminate duplicate content penalties.
- Implement rich social share previews across Facebook, LinkedIn, X (Twitter), Slack, and Discord using Open Graph (`og:*`) and Twitter Card meta tags.
- Master browser speculative loading using resource hints (`preload`, `prefetch`, `preconnect`) and the modern Speculation Rules API.
- Eliminate script parser-blocking using `defer`, `async`, and ES module loading (`type="module"`).
- Embed structured data schemas via `<script type="application/ld+json">` to trigger Google Rich Snippets (Breadcrumbs, Articles, FAQs, Products).
- Deploy client-side Content Security Policies (CSP) via `<meta http-equiv="Content-Security-Policy">`.

---

## Module Notes

| # | Note | Description | Focus / Baseline |
| :--- | :--- | :--- | :--- |
| **01** | [Head Anatomy & Tag Ordering](01-head-anatomy.md) | Parser execution flow, optimal tag order, early hints, `<base>` element risks | Widely Available |
| **02** | [Meta Charset, Viewport & Color Scheme](02-meta-charset-viewport-and-color-scheme.md) | UTF-8 1024-byte rule, mobile viewport scaling, interactive-widget, `color-scheme`, `theme-color` media queries | Widely Available / Baseline 2023 |
| **03** | [Title, Description & Canonical](03-title-description-and-canonical.md) | SERP optimization, title crafting algorithms, meta description truncation, `rel="canonical"` duplicate defenses | Widely Available |
| **04** | [Open Graph & Social Cards](04-open-graph-and-social-cards.md) | `og:*` protocol, Twitter Cards, dimension ratios (1200x630), image caching, WhatsApp/Slack previews | Widely Available |
| **05** | [Link Rel Values & Resource Hints](05-link-rel-values-and-resource-hints.md) | `preload`, `prefetch`, `preconnect`, `dns-prefetch`, `as` attribute table, `modulepreload` | Widely Available |
| **06** | [Script Loading Strategies](06-script-loading-strategies.md) | Parser blocking vs `defer` vs `async` vs `type="module"`, execution order guarantees, DOMContentLoaded | Widely Available |
| **07** | [Structured Data & JSON-LD](07-structured-data-json-ld.md) | Schema.org vocabulary, JSON-LD embedding, Rich Snippets (Article, Product, FAQ, Breadcrumbs), validation | Widely Available |
| **08** | [Robots, Sitemaps & hreflang](08-robots-sitemaps-and-hreflang.md) | Crawler indexing controls (`noindex`, `nofollow`, `noarchive`), XML sitemap links, multi-regional `hreflang` | Widely Available |
| **09** | [PWA Web App Manifest](09-pwa-manifest.md) | `<link rel="manifest">`, manifest JSON spec, icons (maskable), display modes, installation criteria | Widely Available |
| **10** | [Content Security Policy in Meta](10-content-security-policy-meta.md) | CSP via `<meta http-equiv>`, supported vs unsupported directives, script-src, object-src, XSS defense | Widely Available |
| **11** | [Performance Hints & Speculation Rules](11-performance-hints-and-priorities.md) | Speculation Rules API (`<script type="speculationrules">`), prerendering vs prefetching, fetchpriority | Baseline 2024 / Newly Available |

---

## Suggested Study Order
1. Begin with **01-head-anatomy** and **02-meta-charset-viewport-and-color-scheme** for structural setup.
2. Study **03-title-description-and-canonical** and **04-open-graph-and-social-cards** for SEO and social sharing.
3. Master browser loading with **05-link-rel-values-and-resource-hints** and **06-script-loading-strategies**.
4. Learn rich snippet semantics and internationalization in **07-structured-data-json-ld** and **08-robots-sitemaps-and-hreflang**.
5. Conclude with modern security and performance in **09-pwa-manifest**, **10-content-security-policy-meta**, and **11-performance-hints-and-priorities**.

---

[← Previous Module: 04 Media & Embeds](../04-media-and-embeds/README.md) | [Track Index](../INDEX.md) | [Next: Head Anatomy →](01-head-anatomy.md)
