# HTML Learning Roadmaps (ROADMAP.md)

This curriculum offers **five tailored learning roadmaps** suited to different engineering goals and specialization levels.

---

## 1. Roadmap 1: Web Fundamentals & Beginner Track
**Goal:** Master the core grammar of the web, build valid and semantic web pages, and understand how browser engines parse markup.  
**Estimated Time:** 15–20 Hours  
**Prerequisites:** None.

```
00-setup/01-what-is-html-and-the-web.md
   ↓
00-setup/02-how-browsers-turn-html-into-pixels.md
   ↓
00-setup/05-your-first-page.md
   ↓
01-fundamentals/01-document-structure.md
   ↓
01-fundamentals/02-syntax-rules-and-error-recovery.md
   ↓
01-fundamentals/03-text-content-and-typography.md
   ↓
01-fundamentals/04-lists-and-grouping.md
   ↓
01-fundamentals/05-links-and-urls.md
   ↓
01-fundamentals/06-images-and-alt-text.md
   ↓
01-fundamentals/09-global-attributes.md
   ↓
02-semantics-and-structure/01-semantic-html-principles.md
   ↓
02-semantics-and-structure/02-landmarks-and-page-regions.md
   ↓
03-forms/01-form-basics-and-methods.md
   ↓
03-forms/02-input-types-and-mobile-keyboards.md
   ↓
10-projects/01-personal-portfolio/
```

---

## 2. Roadmap 2: Professional Frontend Engineer Track
**Goal:** Master modern HTML APIs, component markup patterns, forms architecture, responsive images, and performance hints.  
**Estimated Time:** 30–40 Hours  
**Prerequisites:** Basic HTML syntax, basic CSS/JS awareness.

```
All of Roadmap 1
   ↓
02-semantics-and-structure/05-details-summary-and-accordions.md (Exclusive accordions)
   ↓
02-semantics-and-structure/06-accessible-data-tables.md
   ↓
03-forms/07-built-in-constraint-validation.md
   ↓
03-forms/08-constraint-validation-api.md
   ↓
03-forms/11-form-submission-and-formdata.md
   ↓
04-media-and-embeds/02-responsive-images-in-depth.md (srcset / sizes)
   ↓
04-media-and-embeds/03-picture-and-art-direction.md
   ↓
04-media-and-embeds/11-lazy-loading-and-fetchpriority.md
   ↓
05-head-metadata-and-seo/06-script-loading-strategies.md (defer, async, import maps)
   ↓
07-modern-html-and-apis/01-dialog-element-in-depth.md
   ↓
07-modern-html-and-apis/02-popover-api-and-top-layer.md
   ↓
07-modern-html-and-apis/07-declarative-shadow-dom.md
   ↓
08-performance-and-security/01-critical-rendering-path.md
   ↓
08-performance-and-security/02-preload-scanner-and-resource-discovery.md
   ↓
10-projects/03-saas-landing-page/ & 05-accessible-signup-form/
```

---

## 3. Roadmap 3: Accessibility (a11y) & WCAG Specialist Track
**Goal:** Attain expert mastery of WCAG 2.2 Level AA compliance, ARIA states and roles, Accessible Name Computation, and screen reader UX.  
**Estimated Time:** 25–30 Hours  
**Prerequisites:** Semantic HTML foundations.

```
02-semantics-and-structure/01-semantic-html-principles.md
   ↓
02-semantics-and-structure/02-landmarks-and-page-regions.md
   ↓
02-semantics-and-structure/03-headings-and-document-outline.md
   ↓
06-accessibility/01-accessibility-fundamentals-and-wcag.md
   ↓
06-accessibility/02-semantic-first-and-the-aria-rules.md
   ↓
06-accessibility/03-aria-roles-states-and-properties.md
   ↓
06-accessibility/04-accessible-names-and-the-a11y-tree.md
   ↓
06-accessibility/05-keyboard-navigation-and-focus.md
   ↓
06-accessibility/06-skip-links-and-landmarks.md
   ↓
06-accessibility/07-alt-text-decision-tree.md
   ↓
06-accessibility/08-accessible-forms-and-error-announcement.md
   ↓
06-accessibility/10-accessible-dialogs-popovers-and-menus.md
   ↓
06-accessibility/11-live-regions-and-status-messages.md
   ↓
06-accessibility/14-testing-with-axe-and-screen-readers.md
   ↓
06-accessibility/15-accessibility-audit-checklist.md
   ↓
10-projects/05-accessible-signup-form/ & 06-restaurant-menu-table/
```

---

## 4. Roadmap 4: Technical SEO & Metadata Architect Track
**Goal:** Master search engine discoverability, social cards, rich snippets, canonicalization, internationalization, and crawl optimization.  
**Estimated Time:** 20–25 Hours  
**Prerequisites:** Basic HTML structure.

```
01-fundamentals/01-document-structure.md (lang attribute & head)
   ↓
02-semantics-and-structure/02-landmarks-and-page-regions.md
   ↓
02-semantics-and-structure/03-headings-and-document-outline.md
   ↓
05-head-metadata-and-seo/01-head-anatomy.md
   ↓
05-head-metadata-and-seo/02-meta-charset-viewport-and-color-scheme.md
   ↓
05-head-metadata-and-seo/03-title-description-and-canonical.md
   ↓
05-head-metadata-and-seo/04-open-graph-and-social-cards.md
   ↓
05-head-metadata-and-seo/07-structured-data-json-ld.md (Schema.org)
   ↓
05-head-metadata-and-seo/08-robots-sitemaps-and-hreflang.md
   ↓
08-performance-and-security/01-critical-rendering-path.md (Core Web Vitals)
   ↓
09-patterns-and-architecture/05-internationalisation-and-rtl.md
   ↓
10-projects/02-accessible-blog/ & 11-multilingual-international-site/
```

---

## 5. Roadmap 5: Senior Technical Interview & Performance Track
**Goal:** Prepare for staff/senior frontend interviews: parsing edge cases, DOM/AOM internals, security vulnerabilities, and markup challenges.  
**Estimated Time:** 15–20 Hours  
**Prerequisites:** Solid frontend industry experience.

```
00-setup/02-how-browsers-turn-html-into-pixels.md
   ↓
01-fundamentals/02-syntax-rules-and-error-recovery.md (Foster parenting & recovery)
   ↓
06-accessibility/04-accessible-names-and-the-a11y-tree.md
   ↓
07-modern-html-and-apis/01-dialog-element-in-depth.md vs 02-popover-api-and-top-layer.md
   ↓
07-modern-html-and-apis/07-declarative-shadow-dom.md
   ↓
08-performance-and-security/05-xss-prevention-and-html-sanitization.md
   ↓
08-performance-and-security/06-csp-sri-and-trusted-types.md
   ↓
08-performance-and-security/07-reverse-tabnabbing-and-referrer-policy.md
   ↓
11-interview/01-html-fundamentals-qa.md through 08-practical-markup-challenges.md
   ↓
12-cheatsheets/ (All 10 Cheatsheets)
```
