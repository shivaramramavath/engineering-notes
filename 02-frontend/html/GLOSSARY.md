# Web Platform & HTML Technical Glossary (GLOSSARY.md)

An authoritative glossary of terminology across the WHATWG HTML Living Standard, W3C Web Accessibility Initiative (WAI), and browser engine internals.

---

### A
- **Accessibility Object Model (AOM)**: The internal browser tree derived from the DOM that exposes roles, states, and properties of nodes to platform assistive technologies (screen readers, braille displays).
- **Accessible Name**: The short label associated with an element in the Accessibility Tree, computed according to the W3C Accessible Name and Description Computation (AccName) algorithm.
- **AccName Algorithm**: The deterministic specification determining how text alternatives, labels, headings, and ARIA attributes resolve into an element's accessible name.
- **Active Form Control**: An input, select, button, or textarea capable of accepting user focus and input events.
- **ARIA (Accessible Rich Internet Applications)**: The W3C technical specification providing roles, states, and properties to enhance HTML semantics where native elements fall short.
- **Art Direction**: In responsive images, altering the image content or cropping across different viewports (using `<picture>` and `<source media="...">`) rather than simply resizing the same image.

### B
- **Baseline (Web Platform)**: A standardized classification created by the WebDX Community Group indicating whether a web feature is universally supported across Chrome, Edge, Firefox, and Safari (divided into *Newly Available* and *Widely Available*).
- **Bi-Directional Text (Bidi)**: Text layout algorithms handling mixed left-to-right (LTR) and right-to-left (RTL) writing systems using elements like `<bdi>`, `<bdo>`, and the `dir` attribute.
- **Blink**: The open-source browser rendering engine used by Google Chrome, Microsoft Edge, Opera, and Brave (derived from WebKit).
- **Block Formatting Context (BFC)**: A part of visual CSS rendering in which block boxes are laid out.
- **Blocking Resource**: A script or stylesheet that pauses HTML parsing or page rendering until it has been completely downloaded and executed.

### C
- **Can I Use**: An industry database tracking cross-browser compatibility tables for modern web standards.
- **Canonical URL**: The authoritative URL designated via `<link rel="canonical">` to prevent duplicate content penalties across search engines.
- **Canvas API**: A procedural, pixel-based graphics drawing API accessed via the `<canvas>` element and JavaScript.
- **CORS (Cross-Origin Resource Sharing)**: An HTTP-header-based mechanism allowing servers to indicate origins permitted to read resource responses.
- **Constraint Validation API**: The native JavaScript interface (`checkValidity()`, `reportValidity()`, `validity`, `setCustomValidity()`) that programmatically inspects and enforces form constraints.
- **Content Security Policy (CSP)**: An HTTP header or meta tag allowing site administrators to declare approved sources of executable scripts, styles, images, and frames to mitigate XSS and injection attacks.
- **Critical Rendering Path**: The sequence of stages a browser executes to transform HTML, CSS, and JavaScript into physical screen pixels (Parse $\rightarrow$ DOM $\rightarrow$ CSSOM $\rightarrow$ Render Tree $\rightarrow$ Layout $\rightarrow$ Paint $\rightarrow$ Composite).
- **Custom Elements**: The Web Components standard allowing developers to define novel HTML tags and lifecycle callbacks via `customElements.define()`.

### D
- **Declarative Shadow DOM (DSD)**: The capability to construct and attach a Shadow Root directly in HTML markup without client-side JavaScript using `<template shadowrootmode="open|closed">`.
- **Dialog Element**: The native HTML `<dialog>` tag providing top-layer modal and non-modal dialog boxes with native focus trapping and backdrop styling.
- **Document Object Model (DOM)**: The in-memory tree representation of the parsed HTML document, accessible via JavaScript interfaces.
- **Document Type Declaration (DOCTYPE)**: The mandatory prologue `<!DOCTYPE html>` instructing the browser engine to parse the page in standard "No-Quirks Mode" rather than Quirks Mode.
- **DOM XSS**: Cross-Site Scripting occurring when client-side JavaScript parses unsanitized data into the DOM (e.g. via `element.innerHTML`).

### E
- **Effective Exactly-Once**: In transactional web apps, combining at-least-once message delivery with idempotent consumer checks to guarantee zero duplicate state changes.
- **Error Recovery (HTML Parsing)**: The deterministic WHATWG algorithm ensuring that any ill-formed HTML input is parsed into a valid DOM tree without throwing fatal errors.
- **Exclusive Accordion**: A native disclosure pattern where opening one `<details>` element automatically collapses siblings sharing the exact same `name` attribute.

### F
- **Fetchpriority**: An HTML attribute (`fetchpriority="high|low|auto"`) providing priority hints to the browser preload scanner and network stack.
- **Focus Management**: The programmatic or keyboard-driven control of active element focus within web documents, essential for accessibility.
- **Foster Parenting**: A specialized HTML parser recovery mode where elements misplaced directly inside a `<table>` are moved outside and immediately before the table in the DOM tree.

### G
- **Gecko**: The open-source browser layout engine developed by Mozilla for the Firefox web browser.
- **Global Attributes**: Standard HTML attributes permitted on every HTML element (`class`, `id`, `hidden`, `inert`, `popover`, `tabindex`, `title`, `lang`, `dir`, `translate`, `data-*`).

### H
- **HTML Living Standard**: The continuously updated, authoritative technical specification for HTML maintained by the Web Hypertext Application Technology Working Group (WHATWG).
- **Hydration**: The process in modern frameworks (React, Next.js, Vue) where client-side JavaScript attaches event listeners and state to server-rendered HTML markup.

### I
- **Idempotency**: The mathematical property ($f(f(x)) = f(x)$) where executing an operation multiple times produces the identical outcome as executing it once.
- **Import Maps**: A declarative JSON mapping specified inside `<script type="importmap">` enabling bare module specifiers in native ES module imports.
- **Inert Attribute**: A global HTML boolean attribute that completely disables interactivity, pointer events, and screen reader visibility for an element subtree.
- **Invoker Commands**: An emerging web standard allowing buttons to trigger actions on interactive targets declaratively via `commandfor` and `command` attributes.

### L
- **Landmark Elements**: Semantic HTML containers (`<header>`, `<nav>`, `<main>`, `<footer>`, `<aside>`, `<search>`, `<form>`, `<section>`) creating structured regions for assistive technologies.
- **Lazy Loading**: Deferring the network request and decoding of off-screen images or iframes until they approach the user's viewport (`loading="lazy"`).
- **Light Dismiss**: The built-in browser behavior where clicking outside an active popover/dialog or pressing the `Esc` key automatically closes the element.

### M
- **MathML Core**: The mathematical markup language standard natively supported by modern browser engines for accessible, crisp mathematical notation.
- **Microdata**: An HTML specification for embedding machine-readable semantic key-value annotations into human-visible HTML markup.

### N
- **No-Quirks Mode**: Modern standards-compliant rendering mode triggered by `<!DOCTYPE html>`.
- **Nu HTML Checker (v.Nu)**: The official W3C/WHATWG validator service and command-line engine for validating HTML documents against current specifications.

### O
- **Open Graph Protocol (OGP)**: A metadata standard originally designed by Facebook allowing web pages to become rich objects in social graph feeds (e.g. `og:title`, `og:image`).
- **Open UI**: A W3C community group focused on standardizing form controls and interactive components for the web platform (creators of customizable select and invokers).

### P
- **Picture Element**: The HTML container `<picture>` coordinating multiple `<source>` tags for art direction, media queries, and modern image format negotiation (AVIF, WebP, JPEG).
- **Popover API**: A native HTML mechanism providing top-layer presentation, light dismiss, and declarative invocation for popovers and tooltips without z-index conflicts.
- **Preload Scanner**: A lightweight, background parser in browser engines that scans raw HTML tokens ahead of the main parser to eagerly discover and fetch external resources.

### Q
- **Quirks Mode**: A legacy rendering engine mode emulating bugs in Internet Explorer 5 and Netscape 4, triggered by an omitted or malformed DOCTYPE.

### R
- **Render Tree**: The internal browser structure combining the DOM and CSSOM, containing only nodes that are visually rendered on screen.
- **Reverse Tabnabbing**: A security vulnerability where a newly opened tab (`target="_blank"`) accesses `window.opener.location` to redirect the parent tab to a phishing page (mitigated by `rel="noopener"`).
- **Ruby Annotation**: Specialized typographic typography used to display pronunciation guides above East Asian characters (`<ruby>`, `<rt>`).

### S
- **Search Element**: The semantic landmark element `<search>` dedicated to wrapping search, filtering, and lookup controls.
- **Selectedcontent**: An element in the emerging customizable select standard that displays the cloned content of the currently selected `<option>` inside the trigger button.
- **Shadow DOM**: A scoped subtree of DOM elements encapsulated within an element, shielding internal styles and markup from global page CSS.
- **Speculation Rules API**: A declarative JSON API via `<script type="speculationrules">` instructing the browser to prefetch or fully prerender candidate navigation pages.
- **Subresource Integrity (SRI)**: A security feature validating that downloaded external scripts and stylesheets match a cryptographic SHA hash specified in the `integrity` attribute.

### T
- **Top Layer**: A dedicated browser rendering layer positioned above all normal document stacking contexts and `z-index` levels, used natively by `<dialog>` and `[popover]`.
- **Trusted Types**: A browser security API enforcing that strings assigned to injection sinks (such as `innerHTML`) must first pass through a sanitizer policy.

### V
- **Void Element**: An HTML element that cannot contain any child nodes or end tags (`<img`, `<input>`, `<br>`, `<meta>`, `<link>`, etc.).

### W
- **WCAG (Web Content Accessibility Guidelines)**: The international technical standard for web accessibility published by the W3C WAI (levels A, AA, AAA).
- **WebKit**: The open-source browser rendering engine used by Apple Safari and all web browsers on iOS.
- **WHATWG**: Web Hypertext Application Technology Working Group, the standards organization maintaining the HTML Living Standard.
