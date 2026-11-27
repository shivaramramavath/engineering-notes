# Cheatsheet 08: Colors and Gradients

| Spec Level | CSS Color Module Level 4 & Level 5 | Baseline Status | Baseline 2023 / 2024 |
|---|---|---|---|
| Browsers Tested | Chrome 120+, Firefox 125+, Safari 17.4+ | Primary Usage | Wide-gamut color design systems, gradients, dynamic theme palettes |

---

## 1. Color Models Quick Comparison

| Color Model | Syntax Example | Gamut Support | Perceptually Uniform? | Use Case |
|---|---|---|---|---|
| `hex` | `#3b82f6` | sRGB only | No | Legacy stylesheets, brand hex codes |
| `rgb()` | `rgb(59 130 246 / 0.5)` | sRGB only | No | Modern space-separated sRGB syntax |
| `hsl()` | `hsl(217 91% 60%)` | sRGB only | No (yellow appears way brighter than blue) | Legacy dynamic lightness adjustments |
| `oklch()` | `oklch(0.62 0.21 259)` | **P3 & Rec. 2020** (Wide Gamut) | **Yes** (Identical perceived luminance across all hues) | **Modern Design Systems (Recommended)** |

---

## 2. OKLCH Color Space Anatomy

$$\text{oklch}(L \quad C \quad H \quad [/\quad A])$$

```
L (Lightness): 0% (black) to 100% (pure white)
C (Chroma):    0 (achromatic grey) to ~0.37 (hyper-saturated P3 vivid colors)
H (Hue Angle): 0 to 360 degrees (0 = Pink/Red, 90 = Yellow, 140 = Green, 260 = Blue)
A (Alpha):     0 to 1 or 0% to 100%
```

```css
:root {
  /* Ultra-vivid display-P3 colors impossible in sRGB */
  --brand-primary:   oklch(0.65 0.24 260);      /* Radiant cobalt blue */
  --brand-success:   oklch(0.72 0.22 145);      /* Vivid emerald */
  --brand-danger:    oklch(0.63 0.26 29);       /* Searing crimson */
  --surface-1:       oklch(0.98 0.01 260);      /* Perceptually neutral tinted surface */
}
```

---

## 3. Dynamic Color Manipulation: `color-mix()` (Baseline 2023)

Blend two colors mathematically across any specified color space:

```css
.card {
  /* 1. Generate 10% tinted subtle hover background */
  background: color-mix(in oklch, var(--brand-primary) 10%, transparent);

  /* 2. Lighten a color by mixing with pure white */
  border-color: color-mix(in oklch, var(--brand-primary) 40%, white);

  /* 3. Darken a color for dark-mode contrast */
  color: color-mix(in oklch, var(--brand-primary) 70%, black);
}
```

---

## 4. Relative Color Syntax (Baseline 2024)

Deconstruct an existing color token into channels, modify one channel, and return a derived color:

```css
:root {
  --brand: #2563eb;
}

.variant {
  /* Extract lightness and hue, crank chroma and set 50% opacity */
  background: oklch(from var(--brand) l calc(c * 1.5) h / 0.5);

  /* Create an inverted contrast text color */
  color: oklch(from var(--brand) calc(1 - l) 0 h);
}
```

---

## 5. CSS Gradients Cheat Sheet

```css
/* 1. Modern Perceptually Linear Gradient (in oklch prevents "dead grey" middle) */
.hero-gradient {
  background: linear-gradient(
    in oklch 135deg,
    oklch(0.65 0.24 260) 0%,
    oklch(0.72 0.22 145) 100%
  );
}

/* 2. Radial Gradient (Focal lighting effect) */
.focal-spotlight {
  background: radial-gradient(
    circle at 50% 30%,
    rgba(59, 130, 246, 0.4) 0%,
    transparent 70%
  );
}

/* 3. Conic Gradient (Color wheels, pie charts, border rotations) */
.pie-chart {
  background: conic-gradient(
    #3b82f6 0% 35%,
    #10b981 35% 65%,
    #f59e0b 65% 100%
  );
  border-radius: 50%;
}
```

---

[← Previous: Cheatsheet 07 - Typography](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/07-typography.md) | [Next: Cheatsheet 09 - Transitions and Animations →](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/09-transitions-and-animations.md)
