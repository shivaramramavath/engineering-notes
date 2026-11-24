# date-fns v4 — Basic to Advanced

A comprehensive reference card for `date-fns` v4 in React 19.

> **Target Version:** date-fns v4.1+  
> **Philosophy:** Modular, immutable, lightweight date utilities for JavaScript and TypeScript  
> **Advantage:** 100% tree-shakable (import only what you use, unlike Moment.js)  

---

## Level 0 — Concepts

- **Immutable & Pure Functions**: All `date-fns` functions are pure: they never mutate the original JavaScript `Date` object; they always return a new `Date` instance or formatted primitive string.
- **Tree-Shaking Native**: Every function is an independent module. Importing `format` only bundles the `format` code (~3KB), rather than a massive 200KB monolithic bundle.
- **Core Primitives Table**:

| Function | Purpose |
| :--- | :--- |
| `format(date, 'yyyy-MM-dd')` | Formats a Date into a customized string format |
| `formatDistanceToNow(date)` | Returns relative time strings (e.g. "5 minutes ago") |
| `parseISO(isoString)` | Safely parses ISO 8601 strings into native Date objects |
| `isBefore()`, `isAfter()` | Precise date comparison utilities |
| `addDays()`, `subMonths()` | Pure date arithmetic |
| `differenceInDays()` | Calculates distance between two dates in specific units |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install date-fns
```

### 1.2 Formatting Dates in React Components
```tsx
import React from 'react';
import { format, formatDistanceToNow, parseISO } from 'date-fns';

export function CommentTimestamp({ createdAtIso }: { createdAtIso: string }) {
  const date = parseISO(createdAtIso);

  return (
    <div className="text-xs text-gray-500">
      {/* Relative time: "3 hours ago" */}
      <span className="font-medium">{formatDistanceToNow(date, { addSuffix: true })}</span>
      {' · '}
      {/* Absolute date: "Jan 15, 2025" */}
      <time dateTime={createdAtIso}>{format(date, 'MMM d, yyyy')}</time>
    </div>
  );
}
```

---

## Level 2 — Intermediate: Date Arithmetic & Ranges

```ts
import { addDays, isWithinInterval, startOfDay, endOfDay } from 'date-fns';

export function isTaskDueThisWeek(dueDate: Date): boolean {
  const today = new Date();
  const nextWeek = addDays(today, 7);

  return isWithinInterval(dueDate, {
    start: startOfDay(today),
    end: endOfDay(nextWeek),
  });
}
```

---

## Level 3 — Advanced: Internationalization & Locales

Load localized relative time strings (e.g. Spanish, Japanese, German):

```tsx
import { formatDistanceToNow } from 'date-fns';
import { es } from 'date-fns/locale';

export function SpanishTimestamp({ date }: { date: Date }) {
  // Renders: "hace 5 minutos"
  return <span>{formatDistanceToNow(date, { addSuffix: true, locale: es })}</span>;
}
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Using `new Date('2025-05-12')` Directly** | Browsers parse date strings inconsistently (UTC vs local midnight offsets), causing 1-day off bugs. | Use `parseISO()` to guarantee consistent parsing across all browsers. |
| **Importing Entire Library Monolith** | Bypasses tree-shaking; increases bundle size. | Use named modular imports: `import { format } from 'date-fns'`. |
| **Using Moment.js in Modern React** | Moment.js is deprecated, mutable, and cannot be tree-shaken (~300KB bundle penalty). | Standardize on `date-fns` for modern lightweight applications. |
| **Hydration Mismatch on Relative Time Strings**| Server renders "just now" at build time; client hydration renders "5 seconds ago", causing hydration warnings. | Render relative times inside `useEffect` or use absolute timestamps for SSR. |
