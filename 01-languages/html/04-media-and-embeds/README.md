# Module 04: Media, Responsive Images, and Embedded Content

## Overview
Module 04 covers the modern web's visual and auditory capabilities: native audio and video playback, complex responsive image workflows (`srcset`, `sizes`, `<picture>`), sandbox-hardened `<iframe>` embeds, scalable vector graphics (SVG), `<canvas>` rendering, WebVTT accessibility subtitles, and next-generation media delivery APIs.

---

## Learning Objectives
By the end of this module, you will be able to:
- Construct accessible, multi-codec `<video>` and `<audio>` players with native fallback and custom control accessibility.
- Master resolution switching and art direction using `srcset`, `sizes`, and the `<picture>` element with modern image formats (AVIF, WebP).
- Isolate untrusted third-party content securely using `<iframe>` sandboxing attributes, feature permissions policies, and lazy loading.
- Choose accurately between declarative inline SVG and programmatic `<canvas>` rendering based on accessibility and performance constraints.
- Author standards-compliant WebVTT subtitle and caption tracks to achieve full WCAG 2.2 AA multimedia compliance.
- Leverage modern performance capabilities: `loading="lazy"`, `decoding="async"`, `fetchpriority="high"`, and zero Cumulative Layout Shift (CLS) layout aspect ratios.

---

## Module Notes

| # | Note | Description | Focus / Baseline |
| :--- | :--- | :--- | :--- |
| **01** | [Audio and Video](01-audio-and-video.md) | Native multimedia elements, codec negotiation, media attributes, custom media controls | Widely Available |
| **02** | [Responsive Images in Depth](02-responsive-images-in-depth.md) | Resolution switching, DPR multipliers (`2x`), `srcset` and `sizes` math, density negotiation | Widely Available |
| **03** | [Picture and Art Direction](03-picture-and-art-direction.md) | The `<picture>` element, media queries for layout cropping, format fallback (AVIF &rarr; WebP &rarr; JPEG) | Widely Available |
| **04** | [iFrames and Sandboxing](04-iframes-and-sandboxing.md) | Secure third-party embeds, `sandbox` token matrix, `allow` permissions policy, lazy loading, CSP framing | Widely Available |
| **05** | [Embedded SVG](05-embedded-svg.md) | Declarative vector graphics in HTML, inline SVG vs `<img>`, symbols, viewBox coordinate systems, SVG accessibility | Widely Available |
| **06** | [Canvas vs. SVG](06-canvas-vs-svg.md) | Retained-mode DOM graphics (SVG) vs Immediate-mode bitmap buffers (`<canvas>`), fallback accessibility trees | Widely Available |
| **07** | [WebVTT and Subtitles](07-webvtt-and-subtitles.md) | The `<track>` element, WebVTT syntax, captions vs subtitles vs audio descriptions, styling cues with `::cue` | Widely Available |
| **08** | [Object and Embed](08-object-and-embed.md) | External resource plugin legacy, `<object>`, `<embed>`, PDF embedding, deprecation of Flash/ActiveX | Legacy & Specialized |
| **09** | [Media Accessibility](09-media-accessibility.md) | WCAG 2.2 AA multimedia guidelines (1.2.1-1.2.5), transcript alternatives, sign language, audio descriptions | Widely Available |
| **10** | [Image Performance and Formats](10-image-performance-and-formats.md) | Next-gen codecs (AVIF, WebP), LCP optimization, `fetchpriority`, native lazy loading, zero-CLS aspect ratio sizing | Baseline 2024 |
| **11** | [Modern Media APIs](11-modern-media-apis.md) | Picture-in-Picture API, Media Session API, Fullscreen API, View Transitions preview | Widely Available / Newly Available |

---

## Suggested Study Order
1. Begin with **01-audio-and-video** and **02-responsive-images-in-depth** for fundamental media syntax.
2. Advance through **03-picture-and-art-direction** and **10-image-performance-and-formats** to master real-world performance engineering.
3. Study **04-iframes-and-sandboxing** for embedding third-party widgets and payment gateways securely.
4. Dive into **05-embedded-svg** and **06-canvas-vs-svg** for graphics and visualization.
5. Finish with **07-webvtt-and-subtitles**, **09-media-accessibility**, and **11-modern-media-apis** for production compliance.

---

[← Previous Module: 03 Forms](../03-forms/README.md) | [Track Index](../INDEX.md) | [Next: Audio & Video →](01-audio-and-video.md)
