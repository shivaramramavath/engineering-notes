# Motion (formerly Framer Motion) — Basic to Advanced

A comprehensive reference card for `motion` (formerly Framer Motion v11+) in React 19.

> **Target Version:** `motion` v11.15+ (imported from `motion/react`)  
> **Philosophy:** Declarative physics-based animations, spring models, shared layout morphs  
> **Package Transition:** Note that `@framer-motion` transitioned to `motion` (`import { motion } from 'motion/react'`)  

---

## Level 0 — Concepts

- **Declarative Motion Components**: Replace standard HTML tags with `motion.<tag>` (e.g. `<motion.div>`, `<motion.button>`).
- **Spring Physics**: Instead of rigid duration curves, Motion uses real-world physical simulations (`stiffness`, `damping`, `mass`) that can be interrupted naturally by user gestures.
- **FLIP Layout Morphs (`layoutId`)**: Automatically calculates bounding box transformations between unmounting and mounting elements, morphing them smoothly across disparate component trees.
- **Core Primitives Table**:

| API | Type | Purpose |
| :--- | :--- | :--- |
| `<motion.div>` | Component | Base animated element supporting `animate`, `initial`, `exit` |
| `<AnimatePresence>` | Component | Enables components to animate their unmounting exit phase |
| `layout` / `layoutId` | Prop | Enables auto-layout reflow animation and shared element morphs |
| `useScroll()` | Hook | Reads scroll progress coordinates (`scrollYProgress`) |
| `useMotionValue()` | Hook | High-performance mutable value driving GPU updates without re-renders |
| `useTransform()` | Hook | Maps one motion value to another range (e.g. scroll -> opacity) |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install motion
```

### 1.2 Basic Animation & Gestures
```tsx
import React from 'react';
import { motion } from 'motion/react';

export function AnimatedCard() {
  return (
    <motion.div
      initial={{ opacity: 0, y: 20 }}
      animate={{ opacity: 1, y: 0 }}
      transition={{ duration: 0.4, ease: 'easeOut' }}
      whileHover={{ scale: 1.05 }}
      whileTap={{ scale: 0.95 }}
      className="p-6 bg-white border rounded-xl shadow-md max-w-sm cursor-pointer"
    >
      <h3 className="font-bold text-lg">Interactive Motion Card</h3>
      <p className="text-gray-600 text-sm mt-1">Hover and tap to see spring physics.</p>
    </motion.div>
  );
}
```

---

## Level 2 — Intermediate: Exit Animations with `<AnimatePresence>`

```tsx
import React, { useState } from 'react';
import { motion, AnimatePresence } from 'motion/react';

export function DismissibleAlert() {
  const [visible, setVisible] = useState(true);

  return (
    <div>
      <button onClick={() => setVisible(!visible)}>Toggle Alert</button>
      <AnimatePresence>
        {visible && (
          <motion.div
            initial={{ opacity: 0, height: 0 }}
            animate={{ opacity: 1, height: 'auto' }}
            exit={{ opacity: 0, height: 0 }}
            transition={{ duration: 0.3 }}
            className="p-4 bg-green-100 text-green-800 rounded border border-green-300 overflow-hidden my-2"
          >
            Operation completed successfully!
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
}
```

---

## Level 3 — Advanced: Shared Layout Morphing with `layoutId`

```tsx
import React, { useState } from 'react';
import { motion } from 'motion/react';

const TABS = ['Home', 'Analytics', 'Settings'];

export function AnimatedTabs() {
  const [activeTab, setActiveTab] = useState('Home');

  return (
    <div className="flex gap-2 p-1 bg-gray-100 rounded-lg">
      {TABS.map((tab) => (
        <button
          key={tab}
          onClick={() => setActiveTab(tab)}
          className={`relative px-4 py-2 text-sm font-semibold rounded-md transition-colors ${
            activeTab === tab ? 'text-blue-600' : 'text-gray-600'
          }`}
        >
          {activeTab === tab && (
            <motion.div
              layoutId="active-tab-pill" // Shared layout morph!
              className="absolute inset-0 bg-white rounded-md shadow-sm"
              transition={{ type: 'spring', stiffness: 350, damping: 30 }}
            />
          )}
          <span className="relative z-10">{tab}</span>
        </button>
      ))}
    </div>
  );
}
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Animating `top`, `left`, `width`, or `height`** | Forces browser layout reflows and drops frame rates on mobile devices. | Always animate hardware-accelerated transforms: `x`, `y`, `scale`. |
| **Using `AnimatePresence` without Direct Keys** | Children inside `AnimatePresence` fail to trigger exit transitions without stable keys. | Always supply a unique `key` prop to elements inside `AnimatePresence`. |
| **Ignoring `prefers-reduced-motion`** | Violates accessibility standards; causes dizziness for users with vestibular disorders. | Use `useReducedMotion()` to disable or reduce motion. |
| **Overusing `layout` on Thousands of Table Rows**| Measures bounding boxes for every row via `getBoundingClientRect()`, causing layout thrashing. | Use `layout` sparingly on primary navigation and modal transitions. |
