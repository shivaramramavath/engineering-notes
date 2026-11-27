# Production Implementation: Enterprise Design Tokens and Theme Switcher

```html
<!DOCTYPE html>
<html lang="en" data-brand="alpha" data-theme="system">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Enterprise Token Architecture &amp; Theme Studio</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>
  <div class="studio-wrapper">
    <!-- Theme & Brand Controller Bar -->
    <header class="theme-controller-panel">
      <div class="panel-brand">
        <h2>Design Token Studio</h2>
        <p>Live Multi-Brand &amp; Color Scheme Orchestrator</p>
      </div>

      <div class="controller-actions">
        <!-- Brand Switcher Radios -->
        <fieldset class="control-group">
          <legend>Brand Identity</legend>
          <label><input type="radio" name="brand" value="alpha" checked onchange="document.documentElement.dataset.brand='alpha'"> FinTech Clean</label>
          <label><input type="radio" name="brand" value="beta" onchange="document.documentElement.dataset.brand='beta'"> Cyberpunk</label>
          <label><input type="radio" name="brand" value="gamma" onchange="document.documentElement.dataset.brand='gamma'"> Editorial</label>
        </fieldset>

        <!-- Color Mode Switcher -->
        <fieldset class="control-group">
          <legend>Color Mode</legend>
          <label><input type="radio" name="theme" value="system" checked onchange="document.documentElement.dataset.theme='system'"> Auto</label>
          <label><input type="radio" name="theme" value="light" onchange="document.documentElement.dataset.theme='light'"> Light</label>
          <label><input type="radio" name="theme" value="dark" onchange="document.documentElement.dataset.theme='dark'"> Dark</label>
        </fieldset>
      </div>
    </header>

    <!-- UI Component Sandbox -->
    <main class="component-showcase">
      <!-- 1. Card Component -->
      <article class="ui-card">
        <div class="card-header">
          <span class="card-kicker">Security Infrastructure</span>
          <h3 class="card-title">Hardware Security Modules (HSM)</h3>
        </div>
        <p class="card-body">
          FIPS 140-3 Level 4 physical cryptographic boundary managing automated root certificate authority rotation across multi-cloud regions.
        </p>
        <div class="card-footer">
          <button type="button" class="btn btn-primary">Deploy HSM Key</button>
          <button type="button" class="btn btn-secondary">Documentation</button>
        </div>
      </article>

      <!-- 2. Form Input Suite -->
      <section class="ui-form-card">
        <h3>Identity Verification</h3>
        <div class="input-field">
          <label for="tenant-id">Tenant ID</label>
          <input type="text" id="tenant-id" value="org_us_prod_99401" readonly>
        </div>
        <div class="input-field">
          <label for="cluster-token">Access Token (Rotated)</label>
          <input type="password" id="cluster-token" value="secret_token_12345" readonly>
        </div>
        <div class="toggle-row">
          <label class="ui-switch">
            <input type="checkbox" checked role="switch">
            <span class="switch-slider"></span>
            <span>Enforce Mutual TLS 1.3</span>
          </label>
        </div>
      </section>
    </main>
  </div>
</body>
</html>
```

```css
/* ==========================================================================
   Tier 1: Global Primitive Tokens
   ========================================================================== */
@property --brand-color {
  syntax: "<color>";
  inherits: true;
  initial-value: oklch(55% 0.22 260);
}

:root {
  /* Primitive Spacing */
  --space-1: 0.25rem;
  --space-2: 0.5rem;
  --space-3: 0.75rem;
  --space-4: 1rem;
  --space-6: 1.5rem;
  --space-8: 2rem;

  /* Primitive OKLCH Palette */
  --palette-blue: oklch(55% 0.22 260);
  --palette-green: oklch(62% 0.24 145);
  --palette-ochre: oklch(58% 0.18 45);
  
  --palette-neutral-50: oklch(98% 0.005 240);
  --palette-neutral-900: oklch(14% 0.03 260);
  --palette-dark-surface: oklch(18% 0.02 260);
  --palette-dark-border: oklch(28% 0.03 260);
}

/* ==========================================================================
   Tier 2: Brand Variations (Semantic Tokens)
   ========================================================================== */
/* Brand Alpha: FinTech Clean */
[data-brand="alpha"] {
  --brand-color: var(--palette-blue);
  --font-family-heading: system-ui, -apple-system, sans-serif;
  --radius-component: 8px;
  --border-width: 1px;
}

/* Brand Beta: Cyberpunk High-Contrast */
[data-brand="beta"] {
  --brand-color: var(--palette-green);
  --font-family-heading: ui-monospace, "SF Mono", monospace;
  --radius-component: 0px;
  --border-width: 2px;
}

/* Brand Gamma: Warm Editorial */
[data-brand="gamma"] {
  --brand-color: var(--palette-ochre);
  --font-family-heading: Georgia, "Times New Roman", serif;
  --radius-component: 16px;
  --border-width: 1px;
}

/* Mode Adaptations */
:root {
  color-scheme: light dark;
  --color-canvas: light-dark(var(--palette-neutral-50), #080c14);
  --color-surface: light-dark(#ffffff, var(--palette-dark-surface));
  --color-border: light-dark(#e2e8f0, var(--palette-dark-border));
  --color-text-main: light-dark(#0f172a, #f8fafc);
  --color-text-muted: light-dark(#64748b, #94a3b8);
}

/* Explicit Overrides when user forces theme radio */
[data-theme="light"] {
  color-scheme: light;
  --color-canvas: var(--palette-neutral-50);
  --color-surface: #ffffff;
  --color-border: #e2e8f0;
  --color-text-main: #0f172a;
  --color-text-muted: #64748b;
}

[data-theme="dark"] {
  color-scheme: dark;
  --color-canvas: #080c14;
  --color-surface: var(--palette-dark-surface);
  --color-border: var(--palette-dark-border);
  --color-text-main: #f8fafc;
  --color-text-muted: #94a3b8;
}

/* ==========================================================================
   Base Layout
   ========================================================================== */
*, *::before, *::after {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

body {
  font-family: system-ui, -apple-system, sans-serif;
  background-color: var(--color-canvas);
  color: var(--color-text-main);
  padding: var(--space-6);
  min-height: 100dvh;
  transition: background-color 200ms ease, color 200ms ease;
}

.studio-wrapper {
  max-width: 960px;
  margin: 0 auto;
}

/* ==========================================================================
   Controller Bar
   ========================================================================== */
.theme-controller-panel {
  background: var(--color-surface);
  border: var(--border-width) solid var(--color-border);
  border-radius: var(--radius-component);
  padding: var(--space-6);
  margin-bottom: var(--space-8);
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: var(--space-4);
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.05);
}

.panel-brand h2 {
  font-family: var(--font-family-heading);
  font-size: 1.5rem;
  letter-spacing: -0.02em;
}

.panel-brand p {
  color: var(--color-text-muted);
  font-size: 0.875rem;
}

.controller-actions {
  display: flex;
  gap: var(--space-6);
}

.control-group {
  border: 1px solid var(--color-border);
  padding: 0.5rem 1rem;
  border-radius: var(--radius-component);
  display: flex;
  gap: 0.75rem;
  font-size: 0.875rem;
}

.control-group legend {
  font-weight: 600;
  padding: 0 0.35rem;
  color: var(--color-text-muted);
  font-size: 0.75rem;
  text-transform: uppercase;
}

.control-group label {
  display: flex;
  align-items: center;
  gap: 0.35rem;
  cursor: pointer;
}

/* ==========================================================================
   Tier 3: Component Token Usage
   ========================================================================== */
.component-showcase {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: var(--space-6);
}

.ui-card,
.ui-form-card {
  background: var(--color-surface);
  border: var(--border-width) solid var(--color-border);
  border-radius: var(--radius-component);
  padding: var(--space-6);
  display: flex;
  flex-direction: column;
  gap: var(--space-4);
  transition: border-color 200ms ease;
}

.ui-card:hover {
  border-color: var(--brand-color);
}

.card-kicker {
  font-size: 0.75rem;
  font-weight: 700;
  text-transform: uppercase;
  color: var(--brand-color);
  letter-spacing: 0.05em;
}

.card-title,
.ui-form-card h3 {
  font-family: var(--font-family-heading);
  font-size: 1.25rem;
  line-height: 1.3;
}

.card-body {
  color: var(--color-text-muted);
  font-size: 0.95rem;
  line-height: 1.5;
  flex: 1;
}

.card-footer {
  display: flex;
  gap: var(--space-3);
}

/* Buttons */
.btn {
  padding: 0.625rem 1.25rem;
  border-radius: var(--radius-component);
  font-weight: 600;
  font-size: 0.875rem;
  cursor: pointer;
  border: var(--border-width) solid transparent;
  transition: all 150ms ease;
}

.btn-primary {
  background: var(--brand-color);
  color: #ffffff;
}

.btn-primary:hover {
  filter: brightness(1.1);
}

.btn-secondary {
  background: transparent;
  border-color: var(--color-border);
  color: var(--color-text-main);
}

.btn-secondary:hover {
  background: rgba(0, 0, 0, 0.05);
}

.btn:focus-visible {
  outline: 2px solid var(--brand-color);
  outline-offset: 2px;
}

/* Form Fields */
.input-field {
  display: flex;
  flex-direction: column;
  gap: 0.35rem;
}

.input-field label {
  font-size: 0.8125rem;
  font-weight: 600;
  color: var(--color-text-muted);
}

.input-field input {
  padding: 0.625rem;
  background: var(--color-canvas);
  border: var(--border-width) solid var(--color-border);
  border-radius: var(--radius-component);
  color: var(--color-text-main);
  font-family: var(--font-family-heading);
  outline: none;
}

.input-field input:focus-visible {
  border-color: var(--brand-color);
  outline: 2px solid var(--brand-color);
}

/* Switch */
.ui-switch {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  cursor: pointer;
  font-size: 0.9rem;
  font-weight: 500;
}

.ui-switch input {
  position: absolute;
  opacity: 0;
}

.switch-slider {
  width: 44px;
  height: 24px;
  background: var(--color-border);
  border-radius: var(--radius-component);
  position: relative;
  transition: background-color 200ms ease;
}

.switch-slider::after {
  content: "";
  position: absolute;
  top: 2px;
  left: 2px;
  width: 20px;
  height: 20px;
  background: #ffffff;
  border-radius: var(--radius-component);
  transition: transform 200ms ease;
}

.ui-switch input:checked + .switch-slider {
  background: var(--brand-color);
}

.ui-switch input:checked + .switch-slider::after {
  transform: translateX(20px);
}

.ui-switch input:focus-visible + .switch-slider {
  outline: 2px solid var(--brand-color);
  outline-offset: 2px;
}

@media (max-width: 680px) {
  .component-showcase {
    grid-template-columns: 1fr;
  }
}
```
