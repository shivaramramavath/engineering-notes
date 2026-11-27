# Practical Layout Challenges Interview Questions

| Topic | Module | Level | Target Roles | Date | Prerequisites |
|---|---|---|---|---|---|
| Live Coding Practical Layout Scenarios | 15: Interview Questions | Mid to Senior / Staff | Frontend Engineer, Full-Stack Lead | 2026-03-31 | Modules 03, 04, 13 |

---

## Overview
This guide contains 8 live-coding architectural layout challenges frequently presented during technical screen and on-site frontend interviews. Each challenge provides the objective, strict engineering constraints, a production-grade verified solution, and architectural analysis.

---

## Live Coding Challenges

### Challenge 1: Perfect Centering with Data-Loss Protection
**Prompt**: Center a modal card horizontally and vertically inside a parent container. If the viewport height becomes smaller than the card height, the user must be able to scroll to read the entire modal from the top without the header being cropped off-screen.
<details>
<summary>View Complete Verified Solution</summary>

```html
<div class="modal-backdrop">
  <article class="modal-card">
    <h2>Important Notice</h2>
    <p>Tall content that might exceed the viewport on small laptop screens...</p>
  </article>
</div>
```

```css
/* Container */
.modal-backdrop {
  display: flex;
  min-height: 100dvh;
  padding: 1.5rem;
  background: rgba(15, 23, 42, 0.6);
  overflow-y: auto; /* Enables scrolling if card exceeds viewport */
}

/* Modal Card */
.modal-card {
  margin: auto; /* THE KEY: Flex margin: auto centers perfectly, but permits scrolling to top: 0! */
  width: 100%;
  max-width: 520px;
  background: #ffffff;
  border-radius: 12px;
  padding: 2rem;
  box-shadow: 0 20px 25px rgba(0, 0, 0, 0.2);
}
```

**Why `place-content: center` fails here**:
Using `display: grid; place-content: center;` centers content along the center coordinate axis. When content overflows a scrollable container, browsers cannot scroll above `scrollTop: 0`. The top half of the modal (including the title and close button) becomes inaccessible off the top edge of the screen (*data loss*). Flexbox with `margin: auto` on the child resolves this bug natively.
</details>

---

### Challenge 2: Sticky Footer that Stays at Bottom on Short Pages
**Prompt**: Construct a page layout where the `<footer>` rests naturally below content on tall pages, but stays pinned flush to the bottom of the viewport when content is short, with zero JavaScript.
<details>
<summary>View Complete Verified Solution</summary>

```html
<div class="site-layout">
  <header>Navigation Header</header>
  <main>Short body content goes here...</main>
  <footer>Copyright 2026</footer>
</div>
```

```css
.site-layout {
  display: flex;
  flex-direction: column;
  min-height: 100dvh;
}

main {
  flex: 1 0 auto; /* Consumes all available vertical free space */
}

footer {
  margin-top: auto; /* Pushes footer flush to bottom */
  background: #0f172a;
  color: #fff;
  padding: 1.5rem;
}
```
</details>

---

### Challenge 3: Responsive Holy Grail Layout with DOM Priority
**Prompt**: Build a 3-column Holy Grail layout (Nav Left, Main Center, Aside Right). For SEO and accessibility, `<main>` must appear *first* in the HTML source code, but render visually in the center on desktop. The layout must collapse cleanly into a single column on mobile.
<details>
<summary>View Complete Verified Solution</summary>

```html
<div class="holy-grail">
  <main class="hg-main">Primary Article Content (First in DOM for SEO)</main>
  <nav class="hg-nav">Navigation Rail</nav>
  <aside class="hg-aside">Contextual Sidebar</aside>
</div>
```

```css
.holy-grail {
  display: grid;
  grid-template-columns: 240px 1fr 260px;
  grid-template-areas: "nav main aside";
  gap: 1.5rem;
  min-height: 100dvh;
}

.hg-main  { grid-area: main;  min-width: 0; }
.hg-nav   { grid-area: nav; }
.hg-aside { grid-area: aside; }

@media (max-width: 840px) {
  .holy-grail {
    grid-template-columns: 1fr;
    grid-template-areas:
      "main"
      "nav"
      "aside";
  }
}
```
</details>

---

### Challenge 4: Two-Dimensional Freeze-Pane Table
**Prompt**: Construct an HTML table that scrolls both horizontally and vertically inside a fixed container. The table header row (`<thead>`) must freeze to the top, the first column must freeze to the left, and the top-left intersection cell must remain pinned above both.
<details>
<summary>View Complete Verified Solution</summary>

```css
.freeze-container {
  max-height: 400px;
  max-width: 100%;
  overflow: auto;
  border: 1px solid #cbd5e1;
}

.freeze-table {
  width: 100%;
  border-collapse: separate; /* Essential: Prevents sticky border collapse bugs */
  border-spacing: 0;
}

.freeze-table th,
.freeze-table td {
  padding: 0.75rem 1rem;
  border-bottom: 1px solid #e2e8f0;
  border-right: 1px solid #e2e8f0;
  background: #ffffff;
}

/* 1. Sticky Header */
.freeze-table thead th {
  position: sticky;
  top: 0;
  background: #f1f5f9;
  z-index: 2;
}

/* 2. Sticky First Column */
.freeze-table th:first-child,
.freeze-table td:first-child {
  position: sticky;
  left: 0;
  background: #f8fafc;
  z-index: 1;
}

/* 3. Sticky Corner Intersection */
.freeze-table thead th:first-child {
  position: sticky;
  top: 0;
  left: 0;
  background: #e2e8f0;
  z-index: 3; /* Must be highest to render above both sticky axes */
}
```
</details>

---

### Challenge 5: Auto-Fitting Bento Grid with Hero Tile
**Prompt**: Build an asymmetric 4-column Bento Grid where Card 1 spans 2 columns and 2 rows, Card 2 spans 2 columns and 1 row, and Cards 3, 4, 5 occupy 1 column each. The grid must collapse gracefully to 1 column on screens below 768px.
<details>
<summary>View Complete Verified Solution</summary>

```css
.bento-grid {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  grid-auto-rows: 180px;
  gap: 1.25rem;
}

.bento-hero {
  grid-column: span 2;
  grid-row: span 2;
}

.bento-wide {
  grid-column: span 2;
  grid-row: span 1;
}

@media (max-width: 768px) {
  .bento-grid {
    grid-template-columns: 1fr;
    grid-auto-rows: auto;
  }
  .bento-hero,
  .bento-wide {
    grid-column: span 1;
    grid-row: span 1;
  }
}
```
</details>

---

### Challenge 6: Responsive Multi-Column Form with Grid Spans
**Prompt**: Create a checkout form where "First Name" and "Last Name" take 50% width on desktop, "Street Address" takes 100%, and "City, State, Zip" take 50%, 25%, 25% respectively. On screens below 600px, all inputs must stack into a single column.
<details>
<summary>View Complete Verified Solution</summary>

```css
.form-grid {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 1rem;
}

.span-full { grid-column: span 4; }
.span-half { grid-column: span 2; }
.span-quarter { grid-column: span 1; }

@media (max-width: 600px) {
  .form-grid {
    grid-template-columns: 1fr;
  }
  .span-full,
  .span-half,
  .span-quarter {
    grid-column: span 1;
  }
}
```
</details>

---

### Challenge 7: Collapsible Dashboard Rail with Zero Window Scroll
**Prompt**: Create an application layout bounded to `100dvh` where the sidebar is 250px on desktop and collapses to 72px when the attribute `data-collapsed="true"` is set, transitioning smoothly without layout shivering.
<details>
<summary>View Complete Verified Solution</summary>

```css
.dash-shell {
  display: grid;
  grid-template-columns: 250px 1fr;
  height: 100dvh;
  overflow: hidden;
  transition: grid-template-columns 200ms ease;
}

.dash-shell[data-collapsed="true"] {
  grid-template-columns: 72px 1fr;
}

.dash-sidebar {
  background: #0f172a;
  overflow-y: auto;
}

.dash-main {
  background: #f8fafc;
  overflow-y: auto;
  min-width: 0; /* Prevents wide children from breaking track sizing */
}
```
</details>

---

### Challenge 8: Zero-Layout-Shift Aspect-Ratio Video Card
**Prompt**: Build a video card thumbnail component that prevents Cumulative Layout Shift (CLS) by maintaining a 16:9 ratio before the video asset loads, with an overlay play button centered in pure CSS.
<details>
<summary>View Complete Verified Solution</summary>

```html
<div class="video-card">
  <video src="hero.mp4" poster="poster.webp"></video>
  <button type="button" class="btn-play" aria-label="Play video">▶</button>
</div>
```

```css
.video-card {
  width: 100%;
  aspect-ratio: 16 / 9; /* Reserves exact geometric volume before asset loads */
  position: relative;
  background: #0f172a;
  border-radius: 12px;
  overflow: hidden;
  display: grid;
  place-items: center;
}

.video-card video {
  width: 100%;
  height: 100%;
  object-fit: cover;
  grid-area: 1 / 1;
}

.btn-play {
  grid-area: 1 / 1;
  z-index: 2;
  width: 64px;
  height: 64px;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.9);
  border: none;
  cursor: pointer;
  font-size: 1.5rem;
  display: grid;
  place-content: center;
  transition: transform 150ms ease;
}

.btn-play:hover {
  transform: scale(1.1);
}
```
</details>

---

## Navigation
- **Previous Guide**: [Output-Based & Debugging Questions](./08-output-based-and-debugging-questions.md)
- **Next Guide**: [System Design, Architecture & Behavioural Q&A](./10-behavioural-and-project-discussion.md)
