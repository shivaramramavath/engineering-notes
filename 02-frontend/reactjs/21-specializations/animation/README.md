# 21.1 — Animation & Gesture Systems

Animation is how a user interface communicates causality, spatial orientation, and state changes. When done well, animation makes an application feel responsive, physical, and alive; when done poorly, it creates dizziness, layout thrashing, and battery drain.

This sub-module covers animation in React from foundational CSS transitions and hardware-accelerated transforms to Motion (formerly Framer Motion), layout morphs (`layoutId`), drag gestures, and WCAG accessibility standards.

---

## What you'll be able to do after this sub-module

- **Master the Hardware Acceleration Pipeline**: Animate strictly with `transform` and `opacity` to keep animations on the GPU compositor thread without triggering layout reflows.
- **Implement Spring-Driven Physics with Motion**: Use the modern `motion/react` package to create natural, interruptible spring animations without rigid bezier curves.
- **Engineer Shared Element Transitions (`layoutId`)**: Animate cards expanding into full-screen dialogs and morphing tab indicators across unrelated component trees.
- **Handle Interactive Gestures**: Build swipeable cards, drag-and-drop handles, and pinch-to-zoom viewports with gesture sensors.
- **Enforce Vestibular Accessibility**: Honor `@media (prefers-reduced-motion: reduce)` automatically to protect users with motion sensitivities.

---

## Sub-Module Syllabus

| # | Note | Description | Key Library / Concept |
|---|------|-------------|:---:|
| 00 | [00-css-transitions-and-animations.md](./00-css-transitions-and-animations.md) | CSS transitions, keyframes, hardware acceleration, FLIP principle | Native CSS |
| 01 | [01-framer-motion.md](./01-framer-motion.md) | Motion fundamentals (`motion/react`), spring physics, variants, exit presence | Motion v11+ |
| 02 | [02-layout-animations.md](./02-layout-animations.md) | Auto-layout animation (`layout`), shared element morphing (`layoutId`) | Layout Morphing |
| 03 | [03-gestures.md](./03-gestures.md) | Drag, hover, tap, pan gestures, drag constraints, swipe-to-dismiss | Gestures |
| 04 | [04-animation-performance-and-accessibility.md](./04-animation-performance-and-accessibility.md) | `prefers-reduced-motion`, will-change, FPS profiling, battery optimization | Performance & a11y |

---

## Boundary with Other Modules

- **View Transitions**: Read [`15-concurrent-and-modern-react/08-view-transitions-api.md`](../../15-concurrent-and-modern-react/08-view-transitions-api.md) for native browser page-level cross-fades.
- **UI Drag and Drop**: Read [`09-ui-components/11-drag-and-drop-dnd-kit.md`](../../09-ui-components/11-drag-and-drop-dnd-kit.md) for accessible enterprise data drag-and-drop with `@dnd-kit`.
- **Accessibility**: Read [`08-accessibility/`](../../08-accessibility/README.md) for WCAG 2.2 animation compliance.

---
