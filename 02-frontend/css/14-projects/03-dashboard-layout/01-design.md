# Design System & Specifications: Cloud Operations Dashboard Shell

## 1. Application Shell Grid Geometry

The outer application shell enforces a 2-tier CSS Grid geometry:

```css
.dash-shell {
  display: grid;
  grid-template-rows: var(--header-height, 64px) 1fr;
  grid-template-columns: var(--sidebar-width, 250px) 1fr;
  grid-template-areas:
    "header  header"
    "sidebar content";
  height: 100dvh;
  width: 100vw;
  overflow: hidden;
}
```

---

## 2. Color Palette (Enterprise High-Density Theme)

| Token | Hex / Fallback | Role |
|---|---|---|
| `--surface-canvas` | `#0b0f19` | Main content canvas background |
| `--surface-panel` | `#111827` | Widget cards, table container |
| `--surface-sidebar` | `#070a11` | Primary navigation sidebar |
| `--surface-header` | `#0f172a` | Top application header bar |
| `--border-subtle` | `#1f2937` | Grid dividers, card borders |
| `--text-primary` | `#f9fafb` | Primary data labels, metrics |
| `--text-muted` | `#9ca3af` | Secondary timestamps, table headers |
| `--status-healthy` | `#10b981` | Emerald green success pill |
| `--status-warning` | `#f59e0b` | Amber warning status |
| `--status-critical` | `#ef4444` | Red critical alert pill |

---

## 3. Responsive Shell States

| Viewport | Sidebar Behavior | Header Actions | Content Grid |
|---|---|---|---|
| **Desktop (>1024px)** | Persistent expanded (250px) | Search + Org + Avatar | 4-col metric ribbon, full table |
| **Tablet (768px–1024px)**| Compact rail (72px icons only)| Search icon + Avatar | 2-col metric ribbon, scrollable table |
| **Mobile (<768px)** | Off-canvas drawer (Popover) | Hamburger + Brand | 1-col metric stack, card reflow table |
