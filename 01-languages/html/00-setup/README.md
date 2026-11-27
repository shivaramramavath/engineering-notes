# Module 00: Setup & Browser Engine Architecture

## Overview

Before writing a single line of markup, an engineer must understand the runtime environment of the web platform: **the browser engine**. 

HTML is not merely a document format or templating syntax; it is the programmatic blueprint consumed by browser engines (Blink, Gecko, WebKit) to construct the **Document Object Model (DOM)** and the **Accessibility Object Model (AOM)**.

This foundational module establishes the theoretical and practical underpinnings of web architecture:
- How the client-server web architecture delivers Hypertext across HTTP/HTTPS.
- The deterministic stages of the browser rendering pipeline: network byte streaming, HTML tokenization, DOM tree construction, CSSOM building, Render Tree calculation, layout, painting, and GPU compositing.
- The evolution of HTML: from Tim Berners-Lee's original 18 tags through the XML/XHTML 2.0 schism to the modern **WHATWG HTML Living Standard**.
- Professional engineering toolchains: VS Code, browser DevTools (Elements panel, Accessibility inspector, Rendering tab), and automated linters/validators.

---

## Module Contents

| Number | Note | Level | Time | Status | Core Topics Covered |
|:---|:---|:---:|:---:|:---:|:---|
| **01** | [What is HTML & The Web](01-what-is-html-and-the-web.md) | Beginner | 25 min | Complete | Client-server architecture, HTTP/2 & HTTP/3, MIME types, URI/URL anatomy, role of HTML vs CSS vs JS. |
| **02** | [How Browsers Turn HTML into Pixels](02-how-browsers-turn-html-into-pixels.md) | Intermediate | 40 min | Complete | Parser state machine, tokenization, tree construction, CSSOM, Render Tree, Layout, Paint, Composite, Preload Scanner. |
| **03** | [HTML History & The Living Standard](03-html-history-and-the-living-standard.md) | Beginner | 25 min | Complete | HTML 1.0 to 4.01, XHTML failure, Ian Hickson & WHATWG formation, W3C convergence, Web Platform Baseline. |
| **04** | [Tools, Editor & DevTools](04-tools-editor-and-devtools.md) | Beginner | 30 min | Complete | VS Code extensions, Chrome/Firefox DevTools, DOM vs Page Source, Accessibility Tree inspection, Force state. |
| **05** | [Your First Page](05-your-first-page.md) | Beginner | 30 min | Complete | Full valid document, `<!DOCTYPE html>`, `html[lang]`, `meta[charset]`, viewport, local server setup, file protocols. |
| **06** | [Validators, Linters & Formatters](06-validators-linters-and-formatters.md) | Intermediate | 35 min | Complete | W3C Nu HTML Checker (v.Nu), html-validate, Prettier formatting rules, CI/CD validation pipelines. |

---

## Learning Outcomes

By the end of this module, you will be able to:
1. Explain the exact sequence of events that occurs between typing a URL into the address bar and pixels rendering on the screen.
2. Differentiate the raw HTML network stream from the in-memory parsed DOM and Accessibility Tree.
3. Use browser DevTools to debug live DOM state, inspect computed accessibility properties, and simulate slow network conditions.
4. Set up an automated linting and validation pipeline that blocks presentational elements, missing alt text, and unclosed tags in CI/CD.

---

## Next Steps

Once comfortable with browser engine internals and development tools, proceed directly to [Module 01: Fundamentals & Core Syntax](../01-fundamentals/README.md) to master document structure, syntax rules, links, and global attributes.
