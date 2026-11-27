# Limitations & Next Steps: Minimalist Engineer Bento Portfolio

## 1. Architectural Limitations & Production Edge Cases

1. **Aspect Ratio Constraints on Multi-Row Bento Spans**:
   When `.card-bio` spans 2 rows while adjacent single-row cards contain unpredictable text volumes (e.g. an expanded tech stack list), the grid row tracks expand. If the content in `.card-tech` or `.card-status` pushes row height up, `.card-bio` stretches vertically. Using `justify-content: space-between` inside the bio card ensures content spreads evenly.
2. **Print Hyperlink Expansion**:
   In print stylesheets, standard practice expands link destinations (`a[href]::after { content: " (" attr(href) ")"; }`). However, applying this globally inside compact Bento cards can wrap and distort layout lines; hence, it was restricted to article publications and contact rows.

---

## 2. Progressive Enhancements & Next Steps

1. **Mouse-Tracking Radial Glow with CSS Custom Properties**:
   Using CSS variables `--mouse-x` and `--mouse-y` updated via a lightweight pointer listener to render an illuminating spotlight border effect:
   ```css
   .bento-card::before {
     content: "";
     position: absolute;
     inset: 0;
     background: radial-gradient(350px circle at var(--mouse-x, 0) var(--mouse-y, 0), var(--accent-surface), transparent);
     pointer-events: none;
   }
   ```
2. **Container Query Enhancements within Individual Cards**:
   Applying `container-type: inline-size` to each `.bento-card` so internal elements (like `.bio-metrics`) re-align based on individual card width rather than global screen breakpoints.
