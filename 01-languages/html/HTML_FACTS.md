# HTML Living Standard & Web Platform Facts (HTML_FACTS.md)

**Generated:** 2026-11-26  
**Primary Standard:** WHATWG HTML Living Standard (https://html.spec.whatwg.org/multipage/)  
**Baseline Reference:** Web Platform Baseline (W3C WebDX Community Group / web.dev)  
**Browser Engines:** Chromium (Blink / V8), Gecko (Firefox / SpiderMonkey), WebKit (Safari / JavaScriptCore)

---

## 1. Baseline Status & Feature Support Matrix

| Feature / Element / Attribute | Spec Level / Standard | Baseline Status | Minimum Browser Engine Support | Fallback / Progressive Enhancement Strategy | Key Spec Reference / URL |
|:---|:---|:---|:---|:---|:---|
| **`popover` attribute** | WHATWG HTML / Popover API | **Baseline: Newly Available** (Jan 2025) | Chrome 116+, Edge 116+, Safari 17.0+, Firefox 125+ | Emulate with JavaScript dialog/inert or polyfill; top-layer native promotion | https://html.spec.whatwg.org/multipage/popover.html |
| **`<dialog>` element** | WHATWG HTML | **Baseline: Widely Available** (2022) | Chrome 98+, Edge 98+, Safari 15.4+, Firefox 98+ | `<dialog>` polyfill; accessible modal with `aria-modal="true"` and manual focus trap | https://html.spec.whatwg.org/multipage/interactive-elements.html#the-dialog-element |
| **`inert` attribute** | WHATWG HTML | **Baseline: Widely Available** (2023) | Chrome 102+, Edge 102+, Safari 15.5+, Firefox 112+ | `wicg-inert` polyfill or manual `tabindex="-1"` and `aria-hidden="true"` | https://html.spec.whatwg.org/multipage/interaction.html#inert-subtrees |
| **`<search>` element** | WHATWG HTML | **Baseline: Widely Available** (Oct 2023) | Chrome 118+, Edge 118+, Safari 17.0+, Firefox 118+ | Fallback: `<div role="search">` or `<form role="search">` | https://html.spec.whatwg.org/multipage/grouping-content.html#the-search-element |
| **`<details name="...">` (Exclusive Accordions)** | WHATWG HTML | **Baseline: Newly Available** (Sep 2024) | Chrome 120+, Edge 120+, Safari 17.2+, Firefox 130+ | Harmless degradation: older browsers open multiple panels independently | https://html.spec.whatwg.org/multipage/interactive-elements.html#the-details-element |
| **Declarative Shadow DOM (`<template shadowrootmode="...">`)** | WHATWG HTML / DOM | **Baseline: Widely Available** (2024) | Chrome 111+, Edge 111+, Safari 16.4+, Firefox 123+ | Client-side hydration via `attachShadow()` polyfill script | https://html.spec.whatwg.org/multipage/scripting.html#declarative-shadow-dom |
| **`fetchpriority` attribute** | W3C Priority Hints / WHATWG | **Baseline: Newly Available** (Oct 2024) | Chrome 102+, Edge 102+, Safari 17.2+, Firefox 132+ | Browser ignores attribute; loads with default engine heuristics | https://html.spec.whatwg.org/multipage/urls-and-fetching.html#priority-hints |
| **`enterkeyhint` attribute** | WHATWG HTML | **Baseline: Widely Available** (2024) | Chrome 77+, Edge 79+, Safari 13.1+, Firefox 94+ | Visual hint only; defaults to generic return key | https://html.spec.whatwg.org/multipage/interaction.html#input-modalities:-the-enterkeyhint-attribute |
| **Import Maps (`<script type="importmap">`)** | WHATWG HTML | **Baseline: Widely Available** (2025) | Chrome 89+, Edge 89+, Safari 16.4+, Firefox 108+ | ES-module shims or bundling fallback | https://html.spec.whatwg.org/multipage/webappapis.html#import-maps |
| **`command` / `commandfor` (Invoker Commands)** | WHATWG HTML (Draft / Candidate) | **Limited Availability / In Development** (Preview) | Chrome 134+ Canary/Dev, Safari TP; in development in Firefox | Use standard JS `button.addEventListener('click', ...)` | https://open-ui.org/components/invokers.explainer/ |
| **Customizable Select (`<selectedcontent>`, `appearance: base-select`)** | WHATWG HTML / Open UI | **Limited Availability / Experimental** | Chrome 130+ behind flag, Open UI standard track | Standard `<select>` with `<option>` elements | https://open-ui.org/components/customizableselect/ |
| **Speculation Rules API (`<script type="speculationrules">`)** | WICG / Chromium standard track | **Limited Availability** (Chromium only) | Chrome 109+, Edge 109+; Firefox/Safari under consideration | Fallback to `<link rel="prefetch">` and `<link rel="prerender">` | https://developer.mozilla.org/en-US/docs/Web/HTML/Element/script/type/speculationrules |
| **`blocking="render"` attribute** | WHATWG HTML | **Limited Availability** | Chrome 105+, Edge 105+, Safari 18.2+; Firefox unsupported | Fallback: standard parser-blocking `<script>` or `<link rel="stylesheet">` | https://html.spec.whatwg.org/multipage/urls-and-fetching.html#blocking-attributes |
| **`loading="lazy"` on `<img>` & `<iframe>`** | WHATWG HTML | **Baseline: Widely Available** (2022) | Chrome 76+, Edge 79+, Safari 15.4+, Firefox 75+ | Native browser viewport intersection; ignored on older browsers | https://html.spec.whatwg.org/multipage/embedded-content.html#lazy-loading-attributes |
| **Responsive Images (`srcset`, `sizes`, `<picture>`)** | WHATWG HTML | **Baseline: Widely Available** (2018) | Universal modern browser support | `src` fallback image inside `<picture>` | https://html.spec.whatwg.org/multipage/images.html |
| **MathML (`<math>`)** | W3C MathML Core / WHATWG | **Baseline: Widely Available** (2023) | Chrome 109+, Edge 109+, Safari 14.1+, Firefox 1+ | MathJax or KaTeX client-side fallback | https://w3c.github.io/mathml-core/ |

---

## 2. Browser Engine Parsing & DOM Construction Invariants

1. **Error Recovery Architecture**:
   - The HTML parser is defined as a deterministic state machine by the WHATWG standard.
   - **HTML never throws a fatal parsing error**: ill-formed tags, missing close tags, or improper nesting are automatically recovered using defined insertion modes (e.g. "in body", "in table", "after body").
   - Void elements (`<area>`, `<base>`, `<br>`, `<col>`, `<embed>`, `<hr>`, `<img>`, `<input>`, `<link>`, `<meta>`, `<source>`, `<track>`, `<wbr>`) cannot have children; self-closing slashes (`<br />`) in HTML5 are treated as syntactic sugar and ignored by the parser.
2. **Tree Construction & Foster Parenting**:
   - Misplaced content inside a `<table>` (e.g. text or `<div>` placed outside `<td>` or `<th>`) triggers **foster parenting**: the parser moves the invalid elements outside and before the table in the DOM tree.
3. **Implicit Tag Insertion**:
   - The parser implicitly synthesizes `<html>`, `<head>`, and `<body>` elements if omitted in simple documents.
   - A `<tbody>` element is automatically inserted inside `<table>` if `<tr>` elements are provided without an explicit `<tbody>`.

---

## 3. Accessibility & The Accessibility Tree (AOM)

1. **First Rule of ARIA**: "If you can use a native HTML element or attribute with the semantics and behavior you require already built in, then do so."
2. **Landmarks**: `<header>` (banner), `<nav>` (navigation), `<main>` (main), `<footer>` (contentinfo), `<aside>` (complementary), `<search>` (search), `<form>` (form), `<section>` (region when labelled).
3. **Accessible Name Computation**: Determined by `aria-labelledby` > `aria-label` > native labelling (`<label for="...">`, `<caption>`, `<legend>`) > element content / `alt` attribute > `title` attribute.

---

## 4. Security & Content Isolation

1. **Target `_blank` Reverse Tabnabbing**: Modern browsers automatically set `rel="noopener"` on `target="_blank"`. Explicit `rel="noopener noreferrer"` remains best practice for legacy environments.
2. **Iframe Sandboxing**: Default `sandbox` attribute blocks scripts, forms, popups, and same-origin access. Explicit tokens: `allow-scripts`, `allow-same-origin`, `allow-forms`.
3. **Cross-Site Scripting (XSS)**: Untrusted input rendered in HTML must be HTML entity encoded (`&` $\rightarrow$ `&amp;`, `<` $\rightarrow$ `&lt;`, `>` $\rightarrow$ `&gt;`, `"` $\rightarrow$ `&quot;`, `'` $\rightarrow$ `&#39;`).
