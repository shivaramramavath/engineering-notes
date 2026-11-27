# Production Implementation: Mini CSS Design System & UI Toolkit

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Apex UI - Component Design System</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>
  <div class="system-container">
    <header class="system-header">
      <span class="system-badge">Cascade Layer Architecture</span>
      <h1>Apex UI Design System</h1>
      <p>A resilient, framework-agnostic component library structured with CSS @layer.</p>
    </header>

    <main class="system-main">
      <!-- Section 1: Buttons -->
      <section class="component-group">
        <h2>Buttons &amp; Actions</h2>
        <div class="component-row">
          <button type="button" class="btn btn-primary">Primary Button</button>
          <button type="button" class="btn btn-secondary">Secondary Button</button>
          <button type="button" class="btn btn-outline">Outline Button</button>
          <button type="button" class="btn btn-destructive">Destructive</button>
          <button type="button" class="btn btn-primary" disabled>Disabled State</button>
        </div>
      </section>

      <!-- Section 2: Badges -->
      <section class="component-group">
        <h2>Status Badges</h2>
        <div class="component-row">
          <span class="badge badge-success"><span class="badge-dot"></span> Operational</span>
          <span class="badge badge-warning"><span class="badge-dot"></span> Degraded Performance</span>
          <span class="badge badge-danger"><span class="badge-dot"></span> Critical Incident</span>
          <span class="badge badge-neutral">Archived</span>
        </div>
      </section>

      <!-- Section 3: Cards & Panels -->
      <section class="component-group">
        <h2>Card Container</h2>
        <div class="cards-grid">
          <article class="card">
            <div class="card-header">
              <span class="card-tag">Infrastructure</span>
              <h3 class="card-title">Distributed Object Storage</h3>
            </div>
            <p class="card-body">High-throughput encrypted S3-compatible blob storage with automatic cross-region replication.</p>
            <div class="card-footer">
              <button type="button" class="btn btn-primary btn-sm">Provision Bucket</button>
            </div>
          </article>

          <article class="card">
            <div class="card-header">
              <span class="card-tag">Networking</span>
              <h3 class="card-title">Anycast Border Gateway</h3>
            </div>
            <p class="card-body">Global dynamic BGP routing directing traffic to the lowest-latency edge point of presence.</p>
            <div class="card-footer">
              <button type="button" class="btn btn-secondary btn-sm">View Telemetry</button>
            </div>
          </article>
        </div>
      </section>

      <!-- Section 4: Utilities -->
      <section class="component-group">
        <h2>Utility Classes</h2>
        <div class="u-flex u-gap-4 u-items-center">
          <span class="u-text-muted">High-priority utility classes resolve above component rules.</span>
          <span class="u-sr-only">Screen reader descriptive label</span>
        </div>
      </section>
    </main>
  </div>
</body>
</html>
```

```css
/* ==========================================================================
   Apex UI: 5-Tier Cascade Layer Definition
   ========================================================================== */
@layer reset, tokens, base, components, utilities;

/* ==========================================================================
   Layer 1: Reset
   ========================================================================== */
@layer reset {
  *, *::before, *::after {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
  }

  html {
    -webkit-text-size-adjust: 100%;
    -webkit-font-smoothing: antialiased;
  }

  img, picture, video, canvas, svg {
    display: block;
    max-width: 100%;
  }

  input, button, textarea, select {
    font: inherit;
  }
}

/* ==========================================================================
   Layer 2: Tokens
   ========================================================================== */
@layer tokens {
  :root {
    color-scheme: light dark;

    /* Primitives */
    --color-blue-500: oklch(55% 0.22 260);
    --color-blue-600: oklch(48% 0.24 260);
    --color-green-500: oklch(62% 0.20 145);
    --color-amber-500: oklch(68% 0.18 75);
    --color-red-500: oklch(58% 0.22 25);
    
    /* Semantics via light-dark */
    --color-bg-canvas: light-dark(#f8fafc, #0b0f19);
    --color-bg-surface: light-dark(#ffffff, #111827);
    --color-border: light-dark(#e2e8f0, #1f2937);
    --color-text-main: light-dark(#0f172a, #f9fafb);
    --color-text-muted: light-dark(#64748b, #9ca3af);

    --color-primary: var(--color-blue-500);
    --color-primary-hover: var(--color-blue-600);

    /* Radii */
    --radius-sm: 4px;
    --radius-md: 8px;
    --radius-lg: 12px;
    --radius-pill: 9999px;

    /* Typography */
    --font-sans: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  }
}

/* ==========================================================================
   Layer 3: Base
   ========================================================================== */
@layer base {
  body {
    font-family: var(--font-sans);
    background-color: var(--color-bg-canvas);
    color: var(--color-text-main);
    line-height: 1.6;
    padding: 3rem 1.5rem;
  }

  h1, h2, h3, h4 {
    color: var(--color-text-main);
    line-height: 1.2;
    letter-spacing: -0.025em;
  }

  h1 { font-size: clamp(2rem, 1.25rem + 2.5vw, 3rem); font-weight: 800; }
  h2 { font-size: 1.5rem; font-weight: 700; }
  h3 { font-size: 1.25rem; font-weight: 600; }

  a {
    color: var(--color-primary);
    text-decoration: none;
  }

  a:hover {
    text-decoration: underline;
  }
}

/* ==========================================================================
   Layer 4: Components
   ========================================================================== */
@layer components {
  /* Layout Wrapper */
  .system-container {
    max-width: 960px;
    margin: 0 auto;
  }

  .system-header {
    margin-bottom: 3.5rem;
  }

  .system-badge {
    display: inline-block;
    font-size: 0.75rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.06em;
    color: var(--color-primary);
    margin-bottom: 0.75rem;
  }

  .component-group {
    background: var(--color-bg-surface);
    border: 1px solid var(--color-border);
    border-radius: var(--radius-lg);
    padding: 2rem;
    margin-bottom: 2rem;
  }

  .component-group h2 {
    margin-bottom: 1.25rem;
    padding-bottom: 0.75rem;
    border-bottom: 1px solid var(--color-border);
  }

  .component-row {
    display: flex;
    flex-wrap: wrap;
    gap: 1rem;
    align-items: center;
  }

  /* Buttons */
  .btn {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    padding: 0.625rem 1.25rem;
    font-size: 0.875rem;
    font-weight: 600;
    border-radius: var(--radius-md);
    cursor: pointer;
    border: 1px solid transparent;
    transition: all 150ms ease;
    white-space: nowrap;
    text-decoration: none;
  }

  .btn-sm {
    padding: 0.4rem 0.875rem;
    font-size: 0.8125rem;
  }

  .btn-primary {
    background-color: var(--color-primary);
    color: #ffffff;
  }

  .btn-primary:hover {
    background-color: var(--color-primary-hover);
  }

  .btn-secondary {
    background-color: light-dark(#f1f5f9, #1e293b);
    border-color: var(--color-border);
    color: var(--color-text-main);
  }

  .btn-secondary:hover {
    background-color: light-dark(#e2e8f0, #334155);
  }

  .btn-outline {
    background: transparent;
    border-color: var(--color-primary);
    color: var(--color-primary);
  }

  .btn-outline:hover {
    background: rgba(37, 99, 235, 0.08);
  }

  .btn-destructive {
    background-color: var(--color-red-500);
    color: #ffffff;
  }

  .btn-destructive:hover {
    filter: brightness(0.9);
  }

  .btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .btn:focus-visible {
    outline: 2px solid var(--color-primary);
    outline-offset: 2px;
  }

  /* Badges */
  .badge {
    display: inline-flex;
    align-items: center;
    gap: 0.375rem;
    padding: 0.25rem 0.625rem;
    font-size: 0.75rem;
    font-weight: 600;
    border-radius: var(--radius-pill);
    line-height: 1;
  }

  .badge-dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
  }

  .badge-success { background: rgba(34, 197, 94, 0.15); color: #22c55e; }
  .badge-success .badge-dot { background: #22c55e; }

  .badge-warning { background: rgba(245, 158, 11, 0.15); color: #f59e0b; }
  .badge-warning .badge-dot { background: #f59e0b; }

  .badge-danger { background: rgba(239, 68, 68, 0.15); color: #ef4444; }
  .badge-danger .badge-dot { background: #ef4444; }

  .badge-neutral { background: light-dark(#f1f5f9, #1e293b); color: var(--color-text-muted); }

  /* Cards */
  .cards-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
    gap: 1.5rem;
  }

  .card {
    background: var(--color-bg-surface);
    border: 1px solid var(--color-border);
    border-radius: var(--radius-md);
    padding: 1.5rem;
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
  }

  .card-tag {
    font-size: 0.75rem;
    font-weight: 700;
    color: var(--color-primary);
    text-transform: uppercase;
  }

  .card-body {
    color: var(--color-text-muted);
    font-size: 0.9rem;
    flex: 1;
  }
}

/* ==========================================================================
   Layer 5: Utilities
   ========================================================================== */
@layer utilities {
  .u-flex { display: flex; }
  .u-grid { display: grid; }
  .u-items-center { align-items: center; }
  .u-justify-between { justify-content: space-between; }
  .u-gap-4 { gap: 1rem; }
  .u-text-center { text-align: center; }
  .u-text-muted { color: var(--color-text-muted); }

  /* Accessible Visually Hidden */
  .u-sr-only {
    position: absolute;
    width: 1px;
    height: 1px;
    padding: 0;
    margin: -1px;
    overflow: hidden;
    clip: rect(0, 0, 0, 0);
    white-space: nowrap;
    border: 0;
  }
}
```
