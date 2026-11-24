# 21 — Specializations & Domain Architectures

The earlier modules cover what nearly every React app needs. **Specializations** are deeper architectural explorations into specialized domains that only *some* products require, but that are notoriously complex to engineer correctly from scratch. You do not need to read these to be a productive React developer; read the ones that align with the specific product domain you are building.

---

## Specialization Domains Catalog

| # | Specialization Area | Description | Industry Standard Tooling |
|---|---------------------|-------------|:---:|
| 01 | [Animation](./animation/README.md) | Motion physics, layout morphs (`layoutId`), gesture tracking | Motion (`motion/react`) |
| 02 | [Node-Based Workflow Editors](./react-flow/README.md) | Visual node editors, graph pipelines, drag-and-drop connectors | React Flow (`@xyflow/react`) |
| 05 | [Data Visualization](./05-data-visualization-recharts-visx.md) | High-level analytics dashboards vs low-level D3 SVG primitives | Recharts / Visx (Airbnb) |
| 06 | [Geospatial Mapping](./06-maps-with-leaflet-maplibre.md) | Raster tiles vs hardware-accelerated WebGL vector tiles, marker clustering | Leaflet / MapLibre GL |
| 07 | [Rich Text Editing](./07-rich-text-tiptap-lexical.md) | Headless document models, custom React node views, markdown AST | Tiptap (ProseMirror) / Lexical |
| 08 | [Progressive Web Apps (PWA)](./08-pwa-and-offline-first.md) | Service Workers, Workbox caching strategies, IndexedDB with Dexie | `vite-plugin-pwa` / Dexie.js |
| 09 | [Real-Time Collaboration](./09-realtime-collaboration-crdt-yjs.md) | Conflict-free Replicated Data Types (CRDTs), live presence awareness, multi-cursors | Yjs / `y-websocket` |
| 10 | [Generative AI Chat UIs](./10-ai-chat-uis-streaming.md) | Token streaming, streaming markdown, Generative UI tool components | Vercel AI SDK (`ai/react`) |
| 11 | [3D Graphics & WebGL](./11-canvas-three-fiber-overview.md) | Three.js scene graphs, custom reconcilers, GLTF models, instancing | React Three Fiber / Drei |

---

## Decision Framework: When to Adopt a Specialization

Before introducing these heavy domain libraries into a codebase, evaluate:

1. **Is it core business functionality or cosmetic decoration?**
   - A workflow pipeline builder *is* React Flow. A button ripple is 2 lines of CSS.
   - A financial analytics dashboard *is* Recharts. A simple progress bar is a native `<progress>` element.
2. **Can the platform already do it?**
   - Modern CSS animations and the View Transitions API handle many transitions without 40KB of JavaScript.
3. **What are the hidden costs?**
   - Bundle size impact, steep learning curves, testing complexity, and accessibility (WCAG) obligations.
4. **Maintenance & Community Health**:
   - Every library listed here is actively maintained and verified compatible with React 19.

---

## Prerequisites

- [Performance Engineering](../14-performance/README.md): All specializations are performance-critical; improper use can easily bottleneck frame rates.
- [Accessibility Guidelines](../08-accessibility/README.md): Interactive canvases, 3D scenes, and animations create significant screen-reader and keyboard accessibility obligations.
- [Custom Renderers & Internals](../16-advanced-react/04-custom-renderers-reconciler.md): React Three Fiber and Tiptap build directly on React reconciler primitives.

---
