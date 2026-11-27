# Search and Filter Forms: UX, Semantics, and Live Filtering

| Metadata | Details |
| :--- | :--- |
| **Specification** | WHATWG HTML Living Standard (§4.4.15 The `search` element, §4.10.5.1.4 Search state) |
| **Prerequisites** | `03-forms/01-form-basics-and-methods.md`, `02-semantics-and-structure/02-landmarks-and-page-regions.md` |
| **Estimated Reading Time** | 24 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`<search>` element Baseline 2023; `<input type="search">` Baseline Widely Available) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, axe-core 4.9, NVDA 2024.1, VoiceOver macOS 14.4 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Canonical Semantic `<search>` Container with `<input type="search">`](#example-1-canonical-semantic-search-container-with-input-typesearch)
   - [Example 2: Multi-Faceted E-Commerce Filter Form (GET-based)](#example-2-multi-faceted-e-commerce-filter-form-get-based)
   - [Example 3: Debounced Live Search with Accessible Live Region Feedback](#example-3-debounced-live-search-with-accessible-live-region-feedback)
   - [Example 4: Search with Datalist and Autocomplete Suggestions](#example-4-search-with-datalist-and-autocomplete-suggestions)
   - [Example 5: URL-Synchronized State with History API](#example-5-url-synchronized-state-with-history-api)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility, SEO & Security Notes](#8-performance-accessibility-seo--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Search and filtering represent the primary discovery interface across almost every digital product—from site-wide documentation indexes and e-commerce product catalogs to enterprise dashboard analytics.

Historically, developers enclosed search bars inside generic `<div>` or `<nav>` containers, styled text inputs without semantic search affordances, or implemented instant client-side filtering that completely neglected assistive technologies and browser back-button history.

HTML provides dedicated native semantics: the `<search>` landmark container, `<input type="search">` with native clear-button affordances, and standard `method="GET"` query string encoding. When properly orchestrated with accessible live regions (`aria-live`) and non-destructive URL state management, search and filter forms offer instant responsiveness, bookmarkable query states, and universal screen reader compatibility.

---

## 2. Core Concepts

### The `<search>` Element
Introduced in the WHATWG specification and achieving cross-browser Baseline in 2023, the `<search>` element is an explicit semantic container designed to enclose form controls, links, or instructions related to search and filtering operations.
- **Implicit ARIA Role**: Exposes `role="search"` natively to the accessibility tree.
- **Nesting Rules**: A `<search>` element typically encloses a `<form>`, or directly wraps search controls and result presentation links.
- **Scope**: Can represent site-wide search or localized in-page filtering.

### `<input type="search">`
A specialized text input tuned specifically for search queries.
- **Rendering Affordance**: On modern desktop and mobile browsers, entering text reveals an integrated "clear" (x) button. Pressing `Escape` or tapping the clear icon immediately clears the input field.
- **Mobile Keyboards**: Mobile virtual keyboards render a dedicated "Search" action key (magnifying glass) in place of the standard "Return" key.
- **Semantic Mapping**: In the accessibility tree, it exposes the role `searchbox` (subclass of `textbox`).

### Query Serialization with `method="GET"`
Search and filter forms should almost universally utilize `method="GET"`. Submitting a GET form appends field parameters to the URL query string (`/search?q=docker&sort=recent`). This guarantees that search queries are:
1. **Bookmarkable**: Users can save exact search states.
2. **Shareable**: URLs can be copied and distributed.
3. **Idempotent**: Refreshing or navigating back never prompts "Confirm Form Resubmission" warnings.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                         Search & Filter Form Query Flow                                 |
+-----------------------------------------------------------------------------------------+

  User enters query into <input type="search">
                          |
             [ Mode of Operation? ]
             /                    \
     TRADITIONAL (GET)          AJAX / ASYNCHRONOUS
            |                              |
  User presses 'Enter'            Debounce Timer fires
            |                              |
  Serialize form fields:          Fetch results via background JSON:
    ?q=laptop&brand=dell            fetch('/api/search?q=laptop')
            |                              |
  Browser performs full           1. Update DOM results container
  GET navigation:                 2. Update History: history.pushState()
  https://example.com/search      3. Announce to AOM via aria-live:
  ?q=laptop&brand=dell                 "Found 14 products for laptop"
```

### Virtual Keyboard & Native Clear Button Mechanics
When a user focuses an `<input type="search">`, the browser signals the operating system IME (Input Method Editor) to display a search-specific return key. Internally, the browser injects a pseudo-element (`::-webkit-search-cancel-button` in Chromium and WebKit) that responds directly to clicks or the `Escape` key, dispatching a native `search` or `input` event upon clearing.

---

## 4. Syntax & Usage

```html
<search>
  <form action="/search" method="get" role="search">
    <label for="site-search">Search documentation:</label>
    <input 
      type="search" 
      id="site-search" 
      name="q" 
      placeholder="e.g., CSS Grid, Flexbox..." 
      required 
    />
    <button type="submit">Search</button>
  </form>
</search>
```

> **Cross-Browser & Baseline Status Box**
>
> - `<search>` element: **Baseline Newly Available (2023)** across Chrome 118+, Safari 17.0+, Firefox 118+, Edge 118+. For legacy browsers, adding `role="search"` ensures identical accessibility tree exposure.
> - `<input type="search">`: **Baseline Widely Available** across all desktop and mobile browsers.

---

## 5. Practical Examples

### Example 1: Canonical Semantic `<search>` Container with `<input type="search">`
A standard header search form adhering to modern semantics and accessibility standards.

```html
<header class="site-header">
  <div class="logo">
    <a href="/">DevDocs</a>
  </div>

  <!-- The semantic <search> container represents the search landmark -->
  <search>
    <form action="/docs/search" method="get" class="search-form">
      <label for="query" class="visually-hidden">Search all documentation articles</label>
      <div class="search-input-wrapper">
        <svg aria-hidden="true" width="20" height="20" viewBox="0 0 24 24">
          <path d="M15.5 14h-.79l-.28-.27A6.471 6.471 0 0 0 16 9.5 6.5 6.5 0 1 0 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z"/>
        </svg>
        <input 
          type="search" 
          id="query" 
          name="q" 
          placeholder="Search documentation..." 
          autocomplete="off" 
          spellcheck="false"
          required 
        />
        <button type="submit">Search</button>
      </div>
    </form>
  </search>
</header>
```

---

### Example 2: Multi-Faceted E-Commerce Filter Form (GET-based)
A complete product filter sidebar utilizing fieldsets, checkboxes, radio buttons, and range controls with pure HTML progressive enhancement.

```html
<search class="catalog-filters">
  <form action="/products" method="get" id="filter-form">
    <h2>Filter Catalog</h2>

    <!-- Hidden search query retained when filtering -->
    <input type="hidden" name="q" value="laptop" />

    <!-- Category Facet -->
    <fieldset>
      <legend>Category</legend>
      <div class="filter-option">
        <input type="checkbox" id="cat-ultrabook" name="cat" value="ultrabook" checked />
        <label for="cat-ultrabook">Ultrabooks (42)</label>
      </div>
      <div class="filter-option">
        <input type="checkbox" id="cat-gaming" name="cat" value="gaming" />
        <label for="cat-gaming">Gaming (18)</label>
      </div>
      <div class="filter-option">
        <input type="checkbox" id="cat-workstation" name="cat" value="workstation" />
        <label for="cat-workstation">Workstations (7)</label>
      </div>
    </fieldset>

    <!-- Price Range Facet -->
    <fieldset>
      <legend>Maximum Price ($)</legend>
      <label for="max-price" class="visually-hidden">Max Price</label>
      <input 
        type="range" 
        id="max-price" 
        name="max_price" 
        min="500" 
        max="3000" 
        step="100" 
        value="2000"
        oninput="document.getElementById('price-val').textContent = this.value"
      />
      <output id="price-val" for="max-price">2000</output>
    </fieldset>

    <!-- Sort Order Facet -->
    <fieldset>
      <legend>Sort Order</legend>
      <div class="filter-option">
        <input type="radio" id="sort-relevance" name="sort" value="rel" checked />
        <label for="sort-relevance">Relevance</label>
      </div>
      <div class="filter-option">
        <input type="radio" id="sort-price-asc" name="sort" value="price_asc" />
        <label for="sort-price-asc">Price: Low to High</label>
      </div>
      <div class="filter-option">
        <input type="radio" id="sort-rating" name="sort" value="rating" />
        <label for="sort-rating">Customer Rating</label>
      </div>
    </fieldset>

    <div class="filter-actions">
      <button type="submit">Apply Filters</button>
      <a href="/products?q=laptop" class="reset-link">Clear All</a>
    </div>
  </form>
</search>
```

---

### Example 3: Debounced Live Search with Accessible Live Region Feedback
Demonstrating instant as-you-type search while ensuring screen readers receive debounced, non-intrusive results announcements.

```html
<search class="live-search-widget">
  <form id="live-search-form" action="/search" method="get">
    <label for="live-query">Find Articles:</label>
    <input 
      type="search" 
      id="live-query" 
      name="q" 
      autocomplete="off" 
      aria-describedby="search-count" 
    />
  </form>

  <!-- Live region: Screen readers will announce count changes politely -->
  <div id="search-count" role="status" aria-live="polite" class="sr-only"></div>

  <!-- Live results container -->
  <ul id="search-results" class="results-list" aria-label="Search results"></ul>
</search>

<script>
  const queryInput = document.getElementById('live-query');
  const countRegion = document.getElementById('search-count');
  const resultsList = document.getElementById('search-results');

  let debounceTimer = null;

  queryInput.addEventListener('input', () => {
    clearTimeout(debounceTimer);
    const query = queryInput.value.trim();

    if (!query) {
      resultsList.innerHTML = '';
      countRegion.textContent = '';
      return;
    }

    // Debounce network requests by 300ms
    debounceTimer = setTimeout(async () => {
      countRegion.textContent = 'Searching...';

      try {
        const response = await fetch(`/api/search?q=${encodeURIComponent(query)}`);
        const items = await response.json();

        resultsList.innerHTML = '';

        if (items.length === 0) {
          countRegion.textContent = `No articles found for "${query}".`;
          return;
        }

        countRegion.textContent = `${items.length} articles found.`;

        items.forEach(item => {
          const li = document.createElement('li');
          const a = document.createElement('a');
          a.href = item.url;
          a.textContent = item.title;
          li.appendChild(a);
          resultsList.appendChild(li);
        });
      } catch (err) {
        countRegion.textContent = 'Error loading results.';
      }
    }, 300);
  });
</script>
```

---

### Example 4: Search with Datalist and Autocomplete Suggestions
Pairing `<input type="search">` with `<datalist>` to offer native browser autocomplete suggestions with zero custom JavaScript dropdown styling.

```html
<search>
  <form action="/explore" method="get">
    <label for="tech-search">Search Technologies:</label>
    <div class="search-box">
      <input 
        type="search" 
        id="tech-search" 
        name="technology" 
        list="tech-suggestions" 
        placeholder="e.g. JavaScript, Python..."
        required 
      />
      
      <datalist id="tech-suggestions">
        <option value="HTML Living Standard"></option>
        <option value="CSS Cascade Layers"></option>
        <option value="JavaScript ES2024"></option>
        <option value="TypeScript Generics"></option>
        <option value="WebAssembly"></option>
        <option value="Node.js Performance"></option>
        <option value="Docker Containers"></option>
      </datalist>

      <button type="submit">Search</button>
    </div>
  </form>
</search>
```

---

### Example 5: URL-Synchronized State with History API
Demonstrates how to update client-side filters asynchronously while keeping the browser URL and Back/Forward buttons in complete synchronization.

```html
<search>
  <form id="stateful-filter-form" action="/search" method="get">
    <input type="search" id="state-q" name="q" placeholder="Keywords..." />
    <select id="state-category" name="category">
      <option value="">All Categories</option>
      <option value="frontend">Frontend</option>
      <option value="backend">Backend</option>
      <option value="devops">DevOps</option>
    </select>
  </form>
</search>

<script>
  const filterForm = document.getElementById('stateful-filter-form');
  const qInput = document.getElementById('state-q');
  const catSelect = document.getElementById('state-category');

  function updateQueryState() {
    const formData = new FormData(filterForm);
    const params = new URLSearchParams();

    for (const [key, value] of formData.entries()) {
      if (value.trim()) params.append(key, value);
    }

    const newUrl = `${window.location.pathname}?${params.toString()}`;
    // Non-destructive history update
    history.pushState(null, '', newUrl);
    console.log('Synchronized URL to:', newUrl);
  }

  qInput.addEventListener('change', updateQueryState);
  catSelect.addEventListener('change', updateQueryState);

  // Restore filter values when navigating through browser history
  window.addEventListener('popstate', () => {
    const currentParams = new URLSearchParams(window.location.search);
    qInput.value = currentParams.get('q') || '';
    catSelect.value = currentParams.get('category') || '';
  });
</script>
```

---

## 6. Best Practices

- [ ] **Always use `method="GET"` for search and filtering**: Enables users to bookmark, copy, and share exact search result URLs without causing browser reload warnings.
- [ ] **Wrap search forms inside the `<search>` semantic landmark**: Exposes `role="search"` to the accessibility tree and allows screen reader users to jump directly to search.
- [ ] **Provide an explicit or visually hidden `<label>`**: Never rely solely on the `placeholder` attribute for search inputs. Screen readers require an accessible name.
- [ ] **Announce asynchronous results counts via `aria-live="polite"`**: When dynamically filtering, notify assistive technologies of the result count change without interrupting active speech.
- [ ] **Debounce live search inputs**: Prevent network flooding by waiting 250–400ms after the user stops typing before dispatching search requests.
- [ ] **Preserve search state across filter toggles**: Ensure secondary filter interactions (checkboxes, select dropdowns) retain the active search query via hidden inputs or URLSearchParams merging.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Using `method="POST"` for search queries | **High** | Search URLs cannot be bookmarked or shared; browser prompts "Confirm Form Resubmission" on Back button. | Use `method="GET"` on search forms. |
| Using `placeholder` as the sole label | **Critical** | Screen readers miss input purpose; placeholder text disappears once user starts typing. | Add a `<label for="...">` with a `.visually-hidden` class if visual design requires. |
| Neglecting `aria-live` during live filtering | **High** | Blind and low-vision users receive no feedback that results updated on screen. | Add a `<div role="status" aria-live="polite">` announcing result counts. |
| Omitting debouncing on `input` event listeners | **Moderate** | Dispatches HTTP requests on every keystroke, causing race conditions and server load. | Debounce the handler with a 300ms `setTimeout`. |
| Wrapping `<search>` inside a `<header>` without fallback role | **Low** | Legacy browsers prior to 2023 ignore `<search>`. | Add `role="search"` to `<search>` or `<form>` for legacy browser support. |
| Forgetting to synchronize the browser URL via History API | **Moderate** | Users cannot use the Back button to return to previous filter permutations in SPAs. | Update the URL via `history.pushState()` when filters change. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Guideline 1.3.1 Info and Relationships**: The `<search>` element or `role="search"` guarantees search region identification.
- **Guideline 4.1.3 Status Messages**: Result count updates must be conveyed using `aria-live="polite"`. Never use `aria-live="assertive"` for search counts, as it abruptly cuts off screen reader speech while the user is typing.

### SEO & Search Engines
- Search results pages should typically include `<meta name="robots" content="noindex, follow">` to prevent search engine crawler spiders from indexing endless combinations of dynamic internal search queries (spider traps).

### Security
- **Reflected Cross-Site Scripting (XSS)**: Always HTML-escape the search query when displaying it back to the user (e.g., `Showing results for "<strong>&lt;script&gt;</strong>"`). Never insert raw user query strings into `innerHTML`.

---

## 9. Comparison / When to Use What

| Pattern | Baseline Status | Advantages | Drawbacks | Best For |
| :--- | :--- | :--- | :--- | :--- |
| **Traditional GET Form** | Baseline Widely Available | Works with zero JS, fully bookmarkable, native browser history. | Full page reload per filter change. | Content-heavy portals, blogs, simple product catalogs. |
| **Debounced Live Search** | Widely Available (Fetch + DOM) | Instant visual feedback, no page reload. | Requires JS, careful race condition and AOM management. | Typeahead auto-suggest, global documentation search. |
| **`<search>` Landmark** | Baseline 2023 | Native semantic landmark, no ARIA role necessary. | Requires `role="search"` fallback for browsers older than late 2023. | All modern web search and filter wrappers. |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the semantic difference between the new <code>&lt;search&gt;</code> element and <code>&lt;form role="search"&gt;</code>?</strong></summary>

The `<search>` element (introduced in HTML Living Standard and Baseline 2023) is a native structural HTML container that automatically conveys the `search` landmark role in the accessibility tree without needing explicit ARIA attributes. Unlike `<form role="search">`, `<search>` does not represent a submittable form entity itself; it is a region that can contain a form, filtering controls, clear links, or instructions. For maximum backward compatibility with older browsers, developers can write `<search role="search">`.
</details>

<details>
<summary><strong>2. Why must search and filter forms use <code>method="GET"</code> instead of <code>method="POST"</code>?</strong></summary>

GET requests append form data to the URL query string (`/search?q=term`), making search results bookmarkable, linkable, and shareable between users. Furthermore, HTTP GET operations are idempotent: refreshing the page or navigating back and forward in browser history does not trigger "Confirm Form Resubmission" warnings or duplicate state mutations on the server.
</details>

<details>
<summary><strong>3. What unique user interface and event capabilities does <code>&lt;input type="search"&gt;</code> provide over <code>type="text"</code>?</strong></summary>

`<input type="search">` exposes the ARIA role `searchbox`, commands mobile virtual keyboards to show a dedicated "Search" button, and renders a native clear icon (`x`) on desktop and mobile browsers. Clicking the clear button or pressing `Escape` clears the input and fires a `search` or `input` event.
</details>

<details>
<summary><strong>4. How should asynchronous "live search" results be announced to screen reader users?</strong></summary>

By routing result count summaries into a visually hidden container configured with `role="status"` or `aria-live="polite"`. Using `polite` ensures the screen reader waits until the user pauses typing before announcing "12 results found", avoiding disruptive audio interruptions.
</details>

<details>
<summary><strong>5. What is a "spider trap" in the context of search and filter forms, and how is it mitigated?</strong></summary>

A spider trap occurs when search engine crawlers index search query pages with endless combinations of filter parameters (e.g., color, size, brand, page), consuming crawler budget and indexing duplicate content. It is mitigated by adding `<meta name="robots" content="noindex, follow">` to internal search pages or blocking internal search URL patterns in `robots.txt`.
</details>

<details>
<summary><strong>6. How do you handle race conditions during asynchronous as-you-type search?</strong></summary>

If a user types "cat", pauses, and then quickly types "cats", two asynchronous fetch requests are fired. If the second request returns faster than the first, the stale "cat" results could overwrite the "cats" results. This is prevented by using `AbortController` to abort in-flight requests whenever a new request begins, or by tracking a monotonic request ID.
</details>

<details>
<summary><strong>7. How can you preserve existing search terms when a user toggles a secondary category filter in a pure HTML form?</strong></summary>

By including a hidden input inside the filter form containing the current search term:
`<input type="hidden" name="q" value="laptop" />`. When the user submits the category filter, both the search term and the new filter parameters are submitted together.
</details>

<details>
<summary><strong>8. What is the benefit of combining <code>&lt;input type="search"&gt;</code> with <code>&lt;datalist&gt;</code>?</strong></summary>

It provides native, zero-JavaScript autocomplete suggestions based on previous popular queries or predefined categories. The browser renders an accessible suggestion dropdown that keyboard users can navigate via Arrow keys, while retaining complete fallback behavior in older browsers.
</details>

---

## 11. Practice Exercises

### Exercise 1: Accessible Documentation Search Form (Easy)
- **Task**: Build an accessible search form inside a `<search>` element. Include a visually hidden label, `<input type="search">`, and a submit button.

<details>
<summary>View Solution</summary>

```html
<search>
  <form action="/search" method="get">
    <label for="doc-query" class="visually-hidden">Search Knowledge Base</label>
    <input 
      type="search" 
      id="doc-query" 
      name="query" 
      placeholder="Search articles..." 
      required 
    />
    <button type="submit">Search</button>
  </form>
</search>

<style>
  .visually-hidden {
    position: absolute;
    width: 1px;
    height: 1px;
    padding: 0;
    margin: -1px;
    overflow: hidden;
    clip: rect(0,0,0,0);
    border: 0;
  }
</style>
```
</details>

---

### Exercise 2: Multi-Checkbox Facet Form with Reset (Medium)
- **Task**: Create a GET filter form for articles with three tag checkboxes (`javascript`, `html`, `css`), a sort select dropdown, and a native reset button. Ensure labels are correctly associated.

<details>
<summary>View Solution</summary>

```html
<search>
  <form action="/articles" method="get">
    <fieldset>
      <legend>Filter by Topic</legend>
      
      <div>
        <input type="checkbox" id="topic-html" name="topic" value="html" />
        <label for="topic-html">HTML</label>
      </div>

      <div>
        <input type="checkbox" id="topic-css" name="topic" value="css" />
        <label for="topic-css">CSS</label>
      </div>

      <div>
        <input type="checkbox" id="topic-js" name="topic" value="javascript" />
        <label for="topic-js">JavaScript</label>
      </div>
    </fieldset>

    <div>
      <label for="sort-by">Sort By:</label>
      <select id="sort-by" name="sort">
        <option value="newest">Newest First</option>
        <option value="popular">Most Popular</option>
      </select>
    </div>

    <div>
      <button type="submit">Apply</button>
      <button type="reset">Reset</button>
    </div>
  </form>
</search>
```
</details>

---

### Exercise 3: Debounced Live Filter with AbortController and Live Region (Hard)
- **Task**: Write a complete live search widget using HTML and JavaScript that cancels previous pending fetch requests using `AbortController`, debounces keystrokes by 300ms, and updates a polite live region with the result count.

<details>
<summary>View Solution</summary>

```html
<search class="live-search">
  <label for="site-filter">Live Document Search:</label>
  <input type="search" id="site-filter" autocomplete="off" />

  <div id="live-status" role="status" aria-live="polite" class="visually-hidden"></div>
  <ul id="results-target"></ul>
</search>

<script>
  const filterInput = document.getElementById('site-filter');
  const statusEl = document.getElementById('live-status');
  const resultsEl = document.getElementById('results-target');

  let timer = null;
  let abortCtrl = null;

  filterInput.addEventListener('input', () => {
    clearTimeout(timer);
    if (abortCtrl) abortCtrl.abort();

    const query = filterInput.value.trim();
    if (!query) {
      resultsEl.innerHTML = '';
      statusEl.textContent = '';
      return;
    }

    timer = setTimeout(async () => {
      abortCtrl = new AbortController();
      statusEl.textContent = 'Searching...';

      try {
        const res = await fetch(`/api/search?q=${encodeURIComponent(query)}`, {
          signal: abortCtrl.signal
        });
        const data = await res.json();

        resultsEl.innerHTML = '';
        if (data.length === 0) {
          statusEl.textContent = `No results found for "${query}".`;
          return;
        }

        statusEl.textContent = `${data.length} results available.`;
        data.forEach(item => {
          const li = document.createElement('li');
          li.textContent = item.name;
          resultsEl.appendChild(li);
        });
      } catch (err) {
        if (err.name !== 'AbortError') {
          statusEl.textContent = 'Error loading results.';
        }
      }
    }, 300);
  });
</script>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                               SEARCH & FILTER FORMS CHEAT SHEET                                 |
+-------------------------------------------------------------------------------------------------+
| Element / Pattern        | Purpose                                                              |
+-------------------------------------------------------------------------------------------------+
| <search>                 | Native semantic landmark enclosing search controls (Baseline 2023)  |
| <form method="get">      | Serializes queries to URL params (?q=val); allows sharing/bookmarks  |
| <input type="search">    | Native clear button, searchbox ARIA role, mobile search return key   |
| <datalist id="...">      | Zero-JS autocomplete suggestion dropdown                             |
| role="status" aria-live  | Informs screen readers of dynamic result count updates               |
| AbortController          | Prevents race conditions during asynchronous as-you-type search      |
| history.pushState()      | Keeps browser Back/Forward navigation in sync with filter state      |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [WHATWG HTML Living Standard: The `search` element](https://html.spec.whatwg.org/multipage/grouping-content.html#the-search-element)
- [MDN Web Docs: `<search>`](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/search)
- [MDN Web Docs: `<input type="search">`](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/search)
- [W3C WAI-ARIA Authoring Practices Guide: Search Landmark Pattern](https://www.w3.org/WAI/ARIA/apg/patterns/landmarks/)

---

[← Previous: Form Submission & FormData](11-form-submission-and-formdata.md) | [Section Index](README.md) | [Next: Forms Accessibility & Error States →](13-forms-accessibility-and-error-states.md)
