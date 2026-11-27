# Module 02: Semantics & Document Structure

Welcome to Module 02 of the HTML Learning Track. This module establishes the structural, rhetorical, and architectural backbone of modern web pages. You will learn how to build accessible, machine-readable documents using semantic landmarks, text-level semantics, accessible data tables, and native accordions.

---

## Module Overview

HTML is not a visual canvas—it is a semantic hypermedia language. Assistive technologies (screen readers, braille displays) and search engine crawlers rely on the HTML document structure to derive meaning, outline hierarchy, and user navigable landmarks. 

In this module, you will master:
1. **The Principles of Semantic Markup**: Why meaning matters over visual presentation, avoiding the "div soup" anti-pattern, and how semantics map to the Accessibility Object Model (AOM).
2. **Page Landmarks and Regions**: Organizing pages using `<header>`, `<nav>`, `<main>`, `<article>`, `<aside>`, `<footer>`, and the newly standardized `<search>` landmark.
3. **Headings and Outline Architecture**: Crafting accessible heading outlines, using `<hgroup>` for subtitles, and understanding why the HTML5 outline algorithm was abandoned.
4. **Text-Level Phrasing Semantics**: Distinguishing semantic elements (`<strong>`, `<em>`, `<time>`, `<code>`, `<kbd>`, `<mark>`, `<abbr>`) from visual tags.
5. **Native Interactive Disclosures**: Mastering `<details>`, `<summary>`, and exclusive name-grouped accordions (`<details name="...">` - 2024 Baseline).
6. **Accessible Tabular Data**: Constructing high-density, accessible data tables with `<caption>`, `<thead>`, `<tbody>`, `<th>` scoping (`scope="col"`, `scope="row"`), and complex multi-header associations.
7. **The Element Selection Decision Matrix**: A complete architectural decision tree for choosing the exact right element for any layout or UI pattern.

---

## Lessons in this Module

| # | Lesson | Description | Core Concepts |
| :--- | :--- | :--- | :--- |
| **01** | [`01-semantic-html-principles.md`](01-semantic-html-principles.md) | The Philosophy of Semantic Hypertext | Semantics vs presentation, AOM mapping, div soup anti-pattern |
| **02** | [`02-landmarks-and-page-regions.md`](02-landmarks-and-page-regions.md) | ARIA Landmarks & Structural Regions | `<header>`, `<nav>`, `<main>`, `<article>`, `<aside>`, `<footer>`, `<search>` |
| **03** | [`03-headings-and-document-outline.md`](03-headings-and-document-outline.md) | Document Outline & Heading Hierarchy | Heading levels, `<hgroup>`, subtitles, assistive technology navigation |
| **04** | [`04-text-level-semantics.md`](04-text-level-semantics.md) | Fine-Grained Inline Phrasing Semantics | `<em>`, `<strong>`, `<time>`, `<code>`, `<kbd>`, `<mark>`, `<abbr>`, `<dfn>` |
| **05** | [`05-details-summary-and-accordions.md`](05-details-summary-and-accordions.md) | Native Disclosures & Grouped Accordions | `<details>`, `<summary>`, `name` grouping (2024), `::details-content` |
| **06** | [`06-accessible-data-tables.md`](06-accessible-data-tables.md) | Accessible Tabular Data Architecture | `<table>`, `<caption>`, `<thead>`, `<th> scope`, complex header IDs |
| **07** | [`07-choosing-the-right-element-guide.md`](07-choosing-the-right-element-guide.md) | Element Selection Decision Framework | Architectural flowcharts, `<article>` vs `<section>` vs `<div>` |

---

## Verification & Prerequisites

Before beginning this module, ensure you are comfortable with the core syntax, void elements, and global attributes covered in [Module 01: Fundamentals](../01-fundamentals/README.md).

All examples in this module comply with the WHATWG HTML Living Standard and WCAG 2.2 AA accessibility requirements, verified via the W3C Nu HTML Checker and Axe-core v4.8.
