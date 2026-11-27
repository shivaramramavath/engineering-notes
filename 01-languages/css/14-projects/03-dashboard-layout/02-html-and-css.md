# Production Implementation: Cloud Operations Dashboard Shell

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>NexusOps - Global Cloud Telemetry</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>
  <div class="dash-shell">
    <!-- Top Application Bar -->
    <header class="dash-header">
      <div class="header-left">
        <button type="button" class="mobile-toggle" popovertarget="sidebar-drawer" aria-label="Toggle navigation menu">☰</button>
        <span class="brand-name">NexusOps</span>
        <span class="badge-env">Production</span>
      </div>

      <div class="header-search">
        <span class="search-icon" aria-hidden="true">🔍</span>
        <input type="search" placeholder="Search clusters, nodes, or pods (Ctrl+K)..." aria-label="Search resources">
      </div>

      <div class="header-right">
        <button type="button" class="btn-icon" aria-label="Notifications (3 unread)">🔔<span class="dot-unread"></span></button>
        <div class="user-profile">
          <span class="user-avatar" aria-hidden="true">JD</span>
          <span class="user-name">Jane Doe</span>
        </div>
      </div>
    </header>

    <!-- Sidebar Navigation Rail -->
    <aside id="sidebar-drawer" class="dash-sidebar" popover="auto">
      <nav class="sidebar-nav" aria-label="Primary Navigation">
        <div class="nav-section">
          <span class="section-label">Fleet</span>
          <a href="#clusters" class="nav-item active"><span class="icon">☸</span><span class="label">Clusters</span></a>
          <a href="#nodes" class="nav-item"><span class="icon">🖥</span><span class="label">Nodes</span></a>
          <a href="#workloads" class="nav-item"><span class="icon">📦</span><span class="label">Workloads</span></a>
        </div>
        <div class="nav-section">
          <span class="section-label">Observability</span>
          <a href="#metrics" class="nav-item"><span class="icon">📊</span><span class="label">Metrics</span></a>
          <a href="#traces" class="nav-item"><span class="icon">⚡</span><span class="label">Traces</span></a>
          <a href="#logs" class="nav-item"><span class="icon">📜</span><span class="label">Audit Logs</span></a>
        </div>
      </nav>
    </aside>

    <!-- Main Content Canvas -->
    <main class="dash-canvas">
      <div class="canvas-header">
        <div>
          <h1 class="page-title">Cluster Infrastructure Fleet</h1>
          <p class="page-subtitle">Real-time health telemetry across 8 active cloud regions.</p>
        </div>
        <div class="canvas-actions">
          <button type="button" class="btn btn-secondary">Export Metrics</button>
          <button type="button" class="btn btn-primary">+ Provision Cluster</button>
        </div>
      </div>

      <!-- 4-Column Subgrid Metric Ribbon -->
      <section class="metrics-ribbon" aria-label="Fleet Key Performance Indicators">
        <article class="metric-card">
          <span class="metric-title">Total Active Nodes</span>
          <span class="metric-value">1,420</span>
          <span class="metric-trend positive">+12.4% vs last week</span>
        </article>

        <article class="metric-card">
          <span class="metric-title">Avg CPU Saturation</span>
          <span class="metric-value">46.2%</span>
          <span class="metric-trend neutral">Stable across fleet</span>
        </article>

        <article class="metric-card">
          <span class="metric-title">Ingress Throughput</span>
          <span class="metric-value">48.6 <small>Gbps</small></span>
          <span class="metric-trend positive">+4.2 Gbps spike</span>
        </article>

        <article class="metric-card">
          <span class="metric-title">Unhealthy Pods</span>
          <span class="metric-value text-warning">2</span>
          <span class="metric-trend negative">eu-west-02 node-4</span>
        </article>
      </section>

      <!-- Scrollable Data Table with Freeze Panes -->
      <section class="table-card" aria-label="Active Clusters">
        <div class="table-card-header">
          <h2>Active Kubernetes Clusters</h2>
          <span class="table-count">Showing 4 of 4 clusters</span>
        </div>

        <div class="table-scroll-wrapper" tabindex="0" role="region" aria-label="Clusters telemetry table">
          <table class="data-table">
            <thead>
              <tr>
                <th scope="col" class="sticky-freeze-corner">Cluster Identifier</th>
                <th scope="col">Region</th>
                <th scope="col">Node Count</th>
                <th scope="col">CPU Utilization</th>
                <th scope="col">Memory Usage</th>
                <th scope="col">Version</th>
                <th scope="col">Health Status</th>
              </tr>
            </thead>
            <tbody>
              <tr>
                <th scope="row" class="sticky-col-header">us-east-prod-alpha</th>
                <td>Virginia (us-east-1)</td>
                <td>320</td>
                <td>54.2%</td>
                <td>68.1%</td>
                <td>v1.30.2</td>
                <td><span class="status-pill healthy">Healthy</span></td>
              </tr>
              <tr>
                <th scope="row" class="sticky-col-header">eu-west-prod-beta</th>
                <td>Frankfurt (eu-central-1)</td>
                <td>280</td>
                <td>82.4%</td>
                <td>89.0%</td>
                <td>v1.30.2</td>
                <td><span class="status-pill warning">Degraded</span></td>
              </tr>
              <tr>
                <th scope="row" class="sticky-col-header">ap-northeast-prod</th>
                <td>Tokyo (ap-northeast-1)</td>
                <td>410</td>
                <td>39.1%</td>
                <td>51.4%</td>
                <td>v1.29.5</td>
                <td><span class="status-pill healthy">Healthy</span></td>
              </tr>
              <tr>
                <th scope="row" class="sticky-col-header">sa-east-prod</th>
                <td>São Paulo (sa-east-1)</td>
                <td>120</td>
                <td>28.5%</td>
                <td>44.2%</td>
                <td>v1.30.2</td>
                <td><span class="status-pill healthy">Healthy</span></td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>
    </main>
  </div>
</body>
</html>
```

```css
/* ==========================================================================
   Design Tokens & Root Dashboard Shell
   ========================================================================== */
:root {
  --surface-canvas: #0b0f19;
  --surface-panel: #111827;
  --surface-sidebar: #070a11;
  --surface-header: #0f172a;
  --border-subtle: #1f2937;
  --text-primary: #f9fafb;
  --text-muted: #9ca3af;
  --brand-primary: #3b82f6;
  --status-healthy: #10b981;
  --status-warning: #f59e0b;
  
  --header-height: 64px;
  --sidebar-width: 250px;
  --font-sans: system-ui, -apple-system, sans-serif;
  --font-mono: ui-monospace, "SF Mono", monospace;
}

*, *::before, *::after {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

body {
  font-family: var(--font-sans);
  background-color: var(--surface-canvas);
  color: var(--text-primary);
  height: 100dvh;
  width: 100vw;
  overflow: hidden;
}

/* 2-Tier Application Shell */
.dash-shell {
  display: grid;
  grid-template-rows: var(--header-height) 1fr;
  grid-template-columns: var(--sidebar-width) 1fr;
  grid-template-areas:
    "header  header"
    "sidebar content";
  height: 100dvh;
  width: 100vw;
  overflow: hidden;
}

/* ==========================================================================
   Top Header Bar
   ========================================================================== */
.dash-header {
  grid-area: header;
  background: var(--surface-header);
  border-bottom: 1px solid var(--border-subtle);
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 1.5rem;
  z-index: 50;
}

.header-left {
  display: flex;
  align-items: center;
  gap: 1rem;
}

.mobile-toggle {
  display: none;
  background: none;
  border: none;
  color: var(--text-primary);
  font-size: 1.5rem;
  cursor: pointer;
}

.brand-name {
  font-weight: 800;
  font-size: 1.25rem;
  letter-spacing: -0.02em;
}

.badge-env {
  font-size: 0.75rem;
  font-weight: 700;
  background: #1e3a8a;
  color: #93c5fd;
  padding: 0.2rem 0.5rem;
  border-radius: 4px;
  text-transform: uppercase;
}

.header-search {
  display: flex;
  align-items: center;
  background: #1e293b;
  border: 1px solid var(--border-subtle);
  border-radius: 8px;
  padding: 0.4rem 0.875rem;
  width: 100%;
  max-width: 420px;
  gap: 0.5rem;
}

.header-search input {
  background: transparent;
  border: none;
  color: var(--text-primary);
  font-size: 0.875rem;
  width: 100%;
  outline: none;
}

.header-search input:focus-visible {
  outline: 2px solid var(--brand-primary);
  border-radius: 4px;
}

.header-right {
  display: flex;
  align-items: center;
  gap: 1.25rem;
}

.btn-icon {
  background: none;
  border: none;
  font-size: 1.2rem;
  color: var(--text-muted);
  cursor: pointer;
  position: relative;
}

.dot-unread {
  position: absolute;
  top: 0;
  right: 0;
  width: 8px;
  height: 8px;
  background: #ef4444;
  border-radius: 50%;
}

.user-profile {
  display: flex;
  align-items: center;
  gap: 0.625rem;
}

.user-avatar {
  width: 32px;
  height: 32px;
  border-radius: 50%;
  background: #3b82f6;
  color: #ffffff;
  display: grid;
  place-content: center;
  font-size: 0.75rem;
  font-weight: 700;
}

.user-name {
  font-size: 0.875rem;
  font-weight: 600;
}

/* ==========================================================================
   Sidebar Navigation
   ========================================================================== */
.dash-sidebar {
  grid-area: sidebar;
  background: var(--surface-sidebar);
  border-right: 1px solid var(--border-subtle);
  overflow-y: auto;
  padding: 1.5rem 1rem;
}

.nav-section {
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
  margin-bottom: 2rem;
}

.section-label {
  font-size: 0.75rem;
  font-weight: 700;
  text-transform: uppercase;
  color: var(--text-muted);
  letter-spacing: 0.05em;
  padding: 0 0.75rem 0.5rem;
}

.nav-item {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.625rem 0.75rem;
  border-radius: 6px;
  text-decoration: none;
  color: var(--text-muted);
  font-size: 0.9rem;
  font-weight: 500;
  transition: all 120ms ease;
}

.nav-item:hover,
.nav-item.active {
  background: #1e293b;
  color: #ffffff;
}

.nav-item.active {
  border-left: 3px solid var(--brand-primary);
}

/* ==========================================================================
   Main Content Canvas
   ========================================================================== */
.dash-canvas {
  grid-area: content;
  background: var(--surface-canvas);
  overflow-y: auto;
  padding: 2rem;
  min-width: 0; /* Protect against grid track blowout */
}

.canvas-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  margin-bottom: 2rem;
}

.page-title {
  font-size: 1.75rem;
  font-weight: 800;
  letter-spacing: -0.02em;
}

.page-subtitle {
  color: var(--text-muted);
  font-size: 0.95rem;
  margin-top: 0.25rem;
}

.canvas-actions {
  display: flex;
  gap: 0.75rem;
}

.btn {
  padding: 0.625rem 1.25rem;
  border-radius: 6px;
  font-weight: 600;
  font-size: 0.875rem;
  cursor: pointer;
  border: 1px solid transparent;
}

.btn-primary {
  background: var(--brand-primary);
  color: #ffffff;
}

.btn-secondary {
  background: #1e293b;
  border-color: var(--border-subtle);
  color: var(--text-primary);
}

/* ==========================================================================
   Subgrid Metric Ribbon
   ========================================================================== */
.metrics-ribbon {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 1.25rem;
  margin-bottom: 2rem;
}

.metric-card {
  background: var(--surface-panel);
  border: 1px solid var(--border-subtle);
  border-radius: 10px;
  padding: 1.25rem;
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  min-width: 0;
}

.metric-title {
  font-size: 0.8125rem;
  font-weight: 600;
  color: var(--text-muted);
  text-transform: uppercase;
}

.metric-value {
  font-size: 2rem;
  font-weight: 800;
  margin: 0.5rem 0;
  font-variant-numeric: tabular-nums;
}

.metric-trend {
  font-size: 0.8125rem;
  font-weight: 600;
}
.metric-trend.positive { color: #34d399; }
.metric-trend.neutral { color: #94a3b8; }
.metric-trend.negative { color: #f87171; }
.text-warning { color: var(--status-warning); }

/* ==========================================================================
   Data Table with Sticky Freeze Panes
   ========================================================================== */
.table-card {
  background: var(--surface-panel);
  border: 1px solid var(--border-subtle);
  border-radius: 10px;
  overflow: hidden;
}

.table-card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1.25rem 1.5rem;
  border-bottom: 1px solid var(--border-subtle);
}

.table-card-header h2 {
  font-size: 1.125rem;
  font-weight: 700;
}

.table-count {
  font-size: 0.8125rem;
  color: var(--text-muted);
}

.table-scroll-wrapper {
  overflow-x: auto;
  max-height: 440px;
}

.data-table {
  width: 100%;
  min-width: 780px;
  border-collapse: separate;
  border-spacing: 0;
  font-size: 0.875rem;
}

.data-table th,
.data-table td {
  padding: 0.875rem 1.25rem;
  text-align: left;
  border-bottom: 1px solid var(--border-subtle);
}

.data-table thead th {
  position: sticky;
  top: 0;
  background: #182234;
  color: var(--text-muted);
  font-weight: 600;
  z-index: 2;
}

.sticky-col-header {
  position: sticky;
  left: 0;
  background: var(--surface-panel) !important;
  font-family: var(--font-mono);
  font-weight: 600;
  z-index: 1;
  border-right: 2px solid var(--border-subtle) !important;
}

.sticky-freeze-corner {
  position: sticky;
  top: 0;
  left: 0;
  background: #182234 !important;
  z-index: 3;
  border-right: 2px solid var(--border-subtle) !important;
}

.status-pill {
  display: inline-block;
  padding: 0.2rem 0.5rem;
  border-radius: 9999px;
  font-size: 0.75rem;
  font-weight: 600;
}
.status-pill.healthy { background: rgba(16, 185, 129, 0.2); color: #34d399; }
.status-pill.warning { background: rgba(245, 158, 11, 0.2); color: #fbbf24; }

/* ==========================================================================
   Responsive Adaptations
   ========================================================================== */
@media (max-width: 1024px) {
  .metrics-ribbon {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (max-width: 768px) {
  .dash-shell {
    grid-template-columns: 1fr;
    grid-template-areas:
      "header"
      "content";
  }
  .mobile-toggle {
    display: block;
  }
  /* Off-canvas Popover Sidebar */
  .dash-sidebar {
    inset: 0 auto 0 0;
    margin: 0;
    width: 260px;
    height: 100dvh;
    border: none;
    box-shadow: 10px 0 25px rgba(0, 0, 0, 0.5);
  }
  .dash-sidebar:popover-open {
    display: block;
  }
  .canvas-header {
    flex-direction: column;
    gap: 1rem;
  }
  .metrics-ribbon {
    grid-template-columns: 1fr;
  }
}
```
