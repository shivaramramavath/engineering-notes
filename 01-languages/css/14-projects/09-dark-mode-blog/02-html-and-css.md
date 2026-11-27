# Production Implementation: Technical Blog with Native Dark Mode

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Understanding Linux eBPF Kernel Probes - Systems Engineering</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>
  <div class="article-container">
    <!-- Header Landmark -->
    <header class="blog-header">
      <div class="header-nav">
        <a href="/" class="blog-brand">KernelCraft</a>
        <nav class="blog-links">
          <a href="#articles">Articles</a>
          <a href="#newsletter">Newsletter</a>
          <a href="#about">About</a>
        </nav>
      </div>
    </header>

    <!-- Main Article Article -->
    <main>
      <article class="blog-article">
        <header class="post-header">
          <div class="post-meta">
            <span class="meta-tag">Linux Internals</span>
            <time datetime="2026-03-24">March 24, 2026</time>
            <span class="bullet" aria-hidden="true">•</span>
            <span>9 min read</span>
          </div>
          <h1 class="post-title">Understanding Linux eBPF Kernel Probes in Production Systems</h1>
          <p class="post-lead">
            How extended Berkeley Packet Filter programs run safely inside the Linux kernel address space to deliver zero-overhead network observability and security enforcement.
          </p>
        </header>

        <section class="post-content">
          <p>
            For decades, inspecting low-level Linux kernel events required compiling and loading custom out-of-tree kernel modules (LKMs). While powerful, a single null pointer dereference in an LKM resulted in immediate kernel panics and catastrophic host crashes.
          </p>

          <p>
            With the introduction of modern eBPF, engineers can attach bytecode safely to tracepoints, kprobes, and socket buffers. The in-kernel eBPF verifier guarantees mathematical termination and memory safety before any bytecode executes.
          </p>

          <div class="callout-box">
            <div class="callout-icon" aria-hidden="true">💡</div>
            <div class="callout-body">
              <strong>Architecture Note:</strong> The eBPF verifier performs static abstract interpretation, disallowing unbounded loops and verifying that every memory access remains strictly within stack boundaries.
            </div>
          </div>

          <h2>Attaching a Minimal Socket Filter</h2>
          <p>
            Below is a minimal Rust program compiled to BPF bytecode targeting network packets arriving at a virtual Ethernet interface:
          </p>

          <div class="code-fence-wrapper">
            <div class="code-fence-bar">
              <span class="filename">filter.rs</span>
              <span class="lang-tag">Rust</span>
            </div>
            <pre><code><span class="kw">use</span> aya_bpf::{macros::<span class="fn">classifier</span>, programs::<span class="ty">TcContext</span>};

<span class="attr">#[classifier]</span>
<span class="kw">pub fn</span> <span class="fn">tc_ingress_filter</span>(ctx: <span class="ty">TcContext</span>) -&gt; <span class="ty">Result</span>&lt;<span class="ty">i32</span>, <span class="ty">i32</span>&gt; {
    <span class="kw">let</span> eth_hdr = ctx.<span class="fn">load_eth_header</span>()?;
    <span class="kw">if</span> eth_hdr.proto == <span class="num">0x0800</span> {
        <span class="kw">return</span> <span class="ty">Ok</span>(<span class="num">0</span>); <span class="comment">// Allow IPv4</span>
    }
    <span class="ty">Ok</span>(-<span class="num">1</span>) <span class="comment">// Drop non-IPv4</span>
}</code></pre>
          </div>

          <h2>Measuring Performance at 100Gbps</h2>
          <p>
            When tested on dual-port 100GbE Intel E810 network interface controllers, eBPF XDP (eXpress Data Path) processed 24 million packets per second without dropping a single packet on the host CPU. By executing directly on the network driver ring buffer before `sk_buff` memory allocation, packet processing latency is eliminated.
          </p>
        </section>
      </article>
    </main>

    <!-- Footer -->
    <footer class="blog-footer">
      <p>© 2026 KernelCraft Engineering Notes. Written for software architects.</p>
    </footer>
  </div>
</body>
</html>
```

```css
/* ==========================================================================
   Zero-JS Dual-Theme Design Tokens (CSS light-dark())
   ========================================================================== */
:root {
  color-scheme: light dark;

  --bg-page: light-dark(#ffffff, #0d1117);
  --bg-surface: light-dark(#f6f8fa, #161b22);
  --border-subtle: light-dark(#d0d7de, #30363d);
  --text-main: light-dark(#1f2328, #e6edf3);
  --text-muted: light-dark(#656d76, #848d97);
  --accent-link: light-dark(#0969da, #2f81f7);
  
  --callout-bg: light-dark(#ddf4ff, rgba(56, 139, 253, 0.1));
  --callout-border: light-dark(#54aeff, rgba(56, 139, 253, 0.35));
  --callout-text: light-dark(#0969da, #79c0ff);
  
  --code-bg: light-dark(#f6f8fa, #161b22);
  --code-border: light-dark(#d0d7de, #30363d);
  --code-header: light-dark(#eaeef2, #21262d);

  --font-sans: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
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
  line-height: 1.7;
  padding: 2rem 1.5rem 4rem 1.5rem;
  -webkit-font-smoothing: antialiased;
}

.article-container {
  max-width: 68ch;
  margin: 0 auto;
}

/* ==========================================================================
   Header Navigation
   ========================================================================== */
.blog-header {
  margin-bottom: 3.5rem;
  padding-bottom: 1.5rem;
  border-bottom: 1px solid var(--border-subtle);
}

.header-nav {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.blog-brand {
  font-weight: 800;
  font-size: 1.25rem;
  color: var(--text-main);
  text-decoration: none;
  letter-spacing: -0.02em;
}

.blog-links {
  display: flex;
  gap: 1.5rem;
}

.blog-links a {
  text-decoration: none;
  color: var(--text-muted);
  font-size: 0.9rem;
  font-weight: 500;
  transition: color 150ms ease;
}

.blog-links a:hover {
  color: var(--text-main);
}

/* ==========================================================================
   Article Header & Typography
   ========================================================================== */
.post-header {
  margin-bottom: 2.5rem;
}

.post-meta {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  font-size: 0.8125rem;
  color: var(--text-muted);
  margin-bottom: 1rem;
}

.meta-tag {
  background: var(--bg-surface);
  border: 1px solid var(--border-subtle);
  padding: 0.15rem 0.5rem;
  border-radius: 9999px;
  font-weight: 600;
  color: var(--text-main);
}

.post-title {
  font-size: clamp(2rem, 1.25rem + 2.5vw, 3rem);
  font-weight: 800;
  line-height: 1.15;
  letter-spacing: -0.03em;
  margin-bottom: 1.25rem;
  text-wrap: balance;
}

.post-lead {
  font-size: 1.2rem;
  line-height: 1.55;
  color: var(--text-muted);
}

/* ==========================================================================
   Body Content Rhythm
   ========================================================================== */
.post-content p {
  font-size: 1.125rem;
  margin-bottom: 1.5rem;
  color: var(--text-main);
}

.post-content h2 {
  font-size: 1.625rem;
  font-weight: 700;
  letter-spacing: -0.02em;
  margin: 2.5rem 0 1rem 0;
  text-wrap: balance;
}

/* Callout Box */
.callout-box {
  display: flex;
  gap: 1rem;
  padding: 1.25rem;
  background: var(--callout-bg);
  border: 1px solid var(--callout-border);
  border-radius: 8px;
  margin: 2rem 0;
}

.callout-icon {
  font-size: 1.25rem;
  line-height: 1.2;
}

.callout-body {
  font-size: 0.95rem;
  line-height: 1.5;
  color: var(--text-main);
}

/* Code Fence */
.code-fence-wrapper {
  background: var(--code-bg);
  border: 1px solid var(--code-border);
  border-radius: 8px;
  margin: 2rem 0;
  overflow: hidden;
}

.code-fence-bar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0.5rem 1rem;
  background: var(--code-header);
  border-bottom: 1px solid var(--code-border);
  font-family: var(--font-mono);
  font-size: 0.75rem;
  color: var(--text-muted);
}

.code-fence-wrapper pre {
  padding: 1.25rem 1rem;
  font-family: var(--font-mono);
  font-size: 0.875rem;
  line-height: 1.6;
  overflow-x: auto;
  color: var(--text-main);
}

.code-fence-wrapper code {
  font-family: inherit;
}

/* Code Syntax Colors */
.kw { color: #f472b6; font-weight: 600; }
.fn { color: #38bdf8; }
.ty { color: #fbbf24; }
.num { color: #a3e635; }
.comment { color: #64748b; font-style: italic; }
.attr { color: #c084fc; }

/* ==========================================================================
   Footer
   ========================================================================== */
.blog-footer {
  margin-top: 4rem;
  padding-top: 2rem;
  border-top: 1px solid var(--border-subtle);
  text-align: center;
  font-size: 0.8125rem;
  color: var(--text-muted);
}
```
