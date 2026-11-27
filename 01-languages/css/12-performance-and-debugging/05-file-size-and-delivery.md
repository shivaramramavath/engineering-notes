# 05. File Size, Minification and Delivery

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | Web Performance Optimization / IETF HTTP/2, HTTP/3, and Brotli Standards |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ |
| **Tested With** | LightningCSS 1.25, Brotli CLI, Vite 5, Cloudflare CDN Headers |
| **Prerequisites** | `00-setup/05-tooling-stylelint-prettier-lightningcss.md`, `12-performance-and-debugging/02-render-blocking-and-critical-css.md` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. Practical Examples
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

### The Problem
CSS is an uncompiled, text-based language. Source stylesheets authored by human developers contain thousands of bytes of whitespace, comments, redundant property declarations, vendor prefixes, and verbose color notations. In large production applications, uncompressed CSS files frequently weigh between **300 KB to 1.5 MB**.

Because CSS is inherently **render-blocking**, transferring unoptimized stylesheets over mobile networks introduces catastrophic latency:
- High network transmission time directly delays **First Contentful Paint (FCP)** and **Largest Contentful Paint (LCP)**.
- Parsing and tokenizing massive stylesheets on low-power mobile CPU cores blocks the browser's main thread, delaying user interaction (**Interaction to Next Paint / INP**).
- Ineffective HTTP caching policies force repeat visitors to redownload unchanged CSS assets on every page load.

### The Solution
A modern, production-grade CSS delivery pipeline employs multi-stage optimization:
1. **High-Performance Minification (LightningCSS)**: Removing comments, collapsing whitespace, calculating shorthand declarations, converting colors to shortest equivalents, and deduplicating rules at native machine speed.
2. **Next-Generation Compression (Brotli / Zstandard)**: Compressing CSS using specialized static dictionaries that achieve **15% to 25% smaller payloads** than standard Gzip.
3. **HTTP/2 & HTTP/3 Multiplexing**: Delivering assets concurrently over single-connection binary streams, eliminating head-of-line blocking.
4. **Immutable Asset Caching**: Generating content-hashed filenames (`app.[hash].css`) backed by `Cache-Control: public, max-age=31536000, immutable`.

---

## 2. Core Concepts

### 1. The Compression Hierarchy: Unminified $\rightarrow$ Minified $\rightarrow$ Compressed

```
RAW AUTHOR CSS: ~450 KB
(Comments, indentation, long color codes #ffffff, uncombined shorthands)
          │
          ▼ [Minifier: LightningCSS / cssnano]
MINIFIED CSS: ~140 KB (-69% Reduction)
(Comments stripped, whitespace eliminated, #fff, margin: 10px 20px)
          │
          ├─────────────────────────────┬─────────────────────────────┐
          ▼ [Gzip Compression]         ▼ [Brotli Compression]       ▼ [Zstandard]
     ~28 KB (-93% Total)          ~21 KB (-95% Total)           ~20.5 KB (-95.4%)
```

### 2. Brotli vs. Gzip for CSS Payloads
- **Gzip (DEFLATE algorithm)**: Uses a rolling sliding window of LZ77 and Huffman coding. It has zero built-in knowledge of web languages.
- **Brotli (`br` - RFC 7932)**: Built by Google specifically for the web. In addition to dynamic sliding windows, Brotli includes a **built-in static dictionary of 120 KB** containing common web strings (e.g. `display: none`, `position: absolute`, `background-color`, `border-radius`, `font-size:`, `!important`).
- Because CSS repetitive keywords match Brotli's static dictionary immediately, Brotli compresses stylesheets **15% to 25% more efficiently than Gzip**, with identical client decompression speeds.

### 3. Immutable Caching Strategy
If an asset's filename is invariant (e.g. `/assets/styles.css`), browsers must issue conditional `If-Modified-Since` or `ETag` HTTP requests on repeat visits to verify freshness.
By injecting a cryptographic content hash into the filename (e.g. `/assets/styles.8f3b2a.css`):
- The URL is guaranteed to change whenever the CSS code changes.
- Servers can configure `Cache-Control: public, max-age=31536000, immutable`.
- Browsers cache the file on disk permanently; repeat navigations load the stylesheet in **0 milliseconds with zero network requests**.

---

## 3. How It Works Internally

### LightningCSS AST Optimization Pipeline
Modern minifiers like **LightningCSS** (written in Rust) parse CSS into a typed Abstract Syntax Tree (AST) and execute deep structural optimizations:

```
INPUT CSS:
  .badge {
    background-color: rgb(255, 255, 255);
    margin-top: 10px;
    margin-right: 20px;
    margin-bottom: 10px;
    margin-left: 20px;
    font-weight: normal;
  }

LIGHTNINGCSS AST TRANSFORM PASS:
  1. Color optimization: rgb(255, 255, 255) ──► #fff (Saves 16 bytes)
  2. Margin merging: Combines 4 properties into shorthand ──► margin: 10px 20px (Saves 42 bytes)
  3. Font weight canonicalization: normal ──► 400 (Saves 3 bytes)
  4. Whitespace and semicolon stripping

OUTPUT CSS:
  .badge{background-color:#fff;margin:10px 20px;font-weight:400}
```

---

## 4. Syntax & Usage

### 1. Configuring LightningCSS in Vite (`vite.config.js`)
LightningCSS compiles and minifies CSS over **100x faster** than legacy JavaScript-based tools like cssnano:

```javascript
import { defineConfig } from 'vite';

export default defineConfig({
  css: {
    transformer: 'lightningcss',
    lightningcss: {
      targets: {
        chrome: 120 << 16,
        firefox: 125 << 16,
        safari: 17 << 16
      },
      minify: true,
      drafts: {
        customMedia: true
      }
    }
  },
  build: {
    cssMinify: 'lightningcss'
  }
});
```

### 2. Production HTTP Cache Headers (Nginx / Cloudflare)
```nginx
# Nginx Configuration for Content-Hashed Static CSS
location ~* \.(css)$ {
    # 1 Year Cache Lifetime + Immutable flag
    add_header Cache-Control "public, max-age=31536000, immutable";
    
    # Enable Brotli and Gzip dynamic compression
    brotli on;
    brotli_comp_level 6;
    brotli_types text/css;

    gzip on;
    gzip_comp_level 6;
    gzip_types text/css;

    # Security headers
    add_header X-Content-Type-Options "nosniff";
}
```

---

## 5. Practical Examples

### Example 1: Comparing Minification Engine Speeds and Output
Benchmarking a 400 KB enterprise CSS bundle across build tools:

| Minification Tool | Engine Language | Build Time | Minified Output | Brotli Size |
| :--- | :--- | :--- | :--- | :--- |
| **Clean-CSS** | Node.js (JavaScript) | 1,420 ms | 128 KB | 22.4 KB |
| **cssnano** | PostCSS (JavaScript) | 1,890 ms | 124 KB | 21.8 KB |
| **esbuild** | Go | 42 ms | 125 KB | 21.9 KB |
| **LightningCSS** | **Rust** | **12 ms** | **121 KB** | **20.8 KB** |

*Conclusion*: LightningCSS delivers both the smallest byte payload and the fastest build execution speed (over 100x faster than PostCSS).

### Example 2: Static Pre-Compression via Node Script (Brotli & Gzip)
In production, compressing stylesheets dynamically on every HTTP request consumes CPU cycles on the origin server. Pre-compressing files at build time allows web servers (Nginx/Caddy) to serve pre-compressed `.br` and `.gz` files directly from disk (zero CPU overhead):

```javascript
// precompress.js
const fs = require('fs');
const zlib = require('zlib');
const path = require('path');

const distCssPath = path.resolve(__dirname, 'dist/assets/app.css');
const cssContent = fs.readFileSync(distCssPath);

// 1. Generate maximum compression Brotli (.br)
const brotliBuffer = zlib.brotliCompressSync(cssContent, {
  params: {
    [zlib.constants.BROTLI_PARAM_QUALITY]: 11, // Maximum compression
    [zlib.constants.BROTLI_PARAM_MODE]: zlib.constants.BROTLI_MODE_TEXT
  }
});
fs.writeFileSync(`${distCssPath}.br`, brotliBuffer);

// 2. Generate maximum compression Gzip (.gz)
const gzipBuffer = zlib.gzipSync(cssContent, { level: 9 });
fs.writeFileSync(`${distCssPath}.gz`, gzipBuffer);

console.log(`[Compression] Raw: ${(cssContent.length / 1024).toFixed(1)} KB | Gzip: ${(gzipBuffer.length / 1024).toFixed(1)} KB | Brotli: ${(brotliBuffer.length / 1024).toFixed(1)} KB`);
```

### Example 3: Route-Based CSS Splitting vs. Monolithic Bundle
In Single Page Applications, bundlers can split stylesheets by route to avoid shipping dashboard CSS to landing page visitors:

```
TRADITIONAL MONOLITHIC BUNDLE:
  app.css (280 KB) ──► Downloaded on homepage! (Contains checkout, settings, admin styles)

ROUTE-BASED CODE SPLITTING:
  index.html ──► loads home.css (12 KB)
  /dashboard ──► asynchronously loads dashboard.css (35 KB)
  /checkout  ──► asynchronously loads checkout.css (18 KB)
```

### Example 4: HTTP/3 0-RTT Handshake Optimization
HTTP/3 utilizes **QUIC over UDP**, replacing the multi-step TCP + TLS 1.3 handshake with a 0-RTT (Zero Round-Trip Time) connection resumption.
When combined with CDN edge caching:
- The browser fetches the stylesheet in **one round trip** without connection handshake stall.
- Time to First Byte (TTFB) for CSS assets drops from **180ms to under 25ms**.

### Example 5: Establishing Performance Budgets with bundlesize / size-limit
Enforce strict CSS size ceilings in continuous integration (CI) pipelines:

```json
// package.json (.size-limit)
[
  {
    "path": "dist/assets/home-*.css",
    "limit": "15 KB",
    "brotli": true
  },
  {
    "path": "dist/assets/dashboard-*.css",
    "limit": "40 KB",
    "brotli": true
  }
]
```
If a developer imports an unpurged icon library that inflates `home.css` beyond 15 KB, the CI build immediately fails and blocks the Pull Request.

---

## 6. Best Practices

1. **Always Serve Brotli (`Content-Encoding: br`)**: Ensure your CDN or origin server supports Brotli compression. Brotli reduces CSS byte size by 15–25% compared to Gzip.
2. **Pre-Compress CSS Assets at Build Time**: Compress stylesheets during the build with Brotli level 11 and Gzip level 9 (`app.css.br`, `app.css.gz`) so servers serve static compressed bytes without runtime CPU load.
3. **Use Content-Hashed Filenames for Immutable Caching**: Hash stylesheet filenames (e.g. `main.[contenthash:8].css`) and apply `Cache-Control: public, max-age=31536000, immutable`.
4. **Deploy LightningCSS as the Default Minifier**: Replace legacy JavaScript-based CSS minifiers with native Rust tools like LightningCSS for faster builds and superior AST optimization.
5. **Split CSS by Route or Layout Boundaries**: Do not bundle administrative panels and complex modal styling into the primary public landing page stylesheet.
6. **Set Strict CSS Budgets in CI**: Enforce a maximum budget of **<15 KB (brotli)** for critical landing page stylesheets and **<50 KB** for complex authenticated routes.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Serving uncompressed CSS (`Content-Encoding` missing)** | Users download 400 KB of raw text; massive FCP delay. | Enable Brotli and Gzip on web servers and CDNs. |
| **Using `max-age=3600` on content-hashed files** | Browsers re-request cached stylesheets every hour; unnecessary network traffic. | Use `max-age=31536000, immutable`. |
| **Changing stylesheet filenames without updating HTML** | 404 Not Found error on CSS, leaving users with raw unstyled HTML. | Let bundlers (Vite/Webpack) inject hashed filenames automatically. |
| **Compressing dynamic responses on-the-fly at quality level 11** | High Brotli compression levels on the fly spike origin server CPU, delaying TTFB. | Pre-compress at level 11 at build time; use dynamic level 4–6 on runtime servers. |
| **Relying on Gzip alone in 2026** | Leaves 20%+ compression savings on the table. | Enable Brotli as primary encoding, with Gzip fallback. |
| **Shipping unused third-party font icon stylesheets (FontAwesome)** | Shipping 200 KB of unused SVG glyphs for only 3 icons. | Use inline SVG components or subset icon fonts strictly. |

---

## 8. Performance, Accessibility & Browser Support

### Browser Support Matrix
- **Brotli (`Content-Encoding: br`)**: Supported across 98%+ of all global browser traffic (Chrome 50+, Firefox 44+, Safari 11+).
- **`Cache-Control: immutable`**: Baseline support across all modern browsers (Firefox 49+, Safari 11+, Chrome 68+).
- **HTTP/2**: Universally supported across all web clients.
- **HTTP/3 (QUIC)**: Supported across all modern browsers and major CDNs (Cloudflare, Fastly, AWS CloudFront).

---

## 9. Comparison / When to Use What

| Compression / Strategy | Compression Ratio vs Raw | Client Decompression Speed | Server CPU Cost | Best Suited For |
| :--- | :--- | :--- | :--- | :--- |
| **Uncompressed** | 0% | Instant | 0% | Local development debugging |
| **Gzip (Level 6)** | ~70% – 80% | Extremely Fast | Low | Universal legacy fallback |
| **Brotli (Level 11 Static)**| **~85% – 95%** | Extremely Fast | 0 (Pre-compressed) | **All modern production web assets** |
| **Zstandard (`zstd`)** | ~85% – 96% | Blazing Fast | Very Low | Emerging modern CDN edge delivery |

---

## 10. Interview Questions

### Q1: Why does Brotli achieve substantially better compression ratios on CSS files than Gzip?
<details>
<summary>Answer</summary>
While both algorithms utilize variants of the LZ77 sliding window and Huffman coding, **Brotli** (RFC 7932) includes a **built-in static dictionary of 120 KB** pre-populated with over 13,000 common words, phrases, and HTML/CSS syntax tokens (such as `display: block`, `background-color`, `border-radius`, `margin`, `padding`, `!important`, `font-family`).

When compressing CSS:
- Gzip must encounter a word repeatedly within its sliding window before it can substitute it with a shorter back-reference.
- Brotli can replace standard CSS property names and keywords with single-byte references to its static dictionary on their very first appearance.
Consequently, Brotli compresses text stylesheets **15% to 25% smaller** than Gzip with zero degradation in browser decompression speed.
</details>

### Q2: What is the purpose of the `immutable` directive in the `Cache-Control` HTTP header?
<details>
<summary>Answer</summary>
Under standard HTTP caching, when a user reloads a page (by clicking the refresh button or pressing `F5`), the browser sends a conditional validation request (`If-None-Match` or `If-Modified-Since`) to the server for every cached resource, even if `max-age` has not expired, to confirm the file has not changed.

The `immutable` directive (`Cache-Control: public, max-age=31536000, immutable`) informs the browser engine:
*"This file's content will never change during its lifetime. Never send a conditional validation request upon page reload or navigation."*
When combined with content-hashed filenames (`app.8f3a1b.css`), `immutable` completely eliminates 304 validation round trips, allowing the browser to load stylesheets directly from local disk or memory cache instantly.
</details>

### Q3: Why is pre-compressing static CSS files at build time preferable to dynamic on-the-fly server compression?
<details>
<summary>Answer</summary>
Brotli compression operates on a quality scale from 1 (fastest, lowest compression) to 11 (slowest, maximum compression).
- Compressing a 500 KB stylesheet at **Level 11** takes several seconds of intense CPU computation. If done dynamically on every incoming HTTP request, server CPU utilization spikes, queuing requests and increasing Time to First Byte (TTFB).
- Consequently, dynamic on-the-fly servers typically compromise at **Level 4 or 5**, losing 10–15% of potential compression savings.

**Pre-compressing** during the build phase executes Level 11 compression once on the build server, producing static `.br` and `.gz` files. Web servers (Nginx, Caddy, Cloudflare) can then stream these pre-compressed bytes directly from disk or memory cache with zero runtime CPU utilization.
</details>

### Q4: How does LightningCSS achieve dramatically faster minification times than tools like cssnano?
<details>
<summary>Answer</summary>
1. **Native Compiled Rust**: LightningCSS is compiled to native machine code in Rust, avoiding JavaScript V8 engine startup overhead, garbage collection pauses, and dynamic type checks.
2. **Single-Pass Parsing**: Unlike PostCSS (which parses CSS into an AST, runs dozens of independent JavaScript plugins in sequence, and serializes the AST back and forth), LightningCSS executes lexing, parsing, minification, and code generation in a unified, highly optimized single pass.
3. **Data-Oriented Memory Layout**: Rust's memory model allows LightningCSS to process CSS tokens in contiguous memory buffers without allocating thousands of intermediate JavaScript objects.
</details>

### Q5: In the era of HTTP/2 and HTTP/3 multiplexing, is bundling all CSS into a single monolithic file still considered best practice?
<details>
<summary>Answer</summary>
No. While HTTP/1.1 encouraged bundling everything into a single massive file to avoid HTTP connection limits (max 6 connections per origin), **HTTP/2 and HTTP/3 support multiplexed streams over a single connection**, making concurrent resource downloads virtually free of transport overhead.

Bundling all CSS into a single monolith creates two severe performance liabilities:
1. **Unused Code Payload**: First-time visitors on the homepage must download CSS for administrative panels, checkout flows, and user profile settings they may never visit.
2. **Cache Invalidation Fragility**: Any single 1-line change to a modal stylesheet invalidates the entire monolithic hash, forcing every user to re-download the entire 300 KB bundle.
Modern best practice is **granular route-based code splitting**: shipping a core layout stylesheet, and loading route-specific CSS chunks on demand.
</details>

### Q6: How does CSS source map generation impact production performance, and how should it be configured?
<details>
<summary>Answer</summary>
CSS source maps (`.css.map`) map minified production styles back to original Sass/CSS module source files for debugging.
- **Performance Impact**: Source maps do **not** slow down page rendering for standard users! Browsers do not download `.map` files unless the user actively opens DevTools.
- **Configuration Best Practice**: Never inline source maps as base64 data strings (`/*# sourceMappingURL=data:application/json;base64,... */`) into production CSS, as this inflates the actual CSS payload by **300% to 500%**. Always configure bundlers to emit separate, external `.css.map` files, or disable them entirely in public production deployments.
</details>

### Q7: What is the difference between Gzip Level 9 and Brotli Level 11 in terms of compression ratio?
<details>
<summary>Answer</summary>
On standard minified CSS files:
- Gzip at Level 9 typically compresses plain text to approximately **20% to 25%** of its original uncompressed size.
- Brotli at Level 11 leverages its 120 KB static web dictionary and larger sliding window to compress the same stylesheet to approximately **15% to 19%** of its original size.
This results in an additional **15% to 25% net reduction in byte payload over the wire** when moving from Gzip to Brotli.
</details>

### Q8: How can a CI/CD pipeline prevent CSS file size regressions?
<details>
<summary>Answer</summary>
By integrating automated bundle size checking tools (such as `size-limit` or `bundlesize`) into GitHub Actions / GitLab CI workflows:
1. Developers define maximum byte ceilings per route in configuration files.
2. On every Pull Request, the CI runner compiles the production bundle and calculates the Brotli-compressed byte size.
3. If the PR exceeds the threshold, the CI job fails with an error report detailing which chunks grew.
4. Tools like Size Limit post an automated PR comment showing exact byte diffs before and after the branch merge.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build-Time Static Brotli and Gzip Compressor Script
**Objective**: Write a Node.js utility script that scans a `dist/css` directory, compresses all `.css` files using native Node `zlib` at maximum quality for both Brotli and Gzip, and outputs a formatted size report.

<details>
<summary>Solution</summary>

```javascript
// scripts/compress-css.js
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const cssDir = path.resolve(__dirname, '../dist/css');

if (!fs.existsSync(cssDir)) {
  console.error(`Directory not found: ${cssDir}`);
  process.exit(1);
}

const files = fs.readdirSync(cssDir).filter(f => f.endsWith('.css'));

console.log('='.repeat(70));
console.log('COMPRESSION BENCHMARK REPORT');
console.log('='.repeat(70));

files.forEach(file => {
  const filePath = path.join(cssDir, file);
  const rawBuffer = fs.readFileSync(filePath);
  const rawSize = rawBuffer.length;

  // Maximum Gzip compression
  const gzipBuffer = zlib.gzipSync(rawBuffer, { level: 9 });
  fs.writeFileSync(`${filePath}.gz`, gzipBuffer);

  // Maximum Brotli compression
  const brotliBuffer = zlib.brotliCompressSync(rawBuffer, {
    params: {
      [zlib.constants.BROTLI_PARAM_QUALITY]: 11,
      [zlib.constants.BROTLI_PARAM_MODE]: zlib.constants.BROTLI_MODE_TEXT
    }
  });
  fs.writeFileSync(`${filePath}.br`, brotliBuffer);

  const gzipSavings = (((rawSize - gzipBuffer.length) / rawSize) * 100).toFixed(1);
  const brotliSavings = (((rawSize - brotliBuffer.length) / rawSize) * 100).toFixed(1);

  console.log(`File: ${file}`);
  console.log(`  Raw:    ${(rawSize / 1024).toFixed(2)} KB`);
  console.log(`  Gzip:   ${(gzipBuffer.length / 1024).toFixed(2)} KB (-${gzipSavings}%)`);
  console.log(`  Brotli: ${(brotliBuffer.length / 1024).toFixed(2)} KB (-${brotliSavings}%)\n`);
});
```
</details>

### Exercise 2: Nginx Production Configuration for Pre-Compressed CSS
**Objective**: Author an Nginx configuration snippet that enables `gzip_static` and `brotli_static` to serve pre-compressed stylesheets directly from disk with immutable caching headers.

<details>
<summary>Solution</summary>

```nginx
# nginx/conf.d/static-css.conf
server {
    listen 443 ssl http2;
    server_name cdn.domain.com;
    root /var/www/static;

    location /assets/css/ {
        # 1. Serve pre-compressed .br or .gz files directly from disk without CPU overhead
        brotli_static on;
        gzip_static on;

        # 2. Immutable caching for content-hashed files (1 year expiration)
        add_header Cache-Control "public, max-age=31536000, immutable";

        # 3. Security and MIME headers
        types {
            text/css css;
        }
        default_type text/css;
        add_header X-Content-Type-Options "nosniff";

        # 4. Fast disk delivery
        sendfile on;
        tcp_nopush on;
        tcp_nodelay on;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Minification**: Removes comments, whitespace, redundant properties, and optimizes color/unit representations. Use **LightningCSS** for maximum speed.
- **Brotli vs. Gzip**: Brotli includes a 120 KB static web dictionary; compresses CSS **15–25% smaller** than Gzip.
- **Pre-Compression**: Compress with Brotli level 11 and Gzip level 9 at build time; serve `.br` and `.gz` directly from disk.
- **Immutable Caching**: Content-hashed assets (`style.8f3a1b.css`) must be served with `Cache-Control: public, max-age=31536000, immutable`.
- **HTTP/2 & HTTP/3**: Enables multi-stream multiplexing; favor granular route-based CSS splitting over monolithic bundles.
- **Performance Budgets**: Enforce max **<15 KB (brotli)** for initial landing page CSS and **<50 KB** for complex app routes in CI.

---

## 13. Further Reading
- [IETF RFC 7932: Brotli Compressed Data Format](https://datatracker.ietf.org/doc/html/rfc7932)
- [LightningCSS Documentation & Benchmarks](https://lightningcss.dev/)
- [MDN Web Docs: Cache-Control - immutable](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cache-Control#immutable)
- [web.dev: Optimize CSS Delivery](https://web.dev/fast/#optimize-css)
- [Google Web Fundamentals: HTTP/2 Multiplexing](https://developers.google.com/web/fundamentals/performance/http2)

---

| Previous Note | Next Note |
| :--- | :--- |
| [04. Containment and Content Visibility](04-containment-and-content-visibility.md) | [06. Debugging Workflow in DevTools](06-debugging-workflow-in-devtools.md) |
