# Production Implementation: Minimalist Engineer Bento Portfolio

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Alex Rivera - Principal Systems Architect</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>
  <div class="portfolio-wrapper">
    <!-- Top Identity Header -->
    <header class="portfolio-header">
      <div class="header-content">
        <h1 class="engineer-name">Alex Rivera</h1>
        <p class="engineer-title">Principal Systems Architect &amp; Distributed Infrastructure Specialist</p>
      </div>
      <div class="header-actions no-print">
        <button type="button" class="btn btn-print" onclick="window.print()">Print Resume (PDF)</button>
      </div>
    </header>

    <!-- Main Bento Grid -->
    <main class="bento-container" id="bento-main">
      <!-- 1. Bio & Overview (2x2 Span on Desktop) -->
      <article class="bento-card card-bio">
        <div class="card-badge">Biography</div>
        <h2 class="card-headline">Building fault-tolerant storage, consensus algorithms, and ultra-low-latency edge runtimes.</h2>
        <p class="bio-text">
          Over 12 years engineering mission-critical cloud backends, Linux kernel networking layers, and globally distributed databases. Former Tech Lead at Cloudscale Infrastructure.
        </p>
        <div class="bio-metrics">
          <div class="bio-stat">
            <span class="stat-number">12+</span>
            <span class="stat-caption">Years Exp</span>
          </div>
          <div class="bio-stat">
            <span class="stat-number">40M+</span>
            <span class="stat-caption">Peak QPS Handled</span>
          </div>
          <div class="bio-stat">
            <span class="stat-number">99.999%</span>
            <span class="stat-caption">Production SLA</span>
          </div>
        </div>
      </article>

      <!-- 2. Tech Stack Pill Matrix -->
      <article class="bento-card card-tech">
        <div class="card-badge">Core Technologies</div>
        <ul class="tech-pills">
          <li>Rust</li>
          <li>Go</li>
          <li>TypeScript</li>
          <li>eBPF</li>
          <li>Raft Consensus</li>
          <li>PostgreSQL</li>
          <li>Kubernetes</li>
          <li>Linux Internals</li>
        </ul>
      </article>

      <!-- 3. Availability Status -->
      <article class="bento-card card-status">
        <div class="card-badge">Status</div>
        <div class="status-indicator">
          <span class="status-pulse" aria-hidden="true"></span>
          <span class="status-label">Advising &amp; Architecture Consulting</span>
        </div>
        <span class="status-location">Based in San Francisco, CA (Remote Global)</span>
      </article>

      <!-- 4. Writing & Research -->
      <article class="bento-card card-writing">
        <div class="card-badge">Selected Publications</div>
        <ul class="writing-list">
          <li>
            <a href="#raft-paper" class="writing-item">
              <span class="writing-title">Optimizing Raft Heartbeats in High-Jitter WANs</span>
              <span class="writing-date">2026</span>
            </a>
          </li>
          <li>
            <a href="#ebpf-routing" class="writing-item">
              <span class="writing-title">Zero-Copy Network Telemetry via eBPF XDP</span>
              <span class="writing-date">2025</span>
            </a>
          </li>
        </ul>
      </article>

      <!-- 5. Featured Architecture Project -->
      <article class="bento-card card-featured">
        <div class="card-badge">Featured System</div>
        <h3 class="project-title">AetherDB: Distributed Consensus Engine</h3>
        <p class="project-summary">
          An open-source, multi-raft distributed relational storage engine achieving 150,000 deterministic transactions per second across 7 geographical regions.
        </p>
        <div class="project-tags">
          <span class="tag">Rust</span>
          <span class="tag">Distributed Systems</span>
          <span class="tag">Apache 2.0</span>
        </div>
      </article>

      <!-- 6. Connect / Public Channels -->
      <article class="bento-card card-contact">
        <div class="card-badge">Contact &amp; Identity</div>
        <div class="contact-links">
          <a href="mailto:alex@rivera-systems.com" class="contact-row">
            <span class="contact-type">Email:</span>
            <span class="contact-val">alex@rivera-systems.com</span>
          </a>
          <a href="https://github.com" class="contact-row">
            <span class="contact-type">GitHub:</span>
            <span class="contact-val">@arivera-arch</span>
          </a>
          <div class="contact-row pgp-row">
            <span class="contact-type">PGP Fingerprint:</span>
            <span class="contact-val mono">8A20 44EF 9021 B71C</span>
          </div>
        </div>
      </article>
    </main>

    <!-- Simple Footer -->
    <footer class="portfolio-footer">
      <p>© 2026 Alex Rivera. Built with modern CSS Grid, Container Queries, and zero JavaScript libraries.</p>
    </footer>
  </div>
</body>
</html>
```

```css
/* ==========================================================================
   Design Tokens & System Color Theming
   ========================================================================== */
:root {
  color-scheme: light dark;
  --bg-page: light-dark(#f8fafc, #090d16);
  --bg-card: light-dark(#ffffff, #111827);
  --border-card: light-dark(#e2e8f0, #1f2937);
  --text-main: light-dark(#0f172a, #f9fafb);
  --text-sub: light-dark(#64748b, #9ca3af);
  --accent: light-dark(#2563eb, #38bdf8);
  --accent-surface: light-dark(#eff6ff, rgba(56, 189, 248, 0.1));
  --font-sans: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  --font-mono: ui-monospace, "SF Mono", Menlo, Consolas, monospace;
}

*, *::before, *::after {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

body {
  font-family: var(--font-sans);
  background-color: var(--bg-page);
  color: var(--text-main);
  line-height: 1.5;
  padding: 2rem 1rem;
  -webkit-font-smoothing: antialiased;
}

.portfolio-wrapper {
  max-width: 1140px;
  margin: 0 auto;
}

/* ==========================================================================
   Header
   ========================================================================== */
.portfolio-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-end;
  margin-bottom: 2.5rem;
  padding-bottom: 1.5rem;
  border-bottom: 1px solid var(--border-card);
}

.engineer-name {
  font-size: clamp(2rem, 1.5rem + 2vw, 3rem);
  font-weight: 800;
  letter-spacing: -0.03em;
  line-height: 1.1;
  color: var(--text-main);
}

.engineer-title {
  font-size: 1.125rem;
  color: var(--text-sub);
  margin-top: 0.5rem;
}

.btn-print {
  background: var(--bg-card);
  border: 1px solid var(--border-card);
  color: var(--text-main);
  padding: 0.5rem 1rem;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
  font-size: 0.875rem;
  transition: all 150ms ease;
}

.btn-print:hover {
  border-color: var(--accent);
  color: var(--accent);
}

.btn-print:focus-visible {
  outline: 2px solid var(--accent);
  outline-offset: 2px;
}

/* ==========================================================================
   Bento Grid Core Layout
   ========================================================================== */
.bento-container {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  grid-auto-rows: minmax(180px, auto);
  gap: 1.25rem;
}

.bento-card {
  background: var(--bg-card);
  border: 1px solid var(--border-card);
  border-radius: 14px;
  padding: 1.75rem;
  display: flex;
  flex-direction: column;
  position: relative;
  transition: transform 200ms cubic-bezier(0.16, 1, 0.3, 1),
              box-shadow 200ms cubic-bezier(0.16, 1, 0.3, 1),
              border-color 200ms ease;
}

.bento-card:hover {
  transform: translateY(-3px);
  border-color: var(--accent);
  box-shadow: 0 12px 24px -6px rgba(0, 0, 0, 0.12);
}

.card-badge {
  font-size: 0.75rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  color: var(--accent);
  margin-bottom: 1rem;
}

/* ==========================================================================
   Bento Grid Area Spans
   ========================================================================== */
/* 1. Bio spans 2 columns, 2 rows */
.card-bio {
  grid-column: span 2;
  grid-row: span 2;
  justify-content: space-between;
}

.card-headline {
  font-size: clamp(1.25rem, 1rem + 1vw, 1.75rem);
  font-weight: 700;
  line-height: 1.25;
  margin-bottom: 1rem;
  color: var(--text-main);
  letter-spacing: -0.02em;
}

.bio-text {
  color: var(--text-sub);
  font-size: 1rem;
  line-height: 1.6;
  margin-bottom: 1.5rem;
}

.bio-metrics {
  display: flex;
  gap: 2rem;
  padding-top: 1.25rem;
  border-top: 1px solid var(--border-card);
}

.bio-stat {
  display: flex;
  flex-direction: column;
}

.stat-number {
  font-size: 1.5rem;
  font-weight: 800;
  color: var(--text-main);
  font-variant-numeric: tabular-nums;
}

.stat-caption {
  font-size: 0.75rem;
  color: var(--text-sub);
  font-weight: 500;
}

/* 2. Tech Stack */
.card-tech {
  grid-column: span 1;
}

.tech-pills {
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  list-style: none;
}

.tech-pills li {
  padding: 0.35rem 0.65rem;
  border-radius: 6px;
  background: var(--accent-surface);
  color: var(--accent);
  font-size: 0.8125rem;
  font-weight: 600;
  font-family: var(--font-mono);
}

/* 3. Status */
.card-status {
  grid-column: span 1;
  justify-content: space-between;
}

.status-indicator {
  display: flex;
  align-items: center;
  gap: 0.625rem;
}

.status-pulse {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: #10b981;
  box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.7);
  animation: pulse-ring 2s infinite;
}

@keyframes pulse-ring {
  0% { box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.7); }
  70% { box-shadow: 0 0 0 8px rgba(16, 185, 129, 0); }
  100% { box-shadow: 0 0 0 0 rgba(16, 185, 129, 0); }
}

.status-label {
  font-weight: 600;
  font-size: 0.95rem;
  color: var(--text-main);
}

.status-location {
  font-size: 0.8125rem;
  color: var(--text-sub);
}

/* 4. Writing */
.card-writing {
  grid-column: span 2;
}

.writing-list {
  list-style: none;
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}

.writing-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  text-decoration: none;
  padding: 0.5rem 0;
  border-bottom: 1px solid var(--border-card);
}

.writing-title {
  font-size: 0.95rem;
  font-weight: 500;
  color: var(--text-main);
  transition: color 150ms ease;
}

.writing-item:hover .writing-title {
  color: var(--accent);
}

.writing-date {
  font-size: 0.8125rem;
  color: var(--text-sub);
  font-family: var(--font-mono);
}

/* 5. Featured Project */
.card-featured {
  grid-column: span 2;
}

.project-title {
  font-size: 1.25rem;
  font-weight: 700;
  margin-bottom: 0.5rem;
  color: var(--text-main);
}

.project-summary {
  font-size: 0.95rem;
  color: var(--text-sub);
  line-height: 1.5;
  margin-bottom: 1.25rem;
}

.project-tags {
  display: flex;
  gap: 0.5rem;
}

.tag {
  font-size: 0.75rem;
  font-family: var(--font-mono);
  padding: 0.2rem 0.5rem;
  border: 1px solid var(--border-card);
  border-radius: 4px;
  color: var(--text-sub);
}

/* 6. Contact */
.card-contact {
  grid-column: span 2;
}

.contact-links {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.contact-row {
  display: flex;
  gap: 0.75rem;
  text-decoration: none;
  font-size: 0.9rem;
  color: var(--text-main);
}

.contact-type {
  color: var(--text-sub);
  font-weight: 500;
  width: 120px;
}

.contact-val {
  font-weight: 600;
}

.contact-val.mono {
  font-family: var(--font-mono);
}

/* ==========================================================================
   Footer
   ========================================================================== */
.portfolio-footer {
  margin-top: 3rem;
  padding-top: 1.5rem;
  border-top: 1px solid var(--border-card);
  text-align: center;
  font-size: 0.8125rem;
  color: var(--text-sub);
}

/* ==========================================================================
   Responsive Adaptations
   ========================================================================== */
@media (max-width: 1024px) {
  .bento-container {
    grid-template-columns: repeat(2, 1fr);
  }
  .card-tech,
  .card-status {
    grid-column: span 1;
  }
}

@media (max-width: 640px) {
  .portfolio-header {
    flex-direction: column;
    align-items: flex-start;
    gap: 1rem;
  }
  .bento-container {
    grid-template-columns: 1fr;
  }
  .card-bio,
  .card-tech,
  .card-status,
  .card-writing,
  .card-featured,
  .card-contact {
    grid-column: span 1;
    grid-row: span 1;
  }
  .bio-metrics {
    flex-direction: column;
    gap: 0.75rem;
  }
}

/* ==========================================================================
   Dedicated Print Resume Stylesheet
   ========================================================================== */
@media print {
  body {
    background: #ffffff !important;
    color: #000000 !important;
    padding: 0 !important;
    font-size: 10pt;
  }
  .no-print {
    display: none !important;
  }
  .portfolio-header {
    border-bottom: 2pt solid #000 !important;
    margin-bottom: 1rem !important;
  }
  .bento-container {
    display: block !important;
  }
  .bento-card {
    border: none !important;
    box-shadow: none !important;
    padding: 0.5rem 0 !important;
    margin-bottom: 1rem !important;
    page-break-inside: avoid;
  }
  .card-badge {
    color: #000 !important;
    border-bottom: 1pt solid #ccc;
    padding-bottom: 2pt;
  }
}
```
