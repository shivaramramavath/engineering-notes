# Design System & Specifications: SaaS Pricing Page with Subgrid

## 1. Subgrid Row Track Mapping

```
Parent Grid: 3 Columns (1fr 1fr 1fr), 5 Row Tracks (auto auto auto 1fr auto)
+----------------------------+----------------------------+----------------------------+
| Row 1: Plan Header (Tier)  | Plan Header (Tier)         | Plan Header (Tier)         |
+----------------------------+----------------------------+----------------------------+
| Row 2: Description Copy   | Description Copy           | Description Copy           |
+----------------------------+----------------------------+----------------------------+
| Row 3: Price Display       | Price Display              | Price Display              |
+----------------------------+----------------------------+----------------------------+
| Row 4: Feature Checkmarks  | Feature Checkmarks         | Feature Checkmarks         |
+----------------------------+----------------------------+----------------------------+
| Row 5: Call to Action Btn  | Call to Action Btn         | Call to Action Btn         |
+----------------------------+----------------------------+----------------------------+
```

Because each pricing card delegates its internal rows to the parent grid via `grid-template-rows: subgrid; grid-row: span 5;`, the tallest description or feature list in any card automatically forces matching heights across all three tiers.

---

## 2. Color Palette & Highlights

| Token | Value | Purpose |
|---|---|---|
| `--bg-pricing-canvas` | `#0f172a` | Dark high-tech background |
| `--surface-card` | `#1e293b` | Standard tier card surface |
| `--surface-popular` | `#1e293b` | Pro tier elevated card |
| `--border-popular` | `#38bdf8` | Vibrant cyan highlight ring |
| `--text-pricing-val` | `#ffffff` | Primary price figures |
| `--badge-save` | `#10b981` | Annual discount badge (-20%) |
