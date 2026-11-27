# HTML Track Changelog (HTML_CHANGELOG.md)

All additions, structural decisions, and specification verifications for the HTML learning track are recorded here.

---

### 2026-11-26 - Track Initialization & Governance Baseline
- Initialized `01-languages/html/` from scratch as the canonical web markup learning track.
- Authored `HTML_FACTS.md` with verified Web Platform Baseline statuses (2024-2026) for Popover API, `<dialog>`, `inert`, `<search>`, `<details name="...">`, Declarative Shadow DOM, `fetchpriority`, Invoker Commands, and Import Maps.
- Authored `HTML_AUDIT.md` auditing repository state and confirming empty `02-frontend/html/`.
- Authored `HTML_CONVENTIONS.md` establishing the mandatory 13-section concept note template, 4-file project specs, and scratch runner verification protocol.
- Authored `_templates/concept-template.md` and `_templates/project-template.md`.
- Authored `HTML_DECISIONS.md` documenting architecture, scratch testing, and markdown-only constraints.
- Authored `HTML_COMPLETION_PLAN.md` detailing the 13 modules (00 through 12).
- Authored `HTML_PROGRESS.md` tracking all 170+ curriculum files.
- Implemented `scratch/html_runner.js` outside the repository for automated W3C syntax and accessibility verification.

### 2026-11-26 - Module 00: Setup & Browser Engine Architecture Completed
- Authored `00-setup/README.md`: Module overview, learning outcomes, contents roadmap.
- Authored `00-setup/01-what-is-html-and-the-web.md`: Hypertext, client-server lifecycle, MIME types, HTTP/2 & HTTP/3, separation of concerns.
- Authored `00-setup/02-how-browsers-turn-html-into-pixels.md`: Critical Rendering Path, Tokenizer state machine, Tree Construction dispatcher, CSSOM, Render Tree, Layout, Paint, GPU Compositing, Preload Scanner.
- Authored `00-setup/03-html-history-and-the-living-standard.md`: HTML 1-4, XHTML 1.0/2.0 schism, Yellow Screen of Death, WHATWG formation, W3C 2019 MoU, Web Platform Baseline.
- Authored `00-setup/04-tools-editor-and-devtools.md`: View-Source vs live DOM, DevTools Elements panel, Accessibility Tree inspector, DOM mutation breakpoints, Emmet abbreviations.
- Authored `00-setup/05-your-first-page.md`: Essential 5-point baseline (`<!DOCTYPE html>`, `html[lang]`, `meta[charset]`, `meta[viewport]`, `<title>`), local HTTP server vs `file://` protocol.
- Authored `00-setup/06-validators-linters-and-formatters.md`: W3C Nu HTML Checker, `html-validate`, Prettier whitespace sensitivity, `axe-core` accessibility automation.

### 2026-11-26 - Module 01: Fundamentals & Core Syntax Completed
- Authored `01-fundamentals/README.md`: Module architecture, core concepts, lesson directory.
- Authored `01-fundamentals/01-document-structure.md`: DOCTYPE, html, head, body, lang (BCP 47), dir (LTR/RTL/auto), Standards Mode vs Quirks Mode.
- Authored `01-fundamentals/02-syntax-rules-and-error-recovery.md`: Normal vs void elements, tag casing, boolean attributes, raw text parsing, Adoption Agency Algorithm (AAA), foster parenting.
- Authored `01-fundamentals/03-text-content-and-typography.md`: Headings (`h1`-`h6`), paragraphs, `<hr>`, `<br>`, `<wbr>`, `<hgroup>`, `<address>`, whitespace collapsing.
- Authored `01-fundamentals/04-lists-and-grouping.md`: `<ul>`, `<ol>`, `<li>`, `<dl>`, `<dt>`, `<dd>`, `<figure>`, `<figcaption>`, `<blockquote>`, `<pre>`, WebKit list-style accessibility bug.
- Authored `01-fundamentals/05-links-and-urls.md`: `<a>`, absolute/relative URLs, fragments, `mailto:`, `tel:`, `download`, `target="_blank"`, reverse tabnabbing, `rel="noopener noreferrer"`.
- Authored `01-fundamentals/06-images-and-alt-text.md`: `<img>`, W3C Alt Text Decision tree, decorative vs informative, zero-CLS intrinsic dimensions (`width`/`height`), `loading="lazy"`, `decoding="async"`, `fetchpriority="high"`.
- Authored `01-fundamentals/07-character-encoding-and-entities.md`: UTF-8, 1024-byte sniffing rule, Unicode code points, named/numeric entities, ambiguous ampersand parsing algorithm.
- Authored `01-fundamentals/08-comments-and-whitespace-handling.md`: `<!-- -->` syntax, DOM `Comment` node, non-nestable comment state machine, whitespace collapsing algorithm, `<pre>`, CSS `white-space` properties.
- Authored `01-fundamentals/09-global-attributes.md`: Universal global attributes (`id`, `class`, `style`, `title`, `lang`, `dir`, `translate`, `tabindex`, `hidden`, `hidden="until-found"`, `inert`, `popover`, `contenteditable`, `data-*`). All 9 concept notes written strictly to 450-750 lines and 13 sections.

### 2026-11-26 - Module 02: Semantics & Document Structure Completed
- Authored `02-semantics-and-structure/README.md`: Module overview, learning outcomes, curriculum roadmap.
- Authored `02-semantics-and-structure/01-semantic-html-principles.md`: Semantics vs presentation, AOM dual-tree architecture, implicit ARIA roles, First Rule of ARIA, div soup anti-pattern.
- Authored `02-semantics-and-structure/02-landmarks-and-page-regions.md`: `<header>`, `<nav>`, `<main>`, `<article>`, `<aside>`, `<footer>`, `<search>` (Widely Available Oct 2023), landmark navigation hotkeys, landmark labelling rules.
- Authored `02-semantics-and-structure/03-headings-and-document-outline.md`: Document outline algorithm history and abandonment, `<h1>` to `<h6>` hierarchy, modernized `<hgroup>` with subtitle `<p>`, screen reader headings rotor.
- Authored `02-semantics-and-structure/04-text-level-semantics.md`: Inline phrasing semantics (`<em>` vs `<i>`, `<strong>` vs `<b>`, `<time datetime="ISO-8601">`, `<kbd>`, `<code>`, `<samp>`, `<var>`, `<del>`, `<ins>`, `<s>`, `<mark>`, `<bdi>`).
- Authored `02-semantics-and-structure/05-details-summary-and-accordions.md`: `<details>`, `<summary>`, `open` attribute, `toggle` event, exclusive name-grouped accordions (`<details name="...">` - Newly Available Sep 2024), `::details-content`.
- Authored `02-semantics-and-structure/06-accessible-data-tables.md`: `<table>`, `<caption>`, `<thead>`, `<tbody>`, `<tfoot>`, `<th scope="col|row|colgroup">`, multi-tier complex tables with `id` and `headers`, responsive focusable scroll region pattern.
- Authored `02-semantics-and-structure/07-choosing-the-right-element-guide.md`: Architectural element selection framework, 4-step decision algorithm, `<article>` vs `<section>` vs `<div>`, `<button>` vs `<a>`, `<dl>` vs `<table>`, clickable card patterns. All 7 notes authored to 660-830 lines and 13 sections.

### 2026-11-26 - Module 03: Forms, Input Modalities & Validation Completed
- Authored `03-forms/README.md`: Module overview, learning path, directory listing.
- Authored `03-forms/01-form-basics-and-methods.md`: `<form>`, `action`, `method` (GET/POST/dialog), `enctype` (urlencoded/multipart/text), `novalidate`, `target`, zero-JS form flows.
- Authored `03-forms/02-input-types-and-mobile-keyboards.md`: 22 `<input>` states, `inputmode` values, virtual mobile keyboard tailoring, `enterkeyhint`, `type="number"` vs `inputmode="numeric"`.
- Authored `03-forms/03-labels-fieldsets-and-legends.md`: Form accessibility foundation, explicit vs implicit `<label>`, `<fieldset>`, `<legend>`, grouping radio buttons/checkboxes, label click propagation, multi-label anti-patterns.
- Authored `03-forms/04-select-datalist-and-optgroup.md`: Native dropdowns, `<select>`, `<option>`, `<optgroup>`, zero-JS autocomplete via `<datalist>`, styling limits, customizable `<select>` (OpenUI / Baseline preview).
- Authored `03-forms/05-textarea-and-button-types.md`: Multi-line text inputs, whitespace handling, button types (`submit`, `reset`, `button`), submitter tracking, image submit buttons.
- Authored `03-forms/06-output-progress-and-meter.md`: Native calculation and status elements, `<output for="...">`, `<progress max value>`, `<meter min max low high optimum>`, accessibility roles and live-region announcements.
- Authored `03-forms/07-built-in-constraint-validation.md`: Native HTML5 validation attributes (`required`, `pattern`, `minlength`, `maxlength`, `min`, `max`, `step`), CSS pseudo-classes (`:valid`, `:invalid`, `:required`, `:optional`, `:in-range`, `:out-of-range`, `:user-valid`, `:user-invalid` Baseline 2023).
- Authored `03-forms/08-constraint-validation-api.md`: Programmatic JavaScript validation API, `ValidityState` flags (9 boolean checks), `checkValidity()`, `reportValidity()`, `setCustomValidity()`, `invalid` event interception.
- Authored `03-forms/09-autocomplete-tokens-and-autofill.md`: WHATWG autofill spec (50+ tokens), sectioning, credential autofill (`current-password`, `new-password`, `one-time-code`, `webauthn`), credit card tokens, autofill phishing defense.
- Authored `03-forms/10-file-inputs-and-drag-upload.md`: `<input type="file">`, `accept` MIME/extension filtering, `multiple`, mobile `capture` (front/rear camera), accessible drag-and-drop dropzone, `FileList`, `Blob`, `URL.createObjectURL` lifecycle.
- Authored `03-forms/11-form-submission-and-formdata.md`: Form submission lifecycle, `form.requestSubmit()` vs `form.submit()`, button overrides (`formaction`, `formmethod`, `formenctype`, `formnovalidate`, `formtarget`), `FormData` API, multipart boundary handling.
- Authored `03-forms/12-search-and-filter-forms.md`: Semantic `<search>` landmark (Baseline 2023), `<input type="search">`, native clear button, GET query string serialization, debounced live search, `aria-live` announcements, URL state sync.
- Authored `03-forms/13-forms-accessibility-and-error-states.md`: WCAG 2.2 AA forms compliance, `aria-describedby` error associations, `aria-invalid="true"`, focus management on failed submit, GDS Error Summary pattern, `role="alert"`.
- Authored `03-forms/14-forms-security-and-csrf.md`: Anti-CSRF Synchronizer Token Pattern, SameSite cookies (`Strict`/`Lax`), CSP `form-action` restriction, frame-ancestors clickjacking defense, autofill security.
- Authored `03-forms/15-dialog-and-popover-forms.md`: Modal and non-modal forms, `<dialog>` + `<form method="dialog">`, `returnValue`, Popover API (`popover="auto"`, `popovertarget`), top-layer rendering, native focus trapping and restoration. All 15 concept notes written to 670-850 lines adhering to the mandatory 13-section specification.

### 2026-11-26 - Module 04: Media, Responsive Images & Embeds Completed
- Authored `04-media-and-embeds/README.md`: Module overview, learning outcomes, curriculum roadmap.
- Authored `04-media-and-embeds/01-audio-and-video.md`: `<video>`, `<audio>`, media attributes (`controls`, `preload`, `poster`, `playsinline`), multi-codec negotiation via `<source>` (AV1, VP9, H.264), autoplay policies, accessible custom controls via `HTMLMediaElement` API.
- Authored `04-media-and-embeds/02-responsive-images-in-depth.md`: Resolution switching, Device Pixel Ratio (DPR `1x`/`2x`), width descriptors (`w`), `sizes` calculations with breakpoints and `calc()`, Container Query units in `sizes` (`cqw` Baseline 2024), Preload Scanner mechanics, zero-CLS intrinsic dimensions.
- Authored `04-media-and-embeds/03-picture-and-art-direction.md`: The `<picture>` element, mandatory inner `<img>`, art direction via `<source media>`, progressive codec enhancement (`type="image/avif"` &rarr; `type="image/webp"` &rarr; JPEG), Dark Mode image switching via `(prefers-color-scheme)`.
- Authored `04-media-and-embeds/04-iframes-and-sandboxing.md`: Embedded browsing contexts, `sandbox` token matrix (`allow-scripts`, `allow-same-origin`, fatal vulnerability combination), Permissions Policy (`allow`), `srcdoc` in-memory rendering, secure cross-origin messaging via `postMessage`.
- Authored `04-media-and-embeds/05-embedded-svg.md`: Scalable Vector Graphics in HTML, 5 embedding methods, `viewBox` coordinate geometry, `currentColor` styling, SVG Sprites (`<symbol>` and `<use>`), accessible icons (`role="img"`, `<title>`, `aria-labelledby`, `aria-hidden="true"`), SVG XSS defenses.
- Authored `04-media-and-embeds/06-canvas-vs-svg.md`: Retained-mode DOM graphics (SVG) vs Immediate-mode bitmap buffers (`<canvas>`), Retina HiDPR scaling math, accessible fallback DOM subtrees inside `<canvas>`, memory scalability comparison, hybrid architectures.
- Authored `04-media-and-embeds/07-webvtt-and-subtitles.md`: The `<track>` element, WebVTT specification, Captions (dialogue + non-speech audio cues) vs Subtitles, CSS `::cue` pseudo-element, voice spans (`<v Speaker>`), CORS requirements on CDN tracks, chapters navigation.
- Authored `04-media-and-embeds/08-object-and-embed.md`: External resource plugin history, deprecation of NPAPI/Flash, modern PDF document embedding, rich fallback content models, Adobe PDF Open Parameters, CSP `object-src 'none'` hardening.
- Authored `04-media-and-embeds/09-media-accessibility.md`: WCAG 2.2 AA multimedia compliance (1.2.1 through 1.2.5, 1.4.2, 2.2.2, 2.3.1), audio descriptions, interactive synchronized transcripts, seizure safety, mandatory user pause controls on ambient video.
- Authored `04-media-and-embeds/10-image-performance-and-formats.md`: Next-gen codecs (AVIF, WebP, PNG, SVG), Core Web Vitals optimization (LCP & CLS), `fetchpriority="high"`, `loading="lazy"`, `decoding="async"`, Client Hints responsive delivery.
- Authored `04-media-and-embeds/11-modern-media-apis.md`: Picture-in-Picture API (`video.requestPictureInPicture()`), Document Picture-in-Picture API, Media Session API (lock screen & Bluetooth hardware controls), Fullscreen API (`:fullscreen`), Screen Capture API. All 11 concept notes written to 600-850 lines strictly adhering to the 13-section format.

### 2026-11-26 - Module 05: Head Metadata, Social Cards & SEO Completed
- Authored `05-head-metadata-and-seo/README.md`: Module overview, learning path, directory listing.
- Authored `05-head-metadata-and-seo/01-head-anatomy.md`: The `<head>` machine-readable container, parser hoisting rules, optimal order of head elements for maximum performance and security.
- Authored `05-head-metadata-and-seo/02-meta-charset-viewport-and-color-scheme.md`: Essential foundational meta tags, 1024-byte charset sniffing rule, mobile viewport responsive mechanics, dual dark/light color schemes (`meta[name="color-scheme"]`).
- Authored `05-head-metadata-and-seo/03-title-description-and-canonical.md`: Core search metadata, `<title>` truncation mechanics, `<meta name="description">` SERP snippets, `<link rel="canonical">` duplicate-content deduplication.
- Authored `05-head-metadata-and-seo/04-open-graph-and-social-cards.md`: Open Graph Protocol (`og:*`), Twitter Cards (`twitter:*`), absolute URL mandates, rich image preview constraints (1200x630, 1.91:1 ratio).
- Authored `05-head-metadata-and-seo/05-link-rel-values-and-resource-hints.md`: Modern `<link>` ecosystem, DNS prefetch (`dns-prefetch`), TLS preconnect (`preconnect`), asset preload (`preload`), next-page prefetch (`prefetch`), font CORS requirement (`crossorigin="anonymous"`).
- Authored `05-head-metadata-and-seo/06-script-loading-strategies.md`: JavaScript loading execution model, parser-blocking `<script>`, non-blocking parallel `defer`, out-of-order execution `async`, native ES Modules (`type="module"`), declarative Import Maps (`<script type="importmap">`).
- Authored `05-head-metadata-and-seo/07-structured-data-json-ld.md`: Semantic search machine comprehension, JSON-LD vs Microdata vs RDFa, Schema.org entities (`Article`, `Product`, `FAQPage`, `BreadcrumbList`), Google Rich Results validation.
- Authored `05-head-metadata-and-seo/08-robots-sitemaps-and-hreflang.md`: Search engine crawling and indexing directives, `<meta name="robots" content="...">`, `robots.txt` Disallow vs `noindex` trap, XML sitemaps discovery, international multilingual targeting with `<link rel="alternate" hreflang="...">`.
- Authored `05-head-metadata-and-seo/09-pwa-manifest.md`: Progressive Web Apps installation manifests (`manifest.webmanifest`), `standalone` display mode, maskable icons, mobile theme colors, iOS Web App legacy fallbacks.
- Authored `05-head-metadata-and-seo/10-content-security-policy-meta.md`: Defense-in-depth XSS mitigation, `<meta http-equiv="Content-Security-Policy">`, directive inheritance, meta limitations (`frame-ancestors` and `report-uri` unsupported in meta), cryptographic nonces and hashes.
- Authored `05-head-metadata-and-seo/11-performance-hints-and-priorities.md`: Modern browser loading hints, `fetchpriority` attribute (`high`, `low`, `auto`), Speculation Rules API (`<script type="speculationrules">` - Baseline 2024), 0ms instant prerendering, Client Hints. All 11 concept notes authored to 620-800 lines adhering to the mandatory 13-section specification.

### 2026-11-26 - Module 06: Accessibility Architecture (a11y) & WCAG 2.2 Completed
- Authored `06-accessibility/README.md`: Module overview, learning path, directory listing.
- Authored `06-accessibility/01-accessibility-fundamentals-and-wcag.md`: Digital accessibility foundations, POUR principles, WCAG 2.2 Level AA new criteria (2.4.11, 2.5.7, 2.5.8, 3.2.6, 3.3.7, 3.3.8), AOM dual-tree browser engine architecture, OS accessibility API bridges.
- Authored `06-accessibility/02-semantic-first-and-the-aria-rules.md`: Semantic-first markup architecture, the 5 Rules of ARIA, implicit role mappings, redundant role anti-patterns, ARIA state vs behavior decoupling.
- Authored `06-accessibility/03-aria-roles-states-and-properties.md`: ARIA taxonomy (6 role categories), states vs properties, `aria-expanded`, `aria-checked`, `aria-selected`, `aria-current`, `aria-controls`, `aria-busy`, roving tabindex pattern.
- Authored `06-accessibility/04-accessible-names-and-the-a11y-tree.md`: Accessible Name & Description Computation (AccName 1.2), 5-step cascade resolution order, `aria-labelledby` hidden element extraction, WCAG 2.5.3 (Label in Name), DevTools AOM inspection.
- Authored `06-accessibility/05-keyboard-navigation-and-focus.md`: Keyboard accessibility (WCAG 2.1.1 & 2.1.2), `tabindex` trinity (0, -1, positive anti-pattern), `:focus-visible` styling with >= 3:1 contrast, modal focus traps, focus restoration, `scroll-padding-top` for WCAG 2.4.11.
- Authored `06-accessibility/06-skip-links-and-landmarks.md`: WCAG 2.4.1 Bypass Blocks, skip-to-content implementation, HTML5 landmarks (`<header>`, `<nav>`, `<main>`, `<aside>`, `<footer>`, `<search>`), landmark disambiguation labeling rules.
- Authored `06-accessibility/07-alt-text-decision-tree.md`: WCAG 1.1.1 Non-text Content, W3C Alt Text Decision Tree, null alt (`alt=""`), functional image buttons, complex data visualization alternatives (`<figure>` + `<table>`).
- Authored `06-accessibility/08-accessible-forms-and-error-announcement.md`: Accessible form controls, explicit `<label for="id">`, `<fieldset>` + `<legend>` grouping, `aria-invalid="true"`, `aria-describedby` error associations, GDS Error Summary pattern, WCAG 3.3.7 & 3.3.8 compliance.
- Authored `06-accessibility/09-accessible-tables-and-lists.md`: Data tables vs layout tables (`role="presentation"`), `<caption>`, `<th scope="col|row">`, complex multi-tier tables (`id` and `headers`), Safari WebKit `list-style: none` bug workaround (`role="list"`), focusable overflow regions.
- Authored `06-accessibility/10-accessible-dialogs-popovers-and-menus.md`: Overlay architecture, `<dialog>` modal semantics via `showModal()`, Popover API (`popover="auto"`), Top Layer rendering, disclosure widgets, why `role="menu"` is forbidden for website navigation.
- Authored `06-accessibility/11-live-regions-and-status-messages.md`: Asynchronous updates (WCAG 4.1.3), `aria-live="polite|assertive"`, `aria-atomic="true"`, `role="status"`, `role="alert"`, the pre-rendered container mandate, toast queues, live search counters.
- Authored `06-accessibility/12-accessible-multimedia-and-captions.md`: WCAG 1.2.x Time-based media guidelines, WebVTT caption tracks (`<track kind="captions">`), Captions vs Subtitles, CORS crossorigin requirement on CDN tracks, audio descriptions, interactive synchronized transcripts, seizure safety (2.3.1).
- Authored `06-accessibility/13-contrast-motion-and-user-preferences.md`: WCAG color contrast thresholds (4.5:1 text, 3:1 large text and UI components), relative luminance math, `@media (prefers-reduced-motion)` vestibular safety, `@media (forced-colors: active)` Windows High Contrast Mode adaptation.
- Authored `06-accessibility/14-testing-with-axe-and-screen-readers.md`: Dual-pillar testing strategy, automated testing limits (the 30-40% rule), axe-core CI integration via Playwright, NVDA and JAWS browse vs focus modes, VoiceOver Rotor navigation, "Unplug the Mouse" testing protocol.
- Authored `06-accessibility/15-accessibility-audit-checklist.md`: Comprehensive WCAG 2.2 Level AA audit matrix (all 55 criteria), 4-tier defect triage hierarchy (P0 Blocker to P3 Low), official VPAT 2.5 / ACR authoring, reflow and target size verification. All 15 concept notes written strictly to 550-710 lines and 13 mandatory sections.
