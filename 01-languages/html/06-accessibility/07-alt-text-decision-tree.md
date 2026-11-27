# Alt Text Decision Tree & Complex Media

| Metadata | Details |
|:---|:---|
| **Level** | Intermediate |
| **Prerequisites** | `01-fundamentals/06-images-and-alt-text.md`, `06-accessibility/01-accessibility-fundamentals-and-wcag.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Universal HTML Image Standards) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA 2024.1, JAWS 2024, VoiceOver, Chrome DevTools |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
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

### What is Alternative Text (Alt Text)?
The `alt` attribute on an `<img>` element provides a programmatic text alternative for non-text visual content. When authored correctly, it allows blind individuals using screen readers, braille displays, and search engine crawlers to perceive the same information and purpose as sighted visitors.

### Why Alt Text is Critical (WCAG 1.1.1)
Under **WCAG Success Criterion 1.1.1 (Non-text Content - Level A)**:
> *"All non-text content that is presented to the user has a text alternative that serves the equivalent purpose."*

The WebAIM Million audit consistently reveals that **missing alternative text is the single most common accessibility barrier on the internet**, affecting over 55% of the world's most visited homepages. Missing alt text results in screen readers reading aloud raw, cryptic file URLs (such as *"slash images slash assets slash uuid dash 492049 dot png"*), creating a jarring and unintelligible user experience.

---

## 2. Core Concepts

### The W3C Alt Text Decision Tree
Writing good alt text is an exercise in determining **contextual intent**, not literal physical description. The W3C Web Accessibility Initiative (WAI) codifies this decision process:

```
                                  Does the image contain text?
                                                │
                                 ┌──────────────┴──────────────┐
                                YES                            NO
                                 │                             │
                     Is text purely decorative?    Does the image trigger an action?
                                 │                 (Inside a link or button)
                    ┌────────────┴────────────┐                │
                   YES                        NO          ┌────┴────┐
                    │                         │          YES        NO
              Use alt=""              Put exact text     │          │
              (Null alt)              in alt attribute   │     Does the image convey
                                                         │     information?
                                            Use alt describing │
                                            action/destination ├────────────┬────────────┐
                                                               │            │            │
                                                              YES        COMPLEX       PURELY
                                                               │         (Charts)    DECORATIVE
                                                               ▼            │            │
                                                        Short, punchy       ▼            ▼
                                                        alt description  Brief alt +   Use alt=""
                                                                         <figcaption>  (Null alt)
                                                                         or data table
```

### The Five Functional Image Categories
1. **Decorative Images**: Background textures, corner flourishes, abstract spacers, or hero illustrations that repeat information already stated in adjacent headings. Must have `alt=""` (Null Alt).
2. **Informative Images**: Photos, editorial illustrations, or diagrams that convey new facts. Must have a concise description of the information (e.g., `alt="Quarterly revenue grew by 18% in Q3"`).
3. **Functional Images**: Icons or photos inside `<a>` or `<button>` elements. Must describe the *action* or *destination*, not the graphic (e.g., a printer icon in a button must have `alt="Print document"`, never `alt="Printer"`).
4. **Images of Text**: Screenshots of typography or logos. If typography cannot be avoided, the `alt` must match the visual text verbatim.
5. **Complex Images**: Data visualizations, scatter plots, financial charts, or anatomical diagrams. Must have a short `alt` summary combined with a comprehensive textual data breakdown in the surrounding markup (e.g., `<figure>` + `<figcaption>` + accessible `<table>`).

---

## 3. How It Works Internally

### Browser Engine Image Parsing & AOM Mapping
When the browser's HTML parser encounters an `<img>` tag, it evaluates the `alt` attribute to construct the corresponding node in the Accessibility Object Model (AOM):

```
Scenario A: Missing alt attribute [<img src="hero.jpg">]
      │
      ├─► Accessibility Engine detects violation (Unlabeled Graphic)
      ├─► Assigns role: "image"
      └─► Fallback Name: Extracts raw URL / filename ("hero.jpg")
          Result: Screen reader speaks "Graphic: hero dot j-p-g"

Scenario B: Decorative null alt [<img src="decor.svg" alt="">]
      │
      ├─► Accessibility Engine detects intentional null text alternative
      └─► Node is completely PRUNED from the Accessibility Tree!
          Result: Screen reader skips element entirely; silence.

Scenario C: Informative alt [<img src="chart.png" alt="Sales up 20%">]
      │
      ├─► Assigns role: "image"
      └─► Accessible Name computed: "Sales up 20%"
          Result: Screen reader speaks "Sales up 20%, graphic"
```

### Alt vs Title vs Aria-label on Images
- `alt="..."`: The native, canonical text alternative recognized across 100% of assistive technologies and web crawlers.
- `title="..."`: Creates a hover desktop tooltip. Does *not* work on touchscreens and should never replace `alt`.
- `aria-label="..."`: Overrides `alt` in the AccName calculation, but is redundant on native `<img>` elements. Always prefer native `alt`.

---

## 4. Syntax & Usage

### Core Image Syntax Reference

```html
<!-- 1. Decorative Image (Pruned from AOM) -->
<img src="divider.svg" alt="" width="800" height="2">

<!-- 2. Informative Editorial Photo -->
<img src="steve-jobs.jpg" alt="Steve Jobs unveiling the original iPhone in 2007" width="600" height="400">

<!-- 3. Functional Icon inside Link -->
<a href="/cart">
  <img src="cart-icon.svg" alt="View Shopping Cart (3 items)" width="24" height="24">
</a>

<!-- 4. Functional Image with Adjacent Text -->
<a href="/profile">
  <!-- When adjacent visible text exists, the image itself is decorative! -->
  <img src="avatar.jpg" alt="" width="32" height="32">
  <span>Sarah Connor</span>
</a>
```

---

## 5. Practical Examples

### Example 1: Basic — The Redundant Repetition Trap
Demonstrating how adjacent text changes an image from informative to decorative:

```html
<!-- BAD: Screen reader speaks "Sarah Connor graphic, Sarah Connor link" -->
<a href="/users/sconnor">
  <img src="sarah.jpg" alt="Sarah Connor" width="40" height="40">
  <span>Sarah Connor</span>
</a>

<!-- GOOD: Image alt is set to null, eliminating redundant chatter -->
<!-- Screen reader speaks: "Sarah Connor link" once cleanly -->
<a href="/users/sconnor">
  <img src="sarah.jpg" alt="" width="40" height="40">
  <span>Sarah Connor</span>
</a>
```

### Example 2: Real-World — Accessible Complex Chart via `<figure>` and `<figcaption>`
A multi-tier financial bar chart providing a visual graph for sighted users and an accessible data table for screen readers and search engines:

```html
<figure class="chart-container">
  <img 
    src="annual-emissions-2025.png" 
    alt="Bar chart showing global carbon emission reductions from 2021 to 2025. Detailed figures in data table below."
    width="700" 
    height="400"
  >
  <figcaption>
    <h3>Global Carbon Reductions (2021–2025)</h3>
    <p>Year-over-year emissions reduction in metric kilotons across enterprise operations.</p>
    
    <!-- Complete accessible text alternative for screen readers -->
    <table>
      <caption>Annual Emission Statistics</caption>
      <thead>
        <tr><th scope="col">Year</th><th scope="col">Emissions (kt)</th><th scope="col">YoY Change</th></tr>
      </thead>
      <tbody>
        <tr><td>2021</td><td>1,420</td><td>Baseline</td></tr>
        <tr><td>2022</td><td>1,310</td><td>-7.7%</td></tr>
        <tr><td>2023</td><td>1,180</td><td>-9.9%</td></tr>
        <tr><td>2024</td><td>990</td><td>-16.1%</td></tr>
        <tr><td>2025</td><td>820</td><td>-17.2%</td></tr>
      </tbody>
    </table>
  </figcaption>
</figure>
```

### Example 3: Edge Case — Functional Image Button with Icon
When an image is the sole child of a button, its `alt` text provides the Accessible Name for the button:

```html
<!-- Accessible Name of button resolves directly to the inner image's alt text -->
<button type="button" class="btn-icon" onclick="downloadReport()">
  <img src="download.svg" alt="Download Annual Financial Report (PDF)" width="20" height="20">
</button>
```

### Example 4: Failure Case — Broken Alt Text Anti-Patterns
Common implementation disasters:

```html
<!-- BROKEN: Missing alt attribute entirely -->
<!-- Screen reader announces: "Graphic, https://cdn.example.com/assets/img_8923.jpg" -->
<img src="https://cdn.example.com/assets/img_8923.jpg">

<!-- BROKEN: Redundant filler words -->
<!-- Screen reader already says "graphic", so this announces "Graphic, Graphic photo of CEO" -->
<img src="ceo.jpg" alt="Graphic photo image of our CEO John Doe">

<!-- BROKEN: Keyword stuffing for SEO (Harms accessibility & penalizes search ranking) -->
<img src="shoes.jpg" alt="shoes running shoes cheap sneakers best athletic footwear discount boots">
```

### Example 5: Accessible Production-Grade — Contextual Alt Text across Editorial Variations
The exact same photo of an electric vehicle requires completely different alt text depending on the editorial context of the article:

```html
<!-- Context 1: Automotive Safety Review -->
<article>
  <h2>Frontal Impact Safety Ratings</h2>
  <img 
    src="ev-crash-test.jpg" 
    alt="Model X cabin remains structurally intact after 40 mph offset frontal barrier impact" 
    width="600" 
    height="350"
  >
  <p>The reinforced passenger cell absorbed over 85% of kinetic energy.</p>
</article>

<!-- Context 2: Exterior Styling & Aerodynamics Article -->
<article>
  <h2>Aerodynamic Design Innovations</h2>
  <img 
    src="ev-crash-test.jpg" 
    alt="Aerodynamic front grille active shutters closed to reduce drag coefficient to 0.23" 
    width="600" 
    height="350"
  >
  <p>The streamlined front fascia channels airflow around the wheel wells.</p>
</article>
```

---

## 6. Best Practices

1. **Every `<img>` Must Have an `alt` Attribute**: Never omit the attribute. If the image is decorative, write `alt=""`.
2. **Omit Redundant Phrases**: Do not write "Image of", "Photo of", or "Graphic of". Screen readers natively announce the element's role (*"graphic"*).
3. **Focus on Function Over Appearance**: In functional links or buttons, write what the control *does* (`alt="Search documentation"`), not what the icon looks like (`alt="Magnifying glass"`).
4. **Use Null Alt (`alt=""`) for Decorative Images**: Null alt instructs the browser to completely remove the node from the Accessibility Tree, silencing screen readers.
5. **Pair Complex Charts with Semantic Tables**: For flowcharts, infographics, and data visualizations, supply a brief summary in `alt` and provide the full data set in a nearby `<table>` or `<figcaption>`.
6. **Keep Informative Alt Text Concise**: Aim for 80 to 120 characters maximum. If an image requires extensive analysis, use a caption or paragraph.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Impact | Correction |
|:---|:---|:---|:---|
| Missing `alt` attribute completely | Critical | Screen reader announces ugly file URL | Add `alt="description"` or `alt=""` |
| Using `alt="image"` or `alt="photo"` | High | Zero informational value; annoys screen reader users | Provide meaningful description or use `alt=""` |
| Image of text without matching alt text | High | Visually impaired users cannot read critical content | Set `alt` to match visible text verbatim |
| Functional icon button with `alt=""` | High | Button becomes unlabelled and unidentifiable | Provide descriptive action (e.g. `alt="Settings"`) |
| Redundant alt text next to identical visible text | Medium | Causes screen reader to stutter duplicate text | Set image `alt=""` |
| SEO keyword stuffing in `alt` | Medium | Inaccessible and triggers Google spam penalties | Write natural, human-centered descriptions |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Zero-CLS Intrinsic Dimensions
Always pair `alt` attributes with explicit `width` and `height` attributes on the `<img>` tag:
`<img src="photo.jpg" alt="Mountain summit at sunrise" width="800" height="600" loading="lazy">`
Explicit dimensions reserve layout aspect-ratio space in the Render Tree before pixels download, preventing Cumulative Layout Shift (CLS) violations.

### SEO Synergies
Google Images indexes alternative text directly into its search graph. Descriptive, context-rich alt text allows your images to rank in image search and reinforces the topical relevance of the host article.

### Security: Attribute Breakout Attacks
When image alt attributes are rendered from dynamic user database input (e.g., `<img src="avatar.png" alt="<%= userBio %>">`), unescaped double quotes will terminate the attribute string and permit attribute-based XSS attacks (e.g., `alt="" onerror="stealCookies()"`). Always escape HTML attributes.

---

## 9. Comparison / When to Use What

### Image Accessibility Attributes Comparison

| Attribute / Technique | Visual Effect | Screen Reader Behavior | When to Use |
|:---|:---|:---|:---|
| **`alt="description"`** | None (Unless image fails) | Speaks description + "graphic" | Informative editorial photos, diagrams |
| **`alt=""` (Null alt)** | None | Completely skipped (silent) | Decorative graphics, spacers, redundant icons |
| **`title="tooltip"`** | Tooltip on mouse hover | Inconsistent / often ignored | Desktop hover advisory only (Never primary a11y) |
| **`aria-hidden="true"`** | None | Pruned from AOM | Best for decorative inline SVG icons |
| **CSS `background-image`** | Pure visual styling | Pruned from AOM | Strictly decorative background patterns |

---

## 10. Interview Questions

### Q1: What is the difference between an `<img>` tag with `alt=""` versus an `<img>` tag with no `alt` attribute at all?
<details>
<summary>View Answer</summary>

- `alt=""` (Null Alt): Explicitly signals to the browser's accessibility engine that the image is **purely decorative**. The browser prunes the image from the Accessibility Tree, and screen readers skip over it in complete silence.
- Missing `alt` attribute (e.g., `<img src="pic.jpg">`): Represents an **accessibility violation**. Because the browser does not know if the image is important, it retains the image in the Accessibility Tree and falls back to reading the raw image file name or URL path aloud to the user (e.g., *"Graphic: p-i-c dot j-p-g"*).
</details>

### Q2: Why should you avoid starting alt text with phrases like "Image of" or "Photo of"?
<details>
<summary>View Answer</summary>

Screen readers automatically announce the element's role (*"graphic"* or *"image"*) based on the HTML `<img>` tag. 

If an author writes `alt="Photo of golden retriever"`, the screen reader announces:
*"Graphic, Photo of golden retriever"*.
The author's prefix creates redundant, annoying repetition. Authors should write `alt="Golden retriever playing fetch in the park"`.
</details>

### Q3: When an image is inside an `<a>` link with adjacent visible text, how should the image's `alt` attribute be handled?
<details>
<summary>View Answer</summary>

The image should be given a **null alt** (`alt=""`).

For example:
```html
<a href="/profile">
  <img src="avatar.jpg" alt="" width="32" height="32">
  <span>Sarah Connor</span>
</a>
```
If the image had `alt="Sarah Connor"`, screen readers would announce *"Sarah Connor graphic, Sarah Connor link"*, repeating the name twice. Setting `alt=""` allows the link text to serve as the sole accessible name.
</details>

### Q4: How should complex data visualizations (e.g., multi-series financial charts) be made accessible?
<details>
<summary>View Answer</summary>

Complex images require a **two-part text alternative**:
1. A short `alt` attribute providing an executive summary and pointing to the detailed breakdown (e.g., `alt="Quarterly revenue chart showing 15% increase in 2025. Detailed data provided in table below."`).
2. A full, structured text alternative located in the surrounding markup—ideally a semantic `<table>` with proper headers (`<th scope="col">`) inside a `<figure>` and `<figcaption>`.
</details>

### Q5: What is the rule for alt text on a functional icon button (e.g., a trash can icon)?
<details>
<summary>View Answer</summary>

The alt text must describe the **intended action**, not the visual appearance of the artwork.
- **Incorrect**: `<button><img src="trash.svg" alt="Trash can"></button>`
- **Correct**: `<button><img src="trash.svg" alt="Delete customer account"></button>`
</details>

### Q6: Can `aria-label` or `aria-labelledby` be used on an `<img>` tag instead of `alt`?
<details>
<summary>View Answer</summary>

While modern browser engines will compute the Accessible Name using `aria-label` or `aria-labelledby` if present on an `<img>` tag (due to the AccName resolution cascade), **it is considered poor practice**. 

The native HTML `alt` attribute has been universally supported across all browsers and assistive technologies for over three decades, functions when JavaScript is disabled, and displays fallback text if the image fails to download. Always use native `alt`.
</details>

### Q7: What is an "Image of Text", and why does WCAG Success Criterion 1.4.5 discourage it?
<details>
<summary>View Answer</summary>

An "Image of Text" is a raster image (JPEG, PNG) used to display textual information instead of native HTML typography styled with CSS.

WCAG 1.4.5 (Level AA) discourages this because:
1. Bitmap text pixelates and blurs when zoomed by low-vision users.
2. Users cannot customize font family, colors, line-spacing, or high-contrast settings.
3. It increases page payload and delays rendering.
Native HTML and web fonts should always be used instead, except for essential brand logos.
</details>

### Q8: What happens if an image with `alt=""` also has a `title` attribute?
<details>
<summary>View Answer</summary>

If an image has `alt=""` and a non-empty `title="..."`, the browser experiences an accessibility conflict. Under the AccName specification, some browsers will promote the `title` attribute to become the accessible name, undoing the intention of `alt=""` and exposing the decorative image to screen readers. Never mix `alt=""` with a `title` attribute.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Fixing an Unlabeled Decorative Spacer
**Problem**: An HTML email uses a 1px decorative divider image `<img src="divider.png">`. Provide the correct markup so screen readers ignore it.

<details>
<summary>View Solution</summary>

```html
<!-- Null alt silences screen readers and removes node from AOM -->
<img src="divider.png" alt="" width="600" height="1">
```
</details>

### Exercise 2: Easy — Authoring Functional Alt Text
**Problem**: Write the HTML for a print button containing an `<img>` icon. Ensure the alt text describes the function.

<details>
<summary>View Solution</summary>

```html
<button type="button" onclick="window.print()">
  <img src="printer.svg" alt="Print this invoice" width="20" height="20">
</button>
```
</details>

### Exercise 3: Medium — Cleaning Up Redundant Link Imagery
**Problem**: Refactor this card link to eliminate redundant screen reader announcements:

```html
<a href="/products/aurora">
  <img src="aurora-headphones.jpg" alt="Aurora Wireless Noise Cancelling Headphones">
  <h3>Aurora Wireless Noise Cancelling Headphones</h3>
  <p>$299.00</p>
</a>
```

<details>
<summary>View Solution</summary>

```html
<!-- Set image alt to null because the heading already provides the exact name -->
<a href="/products/aurora">
  <img src="aurora-headphones.jpg" alt="" width="300" height="200">
  <h3>Aurora Wireless Noise Cancelling Headphones</h3>
  <p>$299.00</p>
</a>
```
</details>

### Exercise 4: Medium — Tracing the Contextual Alt Text Winner
**Problem**: A cooking blog features a photo of diced yellow onions in a recipe article. Which alt text is best suited?
- Option A: `alt="Yellow onion cut into small half-inch squares on wooden cutting board"`
- Option B: `alt="Finely dice one large yellow onion"`
- Option C: `alt="Diced onions"`

<details>
<summary>View Solution</summary>

**Option B** is best in the context of an instructional cooking recipe, because it directly instructs the user on the intended culinary outcome.
Option C is acceptable for a generic glossary, while Option A is overly verbose.
</details>

### Exercise 5: Hard — Accessible Architecture for an Infographic
**Problem**: Write the complete semantic HTML for an infographic showing a 3-step solar panel installation process using `<figure>`, `<figcaption>`, and an ordered list.

<details>
<summary>View Solution</summary>

```html
<figure class="infographic-wrapper">
  <img 
    src="solar-install-steps.png" 
    alt="Infographic illustrating the 3 stages of home solar panel installation: Site Assessment, Roof Mounting, and Grid Interconnection. Steps detailed below."
    width="800" 
    height="450"
  >
  <figcaption>
    <h3>3-Stage Home Solar Installation Workflow</h3>
    <ol>
      <li>
        <strong>1. Site Assessment:</strong> A structural engineer inspects roof rafters and measures annual solar irradiance.
      </li>
      <li>
        <strong>2. Roof Mounting:</strong> Certified installers attach flashing brackets and secure photovoltaic panels to the racking system.
      </li>
      <li>
        <strong>3. Grid Interconnection:</strong> The local utility company installs a bidirectional net meter and activates the system.
      </li>
    </ol>
  </figcaption>
</figure>
```
</details>

### Exercise 6: Hard — Automated DOM Audit for Missing Alt Attributes
**Problem**: Write a vanilla JavaScript function that checks every `<img>` on the page, logs an error for any image missing the `alt` attribute entirely, and logs a warning for images with suspicious filenames in their alt text (like ending in `.jpg` or `.png`).

<details>
<summary>View Solution</summary>

```javascript
function auditImageAccessibility() {
  const images = document.querySelectorAll('img');
  let errors = 0;
  let warnings = 0;

  images.forEach((img, idx) => {
    // 1. Check for missing alt attribute
    if (!img.hasAttribute('alt')) {
      console.error(`[A11y Error] Image #${idx} (${img.src}) is missing the alt attribute entirely!`);
      errors++;
      return;
    }

    const altVal = img.getAttribute('alt').trim();

    // 2. Check for suspicious file extensions in alt text
    if (/\.(jpg|jpeg|png|webp|gif|svg)$/i.test(altVal)) {
      console.warn(`[A11y Warning] Image #${idx} has file extension in alt text: "${altVal}"`);
      warnings++;
    }

    // 3. Check for redundant filler phrases
    if (/^(image of|photo of|picture of)\s+/i.test(altVal)) {
      console.warn(`[A11y Warning] Image #${idx} contains redundant prefix: "${altVal}"`);
      warnings++;
    }
  });

  console.log(`Image Audit Complete: ${errors} errors, ${warnings} warnings.`);
}

auditImageAccessibility();
```
</details>

---

## 12. Summary / Cheat Sheet

- **WCAG 1.1.1 (Non-text Content)**: All non-text content must have a text alternative.
- **Decorative Images**: Use `alt=""` (Null Alt). The browser prunes the element from the AOM.
- **Informative Images**: Short, punchy description of the content and meaning (80-120 chars).
- **Functional Images**: Describe the action or destination (`alt="Search"`, `alt="Print"`).
- **Adjacent Text**: If an image is next to text that says the same thing, set `alt=""` on the image to prevent duplicate speech.
- **Complex Charts**: Short summary in `alt` + full data table in `<figcaption>`.
- **Never Omit `alt`**: Missing `alt` causes screen readers to read the raw file URL.
- **No Filler Words**: Omit "Image of", "Photo of", or "Graphic".

---

## 13. Further Reading

- [W3C WAI: An alt Decision Tree (Official Guide)](https://www.w3.org/WAI/tutorials/images/decision-tree/)
- [WebAIM: Alternative Text Comprehensive Guide](https://webaim.org/techniques/alttext/)
- [MDN Web Docs: The `<img>` element & alt attribute](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/img)
- [Adrian Roselli: Images, Decisions, and Context](https://adrianroselli.com/)

---

[Previous: 06 - Skip Links & Landmarks (06-skip-links-and-landmarks.md)](06-skip-links-and-landmarks.md) | [Section Index](README.md) | [Next: 08 - Accessible Forms & Error Announcement ->](08-accessible-forms-and-error-announcement.md)
