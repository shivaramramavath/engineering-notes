# 21 — Specializations

The earlier folders cover what nearly every React app needs. **Specializations** are deeper dives into areas that only *some* products need, but that are hard to get right when you do. You don't have to read these to be a productive React developer. Read the one that matches the thing you're building.

Each specialization follows the same shape: **the mental model first** (what problem this domain really is), then the **library or API that dominates it**, then **the pitfalls that experienced people learn the hard way**.

## What's here

| Specialization | Build this when you need… | Main tools |
|---|---|---|
| [Animation](./animation/README.md) | Polished transitions, enter/exit animations, shared-element motion, gestures, scroll effects | CSS transitions and keyframes, Motion (formerly Framer Motion) |
| [React Flow](./react-flow/README.md) | Node-based editors: workflow builders, pipelines, diagrams, mind maps, visual programming | `@xyflow/react` |

(The `22-projects` folder applies both: the workflow builder project in particular uses React Flow.)

## How to decide whether to learn one

Ask:

1. **Is it core to my product, or decoration?** A workflow builder *is* React Flow. A button hover effect is two lines of CSS. Don't adopt a heavy library for decoration.
2. **Can the platform already do it?** Modern CSS handles many animations without JavaScript. Check there first.
3. **What does it cost?** Bundle size, a new mental model, accessibility obligations, and testing difficulty. Every specialization here has all four.
4. **Who maintains it?** These are ecosystems with active maintainers. Check release notes, since APIs and package names change (Framer Motion became Motion; `reactflow` became `@xyflow/react`).

## Prerequisites

- [Performance](../14-performance/README.md): both specializations are performance-sensitive, and both can wreck frame rate if used carelessly
- [Accessibility](../08-accessibility/README.md): motion and canvas-style editors both create accessibility obligations
- [Refs](../16-advanced-react/01-refs-and-imperative-handles.md) and [external stores](../16-advanced-react/02-external-stores.md): these libraries lean on both

## Conventions

- TypeScript, React 19.
- Library versions: **Motion** (`motion` package, imported from `motion/react`) and **React Flow v12** (`@xyflow/react`). Older tutorials use `framer-motion` and `reactflow` (v11) with some different names. Where it matters, the notes mention the differences.

## Next

[Animation](./animation/README.md)