# Cheatsheet 01: Selectors and Specificity

| Spec Level | Selectors Level 4 | Baseline Status | Widely Available |
|---|---|---|---|
| Browsers Tested | Chrome 120+, Firefox 125+, Safari 17.4+ | Primary Usage | Targeting DOM elements and cascade resolution |

---

## 1. Selectors Quick Reference Table

| Category | Selector Syntax | Description & Matching Logic | Example |
|---|---|---|---|
| **Universal** | `*` | Matches every element node | `* { box-sizing: border-box; }` |
| **Type** | `button`, `h1` | Matches by HTML tag name | `button { cursor: pointer; }` |
| **Class** | `.card`, `.btn-primary` | Matches `class` attribute tokens | `.card { padding: 1rem; }` |
| **ID** | `#header`, `#root` | Matches unique `id` attribute | `#root { min-height: 100vh; }` |
| **Attribute (Exact)** | `[type="submit"]` | Value exactly matches string | `input[type="text"] { width: 100%; }` |
| **Attribute (Contains Word)** | `[class~="active"]` | Space-separated word list contains value | `[class~="selected"] { opacity: 1; }` |
| **Attribute (Begins With)** | `[href^="https://"]` | String begins with prefix | `a[href^="https://"]::after { content: " ↗"; }` |
| **Attribute (Ends With)** | `[href$=".pdf"]` | String ends with suffix | `a[href$=".pdf"] { color: red; }` |
| **Attribute (Substring)** | `[title*="preview"]` | String contains substring anywhere | `[class*="icon-"] { display: inline-block; }` |
| **Attribute (Case-Insensitive)** | `[href$=".png" i]` | Case-insensitive flag `i` | `img[src$=".png" i] { image-rendering: auto; }` |
| **Descendant** | `nav a` | Any descendant at any depth | `nav a { color: blue; }` |
| **Child** | `ul > li` | Direct immediate children only | `ul > li { margin-bottom: 0.5rem; }` |
| **Next-Sibling** | `h2 + p` | Immediately adjacent sibling | `h2 + p { font-size: 1.25rem; }` |
| **Subsequent-Sibling** | `h2 ~ p` | Any following sibling sharing parent | `h2 ~ p { color: #333; }` |

---

## 2. Pseudo-Classes & Pseudo-Elements Matrix

```css
/* User Action Pseudo-Classes */
:hover             /* Pointer enters hit-test area */
:active            /* Mouse button down / key pressed */
:focus             /* Element has focus (keyboard or click) */
:focus-visible     /* Focus specifically via keyboard/assistive tech */
:focus-within      /* Target OR any of its descendants has focus */

/* Form State Pseudo-Classes */
:disabled          /* User-disabled input/button */
:enabled           /* Active input/button */
:checked           /* Checked radio, checkbox, or selected option */
:valid / :invalid  /* HTML constraint validation state */
:user-valid        /* Valid only AFTER user interaction/blur */
:user-invalid      /* Invalid only AFTER user interaction/blur */
:required          /* Element has required attribute */
:placeholder-shown /* Placeholder text is currently visible */

/* Structural Tree Pseudo-Classes */
:root              /* Root document element (<html> in HTML) */
:first-child       /* First child among siblings */
:last-child        /* Last child among siblings */
:only-child        /* Single child in parent */
:nth-child(2n+1)   /* Formula: odd elements (1, 3, 5, 7...) */
:nth-child(2n)     /* Formula: even elements (2, 4, 6, 8...) */
:nth-child(-n+3)   /* Formula: first 3 elements only (1, 2, 3) */
:nth-of-type(1)    /* First element of its specific tag type */
:empty             /* Has no child nodes (including whitespace text nodes) */

/* Pseudo-Elements (Dual Colons ::) */
::before           /* Virtual inline child before content */
::after            /* Virtual inline child after content */
::placeholder      /* Placeholder text style */
::selection        /* User-highlighted text selection */
::marker           /* List bullet or summary disclosure triangle */
::backdrop         /* Top-layer backdrop behind native <dialog> */
```

---

## 3. Specificity Calculator Formula: `(A, B, C)`

Specificity is calculated as a 3-component tuple: `(IDs, Classes, Types)`.
*Inline styles* count as `(1, 0, 0, 0)` if compared as a 4-part tuple. `!important` bypasses the specificity weight table and elevates declarations into the User Agent / Author Important cascade tiers.

```
+-------------------------------------------------------------+
| Column A: ID Selectors               (#header, #nav)        |
| Column B: Class / Attribute / Pseudo (.card, [type], :hover)|
| Column C: Type Selectors & Elements  (div, h1, ::before)    |
+-------------------------------------------------------------+
```

### Specificity Lookup Table

| Selector | (A, B, C) Tuple | Decimal Equivalent |
|---|---|---|
| `*` | `(0, 0, 0)` | 0 |
| `p` | `(0, 0, 1)` | 1 |
| `p.intro` | `(0, 1, 1)` | 11 |
| `.nav > li > a` | `(0, 1, 2)` | 12 |
| `.menu li a:hover` | `(0, 2, 2)` | 22 |
| `#sidebar` | `(1, 0, 0)` | 100 |
| `#sidebar .menu li a:hover` | `(1, 2, 2)` | 122 |
| `#sidebar #nav-toggle` | `(2, 0, 0)` | 200 |

> [!WARNING]
> Decimal shorthand (e.g. `(0, 1, 12)` vs `(1, 0, 0)`) is a mental heuristic only! 11 classes (`(0, 11, 0)`) can **NEVER** overwrite a single ID (`(1, 0, 0)`). Specificity is compared column-by-column from left to right.

---

## 4. Modern Functional Pseudo-Classes Comparison

| Selector | Specificity Weight | Forgiving Parsing? | Typical Production Use Case |
|---|---|---|---|
| `:is(.a, .b, #c)` | Weight of its **heaviest argument** (here `#c` = `(1, 0, 0)`) | **Yes** (ignores invalid selectors in list) | Deduplicating compound selectors |
| `:where(.a, .b, #c)` | **Zero Specificity** `(0, 0, 0)` always | **Yes** (ignores invalid selectors in list) | CSS Reset libraries, component default styles |
| `:has(.child)` | Weight of its **heaviest argument inside** | **No** (standard parsing) | Parent selector, conditional layout styling |
| `:not(.a, .b)` | Weight of its **heaviest argument inside** | **No** (standard parsing) | Negation filtering (`a:not(.btn)`) |

```css
/* Real-world Modern Selectors */

/* Zero-specificity design-system defaults */
:where(h1, h2, h3, h4) {
  margin-block-end: 0.5em;
  line-height: 1.2;
}

/* Parent styling dependent on child state */
.card:has(img) {
  grid-template-rows: 200px 1fr auto;
}

/* Form styling when any child checkbox is checked */
form:has(input[name="billing"]:checked) .shipping-address {
  display: none;
}
```

---

[← Cheatsheet Index](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/README.md) | [Next: Cheatsheet 02 - Units and Values →](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/02-units-and-values.md)
