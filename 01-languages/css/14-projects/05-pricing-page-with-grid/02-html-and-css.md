# Production Implementation: SaaS Pricing Page with Subgrid

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Pricing Plans - NovaEngine Cloud</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>
  <div class="pricing-container">
    <!-- Header Section -->
    <header class="pricing-header">
      <span class="pricing-kicker">Predictable Cloud Economics</span>
      <h1 class="pricing-headline">Transparent Plans for Every Scale</h1>
      <p class="pricing-subtitle">Start free, build prototypes, and scale seamlessly to hundreds of millions of daily requests.</p>

      <!-- Billing Period Switcher -->
      <div class="billing-toggle-wrapper">
        <span class="toggle-label">Monthly</span>
        <label class="billing-switch">
          <input type="checkbox" id="annual-toggle" class="billing-checkbox" role="switch" aria-label="Toggle annual billing discount">
          <span class="slider"></span>
        </label>
        <span class="toggle-label">Annual <span class="badge-save">Save 20%</span></span>
      </div>
    </header>

    <!-- 3-Tier Subgrid Matrix -->
    <main class="pricing-matrix">
      <!-- 1. Starter Tier -->
      <article class="pricing-card card-starter">
        <div class="tier-header">
          <h2 class="tier-name">Developer</h2>
          <span class="tier-tag">Prototyping &amp; Personal</span>
        </div>
        <p class="tier-desc">For indie hackers and developers building experimental cloud edge microservices.</p>
        <div class="tier-price">
          <span class="currency">$</span>
          <span class="amount monthly">0</span>
          <span class="amount annual">0</span>
          <span class="period">/ month</span>
        </div>
        <ul class="features-list">
          <li>100,000 requests / month</li>
          <li>3 active edge regions</li>
          <li>Community Discord support</li>
          <li>Basic trace metrics (24h retention)</li>
        </ul>
        <div class="tier-cta">
          <a href="#signup-starter" class="btn btn-secondary">Get Started Free</a>
        </div>
      </article>

      <!-- 2. Pro Tier (Popular Highlight) -->
      <article class="pricing-card card-pro popular">
        <div class="popular-ribbon">Most Popular</div>
        <div class="tier-header">
          <h2 class="tier-name">Team Pro</h2>
          <span class="tier-tag">High-Velocity Engineering</span>
        </div>
        <p class="tier-desc">For growing startups and scaling production applications requiring instant multi-region failover.</p>
        <div class="tier-price">
          <span class="currency">$</span>
          <span class="amount monthly">49</span>
          <span class="amount annual">39</span>
          <span class="period">/ seat / mo</span>
        </div>
        <ul class="features-list">
          <li>10,000,000 requests / month</li>
          <li>18 global edge datacenters</li>
          <li>Priority 24/7 Slack &amp; email support</li>
          <li>30-day full OpenTelemetry traces</li>
          <li>Hardware-isolated V8 memory sandboxes</li>
        </ul>
        <div class="tier-cta">
          <a href="#signup-pro" class="btn btn-primary">Start 14-Day Free Trial</a>
        </div>
      </article>

      <!-- 3. Enterprise Tier -->
      <article class="pricing-card card-enterprise">
        <div class="tier-header">
          <h2 class="tier-name">Enterprise</h2>
          <span class="tier-tag">Mission-Critical Scale</span>
        </div>
        <p class="tier-desc">For global enterprises requiring dedicated HSM hardware, zero-trust mesh, and 99.999% SLA.</p>
        <div class="tier-price">
          <span class="currency">$</span>
          <span class="amount monthly">299</span>
          <span class="amount annual">239</span>
          <span class="period">/ month (base)</span>
        </div>
        <ul class="features-list">
          <li>Unlimited monthly requests</li>
          <li>300+ edge points of presence</li>
          <li>Dedicated Technical Account Manager</li>
          <li>Custom BGP routing &amp; dedicated IP ranges</li>
          <li>99.999% uptime financial SLA</li>
          <li>SOC2 Type II &amp; HIPAA compliance reports</li>
        </ul>
        <div class="tier-cta">
          <a href="#contact-sales" class="btn btn-secondary">Contact Sales</a>
        </div>
      </article>
    </main>
  </div>
</body>
</html>
```

```css
/* ==========================================================================
   Design Tokens & Canvas
   ========================================================================== */
:root {
  --bg-canvas: #0f172a;
  --bg-card: #1e293b;
  --bg-card-popular: #1e293b;
  --border-card: #334155;
  --border-popular: #38bdf8;
  --text-main: #f8fafc;
  --text-muted: #94a3b8;
  --accent-brand: #38bdf8;
  --accent-brand-hover: #0ea5e9;
  --font-sans: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  --font-mono: ui-monospace, "SF Mono", monospace;
}

*, *::before, *::after {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

body {
  font-family: var(--font-sans);
  background-color: var(--bg-canvas);
  color: var(--text-main);
  line-height: 1.5;
  padding: 3rem 1.5rem;
  -webkit-font-smoothing: antialiased;
}

.pricing-container {
  max-width: 1200px;
  margin: 0 auto;
}

/* ==========================================================================
   Header Section
   ========================================================================== */
.pricing-header {
  text-align: center;
  max-width: 720px;
  margin: 0 auto 3.5rem auto;
}

.pricing-kicker {
  font-size: 0.8125rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--accent-brand);
  display: block;
  margin-bottom: 0.75rem;
}

.pricing-headline {
  font-size: clamp(2rem, 1.5rem + 2.5vw, 3.5rem);
  font-weight: 800;
  line-height: 1.1;
  letter-spacing: -0.03em;
  margin-bottom: 1rem;
}

.pricing-subtitle {
  font-size: 1.125rem;
  color: var(--text-muted);
  line-height: 1.6;
}

/* Billing Toggle */
.billing-toggle-wrapper {
  display: inline-flex;
  align-items: center;
  gap: 1rem;
  margin-top: 2rem;
  background: rgba(30, 41, 59, 0.6);
  padding: 0.5rem 1.25rem;
  border-radius: 9999px;
  border: 1px solid var(--border-card);
}

.toggle-label {
  font-size: 0.95rem;
  font-weight: 600;
}

.badge-save {
  background: rgba(16, 185, 129, 0.2);
  color: #34d399;
  font-size: 0.75rem;
  font-weight: 700;
  padding: 0.15rem 0.5rem;
  border-radius: 9999px;
  margin-left: 0.25rem;
}

.billing-switch {
  position: relative;
  display: inline-block;
  width: 44px;
  height: 24px;
}

.billing-switch input {
  opacity: 0;
  width: 0;
  height: 0;
}

.slider {
  position: absolute;
  inset: 0;
  background-color: #475569;
  border-radius: 9999px;
  cursor: pointer;
  transition: background-color 200ms ease;
}

.slider::before {
  content: "";
  position: absolute;
  top: 2px;
  left: 2px;
  width: 20px;
  height: 20px;
  background-color: #ffffff;
  border-radius: 50%;
  transition: transform 200ms cubic-bezier(0.16, 1, 0.3, 1);
}

.billing-switch input:checked + .slider {
  background-color: var(--accent-brand);
}

.billing-switch input:checked + .slider::before {
  transform: translateX(20px);
}

.billing-switch input:focus-visible + .slider {
  outline: 2px solid var(--accent-brand);
  outline-offset: 2px;
}

/* Toggle Annual/Monthly Price Displays via CSS :has() */
.pricing-container:has(#annual-toggle:checked) .amount.monthly {
  display: none;
}
.pricing-container:not(:has(#annual-toggle:checked)) .amount.annual {
  display: none;
}

/* ==========================================================================
   CSS Subgrid Pricing Matrix
   ========================================================================== */
.pricing-matrix {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  /* 5 Rows: Header, Description, Price, Features, CTA */
  grid-template-rows: auto auto auto 1fr auto;
  gap: 1.5rem;
  align-items: stretch;
}

.pricing-card {
  display: grid;
  grid-row: span 5;
  grid-template-rows: subgrid;
  background: var(--bg-card);
  border: 1px solid var(--border-card);
  border-radius: 16px;
  padding: 2.25rem 1.75rem;
  position: relative;
  transition: transform 200ms ease, border-color 200ms ease;
}

.pricing-card:hover {
  transform: translateY(-4px);
  border-color: #64748b;
}

/* Pro / Popular Card Elevation */
.pricing-card.popular {
  border-color: var(--border-popular);
  box-shadow: 0 0 35px -5px rgba(56, 189, 248, 0.2);
}

.popular-ribbon {
  position: absolute;
  top: -12px;
  left: 50%;
  transform: translateX(-50%);
  background: var(--accent-brand);
  color: #0f172a;
  font-size: 0.75rem;
  font-weight: 800;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  padding: 0.25rem 0.875rem;
  border-radius: 9999px;
}

/* Subgrid Row 1: Header */
.tier-header {
  margin-bottom: 0.5rem;
}

.tier-name {
  font-size: 1.5rem;
  font-weight: 800;
  margin-bottom: 0.25rem;
}

.tier-tag {
  font-size: 0.8125rem;
  color: var(--text-muted);
  font-weight: 500;
}

/* Subgrid Row 2: Description */
.tier-desc {
  font-size: 0.95rem;
  color: var(--text-muted);
  line-height: 1.5;
  margin-bottom: 1.5rem;
}

/* Subgrid Row 3: Price */
.tier-price {
  display: flex;
  align-items: baseline;
  gap: 0.25rem;
  margin-bottom: 2rem;
  padding-bottom: 1.5rem;
  border-bottom: 1px solid var(--border-card);
}

.tier-price .currency {
  font-size: 1.5rem;
  font-weight: 700;
  color: var(--text-muted);
}

.tier-price .amount {
  font-size: 3rem;
  font-weight: 800;
  line-height: 1;
  font-variant-numeric: tabular-nums;
}

.tier-price .period {
  font-size: 0.875rem;
  color: var(--text-muted);
}

/* Subgrid Row 4: Features */
.features-list {
  list-style: none;
  display: flex;
  flex-direction: column;
  gap: 0.875rem;
  margin-bottom: 2rem;
}

.features-list li {
  position: relative;
  padding-left: 1.5rem;
  font-size: 0.9rem;
  color: #cbd5e1;
  line-height: 1.4;
}

.features-list li::before {
  content: "✓";
  position: absolute;
  left: 0;
  top: 0;
  color: var(--accent-brand);
  font-weight: 800;
}

/* Subgrid Row 5: CTA Button */
.tier-cta {
  display: flex;
}

.btn {
  width: 100%;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 0.875rem 1.5rem;
  border-radius: 8px;
  font-weight: 700;
  font-size: 0.95rem;
  text-decoration: none;
  cursor: pointer;
  transition: all 150ms ease;
}

.btn-primary {
  background: var(--accent-brand);
  color: #0f172a;
  border: 1px solid var(--accent-brand);
}

.btn-primary:hover {
  background: var(--accent-brand-hover);
}

.btn-secondary {
  background: #334155;
  color: #ffffff;
  border: 1px solid #475569;
}

.btn-secondary:hover {
  background: #475569;
}

.btn:focus-visible {
  outline: 2px solid var(--accent-brand);
  outline-offset: 2px;
}

/* ==========================================================================
   Responsive Viewport Adaptations
   ========================================================================== */
@media (max-width: 960px) {
  .pricing-matrix {
    grid-template-columns: 1fr;
    grid-template-rows: auto;
  }
  .pricing-card {
    grid-row: span 1;
    display: flex;
    flex-direction: column;
    margin-bottom: 1.5rem;
  }
}
```
