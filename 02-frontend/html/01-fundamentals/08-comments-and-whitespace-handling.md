# Comments, Whitespace Collapsing, and Preformatted Text

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Fundamental |
| **Prerequisites** | `01-document-structure.md`, `02-syntax-rules-and-error-recovery.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Baseline Status** | Widely Available across all modern browsers |
| **Browsers Tested** | Chrome 120+, Firefox 120+, Safari 17+, Edge 120+ |
| **Tested With** | W3C Nu HTML Checker, Axe-core v4.8, Node.js DOMParser |

---

## Table of Contents
- [1 What & Why](#1-what--why)
- [2 Core Concepts](#2-core-concepts)
- [3 How It Works Internally](#3-how-it-works-internally)
- [4 Syntax & Usage](#4-syntax--usage)
- [5 Practical Examples](#5-practical-examples)
- [6 Best Practices](#6-best-practices)
- [7 Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8 Performance, Accessibility, SEO & Security Notes](#8-performance-accessibility-seo--security-notes)
- [9 Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10 Interview Questions](#10-interview-questions)
- [11 Practice Exercises](#11-practice-exercises)
- [12 Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13 Further Reading](#13-further-reading)

---

## 1 What & Why

In computer programming, source code serves two audiences: the compiler or interpreter that executes the logic, and human software engineers who maintain it. In HTML, **comments** allow authors to annotate markup, document architectural decisions, and temporarily isolate DOM subtrees during local debugging without altering visual output.

Concurrently, the HTML parsing and rendering pipeline employs a sophisticated **whitespace collapsing algorithm**. When an author writes indented, multi-line markup for human readability, the browser does not render dozens of spaces and carriage returns onto the screen. Instead, it systematically normalizes whitespace sequences into single spaces according to the CSS Text Module and WHATWG specifications.

However, naive assumptions about comments and whitespace cause pervasive production bugs:
- **Security Leaks**: Developers leave internal API keys, database schema hints, private endpoints, or staging credentials inside HTML comments, unaware that client-facing comments are transmitted in plaintext across the public network.
- **Layout Bugs**: Unintended inter-element whitespace creates phantom 4px gaps between inline-block navigation items or grid columns.
- **Accurate Preformatting**: When authors actually *do* need to preserve exact tabs, indentation, and spaces (e.g., in ASCII diagrams or computer code), improper use of `<pre>` and the CSS `white-space` property results in clipped layouts or broken formatting.

---

## 2 Core Concepts

### HTML Comments Syntax
An HTML comment begins with the literal string `<!--` and terminates with `-->`:
```html
<!-- This is an authorial comment -->
```
In the WHATWG HTML specification:
- Comments cannot contain the sequence `-->` inside their body.
- In strict HTML syntax, comments cannot contain `--` (double hyphens) anywhere within the comment text, nor can they start with `>` or `->` immediately following `<!--`.
- **HTML comments cannot be nested**. Placing `<!-- <!-- nested --> -->` terminates the entire comment at the very first `-->`, leaving the remaining text exposed as visible content.

### DOM Node Representation: The `Comment` Object
Unlike in compiled programming languages (like C or Rust) where comments are discarded during pre-processing, **HTML comments are preserved in the DOM**.
The HTML parser creates a distinct DOM node of type `Comment` (which inherits from `CharacterData` and `Node`, with `nodeType === Node.COMMENT_NODE` or numerical value `8`). 
Comments can be queried, inspected, created, and mutated dynamically via JavaScript:
```javascript
const comment = document.createComment("Dynamic Telemetry Marker");
document.body.appendChild(comment);
```

### Whitespace Characters Defined
The HTML specification defines **HTML whitespace** as a set of four exact characters:
1. Space (`U+0020`)
2. Tab (`U+0009`, `\t`)
3. Line Feed (`U+000A`, `\n`)
4. Form Feed (`U+000C`, `\f`)
5. Carriage Return (`U+000D`, `\r`)

### The Whitespace Collapsing Rules
By default (under CSS `white-space: normal`):
- Contiguous sequences of multiple whitespace characters (e.g., three tabs, two spaces, and a newline) are collapsed into a **single space character** (`U+0020`).
- Leading and trailing whitespace immediately adjacent to the start or end of a block formatting context is completely stripped (removed).
- Newline characters are treated as regular spaces.

### Preformatted Text (`<pre>`)
The `<pre>` element represents preformatted text. In the browser's default User-Agent stylesheet, `<pre>` has `white-space: pre` and `font-family: monospace`. 
Inside `<pre>`, the layout engine preserves every individual space, tab stop, and newline exactly as typed, and lines only break where an explicit newline character or `<br>` occurs.

---

## 3 How It Works Internally

### Parser Tokenizer: Comment State Machine
The WHATWG HTML tokenizer operates on a state machine specifically handling comments:

```mermaid
stateDiagram-v2
    [*] --> DataState: Reading Markup
    DataState --> TagOpenState: Encounter '<'
    TagOpenState --> MarkupDeclarationOpen: Encounter '!'
    MarkupDeclarationOpen --> CommentStartState: Encounter '--'
    
    CommentStartState --> CommentState: Normal Comment Characters
    CommentState --> CommentEndDashState: Encounter '-'
    CommentEndDashState --> CommentEndState: Encounter '-'
    CommentEndState --> DataState: Encounter '>' (Emit Comment Token)
    CommentEndState --> CommentState: Other characters (Parse Error in strict mode)
```

1. **`<!--`**: Moves the tokenizer through `MarkupDeclarationOpen` into the `Comment` state.
2. **Comment State**: All incoming characters are appended to the comment token's data buffer. No HTML tags or entities are parsed or executed. `<script>` inside a comment is treated as inert text.
3. **`-->`**: The parser exits the comment state and emits a `Comment` node to the tree builder.

### Whitespace Normalization in Layout Engine
During the layout phase (Inline Formatting Context), the layout engine executes the CSS Text Level 3 whitespace processing algorithm:

```
Source HTML:
<div>
  <h1>   System    Status   </h1>
</div>

Layout Engine Transformation:
1. Strips leading whitespace before <h1>.
2. Inside <h1>:
   Raw Text: "\n   System    Status   \n"
3. Collapses multiple spaces between "System" and "Status" -> " "
4. Strips leading space before "System".
5. Strips trailing space after "Status".
6. Final Rendered Text: "System Status"
```

---

## 4 Syntax & Usage

```html
<!-- ================================================================= -->
<!-- SECTION: Cluster Ingress Gateway                                  -->
<!-- Managed by Infrastructure Operations                              -->
<!-- ================================================================= -->

<main>
  <h1>Ingress Telemetry</h1>

  <!-- Multi-line paragraph with intentional human-friendly indentation -->
  <p>
    The cluster routes traffic through edge ingress proxies.
    Even though this sentence is written on multiple source lines,
    the browser engine collapses it into a single flowing line of prose.
  </p>

  <!-- Preformatted Block: Whitespace is preserved verbatim -->
  <pre><code>+-----------------+        +-----------------+
|   Edge Router   | -----> |  Service Mesh   |
|   192.168.1.1   |        |   Port: 8443    |
+-----------------+        +-----------------+</code></pre>
</main>
```

> [!NOTE]
> **Baseline Status: Widely Available**
> HTML comments (`<!-- -->`), the `Comment` DOM node interface, `<pre>`, and CSS `white-space` rules are foundational web standards supported universally across all browsers since the 1990s.

---

## 5 Practical Examples

### Example 1: Architecture Annotation and Code Demarcation
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Comment Structure Guide</title>
</head>
<body>
  <!-- ========================================== -->
  <!-- 1. GLOBAL NAVIGATION BAR                    -->
  <!-- ========================================== -->
  <header>
    <nav aria-label="Global">
      <ul>
        <li><a href="/dashboard">Dashboard</a></li>
        <!-- TODO: Enable billing module once Stripe integration passes QA -->
        <!-- <li><a href="/billing">Billing</a></li> -->
      </ul>
    </nav>
  </header>

  <!-- ========================================== -->
  <!-- 2. PRIMARY DATA CONTAINER                  -->
  <!-- ========================================== -->
  <main id="main-content">
    <h1>Active Node Pool</h1>
  </main>
</body>
</html>
```

### Example 2: ASCII Architecture Diagram in `<pre>`
```html
<figure aria-labelledby="topology-caption">
  <figcaption id="topology-caption">Figure 1: High-Availability Failover Topology</figcaption>
  <pre aria-label="ASCII Diagram showing Primary Node replicating to Secondary Node via Raft protocol">
+---------------------+              +---------------------+
|    Primary Node     |              |   Secondary Node    |
|   (Leader: Active)  |              |  (Follower: Standby)|
+----------+----------+              +----------+----------+
           |                                    ^
           |       Synchronous Raft Log         |
           +------------------------------------+
  </pre>
</figure>
```

### Example 3: Edge Case — Inspecting Comment Nodes via JavaScript DOM API
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>DOM Comment Inspection</title>
</head>
<body>
  <div id="container">
    <!-- build-id: 2026.03.15-rel-49 -->
    <p>Application Content</p>
  </div>

  <script>
    const container = document.getElementById("container");
    
    // Iterate over childNodes (which includes Text, Comment, and Element nodes)
    container.childNodes.forEach(node => {
      if (node.nodeType === Node.COMMENT_NODE) {
        console.log("Found DOM Comment node:", node.nodeValue.trim());
        // Outputs: "Found DOM Comment node: build-id: 2026.03.15-rel-49"
      }
    });
  </script>
</body>
</html>
```

### Example 4: Failure Case — Security Leaks and Phantom Whitespace
```html
<!-- ANTI-PATTERN: DO NOT COPY -->

<!-- Error 1: Leaking sensitive secrets in client-facing HTML comments -->
<!-- DB Connection: postgres://admin:SuperSecret2026@internal-db:5432/prod -->
<!-- Backend API Key: sk_live_94827410938471203498 -->

<!-- Error 2: Malformed double hyphens inside comment -->
<!-- This is an invalid comment -- causing parser confusion -->

<!-- Error 3: Phantom whitespace in inline-block navigation -->
<style>
  .nav-btn { display: inline-block; width: 100px; background: #3b82f6; }
</style>
<nav>
  <a class="nav-btn" href="/one">One</a>
  <a class="nav-btn" href="/two">Two</a>
  <!-- The newline and spaces between </a> and <a class="nav-btn"> render a 4px gap! -->
</nav>
```

### Example 5: Production-Grade Responsive Preformatted Code with Wrapping
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Responsive Preformatted Text</title>
  <style>
    /* Production preformatted styling: Preserves whitespace but WRAPS if viewport is narrow */
    .terminal-output {
      background-color: #0f172a;
      color: #38bdf8;
      padding: 1.25rem;
      border-radius: 6px;
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      font-size: 0.875rem;
      line-height: 1.5;
      
      /* Preserves indentation and newlines, but wraps lines to prevent horizontal page overflow */
      white-space: pre-wrap;
      word-break: break-all;
    }
  </style>
</head>
<body>
  <main>
    <h1>Build Artifact Verification</h1>

    <pre class="terminal-output"><code>$ terraform apply -auto-approve
aws_vpc.production: Refreshing state... [id=vpc-08249821034]
aws_subnet.private_az1: Creating...
aws_subnet.private_az1: Creation complete after 3s [id=subnet-0982310]

Apply complete! Resources: 12 added, 0 changed, 0 destroyed.

Outputs:
cluster_endpoint = "https://k8s-prod-nlb-92847102938410.elb.us-east-1.amazonaws.com:6443"</code></pre>
  </main>
</body>
</html>
```

---

## 6 Best Practices

1. **Never Commit Secrets into HTML Comments**: HTML comments are served in cleartext to all clients. Never include passwords, internal staging URLs, unreleased feature notes, or API tokens in comments.
2. **Use Build-Time Stripping**: Configure production bundlers (e.g., Vite, Webpack, esbuild, Next.js) to automatically strip all HTML comments from production builds to reduce payload size and protect internal details.
3. **Use CSS `white-space: pre-wrap` for Terminal / Code Output**: Standard `<pre>` forces horizontal scrollbars or blows out mobile viewports. Adding `white-space: pre-wrap; word-break: break-all;` preserves line breaks while gracefully wrapping ultra-long tokens.
4. **Beware of Inline-Block Whitespace Gaps**: When using `display: inline-block` on children, any newline or space between closing and opening tags in HTML will render as a ~4px visual gap. Use Flexbox or Grid instead to eliminate whitespace layout artifacts.
5. **Always Pair `<pre>` with `<code>` for Programming Listings**: Use `<pre>` for structural whitespace preservation and wrap the text in `<code>` to convey semantic code meaning.
6. **Label ASCII Art Accessible Figures**: Screen readers attempt to read ASCII punctuation aloud character-by-character (e.g., *"plus minus minus minus vertical bar"*), resulting in an incomprehensible audio drone. Always assign `aria-label` or hide the ASCII art with `aria-hidden="true"` while providing a textual `<figcaption>`.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Bad Code | Good Code | Technical Explanation |
| :--- | :--- | :--- | :--- |
| **Secrets in Comments** | `<!-- Password: dev123 -->` | Store in server environment variables | HTML comments are completely visible to anyone opening DevTools or "View Source". |
| **Attempting to Nest Comments** | `<!-- outer <!-- inner --> still outer? -->` | Remove or refactor inner comments | HTML comments terminate at the very first `-->`, leaving the remaining text exposed as visible content. |
| **Internal `--` in Comments** | `<!-- Step 1 -- Configure DNS -->` | `<!-- Step 1: Configure DNS -->` | Double hyphens inside comments violate strict SGML/XML specifications and trigger parser warnings. |
| **Inline-Block Whitespace Jitter** | `<button>A</button> <button>B</button>` | Use `display: flex; gap: 0.5rem;` | The newline/space between inline elements creates an uncontrollable, font-size-dependent visual gap. |
| **Unwrapped Preformatted Overflow** | `<pre>Long unbroken lines...</pre>` | `pre { white-space: pre-wrap; }` | Without wrapping, long code blocks cause horizontal overflow and mobile layout blowout. |
| **Inaccessible ASCII Diagrams** | `<pre>+---+ \| \| +---+</pre>` | `<figure aria-label="Block diagram"><pre aria-hidden="true">...</pre></figure>` | Screen readers read every punctuation symbol individually, tormenting visually impaired users. |

---

## 8 Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **ASCII Art Accessibility**: ASCII diagrams inside `<pre>` violate WCAG 1.1.1 (Non-text Content) unless accompanied by a text equivalent. Without an accessible alternative, screen readers announce every dash, pipe, and plus sign.
- Comments are completely invisible to the Accessibility Object Model (AOM). Screen readers never announce HTML comments.

### Search Engine Optimization (SEO)
- Search engine spiders (Googlebot) read and parse HTML comments, but give them **zero weight** for keyword ranking.
- Excessive HTML comments bloat the document's total byte size, degrading the **Code-to-Content Ratio** and slowing crawl rates.

### Performance & Byte Weight
- On high-traffic enterprise applications (millions of daily pageviews), leaving 5 KB of developer comments in every HTML response wastes gigabytes of CDN egress bandwidth. Production pipelines should always minify HTML by stripping comments and redundant whitespace.

### Security Notes
- **Source Disclosure**: Attackers inspect HTML comments during reconnaissance to identify software versions (e.g., `<!-- Built with WordPress 6.2 -->`), internal server hostnames, and hidden developer endpoints.
- **Client-Side Framework Injection**: In frameworks like Angular or Vue, malformed comments can occasionally interfere with client-side template hydration.

---

## 9 Comparison / When to Use What

| CSS `white-space` Value | Collapses Whitespace? | Preserves Newlines? | Wraps Lines at Boundary? | Ideal Use Case |
| :--- | :--- | :--- | :--- | :--- |
| `normal` (Default) | Yes | No (Treated as space) | Yes | Standard paragraphs, headings, general prose. |
| `nowrap` | Yes | No | **No** (Never wraps) | Table headers, badges, buttons, single-line tags. |
| `pre` (Default on `<pre>`) | **No** (Preserved) | **Yes** (Preserved) | **No** | Monospace ASCII art, fixed-width columnar logs. |
| `pre-wrap` | **No** (Preserved) | **Yes** (Preserved) | **Yes** (Wraps) | Responsive code blocks, user-generated comments. |
| `pre-line` | **Yes** (Collapsed) | **Yes** (Preserved) | **Yes** (Wraps) | Poetry, multi-line addresses where indentation is unwanted. |
| `break-spaces` | **No** (Preserved) | **Yes** (Preserved) | **Yes** (Wraps at spaces) | Terminal emulators, strict text editors. |

---

## 10 Interview Questions

<details>
<summary><strong>1. Why is nesting HTML comments syntactically impossible, and what does the parser do if you try?</strong></summary>

**Answer:**
In the WHATWG HTML specification, the comment tokenizer is a simple, non-recursive state machine. It begins when it encounters `<!--` and transitions through states looking solely for the exact closing sequence `-->`.

If an author writes:
```html
<!-- Outer comment <!-- Inner comment --> still in comment? -->
```
The parser encounters `<!--`, enters the comment state, and treats `<!-- Inner comment` as literal, meaningless text characters within the comment payload. 

The moment the parser encounters the first `-->` (at the end of the "inner" comment), it **immediately terminates the comment state** and transitions back to the normal Data State. The remaining text (`still in comment? -->`) is now parsed as standard HTML content and rendered directly into the visible document.
</details>

<details>
<summary><strong>2. How are HTML comments represented in the Document Object Model (DOM)?</strong></summary>

**Answer:**
HTML comments are not discarded during DOM tree construction. The parser instantiates a `Comment` node object for every comment encountered. 
- The `Comment` interface inherits from `CharacterData`, which inherits from `Node`.
- Its `nodeType` property is equal to `Node.COMMENT_NODE` (integer value `8`).
- Its text content is accessible via `commentNode.nodeValue` or `commentNode.data`.
- JavaScript can query comments using DOM traversal methods like `TreeWalker` or `NodeIterator`:
```javascript
const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_COMMENT);
while(walker.nextNode()) console.log(walker.currentNode.data);
```
</details>

<details>
<summary><strong>3. Explain how the HTML/CSS whitespace collapsing algorithm handles spaces between `display: inline-block` elements.</strong></summary>

**Answer:**
When elements have `display: inline-block` (or are native inline elements like `<button>` or `<a>`), they participate in an **Inline Formatting Context (IFC)**. In an IFC, the whitespace collapsing algorithm collapses contiguous whitespace characters (including newlines and indentation tabs between tags) into a **single space character (`U+0020`)**.

Because that single space is rendered using the active font metrics, it generates an invisible visual gap (typically 3px to 5px wide) between the two inline-block boxes.
Common solutions:
1. Modern standard: Use Flexbox (`display: flex; gap: ...;`) on the parent container.
2. Markup trick: Remove the whitespace in HTML: `</div><div>` or comment it out: `</div><!-- --><div>`.
3. CSS trick: Set `font-size: 0;` on the parent and restore `font-size` on the children.
</details>

<details>
<summary><strong>4. What were "Conditional Comments" in Internet Explorer, and what is their current status in modern web standards?</strong></summary>

**Answer:**
Conditional comments were a proprietary Microsoft extension introduced in Internet Explorer 5 to target specific versions of IE without relying on CSS selector hacks:
```html
<!--[if IE 6]>
<link rel="stylesheet" href="ie6-fixes.css">
<![endif]-->
```
Non-IE browsers parsed this as a standard HTML comment and ignored the inner markup. In IE, the parser recognized the directive and executed the enclosed HTML.

**Status in Modern Web**: Microsoft completely **deprecated and removed support for conditional comments in Internet Explorer 10** (in standards mode) and modern Microsoft Edge. In the WHATWG HTML Living Standard, conditional comments are parsed as regular, inert HTML comments across all engines.
</details>

<details>
<summary><strong>5. What is the difference between `white-space: pre` and `white-space: pre-wrap`?</strong></summary>

**Answer:**
- `white-space: pre`: Sequences of whitespace are preserved verbatim. Lines are only broken at newline characters in the source or at `<br>` elements. If a single line of text exceeds the viewport or container width, it **never wraps**; it overflows horizontally, requiring scrollbars or clipping.
- `white-space: pre-wrap`: Sequences of whitespace and newlines are preserved verbatim, **but** the layout engine will also break lines automatically at container boundaries when necessary to prevent horizontal overflow.
</details>

<details>
<summary><strong>6. Why should enterprise build pipelines strip HTML comments before deploying to production?</strong></summary>

**Answer:**
There are two primary reasons:
1. **Security / Information Disclosure**: Developers frequently leave internal notes, architectural references, staging URLs, Jira ticket IDs, or legacy commented-out code in templates. Attackers use this reconnaissance data to map internal topologies or find obsolete, vulnerable endpoints.
2. **Network Performance & Bandwidth Costs**: Across millions of HTTP responses, extraneous comments represent useless payload weight that wastes bandwidth, degrades Time to First Byte (TTFB), and increases mobile cellular data consumption.
</details>

<details>
<summary><strong>7. How does the browser parser handle the string `<script>` placed inside an HTML comment?</strong></summary>

**Answer:**
When the HTML tokenizer is in the `Comment State`, all incoming character tokens are consumed into the comment buffer. The parser does **not** recognize tag start tokens. Therefore, writing:
```html
<!-- <script>alert('XSS')</script> -->
```
is completely safe. The `<script>` tag is never created as an `HTMLScriptElement`, and its JavaScript is never executed. It remains purely as character data inside the `Comment` DOM node.
</details>

<details>
<summary><strong>8. What is the effect of `white-space: break-spaces` compared to `white-space: pre-wrap`?</strong></summary>

**Answer:**
Both preserve whitespace and wrap lines. The crucial difference occurs at the end of a line:
- With `pre-wrap`, any sequence of spaces at the end of a line that would overflow into the next line is hanging or collapsed; it does not trigger a line break by itself.
- With `break-spaces`, **every single space character takes up physical space**. If a sequence of spaces reaches the line end boundary, the engine will break the line *in the middle of the spaces*, wrapping the remaining spaces to the start of the following line. This matches terminal emulator behaviors.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Clean Up Problematic Comments
**Task**: Inspect the following markup. Fix all syntax and security issues while preserving valid developer annotations.

```html
<!-- Problematic Source -->
<!-- Version 2.4 -- Deploy Stage: Staging -->
<!-- Database Pass: db_adm_99412! -->
<!-- Outer comment <!-- Inner comment --> Still commented? -->
```

<details>
<summary><strong>Solution</strong></summary>

```html
<!-- Corrected Source -->
<!-- Version 2.4 - Deploy Stage: Staging -->
<!-- Database credentials removed from client-facing markup -->
<!-- Outer comment: Inner comment was removed to prevent premature termination -->
```
*Explanation*:
1. Double hyphens `--` replaced with a single dash `-`.
2. Sensitive database password completely expunged.
3. Nested comment syntax eliminated.
</details>

### Exercise 2 (Easy): Eliminate Whitespace Gap between Buttons
**Task**: Two inline-block `<button>` elements render with a 4px gap between them. Fix the issue using modern CSS without altering the text of the buttons.

```html
<!-- HTML -->
<div class="button-group">
  <button type="button">Edit</button>
  <button type="button">Delete</button>
</div>
```

<details>
<summary><strong>Solution</strong></summary>

```html
<style>
  /* Fix via Flexbox container */
  .button-group {
    display: flex;
    gap: 0; /* Ensures exact 0px spacing */
  }
</style>

<div class="button-group">
  <button type="button">Edit</button>
  <button type="button">Delete</button>
</div>
```
*Explanation*: Switching the container to `display: flex` establishes a Flex Formatting Context, which completely ignores inter-element text whitespace nodes.
</details>

### Exercise 3 (Medium): Accessible ASCII Network Architecture Diagram
**Task**: Create an ASCII diagram inside `<figure>` showing a client connecting to a server. Ensure screen readers do not drone through the punctuation characters.

<details>
<summary><strong>Solution</strong></summary>

```html
<figure aria-labelledby="diagram-caption">
  <figcaption id="diagram-caption">
    Figure 2: TLS Connection Handshake Architecture between Client and Edge Gateway.
  </figcaption>

  <!-- aria-hidden="true" stops screen readers from reading the ASCII dashes and pipes -->
  <pre aria-hidden="true">
[ Browser Client ] -------- HTTPS / TLS 1.3 --------> [ Edge Gateway ]
  </pre>
</figure>
```
*Explanation*: `aria-hidden="true"` removes the confusing ASCII punctuation from the accessibility tree, while `<figcaption>` provides the complete textual description for all users.
</details>

### Exercise 4 (Medium): Build a Responsive Server Log Viewer
**Task**: Write the CSS and HTML for a server log monitor component that preserves log indentation and timestamps while wrapping cleanly on small mobile viewports.

<details>
<summary><strong>Solution</strong></summary>

```html
<section aria-labelledby="log-heading">
  <h2 id="log-heading">Live Cluster Ingress Logs</h2>
  <pre class="log-stream"><code>[2026-03-15T12:00:01.492Z] INFO  ingress-01: Received request POST /v2/telemetry/stream from 192.168.4.110
[2026-03-15T12:00:01.498Z] DEBUG ingress-01: JWT validated subject="svc-telemetry-collector"
[2026-03-15T12:00:01.503Z] WARN  ingress-01: Slow upstream response: duration=104ms target=http://internal-telemetry:8080/ingest</code></pre>
</section>

<style>
  .log-stream {
    background: #111827;
    color: #10b981;
    padding: 1rem;
    border-radius: 6px;
    font-family: monospace;
    font-size: 0.8rem;
    /* Core wrapping properties */
    white-space: pre-wrap;
    word-break: break-all;
    overflow-x: auto;
  }
</style>
```
</details>

### Exercise 5 (Hard): Write a DOM Walker to Extract All HTML Comments
**Task**: Write a JavaScript utility function that scans a given DOM element and returns an array of all comment strings found within its subtree.

<details>
<summary><strong>Solution</strong></summary>

```html
<script>
  function extractComments(rootElement = document.body) {
    const comments = [];
    const walker = document.createTreeWalker(
      rootElement,
      NodeFilter.SHOW_COMMENT,
      null,
      false
    );

    let currentNode = walker.nextNode();
    while (currentNode) {
      comments.push(currentNode.nodeValue.trim());
      currentNode = walker.nextNode();
    }

    return comments;
  }

  // Example verification execution
  document.addEventListener("DOMContentLoaded", () => {
    console.log("Extracted comments:", extractComments());
  });
</script>
```
*Explanation*: `document.createTreeWalker` with `NodeFilter.SHOW_COMMENT` provides high-performance, native C++ iteration over the DOM tree targeting exclusively `Comment` nodes.
</details>

### Exercise 6 (Hard): Predict Rendered Whitespace Output
**Task**: For each of the following snippets, determine the exact rendered visual text:
1. `<span>   Alpha   </span><span>   Beta   </span>`
2. `<div>\n   Hello\n   World\n</div>`
3. `<div style="white-space: pre-line;">\n   Line 1\n   Line 2\n</div>`

<details>
<summary><strong>Solution</strong></summary>

```
1. <span>   Alpha   </span><span>   Beta   </span>
   Rendered: "Alpha Beta" (with exactly one space between them).
   Explanation: Contiguous spaces between inline elements collapse into a single space token; leading and trailing spaces at inline boundaries are normalized.

2. <div>\n   Hello\n   World\n</div>
   Rendered: "Hello World"
   Explanation: Leading/trailing newlines are stripped; the internal newline and spaces between "Hello" and "World" collapse to a single space.

3. <div style="white-space: pre-line;">\n   Line 1\n   Line 2\n</div>
   Rendered:
   Line 1
   Line 2
   Explanation: pre-line preserves newline characters (\n) as line breaks, while collapsing the leading spaces ("   ") on each line down to zero or single space.
```
</details>

---

## 12 Summary / Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                        COMMENTS & WHITESPACE CHEAT SHEET               │
├───────────────────┬────────────────────────────────────────────────────┤
│ Syntax / Rule     │ Technical Requirement                              │
├───────────────────┼────────────────────────────────────────────────────┤
│ Comment Format    │ <!-- Comment Text -->                              │
│ Comment Nesting   │ IMPOSSIBLE: First --> terminates comment           │
│ DOM Node Type     │ Node.COMMENT_NODE (type 8)                         │
│ Production Policy │ Strip all comments during build minification       │
│ Whitespace Norm   │ Contiguous spaces/tabs/newlines collapse to 1 space│
│ Inline-Block Gap  │ Solve via display: flex; gap: 0;                   │
│ <pre> Element     │ Preserves whitespace verbatim (white-space: pre)   │
│ Responsive Code   │ pre { white-space: pre-wrap; word-break: break-all}│
└───────────────────┴────────────────────────────────────────────────────┘
```

---

## 13 Further Reading

- [WHATWG HTML Living Standard: Section 13.1.6 Comments](https://html.spec.whatwg.org/multipage/syntax.html#comments)
- [W3C CSS Text Module Level 3: White Space Processing](https://www.w3.org/TR/css-text-3/#white-space-processing)
- [MDN Web Docs: The Comment Interface](https://developer.mozilla.org/en-US/docs/Web/API/Comment)
- [MDN Web Docs: CSS white-space Property](https://developer.mozilla.org/en-US/docs/Web/CSS/white-space)
- [CSS-Tricks: Fighting the Space Between Inline-Block Inputs](https://css-tricks.com/fighting-the-space-between-inline-block-elements/)

---

| [Previous: 07 Character Encoding & Entities](07-character-encoding-and-entities.md) | [Section Index](README.md) | [Next: 09 Global Attributes](09-global-attributes.md) |
