# Module 01: Fundamentals & Core Syntax

## Overview

HTML is the structural foundation of the web. While modern frameworks and tools abstract component creation, browser engines still consume, parse, and execute raw HTML markup.

This module provides a rigorous, deep dive into the fundamental syntax, rules, and building blocks of the **HTML Living Standard**:
- The exact syntax rules governing elements, tags, attributes, and void elements.
- How the WHATWG parser operates under error recovery to normalize malformed markup.
- Core document anatomy: `<!DOCTYPE html>`, `<html>`, `<head>`, `<body>`, and language/direction declarations.
- Text content modeling, heading hierarchies, whitespace handling, and character entity encoding.
- Structuring lists (ordered, unordered, description) and links (absolute, relative, fragments, security attributes).
- Accessible image presentation: `alt` text strategy, intrinsic aspect-ratio sizing, and modern formats.
- Complete reference to **Global Attributes**: `id`, `class`, `inert`, `hidden`, `popover`, `tabindex`, `data-*`, `contenteditable`, and more.

---

## Module Contents

| Number | Note | Level | Time | Status | Core Topics Covered |
|:---|:---|:---:|:---:|:---:|:---|
| **01** | [Document Structure](01-document-structure.md) | Beginner | 30 min | Complete | DOCTYPE, html, head, body, lang, dir (LTR/RTL), title, viewport, encoding. |
| **02** | [Syntax Rules & Error Recovery](02-syntax-rules-and-error-recovery.md) | Intermediate | 35 min | Complete | Normal vs void elements, tag casing, attribute quoting, omitted tags, foster parenting. |
| **03** | [Text Content & Typography](03-text-content-and-typography.md) | Beginner | 30 min | Complete | Headings (h1-h6), paragraphs, line breaks (`<br>`), word breaks (`<wbr>`), thematic breaks (`<hr>`). |
| **04** | [Lists & Grouping](04-lists-and-grouping.md) | Beginner | 25 min | Complete | Ordered (`<ol>`), unordered (`<ul>`), description lists (`<dl>`, `<dt>`, `<dd>`), nesting. |
| **05** | [Links & URLs](05-links-and-urls.md) | Beginner | 35 min | Complete | Anchor tag (`<a>`), relative vs absolute URLs, fragments, `mailto:`, `tel:`, `download`, `target="_blank"`, `rel="noopener noreferrer"`. |
| **06** | [Images & Alt Text](06-images-and-alt-text.md) | Intermediate | 40 min | Complete | `<img>` tag, alt decision tree, decorative vs informative, intrinsic width/height, aspect ratios, `loading="lazy"`. |
| **07** | [Character Encoding & Entities](07-character-encoding-and-entities.md) | Intermediate | 25 min | Complete | UTF-8 byte sequences, named character references, numeric entities, ambiguous ampersands. |
| **08** | [Comments & Whitespace Handling](08-comments-and-whitespace-handling.md) | Beginner | 20 min | Complete | `<!-- -->` syntax, whitespace collapsing algorithm, preformatted text (`<pre>`), conditional comments history. |
| **09** | [Global Attributes](09-global-attributes.md) | Advanced | 45 min | Complete | `id`, `class`, `data-*`, `hidden`, `inert`, `popover`, `tabindex`, `title`, `lang`, `dir`, `translate`, `contenteditable`, `draggable`, `style`. |

---

## Learning Outcomes

By the end of this module, you will be able to:
1. Construct standards-compliant, error-free HTML5 documents that pass strict W3C validation.
2. Predict how the browser's parser handles omitted tags, missing attributes, and nesting edge cases.
3. Choose the exact right text, list, link, and image elements for any UI component without defaulting to `<div>` soup.
4. Effectively use modern global attributes like `inert` and `popover` to manage focus and top-layer UI states natively.

---

## Next Steps

Once comfortable with core syntax, proceed to [Module 02: Semantics & Document Structure](../02-semantics-and-structure/README.md) to master semantic landmarks, document outlines, and accessible tables.
