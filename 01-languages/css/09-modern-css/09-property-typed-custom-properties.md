# Typed Custom Properties (`@property`)

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | CSS Properties and Values API Level 1 (W3C Candidate Recommendation) |
| **Baseline Status** | Baseline 2024 (Widely Available across Chrome 85+, Safari 16.4+, Firefox 128+, Edge 85+) |
| **Browsers Tested** | Chrome 85+, Firefox 128+, Safari 16.4+, Edge 85+ |
| **Tested With** | W3C Web Platform Tests, Chrome DevTools Animation Inspector, Playwright |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility & Browser Support](#8-performance-accessibility--browser-support)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Standard CSS Custom Properties (`--my-var: 20px;`) were a massive leap forward for stylesheets, but they possess a fundamental architectural limitation: to the browser engine, custom properties are **untyped token streams** (plain text strings).

Because custom properties lack type metadata:
- **CSS Gradients Cannot Be Animated**: Transitioning a gradient like `linear-gradient(var(--angle), red, blue)` fails completely; the browser cannot interpolate a text string between `0deg` and `360deg`, causing the gradient to jump abruptly.
- **Uncontrolled Inheritance**: By default, all CSS variables inherit down the entire DOM tree, causing unintended style pollution and excessive memory overhead.
- **No Type Validation**: If a component expects `--radius: 8px` and a consumer supplies `--radius: red`, the browser simply ignores the property at computed-value time, reverting to `initial` with no error or fallback guardrails.

Part of the **CSS Houdini** suite, the **`@property`** at-rule (CSS Properties and Values API) brings strict typing, explicit inheritance control, and guaranteed default values directly to CSS custom properties. With full cross-browser Baseline 2024 support, `@property` unlocks pure CSS gradient transitions, smooth angle rotations, and type-safe design systems.

---

## 2. Core Concepts

### The `@property` Declaration
The `@property` rule defines a custom property directly in CSS using three required descriptors:
1. **`syntax`**: A string defining the expected data type (e.g. `"<color>"`, `"<length>"`, `"<percentage>"`, `"<angle>"`).
2. **`inherits`**: A boolean (`true` or `false`) specifying whether the property cascades down through child elements.
3. **`initial-value`**: The default fallback value if no value is assigned (mandatory for almost all non-universal syntaxes).

```css
@property --gradient-angle {
  syntax: "<angle>";
  inherits: false;
  initial-value: 0deg;
}
```

### Animating the Unanimatable
Once the browser knows that `--gradient-angle` is of type `<angle>`, its interpolation engine can calculate intermediate frames between `0deg` and `360deg`. Gradients, radial color stops, and numeric offsets can now transition smoothly via standard CSS `transition` and `@keyframes`!

```css
.card {
  --gradient-angle: 0deg;
  background: linear-gradient(var(--gradient-angle), #3b82f6, #ec4899);
  transition: --gradient-angle 0.5s ease;
}

.card:hover {
  --gradient-angle: 180deg; /* Smooth 60 FPS rotation! */
}
```

### Supported Syntax Types
| Type Descriptor | Permitted Values | Example |
| :--- | :--- | :--- |
| `"<length>"` | Any CSS length unit (`px`, `rem`, `em`, `vh`) | `16px`, `1.5rem` |
| `"<percentage>"` | Percent values | `50%`, `100%` |
| `"<length-percentage>"` | Mix of lengths and percentages | `calc(100% - 20px)` |
| `"<color>"` | Any valid CSS color format | `#2563eb`, `oklch(0.6 0.2 250)` |
| `"<angle>"` | Angle units (`deg`, `rad`, `turn`) | `45deg`, `0.5turn` |
| `"<number>"` | Floating point or decimal numbers | `1.5`, `42` |
| `"<integer>"` | Whole numbers only | `3`, `10` |
| `"<time>"` | Duration units (`s`, `ms`) | `300ms`, `1.5s` |
| `"<image>"` | URL or gradient | `url(...)`, `linear-gradient(...)` |
| `"<custom-ident>"` | User-defined keywords | `small`, `expanded` |

---

## 3. How It Works Internally

```
Untyped CSS Variable vs. Typed @property Pipeline:

Untyped: --angle: 0deg;
  - CSS Parser stores: TokenString("0deg")
  - Transition Engine attempts interpolation: "0deg" -> "180deg"
  - Engine check: Can strings interpolate? NO.
  - Result: Hard binary jump at 50% time. Stutters!

Typed: @property --angle { syntax: "<angle>"; ... }
  - CSS Parser stores: CSSAngleValue(0, "deg")
  - Transition Engine attempts interpolation: 0deg -> 180deg
  - Engine check: Type is <angle>. Interpolatable along unit circle.
  - Generates 60 FPS intermediate values: 1deg, 2deg, 3deg... 180deg.
  - Result: Silky smooth animated gradient!
```

```
Houdini Properties & Values API Descriptors:
┌────────────────────┬──────────────────────────────────────────────────────────┐
│ Descriptor         │ Requirement & Behavior                                   │
├────────────────────┼──────────────────────────────────────────────────────────┤
│ `syntax`           │ Mandatory string defining type grammar (e.g. `"<color>"`).│
│ `inherits`         │ Mandatory boolean (`true` / `false`). Controls cascade.  │
│ `initial-value`    │ Mandatory if syntax != `*`. Guarantees fallback value.    │
└────────────────────┴──────────────────────────────────────────────────────────┘
```

---

## 4. Syntax & Usage

```
Baseline 2024 Support Box:
├── Chrome / Edge: 85+ (August 2020)
├── Safari: 16.4+ (March 2023)
└── Firefox: 128+ (July 2024 - Complete cross-browser Baseline)
```

### CSS `@property` Syntax
```css
@property --custom-prop-name {
  syntax: "<data-type>";
  inherits: true | false;
  initial-value: fallback-value;
}
```

### JavaScript Registration Equivalent (`CSS.registerProperty`)
You can also register properties via the Houdini JavaScript API:
```javascript
window.CSS.registerProperty({
  name: '--glow-radius',
  syntax: '<length>',
  inherits: false,
  initialValue: '0px'
});
```

---

## 5. Practical Examples

### Example 1: Smooth Conic Gradient Spinner (Animated Angle)
Animate a spinning conic gradient cleanly without rotating the entire DOM node or triggering layout recalculations.

```html
<div class="conic-card">
  <div class="card-inner">
    <h3>Neon Gradient Border</h3>
    <p>Animated smoothly with @property angle interpolation.</p>
  </div>
</div>
```

```css
@property --border-angle {
  syntax: "<angle>";
  inherits: false;
  initial-value: 0deg;
}

.conic-card {
  position: relative;
  width: 20rem;
  padding: 3px;
  border-radius: 0.75rem;
  background: conic-gradient(
    from var(--border-angle),
    #3b82f6,
    #ec4899,
    #8b5cf6,
    #3b82f6
  );
  animation: spinBorder 4s linear infinite;
}

@keyframes spinBorder {
  to {
    --border-angle: 360deg;
  }
}

.card-inner {
  background-color: #0f172a;
  color: #f8fafc;
  padding: 1.5rem;
  border-radius: calc(0.75rem - 3px);

  h3 {
    margin-top: 0;
  }
}
```

### Example 2: Animating Radial Gradient Color Stops
Smoothly expand a spotlight glow effect on button hover.

```html
<button class="glow-button">
  <span>Explore Features</span>
</button>
```

```css
@property --spotlight-radius {
  syntax: "<percentage>";
  inherits: false;
  initial-value: 0%;
}

@property --spotlight-opacity {
  syntax: "<number>";
  inherits: false;
  initial-value: 0;
}

.glow-button {
  position: relative;
  padding: 0.75rem 1.75rem;
  font-family: inherit;
  font-weight: 600;
  color: #ffffff;
  background-color: #1e293b;
  border: 1px solid #334155;
  border-radius: 0.5rem;
  cursor: pointer;
  overflow: hidden;

  /* Radial spotlight background */
  background-image: radial-gradient(
    circle at 50% 50%,
    rgba(59, 130, 246, var(--spotlight-opacity)) 0%,
    transparent var(--spotlight-radius)
  );

  transition:
    --spotlight-radius 0.4s cubic-bezier(0.16, 1, 0.3, 1),
    --spotlight-opacity 0.4s ease;

  &:hover {
    --spotlight-radius: 120%;
    --spotlight-opacity: 0.8;
  }

  &:focus-visible {
    outline: 2px solid #3b82f6;
    outline-offset: 2px;
  }
}
```

### Example 3: Pure CSS Animated Numerical Counter
Animate an integer count from 0 to 100 on page load using CSS `@property` and CSS counters.

```html
<div class="counter-display" aria-label="100 percent complete">
  <span class="counter-val"></span>%
</div>
```

```css
@property --count-val {
  syntax: "<integer>";
  inherits: false;
  initial-value: 0;
}

.counter-display {
  font-family: system-ui, sans-serif;
  font-size: 3rem;
  font-weight: 800;
  color: #2563eb;
  animation: countUp 2s ease-out forwards;
  counter-reset: num var(--count-val);
}

@keyframes countUp {
  to {
    --count-val: 100;
  }
}

.counter-val::after {
  content: counter(num);
}
```

### Example 4: Inheritance Boundary Isolation (`inherits: false`)
Prevent a component's local theme variable from accidentally cascading down into embedded sub-widgets.

```css
/* Local token strictly contained within .widget-container */
@property --widget-accent {
  syntax: "<color>";
  inherits: false;
  initial-value: #2563eb;
}

.widget-container {
  --widget-accent: #10b981;
  border-left: 4px solid var(--widget-accent);
}

/* Child widget does NOT inherit --widget-accent; uses initial-value (#2563eb)! */
.child-widget {
  color: var(--widget-accent);
}
```

### Example 5: Multi-Color Gradient Mesh Transition
Smoothly cross-fade three distinct color stops in a linear gradient on card hover.

```css
@property --c1 {
  syntax: "<color>";
  inherits: false;
  initial-value: #6366f1;
}

@property --c2 {
  syntax: "<color>";
  inherits: false;
  initial-value: #ec4899;
}

.mesh-card {
  height: 12rem;
  border-radius: 0.75rem;
  background: linear-gradient(135deg, var(--c1), var(--c2));
  transition: --c1 0.6s ease, --c2 0.6s ease;

  &:hover {
    --c1: #06b6d4;
    --c2: #3b82f6;
  }
}
```

---

## 6. Best Practices

1. **Always specify `inherits: false` unless cascading is deliberate**: Untyped custom properties always inherit, which consumes style invalidation memory. Setting `inherits: false` isolates variables to the element where they are defined, improving rendering performance.
2. **Always supply an explicit `initial-value`**: The CSS parser will reject the entire `@property` block if an `initial-value` is omitted for typed syntaxes (e.g. `<length>`, `<color>`).
3. **Use `<color>` rather than `<string>` for animatable color tokens**: Declaring `syntax: "<color>"` ensures color values interpolate smoothly through intermediate color spaces rather than jumping abruptly.
4. **Name typed properties descriptively**: Prefix typed properties with a semantic identifier (e.g. `--card-angle`, `--btn-glow-radius`) to avoid collisions with standard global tokens.
5. **Use `@property` for CSS Counter animations**: Animating integers via `@property` combined with `counter-reset` enables pure CSS odometer/counter animations without JavaScript.
6. **Ensure accessible motion reduction**: When animating gradients or angles continuously, wrap animations in `@media (prefers-reduced-motion: no-preference)`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Omitting `initial-value` | Entire `@property` definition is discarded as invalid by the parser. | Always declare an explicit `initial-value` (e.g., `initial-value: 0px`). |
| Omitting quotes around syntax descriptor: `syntax: <length>;` | Syntax error; browser ignores the property. | Enclose syntax in quotes: `syntax: "<length>";`. |
| Writing invalid `initial-value` for specified syntax | If syntax is `"<length>"` and initial value is `0` (missing unit), the block fails. | Ensure initial value matches syntax: `0px` instead of `0`. |
| Attempting to declare `@property` inside a selector rule | `@property` is an at-rule that can only be declared at the top level of the stylesheet. | Move `@property` outside of class blocks. |
| Forgetting `inherits` descriptor | Descriptor is mandatory; omission causes the entire rule to fail. | Explicitly specify `inherits: true;` or `inherits: false;`. |
| Animating `@property` without defining it in `transition-property` | Transition does not run; property jumps immediately. | Add the custom property to `transition` (e.g. `transition: --angle 0.3s ease;`). |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **Compositor Efficiency**: When typed properties are used in conjunction with composited transforms or canvas/WebGL shaders, the engine calculates intermediate values in native C++ without triggering the V8 / SpiderMonkey JavaScript engines.
- **Inheritance Tree Pruning**: Setting `inherits: false` reduces the size of the browser's style inheritance tree, speeding up DOM recalculation on large pages.

### Accessibility (A11y)
- Continuous gradient rotations or pulsing glows can induce dizziness or nausea in users with vestibular sensitivity. Always guard continuous `@property` keyframe animations with `prefers-reduced-motion`:

```css
@media (prefers-reduced-motion: reduce) {
  .conic-card {
    animation: none;
  }
}
```

### Browser Compatibility
- **Baseline 2024**: Supported across Chrome (85+), Safari (16.4+), and Firefox (128+).
- **Feature Detection**:
```css
@supports (background: paint(something)) or (--test: 1) {
  /* @property is supported */
}
```

---

## 9. Comparison / When to Use What

| Feature | Standard CSS Variables (`--var`) | Typed Custom Properties (`@property`) |
| :--- | :--- | :--- |
| **Type Checking** | None (Untyped Token String) | Strict Type Validation (`<color>`, `<length>`, etc.) |
| **CSS Transitions & Animations**| Fails on gradients and numbers | **Smooth 60 FPS Interpolation** |
| **Inheritance Control** | Always inherits (forced) | Configurable (`inherits: true \| false`) |
| **Guaranteed Initial Value** | Handled manually with `var(--x, fallback)` | Native guaranteed fallback via `initial-value` |
| **JavaScript Registration** | `element.style.setProperty()` | `window.CSS.registerProperty()` |

---

## 10. Interview Questions

### Q1: Why couldn't CSS gradients be animated using standard CSS custom properties before `@property`?
<details>
<summary>View Answer</summary>
Standard CSS variables are treated as untyped string tokens. When an author wrote `linear-gradient(var(--angle), red, blue)`, the browser's interpolation engine did not know whether `--angle` was an angle, length, number, or string. Because the browser cannot mathematically calculate intermediate frames between two arbitrary strings, the transition engine failed, resulting in an immediate jump at $t = 50\%$. `@property` informs the engine that the token is of type `"<angle>"`, enabling mathematical interpolation.
</details>

### Q2: What are the three mandatory descriptors required in an `@property` at-rule?
<details>
<summary>View Answer</summary>
1. `syntax`: A string describing the allowed value types (e.g., `"<length>"`, `"<color>"`).
2. `inherits`: A boolean (`true` or `false`) defining whether child elements inherit this property.
3. `initial-value`: The fallback value, required for all syntaxes except the universal syntax (`"*"`).
</details>

### Q3: What happens if an `@property` definition contains a syntax error in its `initial-value`?
<details>
<summary>View Answer</summary>
If the `initial-value` does not strictly validate against the defined `syntax` (for example, setting `syntax: "<length>"` and `initial-value: 0` without a unit like `0px`), the entire `@property` at-rule is considered invalid and discarded by the browser. The variable will then behave as a standard untyped custom property.
</details>

### Q4: Why is `inherits: false` considered a performance optimization?
<details>
<summary>View Answer</summary>
Standard CSS custom properties inherit down the entire DOM tree by default. Whenever a custom property changes, the browser must traverse all child and descendant nodes to check if any child styles are invalidated. Setting `inherits: false` breaks the cascade for that variable, restricting style recalculation strictly to the element where the variable is explicitly declared or mutated.
</details>

### Q5: Can `@property` be registered dynamically in JavaScript?
<details>
<summary>View Answer</summary>
Yes. Using the CSS Houdini API, developers can call `window.CSS.registerProperty({ name: '--my-prop', syntax: '<color>', inherits: false, initialValue: '#000' })`. Once registered in JS, it behaves identically to an `@property` rule declared in CSS.
</details>

### Q6: Can a custom property have a list syntax, such as a list of lengths?
<details>
<summary>View Answer</summary>
Yes. The syntax grammar supports multiplier suffixes:
- `+`: Space-separated list (e.g., `"<length>+"`).
- `#`: Comma-separated list (e.g., `"<color>#"`).
This allows typed registration of complex values like multi-stop gradients or box shadows.
</details>

### Q7: What is the specificity of an `@property` at-rule?
<details>
<summary>View Answer</summary>
`@property` is an at-rule that registers property metadata in the CSSOM; it does not contain selectors and therefore has no specificity. The values assigned to the custom property within selector blocks follow standard CSS specificity and cascade rules.
</details>

### Q8: What happens if an element assigns a value that violates the `@property` syntax?
<details>
<summary>View Answer</summary>
If a declaration assigns a value that fails type validation (e.g., assigning `red` to a property registered as `"<length>"`), the declaration is considered invalid at computed-value time. The browser discards the invalid assignment and falls back to the registered `initial-value` or the inherited value (if `inherits: true`).
</details>

---

## 11. Practice Exercises

### Exercise 1: Register and Animate an Angle
**Objective**: Declare a typed property `--rotation` of type `<angle>` and smoothly rotate a button gradient on hover.
<details>
<summary>View Solution</summary>

```css
@property --rotation {
  syntax: "<angle>";
  inherits: false;
  initial-value: 0deg;
}

.rotate-btn {
  padding: 0.75rem 1.5rem;
  color: white;
  background: linear-gradient(var(--rotation), #2563eb, #7c3aed);
  border: none;
  border-radius: 0.375rem;
  cursor: pointer;
  transition: --rotation 0.5s ease;

  &:hover {
    --rotation: 180deg;
  }

  &:focus-visible {
    outline: 2px solid #2563eb;
    outline-offset: 2px;
  }
}
```
</details>

### Exercise 2: Animated Pure CSS Progress Bar
**Objective**: Create a progress bar whose fill percentage smoothly interpolates from 0% to 75% using a typed `<percentage>` property.
<details>
<summary>View Solution</summary>

```css
@property --progress-pct {
  syntax: "<percentage>";
  inherits: false;
  initial-value: 0%;
}

.progress-track {
  width: 100%;
  height: 1rem;
  background-color: #e2e8f0;
  border-radius: 1rem;
  overflow: hidden;
  background-image: linear-gradient(
    90deg,
    #10b981 var(--progress-pct),
    transparent var(--progress-pct)
  );
  animation: fillProgress 1.5s cubic-bezier(0.16, 1, 0.3, 1) forwards;
}

@keyframes fillProgress {
  to {
    --progress-pct: 75%;
  }
}
```
</details>

### Exercise 3: Non-Inheriting Card Token
**Objective**: Define a property `--theme-accent` of type `<color>` that does NOT inherit to child elements.
<details>
<summary>View Solution</summary>

```css
@property --theme-accent {
  syntax: "<color>";
  inherits: false;
  initial-value: #3b82f6;
}

.parent-card {
  --theme-accent: #ef4444;
  border: 2px solid var(--theme-accent);
}

.child-badge {
  /* Resolves to #3b82f6 (initial-value) because inherits: false */
  background-color: var(--theme-accent);
  color: white;
}
```
</details>

### Exercise 4: Smooth Shadow Elevation Interpolation
**Objective**: Transition a box shadow's spread radius smoothly using a typed `<length>` property.
<details>
<summary>View Solution</summary>

```css
@property --glow-size {
  syntax: "<length>";
  inherits: false;
  initial-value: 0px;
}

.glow-card {
  padding: 1.5rem;
  background: white;
  border-radius: 0.5rem;
  box-shadow: 0 0 var(--glow-size) rgba(59, 130, 246, 0.5);
  transition: --glow-size 0.3s ease;

  &:hover {
    --glow-size: 20px;
  }
}
```
</details>

### Exercise 5: Pure CSS Stepper Counter
**Objective**: Animate an integer score counter from 0 to 50 on view using `@property --score` and CSS counter.
<details>
<summary>View Solution</summary>

```css
@property --score {
  syntax: "<integer>";
  inherits: false;
  initial-value: 0;
}

.score-counter {
  font-size: 2.5rem;
  font-weight: 700;
  color: #0f172a;
  counter-reset: score-num var(--score);
  animation: incrementScore 1.8s ease-out forwards;
}

@keyframes incrementScore {
  to {
    --score: 50;
  }
}

.score-counter::after {
  content: counter(score-num);
}
```
</details>

### Exercise 6: Dual Stop Gradient Fade
**Objective**: Declare two typed `<percentage>` properties to independently control the start and end pinch points of a linear gradient.
<details>
<summary>View Solution</summary>

```css
@property --pinch-start {
  syntax: "<percentage>";
  inherits: false;
  initial-value: 20%;
}

@property --pinch-end {
  syntax: "<percentage>";
  inherits: false;
  initial-value: 80%;
}

.gradient-banner {
  height: 8rem;
  background: linear-gradient(
    90deg,
    #4f46e5 var(--pinch-start),
    #06b6d4 var(--pinch-end)
  );
  transition: --pinch-start 0.4s ease, --pinch-end 0.4s ease;

  &:hover {
    --pinch-start: 40%;
    --pinch-end: 60%;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* Registration Syntax */
@property --my-property {
  syntax: "<color>";         /* Required type grammar */
  inherits: false;           /* Required cascade control */
  initial-value: #000000;    /* Required fallback */
}

/* Enable Animated Gradients */
@property --grad-angle {
  syntax: "<angle>";
  inherits: false;
  initial-value: 0deg;
}

.box {
  background: linear-gradient(var(--grad-angle), red, blue);
  transition: --grad-angle 0.5s ease;
}
.box:hover {
  --grad-angle: 180deg;
}
```

- `@property` converts plain-text custom properties into strictly typed variables.
- Enables 60 FPS transitions and keyframe animations on gradients, angles, and numbers.
- `inherits: false` prevents accidental style leakage and optimizes recalculation trees.
- Baseline 2024 (Supported in Chrome 85+, Safari 16.4+, Firefox 128+).

---

## 13. Further Reading

- [MDN Web Docs: @property](https://developer.mozilla.org/en-US/docs/Web/CSS/@property)
- [W3C CSS Properties and Values API Level 1](https://www.w3.org/TR/css-properties-values-api-1/)
- [web.dev: @property: Next-gen CSS variables now with universal browser support](https://web.dev/at-property/)
- [Smashing Magazine: The Power of CSS @property](https://www.smashingmagazine.com/2024/05/power-css-at-property/)

---

### Navigation
- **Previous**: [08-color-mix-oklch-and-relative-colors.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/08-color-mix-oklch-and-relative-colors.md)
- **Next**: [10-light-dark-and-theming.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/10-light-dark-and-theming.md)
