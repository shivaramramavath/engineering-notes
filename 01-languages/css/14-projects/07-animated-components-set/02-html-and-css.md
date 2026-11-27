# Production Implementation: Micro-Interaction & Animated Components Set

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Micro-Interaction Suite</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>
  <div class="motion-showcase-container">
    <header class="showcase-header">
      <h1>Hardware-Accelerated Micro-Interactions</h1>
      <p>Accessible, 60fps composited animations built with modern CSS primitives.</p>
    </header>

    <main class="components-grid">
      <!-- 1. Tactile Button -->
      <section class="demo-card">
        <h3>1. Tactile Spring Button</h3>
        <p>Spring-press downscale with ripple-sheen hover state.</p>
        <button type="button" class="btn-tactile">
          <span>Confirm Transaction</span>
        </button>
      </section>

      <!-- 2. Zero-CLS Loading Button -->
      <section class="demo-card">
        <h3>2. Zero-CLS Loading Button</h3>
        <p>Grid-stacked text and spinner retaining exact computed width.</p>
        <button type="button" class="btn-loading" id="demo-load-btn" aria-busy="true">
          <span class="btn-label">Deploy Microservice</span>
          <span class="btn-spinner" aria-hidden="true"></span>
        </button>
      </section>

      <!-- 3. Floating Label Input -->
      <section class="demo-card">
        <h3>3. Animated Floating Label</h3>
        <p>Pure CSS transition driven by :placeholder-shown and focus.</p>
        <div class="floating-group">
          <input type="text" id="api-endpoint" placeholder=" " required>
          <label for="api-endpoint">Cluster API Gateway Endpoint</label>
        </div>
      </section>

      <!-- 4. Native Dialog with Discrete Transition -->
      <section class="demo-card">
        <h3>4. Native Discrete Dialog</h3>
        <p>@starting-style zoom and backdrop fade with Esc key trapping.</p>
        <button type="button" class="btn-open-dialog" onclick="document.getElementById('demo-modal').showModal()">
          Open Modal Dialog
        </button>

        <dialog id="demo-modal" class="animated-dialog">
          <div class="dialog-inner">
            <h4>Security Confirmation</h4>
            <p>Revoke active cryptographic bearer tokens across all regional clusters?</p>
            <div class="dialog-actions">
              <button type="button" class="btn-ghost" onclick="document.getElementById('demo-modal').close()">Cancel</button>
              <button type="button" class="btn-danger" onclick="document.getElementById('demo-modal').close()">Revoke Access</button>
            </div>
          </div>
        </dialog>
      </section>

      <!-- 5. Composited Shimmer Skeleton -->
      <section class="demo-card">
        <h3>5. GPU Shimmer Skeleton</h3>
        <p>Transform-based shimmer with reduced-motion fallback.</p>
        <div class="skeleton-widget" aria-busy="true" aria-label="Loading node statistics...">
          <div class="sk-shape sk-avatar" aria-hidden="true"></div>
          <div class="sk-meta">
            <div class="sk-shape sk-line title" aria-hidden="true"></div>
            <div class="sk-shape sk-line subtitle" aria-hidden="true"></div>
          </div>
        </div>
      </section>
    </main>
  </div>
</body>
</html>
```

```css
/* ==========================================================================
   Motion Tokens & Showcase Layout
   ========================================================================== */
:root {
  --ease-spring: cubic-bezier(0.16, 1, 0.3, 1);
  --ease-press: cubic-bezier(0.4, 0, 0.2, 1);
  --color-canvas: #0f172a;
  --color-surface: #1e293b;
  --color-border: #334155;
  --color-brand: #38bdf8;
  --color-brand-hover: #0ea5e9;
  --color-text: #f8fafc;
  --color-text-muted: #94a3b8;
}

*, *::before, *::after {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

body {
  font-family: system-ui, -apple-system, sans-serif;
  background-color: var(--color-canvas);
  color: var(--color-text);
  line-height: 1.5;
  padding: 3rem 1.5rem;
  -webkit-font-smoothing: antialiased;
}

.motion-showcase-container {
  max-width: 960px;
  margin: 0 auto;
}

.showcase-header {
  text-align: center;
  margin-bottom: 3rem;
}

.showcase-header h1 {
  font-size: 2rem;
  font-weight: 800;
  letter-spacing: -0.02em;
}

.showcase-header p {
  color: var(--color-text-muted);
  margin-top: 0.5rem;
}

.components-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: 1.5rem;
}

.demo-card {
  background: var(--color-surface);
  border: 1px solid var(--color-border);
  border-radius: 12px;
  padding: 1.75rem;
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.demo-card h3 {
  font-size: 1.125rem;
  font-weight: 700;
}

.demo-card p {
  color: var(--color-text-muted);
  font-size: 0.875rem;
  flex: 1;
}

/* ==========================================================================
   1. Tactile Spring Button
   ========================================================================== */
.btn-tactile {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 0.75rem 1.5rem;
  background: var(--color-brand);
  color: #0f172a;
  font-weight: 700;
  font-size: 0.95rem;
  border: none;
  border-radius: 8px;
  cursor: pointer;
  position: relative;
  overflow: hidden;
  transition: transform 120ms var(--ease-press),
              box-shadow 150ms ease,
              background-color 150ms ease;
  user-select: none;
}

.btn-tactile:hover {
  background: var(--color-brand-hover);
  box-shadow: 0 4px 12px rgba(56, 189, 248, 0.3);
}

.btn-tactile:active {
  transform: scale(0.96);
  box-shadow: 0 1px 4px rgba(56, 189, 248, 0.2);
}

.btn-tactile:focus-visible {
  outline: 2px solid var(--color-brand);
  outline-offset: 3px;
}

/* ==========================================================================
   2. Zero-CLS Loading Button
   ========================================================================== */
.btn-loading {
  display: inline-grid;
  place-items: center;
  padding: 0.75rem 1.5rem;
  background: #2563eb;
  color: #ffffff;
  font-weight: 700;
  border: none;
  border-radius: 8px;
  cursor: pointer;
}

.btn-loading > * {
  grid-area: 1 / 1;
}

.btn-loading[aria-busy="true"] .btn-label {
  visibility: hidden; /* Retains intrinsic DOM volume */
}

.btn-loading .btn-spinner {
  display: none;
  width: 20px;
  height: 20px;
  border: 2px solid rgba(255, 255, 255, 0.3);
  border-top-color: #ffffff;
  border-radius: 50%;
  animation: spinner-rotate 600ms linear infinite;
}

.btn-loading[aria-busy="true"] .btn-spinner {
  display: block;
}

@keyframes spinner-rotate {
  to { transform: rotate(360deg); }
}

.btn-loading:focus-visible {
  outline: 2px solid #38bdf8;
  outline-offset: 2px;
}

/* ==========================================================================
   3. Floating Label Input
   ========================================================================== */
.floating-group {
  position: relative;
  margin-top: 0.5rem;
}

.floating-group input {
  width: 100%;
  padding: 1.125rem 0.875rem 0.5rem 0.875rem;
  background: var(--color-canvas);
  border: 1px solid var(--color-border);
  border-radius: 6px;
  color: #ffffff;
  font-size: 0.95rem;
  outline: none;
  transition: border-color 150ms ease;
}

.floating-group input:focus-visible {
  border-color: var(--color-brand);
  outline: 2px solid var(--color-brand);
}

.floating-group label {
  position: absolute;
  top: 50%;
  left: 0.875rem;
  transform: translateY(-50%);
  color: var(--color-text-muted);
  font-size: 0.9rem;
  pointer-events: none;
  transition: transform 150ms var(--ease-spring),
              font-size 150ms var(--ease-spring),
              color 150ms ease,
              top 150ms var(--ease-spring);
}

.floating-group input:focus ~ label,
.floating-group input:not(:placeholder-shown) ~ label {
  top: 0.5rem;
  transform: translateY(0);
  font-size: 0.75rem;
  font-weight: 600;
  color: var(--color-brand);
}

/* ==========================================================================
   4. Native Discrete Dialog
   ========================================================================== */
.btn-open-dialog {
  padding: 0.75rem 1.25rem;
  background: #334155;
  color: #ffffff;
  border: 1px solid var(--color-border);
  border-radius: 6px;
  cursor: pointer;
  font-weight: 600;
}

.animated-dialog {
  margin: auto;
  padding: 0;
  border: 1px solid var(--color-border);
  border-radius: 12px;
  background: var(--color-surface);
  color: var(--color-text);
  box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);
  max-width: 440px;
  width: 90vw;
  
  /* Discrete transition mechanics */
  opacity: 0;
  transform: scale(0.92) translateY(12px);
  transition: opacity 200ms ease,
              transform 200ms var(--ease-spring),
              display 200ms allow-discrete,
              overlay 200ms allow-discrete;
}

.animated-dialog[open] {
  opacity: 1;
  transform: scale(1) translateY(0);
}

@starting-style {
  .animated-dialog[open] {
    opacity: 0;
    transform: scale(0.92) translateY(12px);
  }
}

.animated-dialog::backdrop {
  background: rgba(15, 23, 42, 0.7);
  backdrop-filter: blur(4px);
  opacity: 0;
  transition: opacity 200ms ease, display 200ms allow-discrete, overlay 200ms allow-discrete;
}

.animated-dialog[open]::backdrop {
  opacity: 1;
}

@starting-style {
  .animated-dialog[open]::backdrop {
    opacity: 0;
  }
}

.dialog-inner {
  padding: 1.75rem;
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.dialog-actions {
  display: flex;
  justify-content: flex-end;
  gap: 0.75rem;
  margin-top: 0.5rem;
}

.btn-ghost {
  background: transparent;
  border: 1px solid var(--color-border);
  color: var(--color-text-muted);
  padding: 0.5rem 1rem;
  border-radius: 6px;
  cursor: pointer;
}

.btn-danger {
  background: #dc2626;
  color: #fff;
  border: none;
  padding: 0.5rem 1rem;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
}

/* ==========================================================================
   5. GPU Shimmer Skeleton
   ========================================================================== */
.skeleton-widget {
  display: flex;
  align-items: center;
  gap: 1rem;
}

.sk-shape {
  background: #334155;
  border-radius: 4px;
  position: relative;
  overflow: hidden;
}

.sk-shape::after {
  content: "";
  position: absolute;
  inset: 0;
  transform: translateX(-100%);
  background: linear-gradient(
    90deg,
    transparent 0%,
    rgba(255, 255, 255, 0.15) 50%,
    transparent 100%
  );
  animation: sk-shimmer 1.5s infinite;
}

@keyframes sk-shimmer {
  to { transform: translateX(100%); }
}

.sk-avatar {
  width: 48px;
  height: 48px;
  border-radius: 50%;
  flex-shrink: 0;
}

.sk-meta {
  flex: 1;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.sk-line {
  height: 12px;
}

.sk-line.title { width: 70%; height: 14px; }
.sk-line.subtitle { width: 45%; }

/* Reduced Motion Override */
@media (prefers-reduced-motion: reduce) {
  .btn-tactile,
  .floating-group label,
  .animated-dialog {
    transition: none !important;
  }
  .sk-shape::after {
    display: none;
  }
  .sk-shape {
    animation: sk-pulse 2s ease-in-out infinite;
  }
  @keyframes sk-pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.4; }
  }
}
```
