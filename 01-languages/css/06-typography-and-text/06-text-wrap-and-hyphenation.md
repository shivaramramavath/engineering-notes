# Text Wrapping & Hyphenation

| Metadata | Details |
| :--- | :--- |
| **Topic** | Modern Wrapping Algorithms (`text-wrap: balance / pretty`), Overflow Wrapping (`overflow-wrap`), Hyphenation (`hyphens: auto`) |
| **Spec Level** | CSS Text Module Level 4 / CSS Text Module Level 3 |
| **Baseline Status** | Widely Available (`overflow-wrap` Baseline 2017; `text-wrap: balance` Baseline 2023; `text-wrap: pretty` Baseline 2024) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Chromium LayoutNG, Safari WebKit, Firefox Gecko, Screen Readers |
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

On the web, text flows dynamically inside responsive containers. Traditionally, browser layout engines wrap text using a simple "greedy" algorithm: words are packed into the current line until another word would exceed the container boundary, at which point the word drops to the next line. 

While greedy wrapping is computationally fast ($O(N)$), it creates notorious typographic defects:
- **Typographic Widows / Orphans**: A single lonely word stranded on the final line of a card title or paragraph.
- **Ragged Asymmetry**: A two-line headline where line 1 has 9 words and line 2 has 1 word.
- **Horizontal Viewport Blowout**: Massive unbroken strings (e.g., long URLs, hash IDs, or API tokens) that break out of flex/grid cards and expand the document width.

Modern CSS solves these issues natively through two major property families:
1. `text-wrap: balance` (Baseline 2023) and `text-wrap: pretty` (Baseline 2024).
2. Advanced word breaking and hyphenation: `overflow-wrap: anywhere`, `word-break: break-all`, and `hyphens: auto`.

---

## 2. Core Concepts

### `text-wrap: balance` (For Headlines)
`text-wrap: balance` instructs the browser to calculate the total length of the text and balance line lengths evenly across all wrapped lines.
- Best for: Short headings, hero titles, card titles, and tooltips (up to 4–6 lines).
- Result: Eliminates awkward line breaks, creating symmetrical, polished visual blocks.
- Performance boundary: To prevent severe layout performance degradation, browser engines enforce an internal cap (typically 4 to 6 lines max); on longer paragraphs, browsers automatically fall back to standard greedy wrapping.

### `text-wrap: pretty` (For Body Paragraphs)
`text-wrap: pretty` (CSS Text Level 4, Baseline 2024) specifically targets **orphan prevention** on body copy.
- Best for: Editorial body copy, paragraphs, list items, and blockquotes.
- Mechanics: Evaluates the last four lines of a paragraph. If the final line would contain only a single solitary word ("orphan"), the layout engine adjusts the preceding lines slightly to drop two or more words onto the final line.
- Unlike `balance`, `pretty` has minimal performance overhead and does not attempt to equalize all line lengths.

### Handling Unbroken Strings: `overflow-wrap` vs `word-break`
When long strings (URLs, email addresses, continuous code strings) exceed container width:
- **`overflow-wrap: anywhere` (Recommended)**: Wraps at standard soft-wrap break opportunities (spaces/hyphens) first. If and *only if* an unbreakable word exceeds the container width, it splits the word anywhere. Calculates intrinsic min-content sizing based on the broken fragments.
- **`overflow-wrap: break-word`**: Similar to `anywhere`, but does not calculate min-content sizing around broken fragments.
- **`word-break: break-all`**: Aggressive; breaks words between any two characters indiscriminately, even when ordinary spaces exist on the line (use only for code/hashes, never for natural prose).

### Automatic Hyphenation (`hyphens: auto`)
Enables language-specific hyphenation dictionaries to insert hyphens at appropriate syllable boundaries.
- **Strict Requirement**: Requires the HTML document or element to have an explicit language attribute (`<html lang="en">` or `<p lang="de">`). Without `lang`, `hyphens: auto` is completely ignored by the browser.
- `hyphens: manual`: Only breaks at explicit soft hyphens (`&shy;`) inserted into the markup.

---

## 3. How It Works Internally

```
Comparison of Wrapping Algorithms

1. Default Greedy Wrapping (text-wrap: wrap):
+------------------------------------+
| Building High-Performance Websites |
| with                               |  <--- Lonely widow/orphan!
+------------------------------------+

2. Balanced Wrapping (text-wrap: balance):
+------------------------------------+
| Building High-Performance          |
| Websites with Modern CSS           |  <--- Balanced line lengths!
+------------------------------------+

3. Pretty Body Wrapping (text-wrap: pretty):
Default:
| ...the browser engine calculates layout and creates a new |
| stacking context.                                         | <--- 2 words (ugly)

text-wrap: pretty:
| ...the browser engine calculates layout and creates       |
| a new stacking context.                                   | <--- 4 words (graceful)
```

### The Binary Search Mechanics of `text-wrap: balance`
To balance lines without testing infinite line combinations:
1. The engine calculates the minimum inline size required to fit the entire string on one line.
2. It divides by the target line count to establish an ideal balanced target width.
3. It performs a fast binary search on container widths to find the narrowest width that maintains the minimum line count while minimizing variance in line lengths.

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`overflow-wrap: anywhere`**: Baseline Widely Available (Chrome 80+, Safari 15.4+, Firefox 65+).
- **`text-wrap: balance`**: Baseline 2023 (Chrome 114+, Safari 17.0+, Firefox 121+).
- **`text-wrap: pretty`**: Baseline 2024 (Chrome 117+, Safari 18.0+, Firefox 125+).
- **`hyphens: auto`**: Baseline Widely Available (with appropriate language dictionaries installed).

```css
/* Modern Headline Balancing */
h1, h2, h3, h4 {
  text-wrap: balance;
}

/* Modern Body Copy Orphan Prevention */
p, li, blockquote {
  text-wrap: pretty;
}

/* Bulletproof Container Word Wrapping */
.user-generated-content {
  overflow-wrap: anywhere;
  word-break: normal;
}
```

---

## 5. Practical Examples

### Example 1: Eliminating Headline Widows in Responsive Cards
```html
<div class="feature-card">
  <span class="category">Cloud Infrastructure</span>
  <h3 class="card-title">Distributed Key-Value Storage at Planetary Scale</h3>
  <p class="card-desc">
    Deploy ultra-low latency key-value databases across sixty global edge points of presence.
  </p>
</div>
```

```css
.feature-card {
  max-width: 380px;
  padding: 1.5rem;
  border: 1px solid #e2e8f0;
  border-radius: 12px;
  background-color: #ffffff;
}

.card-title {
  font-family: system-ui, sans-serif;
  font-size: 1.375rem;
  font-weight: 700;
  line-height: 1.25;
  color: #0f172a;
  margin-block: 0.5rem 0.75rem;

  /* Balances headline evenly across lines; eliminates single-word drops */
  text-wrap: balance;
}

.card-desc {
  font-size: 0.9375rem;
  line-height: 1.5;
  color: #475569;

  /* Prevents single orphan words on the last line */
  text-wrap: pretty;
}
```

### Example 2: Safeguarding Chat Bubbles Against URL Blowout
```html
<div class="chat-message bubble-incoming">
  <p>Please check this transaction log:</p>
  <a href="https://explorer.domain.internal/tx/0x88dfb2c8928392183928193821938219381293812983192831928319">
    https://explorer.domain.internal/tx/0x88dfb2c8928392183928193821938219381293812983192831928319
  </a>
</div>
```

```css
.chat-message {
  max-width: 75%;
  padding: 0.75rem 1rem;
  border-radius: 12px;
  background-color: #f1f5f9;
  color: #1e293b;
  font-size: 0.875rem;
  line-height: 1.4;

  /* Critical: Allows long URLs to split safely without overflowing container */
  overflow-wrap: anywhere;
}

.chat-message a {
  color: #2563eb;
  text-decoration: underline;
  text-underline-offset: 3px;
}
```

### Example 3: Multi-Language Hyphenation
```html
<div class="editorial-column" lang="de">
  <h2>Donaudampfschifffahrtselektrizitätenhauptbetriebswerkbauunterbeamtengesellschaft</h2>
  <p>
    Lange deutsche Komposita stellen Layouts vor große Herausforderungen, wenn keine
    automatische Silbentrennung aktiviert ist.
  </p>
</div>
```

```css
.editorial-column {
  max-width: 320px;
  border: 1px solid #cbd5e1;
  padding: 1rem;
}

.editorial-column h2,
.editorial-column p {
  /* Automatically breaks at linguistic syllables based on lang="de" */
  hyphens: auto;
  hyphenate-limit-chars: 6 3 3; /* Min word length 6, min 3 chars before & after */
}
```

### Example 4: Manual Soft Hyphen Control (`&shy;`)
```html
<!-- Inserts optional hyphen that is ONLY visible if word actually breaks -->
<h1 class="hero-title">
  Ultra&shy;micro&shy;scopic&shy;silico&shy;volcano&shy;coniosis
</h1>
```

```css
.hero-title {
  font-size: 2.5rem;
  hyphens: manual; /* Only breaks at &shy; opportunities */
}
```

### Example 5: Code Container Word Break Defense
```css
/* Code containers: Break long API hashes without destroying normal syntax words */
.api-token-container {
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
  font-size: 0.8125rem;
  padding: 0.75rem;
  background-color: #0f172a;
  color: #38bdf8;
  border-radius: 6px;
  overflow-wrap: anywhere;
  white-space: pre-wrap; /* Honors newlines but wraps when reaching edge */
}
```

---

## 6. Best Practices

1. **Apply `text-wrap: balance` Exclusively to Headings**: Do not apply `text-wrap: balance` to `p`, `body`, or `*`. Because balancing requires iterative binary search calculations, applying it to thousands of paragraphs damages page rendering performance.
2. **Apply `text-wrap: pretty` to Body Copy**: `text-wrap: pretty` has virtually zero performance cost and eliminates awkward orphan words across editorial paragraphs.
3. **Always Add `overflow-wrap: anywhere` to User-Generated Content**: Any forum, chat message, comment, or card rendering user input must have `overflow-wrap: anywhere` to prevent malicious or accidental long strings from breaking layout.
4. **Always Declare `lang="..."` When Using `hyphens: auto`**: Browsers cannot hyphenate text without knowing the linguistic rules of that language. Always declare `<html lang="en">` (or appropriate language code).
5. **Use `hyphenate-limit-chars` to Prevent Micro-Breaks**: Configure `hyphenate-limit-chars: 6 3 3;` to ensure short words are never chopped awkwardly (e.g., breaking "in-to").
6. **Prefer `overflow-wrap: anywhere` Over `word-break: break-all`**: `word-break: break-all` will break normal English words mid-syllable even when there is plenty of room on the next line; `overflow-wrap` only breaks words if there is literally no other choice.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **`text-wrap: balance` on `body` or `p`** | Destroys scroll and render performance as the browser tries to balance millions of words. | Reserve `balance` for `h1–h6`; use `text-wrap: pretty` for `p`. |
| **`hyphens: auto` without HTML `lang`** | Browser has no hyphenation dictionary loaded; property fails silently. | Set `<html lang="en">` in HTML markup. |
| **Using `word-break: break-all` on body text** | Slices standard English words in half at line endings ("th-is is a car-d"). | Use `overflow-wrap: anywhere;` with `word-break: normal;`. |
| **Expectation that `balance` works on 20-line texts** | Browsers deliberately enforce a 4–6 line ceiling on `balance` to safeguard performance. | Use `balance` only on short titles; accept standard wrapping on long texts. |
| **Using deprecated `word-break: break-word`** | Deprecated WebKit-prefixed legacy property; inconsistent behavior in modern standards. | Use standard `overflow-wrap: anywhere;` or `break-word;`. |
| **Hardcoding `<br>` tags to fix mobile titles** | Hard breaks remain active on desktop, creating bizarre breaks on large screens. | Remove `<br>` and use `text-wrap: balance`. |

---

## 8. Performance, Accessibility & Browser Support

### Layout Complexity & Rendering Budgets
- `text-wrap: wrap`: $O(N)$ linear pass. Fast enough for 100,000 words.
- `text-wrap: pretty`: Evaluates only the last 4 lines of a paragraph. Near-zero CPU overhead; safe for all body text.
- `text-wrap: balance`: $O(N \log M)$ binary search iterations per element. Safe for headings ($< 6$ lines), but dangerous if applied globally.

### Screen Reader Pronunciation
Neither `text-wrap: balance` nor `text-wrap: pretty` alters the underlying DOM string or character sequence. Screen readers read the sentence uninterrupted without pausing or stumbling. In contrast, hardcoded `<br>` tags frequently cause screen readers to pause unnaturally between words.

---

## 9. Comparison / When to Use What

| Feature | Property | Algorithm | Best Target |
| :--- | :--- | :--- | :--- |
| **Headline Balancing** | `text-wrap: balance` | Binary search width minimization | Headings, hero banners, modal titles |
| **Orphan Prevention** | `text-wrap: pretty` | Scans last 4 lines for single words | Body copy, article paragraphs, list items |
| **Standard Fast Wrap** | `text-wrap: wrap` | Standard greedy algorithm | High-volume streaming logs, tables |
| **Safe String Overflow**| `overflow-wrap: anywhere`| Soft-wraps first; splits only when overflow occurs | Comments, chat bubbles, URLs |
| **Linguistic Syllables**| `hyphens: auto` | Dictionary hyphen insertion | Narrow editorial newspaper columns |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between <code>text-wrap: balance</code> and <code>text-wrap: pretty</code>?</summary>

- `text-wrap: balance` aims for **visual symmetry across all lines**. It equalizes the line lengths of a multi-line heading using a binary search algorithm. It is capped by browser layout engines at 4–6 lines to prevent performance bottlenecks.
- `text-wrap: pretty` aims specifically for **orphan prevention on the final line**. It does not try to equalize every line; it only evaluates the last four lines of a paragraph, slightly adjusting line breaks so that the final line contains at least two words. Because of its minimal performance footprint, `pretty` is designed for body copy.
</details>

<details>
<summary>2. Why does the browser impose a line-count limit on <code>text-wrap: balance</code>?</summary>

Balancing line lengths across a paragraph is an $O(N \log M)$ optimization problem requiring multiple layout passes and line-breaking iterations. If applied to a 100-line legal contract or long blog post, calculating the optimal line balance across hundreds of words would stall the browser main thread and cause frame drops during scrolling and resizing. Browsers therefore clamp `balance` to short blocks (typically 4–6 lines).
</details>

<details>
<summary>3. What is the difference between <code>overflow-wrap: anywhere</code> and <code>word-break: break-all</code>?</summary>

- `word-break: break-all` will break a word between any two arbitrary characters whenever the line reaches the edge, ignoring natural spaces if convenient. It destroys normal prose readability.
- `overflow-wrap: anywhere` prioritizes normal break opportunities (spaces, hyphens) first. It **only** splits a word if that word is individually longer than the entire width of the containing block (such as a 60-character URL). Furthermore, `anywhere` correctly calculates `min-content` intrinsic size based on the broken fragments.
</details>

<details>
<summary>4. Why does <code>hyphens: auto</code> frequently fail to work on web pages?</summary>

`hyphens: auto` strictly depends on the browser loading the correct linguistic hyphenation dictionary for that document or element. If the developer fails to define an explicit `lang` attribute on `<html>` or the element itself (e.g., `<html lang="en">` or `<p lang="fr">`), the browser engine has no way of knowing whether the text follows English, German, or French syllable rules, and silently falls back to `hyphens: manual`.
</details>

<details>
<summary>5. Why is using hardcoded <code>&lt;br&gt;</code> tags considered bad practice for responsive heading formatting?</summary>

A `<br>` tag is rigid and immutable. While it may create a pleasing two-line break on a specific 375px mobile screen, when the page is viewed on a 768px tablet or 1440px desktop monitor, the `<br>` remains active, resulting in awkward, stunted line breaks. Additionally, some screen readers interpret `<br>` as a sentence pause. `text-wrap: balance` handles line breaking dynamically and responsively across all screen widths.
</details>

<details>
<summary>6. What is a soft hyphen (<code>&amp;shy;</code>) and when should it be used?</summary>

A soft hyphen (`&shy;` or `U+00AD`) is an invisible character entity placed inside a long word. If the word fits completely on a line, the soft hyphen remains totally invisible. If the word must break at the edge of the container, the browser splits the word at that exact location and renders a visible hyphen (`-`). It provides precise, manual control over hyphenation without relying on browser dictionary support.
</details>

<details>
<summary>7. How does <code>hyphenate-limit-chars</code> enhance automatic hyphenation?</summary>

`hyphenate-limit-chars: [total] [before] [after]` prevents awkward or distracting hyphenation:
- `total`: Minimum character count a word must have to be eligible for hyphenation.
- `before`: Minimum number of characters that must remain on the line before the hyphen.
- `after`: Minimum number of characters that must be carried over to the next line.
For example, `hyphenate-limit-chars: 6 3 3;` ensures a word like "eating" is never broken into "ea-ting".
</details>

<details>
<summary>8. Does <code>text-wrap: balance</code> cause Cumulative Layout Shift (CLS)?</summary>

No. Modern browser engines (Chromium LayoutNG, Safari WebKit) execute `text-wrap: balance` during the initial layout phase before the first paint frame is submitted to the compositor. The balanced line box dimensions are established prior to rendering, resulting in zero CLS.
</details>

---

## 11. Practice Exercises

### Exercise 1: Implement Balanced Typography on Responsive Cards
**Goal**: Configure all card titles to balance automatically without orphan words.

<details>
<summary>View Solution</summary>

```css
.card-grid .card-title {
  font-family: system-ui, sans-serif;
  font-size: 1.25rem;
  font-weight: 700;
  line-height: 1.3;
  text-wrap: balance;
  margin-block-end: 0.5rem;
}

.card-grid .card-summary {
  font-size: 0.875rem;
  line-height: 1.5;
  color: #64748b;
  text-wrap: pretty;
}
```
</details>

### Exercise 2: Bulletproof Overflow Wrapping for User Comments
**Goal**: Prevent long URLs and hash strings in user comments from breaking out of flex containers.

<details>
<summary>View Solution</summary>

```css
.comment-body {
  width: 100%;
  min-width: 0; /* Critical flex child defense */
  overflow-wrap: anywhere;
  word-break: normal;
  line-height: 1.5;
}
```
</details>

### Exercise 3: Editorial Column Multi-Lingual Hyphenation
**Goal**: Create a narrow editorial column that automatically hyphenates German text with a 6-3-3 character limit.

<details>
<summary>View Solution</summary>

```html
<article class="narrow-column" lang="de">
  <p>
    Die Automobil-Industrie-Aktiengesellschaft plant umfangreiche Investitionen in neue Fertigungsanlagen.
  </p>
</article>
```

```css
.narrow-column {
  max-width: 260px;
  padding: 1rem;
  border: 1px solid #e2e8f0;
}

.narrow-column p {
  hyphens: auto;
  hyphenate-limit-chars: 6 3 3;
  line-height: 1.6;
}
```
</details>

### Exercise 4: Prevent Text Truncation with `text-wrap: nowrap`
**Goal**: Ensure status badge labels never wrap to multiple lines under any circumstances.

<details>
<summary>View Solution</summary>

```css
.status-badge {
  display: inline-flex;
  align-items: center;
  padding: 0.25rem 0.75rem;
  border-radius: 9999px;
  font-size: 0.75rem;
  font-weight: 600;
  text-wrap: nowrap; /* Guarantees badge text stays single-line */
  white-space: nowrap; /* Fallback for older browsers */
}
```
</details>

### Exercise 5: Test Intrinsic Sizing with `overflow-wrap: anywhere`
**Goal**: Build an intrinsically-sized component (`width: min-content`) containing a long URL to verify wrapping behavior.

<details>
<summary>View Solution</summary>

```css
.intrinsic-box {
  width: min-content;
  border: 2px solid #2563eb;
  padding: 1rem;
  overflow-wrap: anywhere;
}
```
</details>

### Exercise 6: Responsive Hero Headline Tuning
**Goal**: Style a hero headline that balances lines on desktop and allows smooth hyphenation if words exceed mobile screen bounds.

<details>
<summary>View Solution</summary>

```css
.hero-title {
  font-size: clamp(2rem, 5vw, 4rem);
  font-weight: 800;
  line-height: 1.1;
  text-wrap: balance;
  overflow-wrap: anywhere;
  hyphens: auto;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Property | Values | Purpose |
| :--- | :--- | :--- |
| `text-wrap` | `balance` | Equalizes line lengths; eliminates headline orphans |
| `text-wrap` | `pretty` | Prevents single-word orphans on paragraph endings |
| `text-wrap` | `nowrap` | Forbids line wrapping entirely |
| `overflow-wrap` | `anywhere` | Safely breaks unbreakable strings; supports min-content |
| `word-break` | `break-all` | Indiscriminately breaks words at boundary |
| `hyphens` | `auto` | Dictionary-based syllable hyphenation (requires `lang`) |
| `hyphenate-limit-chars` | `6 3 3` | Enforces minimum word and segment character limits |

---

## 13. Further Reading
- [W3C CSS Text Module Level 4 - text-wrap](https://www.w3.org/TR/css-text-4/#text-wrap)
- [MDN Web Docs: text-wrap](https://developer.mozilla.org/en-US/docs/Web/CSS/text-wrap)
- [web.dev: CSS text-wrap: balance](https://web.dev/blog/css-text-wrap-balance)
- [web.dev: CSS text-wrap: pretty](https://developer.chrome.com/blog/css-text-wrap-pretty)
- [MDN Web Docs: hyphens](https://developer.mozilla.org/en-US/docs/Web/CSS/hyphens)

---

[Previous: 05-line-height-and-readability.md](05-line-height-and-readability.md) | [Module README](README.md) | [Next: 07-writing-modes-and-internationalisation.md](07-writing-modes-and-internationalisation.md)
