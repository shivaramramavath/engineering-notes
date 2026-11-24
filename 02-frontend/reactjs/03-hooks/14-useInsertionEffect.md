# React useInsertionEffect: High-Performance CSS-in-JS Style Injection

- **Topic**: `03-hooks`
- **React version**: React 18+ (Standard in React 19)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Strictly intended for CSS-in-JS library authors (e.g. Emotion, styled-components). Application code should prefer native CSS modules, Tailwind CSS v4, or zero-runtime solutions.

---

## 1. Concept Overview

Dynamic CSS-in-JS libraries (such as Emotion and Styled Components) generate `<style>` tags and inject CSS rules into the document `<head>` dynamically at runtime based on component props.

In React 17 and earlier, libraries typically injected styles using `useLayoutEffect` or during the render phase:
1. **Injecting during render**: Causes side effects during the render phase, which breaks React Concurrent Mode because render phases can be aborted, restarted, or executed concurrently on background threads.
2. **Injecting inside `useLayoutEffect`**: Forces the browser to recalculate styles and re-run layout calculations (layout thrashing). When a component renders, `useLayoutEffect` measures DOM nodes (e.g. `getBoundingClientRect()`). If a sibling or child component injects a new `<style>` tag during its own `useLayoutEffect`, the browser must invalidate all previously computed layouts and recalculate the layout tree from scratch.

`useInsertionEffect` was introduced in React 18 to solve this problem. It fires **synchronously before any DOM mutations occur**. This guarantees that all dynamic `<style>` tags are inserted into the DOM before any `useLayoutEffect` reads layout measurements.

```
React Commit Pipeline Timing:
┌────────────────────────────────────────────────────────┐
│ 1. Render Phase (Pure VDOM calculation)               │
└────────────────────────────────────────────────────────┘
                           │
                           ▼
┌────────────────────────────────────────────────────────┐
│ 2. useInsertionEffect (Inject <style> rules here!)     │ ◄── Style injection
└────────────────────────────────────────────────────────┘
                           │
                           ▼
┌────────────────────────────────────────────────────────┐
│ 3. React Mutates Real DOM (Host tree insertion)        │
└────────────────────────────────────────────────────────┘
                           │
                           ▼
┌────────────────────────────────────────────────────────┐
│ 4. useLayoutEffect (Safe to read DOM measurements!)    │ ◄── Layout reads
└────────────────────────────────────────────────────────┘
                           │
                           ▼
┌────────────────────────────────────────────────────────┐
│ 5. Browser Paints Pixels to Screen                     │
└────────────────────────────────────────────────────────┘
                           │
                           ▼
┌────────────────────────────────────────────────────────┐
│ 6. useEffect (Passive asynchronous effects)            │
└────────────────────────────────────────────────────────┘
```

### When to Use
- **Library Authors Only**: Authoring runtime CSS-in-JS styling engines that must insert rules dynamically into `<style>` tags.
- Injecting critical CSS rules before layout measurement hooks run.

### When NOT to Use
- **Application Developers**: Do NOT use in application code. Use `useEffect` or `useLayoutEffect`.
- **DOM Manipulations**: Do NOT mutate DOM elements other than `<style>` or `<svg>` definitions in this hook.
- **Reading Refs**: DOM refs are not yet attached or updated when `useInsertionEffect` fires.
- **Updating State**: Scheduling React state updates inside `useInsertionEffect` is disallowed.

---

## 2. Key Mechanics & Architecture

### API Signature
```ts
useInsertionEffect(setup: () => (() => void) | void, dependencies?: any[]): void;
```

`useInsertionEffect` shares the exact same signature as `useEffect` and `useLayoutEffect`. However, its execution semantics are strictly isolated to the pre-mutation commit sub-phase.

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Execution Phase** | Pre-mutation commit phase (before DOM writes) |
| **Ref Access** | DOM refs are NOT yet attached (`ref.current === null`) |
| **State Scheduling** | Prohibited; setting state here logs warnings / throws |
| **SSR Support** | Does not execute on the server (like `useLayoutEffect`) |
| **Target Audience** | CSS-in-JS library developers |
| **Browser Reflows** | Prevents layout thrashing by ensuring styles exist before measurements |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
Injecting a dynamic CSS rule into a shared `<style>` element before DOM nodes render.

```tsx
import React, { useInsertionEffect } from 'react';

// In-memory cache for inserted CSS rule hashes
const insertedRules = new Set<string>();

function useCSS(rule: string): string {
  // Generate a deterministic classname hash
  const className = `css-${Math.abs(hashString(rule)).toString(36)}`;

  useInsertionEffect(() => {
    if (!insertedRules.has(className)) {
      insertedRules.add(className);
      const styleElement = document.createElement('style');
      styleElement.textContent = `.${className} { ${rule} }`;
      document.head.appendChild(styleElement);
    }
  }, [className, rule]);

  return className;
}

function hashString(str: string): number {
  let hash = 0;
  for (let i = 0; i < str.length; i++) {
    hash = (hash << 5) - hash + str.charCodeAt(i);
    hash |= 0;
  }
  return hash;
}

export function StyledBadge({ text, color }: { text: string; color: string }) {
  const badgeClass = useCSS(`background-color: ${color}; color: white; padding: 4px 8px; border-radius: 9999px;`);

  return <span className={badgeClass}>{text}</span>;
}
```
Output:
```html
<!-- Injected in <head> before DOM mutation -->
<style>.css-1k9x8 { background-color: #4f46e5; color: white; padding: 4px 8px; border-radius: 9999px; }</style>

<!-- Rendered element -->
<span class="css-1k9x8">Active</span>
```

### Example 2: Production / Real-World Integration
A mini scoped CSS-in-JS runtime with CSS rule deduplication and cleanup.

```tsx
import React, { useInsertionEffect, useId } from 'react';

class StyleRegistry {
  private sheet: CSSStyleSheet | null = null;
  private rules = new Set<string>();

  private getSheet(): CSSStyleSheet {
    if (!this.sheet) {
      const styleTag = document.createElement('style');
      styleTag.setAttribute('data-runtime-css', 'true');
      document.head.appendChild(styleTag);
      this.sheet = styleTag.sheet as CSSStyleSheet;
    }
    return this.sheet;
  }

  insertRule(rule: string) {
    if (this.rules.has(rule)) return;
    this.rules.add(rule);
    const sheet = this.getSheet();
    try {
      sheet.insertRule(rule, sheet.cssRules.length);
    } catch (e) {
      console.warn('Failed to insert CSS rule:', rule, e);
    }
  }
}

const registry = new StyleRegistry();

export function useDynamicThemeClass(primaryColor: string): string {
  const className = `theme-${primaryColor.replace('#', '')}`;

  useInsertionEffect(() => {
    const css = `.${className} { border-color: ${primaryColor}; color: ${primaryColor}; }`;
    registry.insertRule(css);
  }, [className, primaryColor]);

  return className;
}

export function ThemedButton({ color, label }: { color: string; label: string }) {
  const themeClass = useDynamicThemeClass(color);

  return (
    <button type="button" className={`border-2 font-bold px-4 py-2 rounded ${themeClass}`}>
      {label}
    </button>
  );
}
```
Output:
```
// Renders button with dynamically generated class, styles injected prior to layout computation.
```

### Example 3: Edge Case / Preventing Layout Thrashing in Sibling Measuring Components
Demonstrating why `useInsertionEffect` prevents layout invalidation when a sibling measures layout in `useLayoutEffect`.

```tsx
import React, { useInsertionEffect, useLayoutEffect, useRef, useState } from 'react';

// Sibling A uses useInsertionEffect to insert layout-affecting styles
function DynamicStyler() {
  useInsertionEffect(() => {
    const style = document.createElement('style');
    style.id = 'dynamic-box-style';
    style.textContent = `.measured-box { width: 300px; height: 100px; padding: 20px; }`;
    document.head.appendChild(style);
    return () => {
      style.remove();
    };
  }, []);

  return null;
}

// Sibling B measures DOM geometry in useLayoutEffect
function MeasuringBox() {
  const boxRef = useRef<HTMLDivElement>(null);
  const [width, setWidth] = useState(0);

  useLayoutEffect(() => {
    // Because DynamicStyler used useInsertionEffect, the style tag is ALREADY in the DOM!
    // The browser measures the correct 300px immediately without thrashing!
    if (boxRef.current) {
      setWidth(boxRef.current.getBoundingClientRect().width);
    }
  }, []);

  return (
    <div ref={boxRef} className="measured-box border">
      Measured Width: {width}px
    </div>
  );
}

export function ThrashProofContainer() {
  return (
    <div>
      <DynamicStyler />
      <MeasuringBox />
    </div>
  );
}
```
Output:
```
// Box displays: "Measured Width: 300px" with single layout computation pass.
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Attempting to read a DOM ref or set state inside `useInsertionEffect`.

```tsx
// ❌ WRONG: Attempting to access DOM ref or call setState inside useInsertionEffect
export function FaultyComponent() {
  const divRef = useRef<HTMLDivElement>(null);
  const [, setCount] = useState(0);

  useInsertionEffect(() => {
    // ⚠️ BUG 1: divRef.current is ALWAYS NULL here!
    // The DOM has not been created or mutated yet!
    console.log(divRef.current?.offsetWidth); // TypeError: Cannot read property

    // ⚠️ BUG 2: React logs error if state is updated here!
    // setCount(1);
  }, []);

  return <div ref={divRef}>Content</div>;
}

// ✅ REMEDIATION: Move DOM measurements and state updates to useLayoutEffect or useEffect!
export function FixedComponent() {
  const divRef = useRef<HTMLDivElement>(null);

  useLayoutEffect(() => {
    // Safe: DOM mutations have occurred, ref is bound!
    console.log('Measured width:', divRef.current?.offsetWidth);
  }, []);

  return <div ref={divRef}>Content</div>;
}
```
Output:
```
// Console logs: Measured width: 120
```

### Example 5: Idiomatic Modern React 19 Recommendation
In React 19, zero-runtime styling (Tailwind CSS v4 with `@tailwindcss/vite` or CSS Modules) replaces runtime CSS-in-JS.

```tsx
// Modern React 19 Idiom: Zero-Runtime CSS with Tailwind CSS v4
// No useInsertionEffect needed; CSS is compiled at build time by Vite!
export function ModernReact19Card({ title, subtitle }: { title: string; subtitle: string }) {
  return (
    <div className="p-6 max-w-sm mx-auto bg-white rounded-xl shadow-md flex items-center gap-x-4">
      <div>
        <div className="text-xl font-medium text-black">{title}</div>
        <p className="text-slate-500">{subtitle}</p>
      </div>
    </div>
  );
}
```
Output:
```
// Clean zero-runtime output with zero JS execution overhead for styling.
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Using `useInsertionEffect` in regular application code** | Unnecessary complexity; application code almost never needs to inject `<style>` tags directly. | Use standard `useEffect`, CSS Modules, or Tailwind CSS v4. |
| **Accessing `ref.current` inside `useInsertionEffect`** | React has not executed host mutations yet; refs are `null`. | Access DOM refs inside `useLayoutEffect` or `useEffect`. |
| **Calling `setState` inside `useInsertionEffect`** | React warns or throws because state updates during pre-mutation corrupt scheduling lanes. | Call `setState` in event handlers or `useEffect`. |
| **Injecting styles during the render phase** | Breaks React Concurrent Mode; renders can be called multiple times without committing. | Always inject runtime styles inside `useInsertionEffect`. |
| **Creating unbounded duplicate `<style>` tags** | Memory leak and stylesheet bloat in the browser DOM. | Cache injected rules in a `Set` or use a shared `CSSStyleSheet`. |
| **Using `useInsertionEffect` for animation triggers** | Animations require painted DOM nodes; styles injected here cannot animate unmounted nodes. | Use CSS transitions or Framer Motion / Web Animations API in `useEffect`. |

---

## 5. React Internals: Beneath the Abstraction

Inside `ReactFiberCommitWork.js`:
When a render completes and React enters the commit phase:
1. React executes `commitMutationEffects`:
   - It iterates through the fiber subtree and invokes all `useInsertionEffect` callbacks.
   - At this instant, the work-in-progress Fiber tree is calculated, but no DOM operations (like `appendChild` or `removeChild`) have taken place.
2. Next, React executes `commitBeforeMutationEffects` and commits the actual DOM modifications (Host mutations).
3. Next, React invokes `commitLayoutEffects` (`useLayoutEffect`).
4. Finally, React yields to the browser for layout and paint, and subsequently schedules passive effects (`useEffect`).

---

## 6. Performance, Bundling & React Compiler

- **Reflow Prevention**: Eliminates multiple synchronous layout recalculations per frame during dynamic component mounting.
- **Tree-Shaking**: Bundlers can eliminate `useInsertionEffect` if no CSS-in-JS runtime is used in the final production artifact.
- **React Compiler**: The compiler leaves `useInsertionEffect` intact, treating its setup callback as an unoptimizable boundary due to its library-author focus.

---

## 7. Accessibility (a11y) & Usability

When CSS-in-JS libraries dynamically inject styles, layout thrashing can cause visible page stutter or delayed paint of accessible content. By ensuring styles are parsed before DOM layout occurs, `useInsertionEffect` prevents layout shifts (CLS) that disorient screen reader users or users with vestibular motion disorders.

---

## 8. Testing & Verification Patterns

Unit testing style injection with Vitest and JSDOM:

```tsx
import { render } from '@testing-library/react';
import { describe, it, expect, beforeEach } from 'vitest';
import React from 'react';
import { StyledBadge } from './StyledBadge';

describe('useInsertionEffect - StyledBadge', () => {
  beforeEach(() => {
    document.head.innerHTML = '';
  });

  it('injects style tag into document head before component renders', () => {
    render(<StyledBadge text="Success" color="#10b981" />);

    const styleTags = document.head.querySelectorAll('style');
    expect(styleTags.length).toBeGreaterThan(0);

    const injectedCSS = styleTags[0].textContent;
    expect(injectedCSS).toContain('background-color: #10b981');
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 17** | Did not exist. CSS-in-JS libraries injected styles inside `useLayoutEffect` or render phase. | Caused layout thrashing in concurrent mode tests. |
| **React 18** | Introduced `useInsertionEffect` specifically for CSS-in-JS libraries. | Emotion 11 and styled-components v6 adopted `useInsertionEffect`. |
| **React 19** | Fully stabilized. Ecosystem trend favors zero-runtime CSS (Tailwind CSS v4, CSS Modules) over dynamic CSS-in-JS. | Keep only for legacy runtime styling libraries. |

---

## 10. Security & Hardening

1. **CSS Injection Vulnerabilities**: When generating dynamic CSS rules from user-supplied props, sanitize values to prevent CSS injection attacks (e.g., escaping `url()`, closing brackets `}`, or `expression()` hacks).
2. **CSP (Content Security Policy)**: Injecting `<style>` tags dynamically requires `'unsafe-inline'` unless a `nonce` attribute is appended to the generated style elements. Modern CSS-in-JS libraries allow specifying a CSP nonce.

---

## 11. Real-World Case Study

### Emotion / Styled-Components Internal Style Collector

How a modern runtime CSS-in-JS engine structures its insertion layer to support React 18/19 concurrent rendering:

```tsx
import React, { useInsertionEffect } from 'react';

interface CacheEntry {
  className: string;
  css: string;
}

const globalCache = new Map<string, CacheEntry>();

export function useStyleInjector(css: string): string {
  let entry = globalCache.get(css);
  if (!entry) {
    const className = `dyn-${Math.random().toString(36).substring(2, 8)}`;
    entry = { className, css };
    globalCache.set(css, entry);
  }

  useInsertionEffect(() => {
    const style = document.createElement('style');
    style.setAttribute('data-emotion', 'dynamic');
    style.textContent = `.${entry!.className} { ${entry!.css} }`;
    document.head.appendChild(style);

    return () => {
      style.remove();
    };
  }, [entry.className, entry.css]);

  return entry.className;
}
```

---

## 12. Interview Q&A

<details>
<summary>1. What exact problem does `useInsertionEffect` solve, and why wasn't `useLayoutEffect` sufficient?</summary>

`useInsertionEffect` solves layout thrashing caused by dynamic runtime CSS injection. When CSS rules are injected inside `useLayoutEffect`, any other component in the tree that reads layout geometry (via `getBoundingClientRect()`, `offsetWidth`, etc.) in its own `useLayoutEffect` forces the browser to synchronously recalculate layout multiple times.
`useInsertionEffect` runs before DOM mutations and before layout effects, ensuring all styles are in place before any code reads DOM geometry.
</details>

<details>
<summary>2. Why can't we access DOM refs inside `useInsertionEffect`?</summary>

`useInsertionEffect` fires in the pre-mutation phase of the commit pipeline. At this stage, React has computed the Virtual DOM changes, but it has not yet applied mutations to the real host DOM. Consequently, DOM nodes may not exist yet, and `ref.current` is `null`.
</details>

<details>
<summary>3. Can regular application developers use `useInsertionEffect` for data fetching or UI side effects?</summary>

No. The React team explicitly designed `useInsertionEffect` solely for CSS-in-JS library authors. Regular side effects belong in `useEffect`, and DOM layout measurements belong in `useLayoutEffect`. Scheduling state updates in `useInsertionEffect` is prohibited.
</details>

<details>
<summary>4. What is the execution order of all three effect hooks in React?</summary>

1. `useInsertionEffect` (runs synchronously before DOM mutations).
2. DOM mutations occur (React writes to the DOM).
3. `useLayoutEffect` (runs synchronously after DOM mutations, before browser paint).
4. Browser paints screen.
5. `useEffect` (runs asynchronously after browser paint).
</details>

<details>
<summary>5. How does `useInsertionEffect` interact with Server-Side Rendering (SSR)?</summary>

Like `useLayoutEffect`, `useInsertionEffect` does not run on the server. For SSR, CSS-in-JS libraries extract critical CSS during the server render string evaluation and insert `<style>` tags directly into the server HTML stream.
</details>

<details>
<summary>6. How does Content Security Policy (CSP) impact `useInsertionEffect`?</summary>

If a website uses a strict Content Security Policy (`script-src`, `style-src` without `'unsafe-inline'`), creating `<style>` tags dynamically at runtime will be blocked by the browser. To make `useInsertionEffect` work with CSP, the injected `<style>` tag must include a matching `nonce` attribute.
</details>

<details>
<summary>7. What happens if you call `setState` inside `useInsertionEffect`?</summary>

React throws a development warning or error because scheduling state updates during the pre-mutation phase disrupts the React scheduler and fiber lane assignment.
</details>

<details>
<summary>8. Why is the React ecosystem moving away from runtime CSS-in-JS towards zero-runtime styling?</summary>

Runtime CSS-in-JS requires shipping a JS parsing engine to the client, generates runtime style injection overhead, causes serialization delays, and complicates Server Components (RSC) where client JS cannot run. Zero-runtime solutions (Tailwind CSS v4, StyleX, CSS Modules) compile styles at build time with zero client JS runtime cost.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Injected Keyframe Animation Hook</summary>

**Specification**: Build a hook that injects a dynamic CSS `@keyframes` definition into `<head>` before an animated spinner mounts.

```tsx
import React, { useInsertionEffect } from 'react';

export function useKeyframeAnimation(name: string, keyframeBody: string) {
  useInsertionEffect(() => {
    const style = document.createElement('style');
    style.textContent = `@keyframes ${name} { ${keyframeBody} }`;
    document.head.appendChild(style);
    return () => {
      style.remove();
    };
  }, [name, keyframeBody]);
}

export function PulsingDot() {
  useKeyframeAnimation('pulseAnimation', '0%, 100% { opacity: 1; } 50% { opacity: 0.3; }');

  return (
    <div
      style={{ animation: 'pulseAnimation 1.5s infinite ease-in-out' }}
      className="w-4 h-4 rounded-full bg-emerald-500"
    />
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): CSS Variable Injector</summary>

**Specification**: Build a hook using `useInsertionEffect` that defines CSS root variables dynamically based on a tenant theme object.

```tsx
import { useInsertionEffect } from 'react';

export function useCSSVariables(vars: Record<string, string>) {
  useInsertionEffect(() => {
    const style = document.createElement('style');
    const varDeclarations = Object.entries(vars)
      .map(([k, v]) => `--${k}: ${v};`)
      .join(' ');
    style.textContent = `:root { ${varDeclarations} }`;
    document.head.appendChild(style);

    return () => {
      style.remove();
    };
  }, [JSON.stringify(vars)]);
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Constructable Stylesheet Injector</summary>

**Specification**: Use `document.adoptedStyleSheets` and `CSSStyleSheet` in `useInsertionEffect` for modern browsers.

```tsx
import { useInsertionEffect } from 'react';

export function useAdoptedSheet(cssText: string) {
  useInsertionEffect(() => {
    if ('adoptedStyleSheets' in document) {
      const sheet = new CSSStyleSheet();
      sheet.replaceSync(cssText);
      document.adoptedStyleSheets = [...document.adoptedStyleSheets, sheet];

      return () => {
        document.adoptedStyleSheets = document.adoptedStyleSheets.filter((s) => s !== sheet);
      };
    }
  }, [cssText]);
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Font Face Dynamic Loader</summary>

**Specification**: Inject dynamic `@font-face` rules before typography components execute their layout calculations.

```tsx
import { useInsertionEffect } from 'react';

export function useFontFace(fontFamily: string, fontUrl: string) {
  useInsertionEffect(() => {
    const style = document.createElement('style');
    style.textContent = `
      @font-face {
        font-family: '${fontFamily}';
        src: url('${fontUrl}') format('woff2');
        font-display: swap;
      }
    `;
    document.head.appendChild(style);
    return () => {
      style.remove();
    };
  }, [fontFamily, fontUrl]);
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Scoped CSS-in-JS Component Factory</summary>

**Specification**: Build a `createStyledComponent` HOC that parses template literals, hashes classes, and injects styles with `useInsertionEffect`.

```tsx
import React, { useInsertionEffect, type ComponentType } from 'react';

export function createStyled<P extends object>(
  Tag: keyof JSX.IntrinsicElements | ComponentType<P>,
  cssString: string
) {
  const className = `styled-${Math.abs(hash(cssString)).toString(36)}`;

  return function StyledComponent(props: P & { className?: string }) {
    useInsertionEffect(() => {
      const style = document.createElement('style');
      style.textContent = `.${className} { ${cssString} }`;
      document.head.appendChild(style);
      return () => {
        style.remove();
      };
    }, []);

    const combinedClass = [className, props.className].filter(Boolean).join(' ');
    const Component = Tag as any;

    return <Component {...props} className={combinedClass} />;
  };
}

function hash(str: string): number {
  let h = 0;
  for (let i = 0; i < str.length; i++) {
    h = (h << 5) - h + str.charCodeAt(i);
    h |= 0;
  }
  return h;
}

export const RedCard = createStyled('div', 'background-color: #fee2e2; color: #991b1b; padding: 16px; border-radius: 8px;');
```
</details>

<details>
<summary>Exercise 6 (Hard): SSR Rehydration Hydration Marker Registry</summary>

**Specification**: Build an insertion hook that checks whether server-rendered `<style data-ssr="true">` elements exist and avoids duplicate insertion during client hydration.

```tsx
import { useInsertionEffect } from 'react';

const clientRenderedStyles = new Set<string>();

export function useSSRSafeInsertion(id: string, css: string) {
  useInsertionEffect(() => {
    // Check if server already rendered this style tag in HTML
    const existingServerTag = document.querySelector(`style[data-ssr-id="${id}"]`);
    if (existingServerTag) {
      clientRenderedStyles.add(id);
      return;
    }

    if (!clientRenderedStyles.has(id)) {
      clientRenderedStyles.add(id);
      const style = document.createElement('style');
      style.setAttribute('data-ssr-id', id);
      style.textContent = css;
      document.head.appendChild(style);

      return () => {
        style.remove();
        clientRenderedStyles.delete(id);
      };
    }
  }, [id, css]);
}
```
</details>
