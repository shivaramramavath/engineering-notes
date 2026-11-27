# Module 15: CSS Technical Interview Questions & Answers

| Topic | Module | Level | Baseline Status | Browsers Tested | Date | Prerequisites |
|---|---|---|---|---|---|---|
| Technical Interview Prep | 15: Interview Questions | Junior to Staff Level | Modern Web Baseline (2024–2026) | Chrome 120+, Firefox 120+, Safari 17.4+ | 2026-03-31 | Modules 00–14 |

---

## Overview

Welcome to the **CSS Technical Interview Master Guide**. This module provides an exhaustive, production-grade compilation of real-world frontend, full-stack, and systems design interview questions asked at top-tier technology companies (Google, Meta, Apple, Amazon, Netflix, Stripe, Vercel).

Rather than superficial trivia or outdated hacks (such as float clearing or vendor prefix memorization), these guides evaluate deep browser engine mechanics, mathematical cascade resolution, layout algorithms, Core Web Vitals optimization, accessibility laws, and architectural decision-making.

---

## Interview Guide Directory

| Guide | Topic Area | Target Seniority | Key Subjects Covered |
|---|---|---|---|
| **[01-css-basics-qa](./01-css-basics-qa.md)** | Core CSS Syntax & Web Platform | Junior / Mid | CRP, CSSOM, Linking methods, At-rules, Pseudo-classes vs Pseudo-elements |
| **[02-cascade-specificity-and-inheritance-qa](./02-cascade-specificity-and-inheritance-qa.md)** | Cascade, Specificity & Layers | Mid / Senior | Specificity tuple `[A, B, C]`, Cascade origin hierarchy, `@layer`, `revert-layer` |
| **[03-box-model-and-positioning-qa](./03-box-model-and-positioning-qa.md)** | Box Model & Stacking Contexts | Mid / Senior | Content vs Border-box, Margin collapse, Stacking contexts, Sticky positioning |
| **[04-flexbox-and-grid-qa](./04-flexbox-and-grid-qa.md)** | Modern Layout Algorithms | Mid / Senior | 1D vs 2D layout, `fr` unit, Subgrid, Auto-placement algorithm, Min-content blowouts |
| **[05-responsive-design-qa](./05-responsive-design-qa.md)** | Responsive & Adaptive Systems | Mid / Senior | Fluid clamp equations, Container Queries vs Media Queries, Dynamic viewport units |
| **[06-animations-and-performance-qa](./06-animations-and-performance-qa.md)** | Motion, Rendering & Performance | Senior / Lead | Compositor thread, Layout thrashing, FLIP technique, `@starting-style`, INP/CLS |
| **[07-modern-css-qa](./07-modern-css-qa.md)** | Modern CSS & Platform Baseline | Senior / Staff | CSS `:has()`, Anchor Positioning, Popover API, OKLCH, Relative Color Syntax |
| **[08-output-based-and-debugging-questions](./08-output-based-and-debugging-questions.md)** | Output Prediction & Debugging | Senior / Staff | "What does this render?", Specificity collisions, Tracing invisible layout bugs |
| **[09-practical-layout-challenges](./09-practical-layout-challenges.md)** | Live Coding Layout Scenarios | All Levels | Centering without data loss, Sticky footer, Holy grail, Freeze-pane tables |
| **[10-behavioural-and-project-discussion](./10-behavioural-and-project-discussion.md)** | System Design & Architecture | Senior / Staff | Design system migrations, Tailwind vs CSS Modules, CSS governance at scale |

---

## How to Prepare

1. **Internalize Browser Engine Mechanics**: Understand *why* properties behave the way they do (e.g. why `transform` doesn't trigger layout recalculation, or how `min-width: auto` affects grid tracks).
2. **Review Code Examples with `<details>`**: Attempt to answer each question or solve the debugging scenario independently before expanding the verified solution.
3. **Connect Theory to Business Impact**: Relate CSS architectural decisions directly to Core Web Vitals (LCP, INP, CLS), accessibility compliance (WCAG 2.2 AA), and bundle size delivery.
