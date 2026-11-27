# Limitations & Next Steps: SaaS Pricing Page with Subgrid

## 1. Architectural Limitations & Production Edge Cases

1. **Subgrid Browser Support Fallbacks**:
   CSS Subgrid is Widely Available across modern browsers (Chrome 117+, Firefox 71+, Safari 16+). For older enterprise browsers lacking subgrid support, cards degrade to standard Flexbox cards (`display: flex; flex-direction: column; .tier-cta { margin-top: auto; }`). While the CTA buttons still align to the bottom, intermediate rows (descriptions and prices) may have slightly unaligned baselines.
2. **Dynamic Add-On Sliders**:
   For complex usage-based pricing models (e.g. paying per gigabyte of egress or per million events), pure CSS toggles must be augmented with lightweight JavaScript to calculate continuous slider curves.

---

## 2. Progressive Enhancements & Next Steps

1. **Comparison Matrix Drawer**:
   Pairing the 3 cards with an expandable `<details>` table comparing 40+ granular feature rows across all 3 tiers.
2. **Currency Localization**:
   Supporting multi-currency display (EUR, GBP, JPY) using CSS container queries or localized data attributes.
