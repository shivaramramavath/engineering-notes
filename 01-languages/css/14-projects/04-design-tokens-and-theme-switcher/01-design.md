# Design System & Specifications: Enterprise Design Tokens

## 1. The Three-Tier Token Architecture

```
[Tier 1: Global Primitive Tokens]
  --palette-blue-500: oklch(55% 0.22 260);
  --palette-slate-900: oklch(18% 0.03 260);
         │
         ▼
[Tier 2: Semantic Intent Tokens]
  --color-action-primary: var(--palette-blue-500);
  --color-text-main: light-dark(var(--palette-slate-900), #ffffff);
         │
         ▼
[Tier 3: Component-Scoped Tokens]
  .btn-primary {
    --btn-bg: var(--color-action-primary);
  }
```

---

## 2. Multi-Brand Identities

| Token | Brand Alpha (Fintech Clean) | Brand Beta (Cyberpunk Glow) | Brand Gamma (Editorial Warm) |
|---|---|---|---|
| `--brand-hue` | `260` (Royal Blue) | `145` (Electric Neon Green) | `45` (Deep Bronze/Ochre) |
| `--radius-sm` | `4px` | `0px` (Square Cyber) | `8px` (Curved) |
| `--radius-md` | `8px` | `0px` | `16px` |
| `--font-heading`| System Sans-serif | Monospace (`ui-monospace`) | Serif (`Georgia, serif`) |
