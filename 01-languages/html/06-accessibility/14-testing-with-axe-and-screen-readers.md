# Testing with axe-core & Screen Readers

| Metadata | Details |
|:---|:---|
| **Level** | Advanced |
| **Prerequisites** | `06-accessibility/01-accessibility-fundamentals-and-wcag.md`, `06-accessibility/05-keyboard-navigation-and-focus.md` |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Industry Testing Toolchains & OS Assistive Tech) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA 2024.1, JAWS 2024, VoiceOver, axe-core 4.8, Lighthouse |

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

### The Dual-Pillar Testing Strategy
Engineering accessible software requires a disciplined, two-pillar testing methodology:
1. **Automated Auditing (axe-core, Lighthouse, Linters)**: Fast, programmatic rules checking executed on every commit and CI/CD pull request build.
2. **Manual & Assistive Technology Verification (Keyboard, NVDA, JAWS, VoiceOver)**: Hands-on functional testing that validates the human perceptual and interactive experience.

### The Limits of Automation: The 30–40% Rule
A widespread and dangerous corporate misconception is assuming that achieving a "100% Score" on Google Lighthouse or passing automated axe-core scans means a website is fully accessible.

In reality, **automated accessibility test suites can detect only 30% to 40% of all WCAG Success Criteria**.
- **What Automation CAN Detect**: Missing `alt` attributes, invalid ARIA roles, duplicate IDs, missing `<html lang>`, insufficient programmatic color contrast, unlabelled form inputs.
- **What Automation CANNOT Detect**:
  - Whether image alt text is *accurate and meaningful* (an image of a login screen with `alt="banana"` passes automated checks 100%).
  - Whether keyboard tab order follows a *logical reading sequence*.
  - Whether custom modal dialogs trap focus and close on `Escape`.
  - Whether dynamic live region updates make sense in context.
  - Whether video captions synchronize with real dialogue.

Automated tools catch the syntax bugs; manual screen reader testing verifies the actual human experience.

---

## 2. Core Concepts

### The Screen Reader Landscape
Screen readers are complex software applications that bridge the operating system's platform accessibility APIs to text-to-speech engines and refreshable braille displays:

```
+--------------------------------------------------------------------------+
|                        PRIMARY SCREEN READERS                            |
+--------------------------------------------------------------------------+
| Tool       | Operating System | Engine / Browser Pairing | License       |
+------------+------------------+--------------------------+---------------+
| NVDA       | Windows          | Firefox / Chrome / Edge  | Free, Open Src|
| JAWS       | Windows          | Chrome / Edge / Firefox  | Commercial    |
| VoiceOver  | macOS & iOS      | Safari (WebKit)          | Built-in (OS) |
| TalkBack   | Android          | Chrome (Blink)           | Built-in (OS) |
| Orca       | Linux (GNOME)    | Firefox / Chromium       | Free, Open Src|
+--------------------------------------------------------------------------+
```

### Screen Reader Operating Modes: Browse Mode vs Focus Mode
When using NVDA or JAWS on Windows, the screen reader operates in two distinct modes:
1. **Browse Mode (Virtual Cursor)**: The default reading mode. Sighted keys act as navigation hotkeys:
   - Pressing **`H`** jumps to the next heading (`<h1>`-`<h6>`).
   - Pressing **`1`** through **`6`** jumps to headings of that specific rank.
   - Pressing **`D`** jumps to the next landmark (`<main>`, `<nav>`, `<header>`).
   - Pressing **`T`** jumps to the next table; `Ctrl + Alt + Arrows` navigates table cells.
   - Pressing **`K`** jumps to the next hyperlink.
   - Pressing **`F`** jumps to the next form control.
2. **Focus Mode (Forms Mode)**: When the user enters an `<input>`, `<textarea>`, or `<select>`, the screen reader chimes and switches to Focus Mode. Hotkeys are deactivated so typing the letter "H" enters an "H" into the text field rather than jumping to a heading.

---

## 3. How It Works Internally

### How axe-core Evaluates the DOM
**axe-core** (developed by Deque Systems) is the open-source evaluation engine powering Google Lighthouse, Chrome DevTools Issues panel, Microsoft Accessibility Insights, and the axe DevTools browser extension:

```
                      Target Web Page Loaded in Headless Browser
                                          │
                                          ▼
                         axe-core injected into Page Context
                                          │
                   ┌──────────────────────┴──────────────────────┐
                   ▼                                             ▼
       DOM Hierarchy Traversal                      Computed Style & Layout
     - Evaluates parent-child rules               - Computes relative luminance
     - Checks ID uniqueness across DOM            - Measures visual target size
     - Verifies ARIA role validity                - Verifies element visibility
                   │                                             │
                   └──────────────────────┬──────────────────────┘
                                          ▼
                                AccName Engine Check
                        Calculates Accessible Names & Labels
                                          │
                                          ▼
                            Rules & Success Criteria Match
                                          │
                   ┌──────────────────────┼──────────────────────┐
                   ▼                      ▼                      ▼
              PASSES (0)            VIOLATIONS (N)         INCOMPLETE (N)
           Clean compliance       Definitive errors     Requires human review
                                  (Missing alt, etc.)   (Color contrast on img)
```

---

## 4. Syntax & Usage

### Automating axe-core in Playwright E2E Tests
Modern engineering teams run automated accessibility checks directly within Continuous Integration pipelines using **@axe-core/playwright**:

```typescript
// tests/accessibility.spec.ts
import { test, expect } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';

test.describe('Automated Accessibility Suite', () => {
  test('Dashboard page must have zero detectable a11y violations', async ({ page }) => {
    await page.goto('https://staging.example.com/dashboard');
    
    // Wait for dynamic React / Vue components to mount
    await page.waitForSelector('main');

    // Run axe analysis across the complete DOM
    const accessibilityScanResults = await new AxeBuilder({ page })
      .withTags(['wcag2a', 'wcag2aa', 'wcag22aa'])
      .analyze();

    // Assert that violations array is completely empty
    expect(accessibilityScanResults.violations).toEqual([]);
  });
});
```

---

## 5. Practical Examples

### Example 1: Basic — Manual Keyboard Audit Checklist
Before running any screen reader, perform the **"Unplug the Mouse"** test:

```markdown
### 5-Minute Manual Keyboard Walkthrough Protocol
1. Unplug mouse and disable trackpad.
2. Press [Tab] to enter page: Does a high-contrast Skip Link appear?
3. Continue pressing [Tab]: Does focus move in a logical left-to-right, top-to-bottom order?
4. Look at every interactive element: Is the focus ring clearly visible with >= 3:1 contrast?
5. Press [Enter] and [Space] on buttons: Do they trigger actions reliably?
6. Open a modal: Is focus trapped inside? Can you exit with [Escape]?
7. When modal closes: Does focus return to the button that opened it?
8. Shift+Tab backward: Can you navigate all the way to the top without getting stuck?
```

### Example 2: Real-World — Essential NVDA & JAWS Keyboard Reference
The primary keystrokes used by Windows accessibility auditors:

```
+--------------------------------------------------------------------------+
|                  NVDA / JAWS AUDITING KEYBOARD SHORTCUTS                 |
+--------------------------------------------------------------------------+
| Key Combination         | Action                                         |
+-------------------------+------------------------------------------------+
| Insert + Down Arrow     | Say All: Read continuously from current cursor |
| Control                 | Silence: Stop speech immediately               |
| H / Shift + H           | Next / Previous Heading                        |
| 1 to 6                  | Jump to Heading level 1 through 6              |
| D / Shift + D           | Next / Previous Landmark (<main>, <nav>, etc.) |
| K / Shift + K           | Next / Previous Link                           |
| F / Shift + F           | Next / Previous Form field                     |
| T / Shift + T           | Next / Previous Table                          |
| Ctrl + Alt + Arrows     | Navigate 2D cells inside Table                 |
| Insert + F7             | Open Elements List (Headings, Links, Landmarks)|
| Insert + Space          | Toggle between Browse Mode and Focus Mode      |
+--------------------------------------------------------------------------+
```

### Example 3: Edge Case — Testing VoiceOver on macOS
VoiceOver uses the **VO modifier keys** (`Control + Option` or `Caps Lock`):

```
+--------------------------------------------------------------------------+
|                 VOICEOVER (macOS) AUDITING SHORTCUTS                     |
+--------------------------------------------------------------------------+
| Key Combination         | Action                                         |
+-------------------------+------------------------------------------------+
| Cmd + F5                | Turn VoiceOver On / Off                        |
| VO + Right Arrow        | Read next item                                 |
| VO + Left Arrow         | Read previous item                             |
| VO + Space              | Activate button / link                         |
| VO + U                  | Open the VoiceOver ROTOR (Web rotor)           |
| Left / Right in Rotor   | Switch between Headings, Links, Landmarks, Form|
| Up / Down in Rotor      | Navigate through items in current category     |
| VO + Shift + Down Arrow | Interact with sub-component (table, list)      |
| VO + Shift + Up Arrow   | Stop interacting with sub-component            |
+--------------------------------------------------------------------------+
```

### Example 4: Failure Case — Catching the "False Positive Pass" Trap
Demonstrating why automated tools can pass completely broken markup:

```html
<!-- AUTOMATED TOOLS (axe / Lighthouse) SCORE: 100% PASS! -->
<!-- HUMAN SCREEN READER TEST: COMPLETELY UNUSABLE! -->

<article>
  <!-- Passing: <img> has an alt attribute! But text is useless garbage. -->
  <img src="quarterly-report.png" alt="image1">

  <!-- Passing: Button has accessible text! But keyboard user cannot press it -->
  <div class="btn" onclick="saveData()">Submit Form</div>

  <!-- Passing: Links exist! But generic text destroys Rotor navigation -->
  <p>Learn more about our enterprise pricing: <a href="/pricing">Click here</a></p>
  <p>Read about our security encryption: <a href="/security">Click here</a></p>
  <p>View API technical specifications: <a href="/docs">Click here</a></p>
</article>
<!-- In the VoiceOver Links Rotor, the user hears:
     1. "Click here"
     2. "Click here"
     3. "Click here"
     User has zero context for where any link leads! -->
```

### Example 5: Accessible Production-Grade — Custom Test Helper for AccName Assertion
Writing a unit test using `@testing-library/dom` to mathematically verify the computed accessible name of a complex composite widget:

```javascript
import { screen } from '@testing-library/dom';

// Verifying that AccName 1.2 resolution matches engineering expectations
test('Modal confirm button computes accessible name concatenating entity title', () => {
  document.body.innerHTML = `
    <div id="project-42">Project Apollo</div>
    <button 
      type="button" 
      id="delete-btn" 
      aria-labelledby="delete-btn project-42"
    >
      Archive
    </button>
  `;

  const button = screen.getByRole('button', {
    name: 'Archive Project Apollo'
  });

  expect(button).not.toBeNull();
  expect(button.getAttribute('id')).toBe('delete-btn');
});
```

---

## 6. Best Practices

1. **Integrate Automated Testing in CI/CD**: Run axe-core in pull request validation suites to catch regression errors before merging code.
2. **Always Conduct Keyboard-Only Walkthroughs**: Test without a mouse on every new UI feature.
3. **Audit Headings and Landmarks with the Rotor**: Open the VoiceOver Rotor (`VO + U`) or NVDA Elements List (`Insert + F7`) to verify that the document outline reads like a coherent table of contents.
4. **Test Across Multiple Screen Readers**: Test on at least one Windows pair (NVDA + Firefox/Chrome) and one mobile pair (VoiceOver on iOS or TalkBack on Android).
5. **Never Mute the Screen Reader During Testing**: Listen to the speech output with your eyes closed to verify that visual relationships are communicated verbally.
6. **Beware of False Confidence**: Remember that automated tests only catch syntax violations. A clean automated scan is the starting line, not the finish line.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Impact | Correction |
|:---|:---|:---|:---|
| Relying solely on Lighthouse/axe scores | Critical | Leaves 60-70% of accessibility barriers undetected | Perform manual keyboard & screen reader testing |
| Testing screen reader with the mouse | High | Destroys testing validity; masks keyboard traps | Use keyboard shortcuts exclusively |
| Generic link text ("Click here", "More") | High | Screen reader links rotor becomes unusable | Provide descriptive link text (`"Read Pricing"`) |
| Testing only in Chrome DevTools emulation | High | Misses platform-specific OS accessibility API quirks | Test on physical devices with real screen readers |
| Disabling automated rules in CI to pass builds | High | Conceals severe compliance regressions | Fix root cause in HTML markup |
| Testing with CSS turned off only | Medium | Fails to detect CSS-driven `:focus-visible` bugs | Test with full CSS cascade enabled |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Continuous Integration Performance
Running axe-core across 500 web pages can slow down CI pipeline runtimes. Optimize performance by running axe scans against **design system component storybooks** (isolated unit level) and running end-to-end page audits on core business flows (landing, signup, checkout, dashboard).

### SEO Synergy
Manual screen reader testing directly mimics search engine web crawler behavior. Googlebot parses heading outlines, landmark hierarchies, and anchor text exactly like a blind user navigating via the Rotor. Fixing rotor navigation flaws directly optimizes organic search indexation.

---

## 9. Comparison / When to Use What

### Testing Toolchain Comparison

| Tool | Type | Scope | When to Run |
|:---|:---|:---|:---|
| **ESLint (`jsx-a11y`)** | Static Analysis | Code syntax, attributes | On every code keystroke / save |
| **axe-core (Playwright)** | Automated Runtime | DOM, styles, contrast | On every pull request CI build |
| **Lighthouse** | Quick Audit | High-level compliance | During development / release |
| **Keyboard Walkthrough** | Manual Interaction | Tab order, focus, traps | On every feature completion |
| **NVDA / VoiceOver** | Assistive Technology | Full sensory experience | Final QA acceptance gate |

---

## 10. Interview Questions

### Q1: What percentage of accessibility issues can automated testing tools like axe-core and Lighthouse catch, and why?
<details>
<summary>View Answer</summary>

Automated testing tools can detect only approximately **30% to 40%** of all accessibility issues.

Automated tools excel at analyzing static DOM rules (e.g., verifying that `<img>` has an `alt` attribute, ensuring IDs are unique, checking that color contrast exceeds 4.5:1, and validating ARIA attribute names). 

However, they cannot understand context or human experience: they cannot tell if an alt text accurately describes an image, if keyboard navigation order is logical, if a focus trap allows escape via `Esc`, if video captions match spoken audio, or if status messages are announced gracefully.
</details>

### Q2: What is the difference between Browse Mode and Focus Mode in Windows screen readers like NVDA and JAWS?
<details>
<summary>View Answer</summary>

- **Browse Mode (Virtual Cursor)**: The default reading mode used for navigating documents. Alphanumeric keys act as single-key navigation shortcuts (e.g., `H` for headings, `D` for landmarks, `K` for links, `T` for tables).
- **Focus Mode (Forms Mode)**: Triggered when the user enters an interactive form field (`<input>`, `<textarea>`). Single-key navigation shortcuts are disabled so that typing characters inputs text into the form rather than jumping across the document.
</details>

### Q3: What is the "Rotor" in Apple VoiceOver, and how do blind users use it to navigate web pages?
<details>
<summary>View Answer</summary>

The **VoiceOver Rotor** (`Control + Option + U` on macOS, or two-finger rotation gesture on iOS) is a dynamic navigation menu that groups page elements into distinct categorical lists:
- Headings
- Links
- Landmarks
- Form Controls
- Tables

Users spin the rotor to a category (e.g., "Headings") and press the arrow keys to jump directly to any heading in the document, treating it like a generated table of contents.
</details>

### Q4: Why is having multiple links with the text "Read More" or "Click Here" an accessibility barrier?
<details>
<summary>View Answer</summary>

Screen reader users frequently navigate pages out of context by pulling up the **Links List** (using the VoiceOver Rotor or NVDA `Insert + F7`).

If a page has ten article cards each ending in an anchor tag reading "Read More", the Links List displays ten identical entries:
- *Read more*
- *Read more*
- *Read more*
The user has no idea which article any link leads to without closing the rotor and manually reading the surrounding text. Links must be self-describing (e.g., `<a href="...">Read more <span class="sr-only">about Cloud Architecture</span></a>`).
</details>

### Q5: How can automated accessibility tests be integrated into a Playwright or Cypress CI pipeline?
<details>
<summary>View Answer</summary>

Using **`@axe-core/playwright`** (or `cypress-axe`), developers inject the axe-core testing engine into the running browser instance during automated end-to-end tests:
```javascript
const results = await new AxeBuilder({ page })
  .withTags(['wcag2a', 'wcag2aa', 'wcag22aa'])
  .analyze();
expect(results.violations).toEqual([]);
```
If any accessibility violations are found, the test assertion fails, blocking the pull request from merging until the markup is corrected.
</details>

### Q6: What is a "False Positive" and a "False Negative" in accessibility auditing?
<details>
<summary>View Answer</summary>

- **False Positive**: When an automated testing tool reports an accessibility violation on markup that is actually fully accessible to users (rare in modern axe-core, which prioritizes zero false-positive rules).
- **False Negative**: When an automated tool reports that an element passes compliance when it is actually completely broken for real assistive technology users (e.g., passing an empty button that contains an unlabelled icon font, or passing `alt="photo"`).
</details>

### Q7: What are the three essential keystrokes used to test an interactive custom widget with a keyboard?
<details>
<summary>View Answer</summary>

1. **`Tab` / `Shift + Tab`**: Navigate into and out of the component sequentially.
2. **`Enter` / `Space`**: Activate buttons, toggle checkboxes, or expand disclosures.
3. **`Escape`**: Dismiss modals, popovers, or dropdowns and restore focus.
4. **`Arrow Keys`**: Navigate internal child items in composite widgets (tabs, radio groups, sliders).
</details>

### Q8: Why should developers test web applications on real mobile screen readers (VoiceOver on iOS / TalkBack on Android)?
<details>
<summary>View Answer</summary>

Desktop screen readers (NVDA, JAWS) navigate using physical keyboards, whereas mobile screen readers (VoiceOver on iPhone, TalkBack on Android) use **touch-based gesture exploration** (e.g., swiping right to move cursor, double-tapping to activate, two-finger scrubbing to dismiss).

Mobile testing uncovers unique barriers that never manifest on desktop:
- Touch target sizing failures (< 24x24px).
- Gestures hijacked by custom touch event listeners.
- Virtual keyboard obscuring input fields.
- Mobile viewport scaling and responsive layout overlaps.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Fixing Generic Link Text for the Rotor
**Problem**: Fix the following blog excerpt link so it provides full context in the screen reader Links Rotor while preserving clean visual presentation.

```html
<article>
  <h3>Announcing TypeScript 5.5 Support</h3>
  <p>Explore inferred type predicates and ECMAScript regex syntax.</p>
  <a href="/posts/ts-5-5">Read more</a>
</article>
```

<details>
<summary>View Solution</summary>

```html
<article>
  <h3>Announcing TypeScript 5.5 Support</h3>
  <p>Explore inferred type predicates and ECMAScript regex syntax.</p>
  <a href="/posts/ts-5-5">
    Read more <span class="sr-only">about TypeScript 5.5 Support</span>
  </a>
</article>

<style>
  .sr-only {
    position: absolute;
    width: 1px;
    height: 1px;
    padding: 0;
    margin: -1px;
    overflow: hidden;
    clip: rect(0, 0, 0, 0);
    border: 0;
  }
</style>
```
</details>

### Exercise 2: Easy — Simulating Focus Trap Testing
**Problem**: What sequence of keystrokes proves that a modal dialog has an unbreakable focus trap?

<details>
<summary>View Solution</summary>

1. Tab forward through all focusable elements until reaching the **last interactive element** in the modal.
2. Press `Tab` once more: Focus must wrap around and land on the **first interactive element** inside the modal (it must NOT jump to the browser address bar or background page).
3. Press `Shift + Tab`: Focus must wrap backward and land on the **last interactive element** inside the modal.
4. Press `Escape`: The modal must close immediately, and focus must return to the opening trigger button.
</details>

### Exercise 3: Medium — Writing an Automated axe-core Test
**Problem**: Write a Playwright test snippet that visits `/checkout` and verifies that there are no WCAG 2.2 AA violations in the `#payment-form` region.

<details>
<summary>View Solution</summary>

```typescript
import { test, expect } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';

test('Payment form satisfies WCAG 2.2 AA standards', async ({ page }) => {
  await page.goto('/checkout');
  await page.waitForSelector('#payment-form');

  const results = await new AxeBuilder({ page })
    .include('#payment-form')
    .withTags(['wcag2aa', 'wcag22aa'])
    .analyze();

  expect(results.violations).toEqual([]);
});
```
</details>

### Exercise 4: Medium — Identifying Incomplete axe-core Rules
**Problem**: Why does axe-core flag some rules as `"incomplete"` rather than `"violations"` or `"passes"`? Give an example.

<details>
<summary>View Solution</summary>

Axe-core flags a rule as **"incomplete"** when it can programmatically detect that a rule applies, but **cannot conclusively determine pass or fail without human evaluation**.

**Example**: A text element rendered over a complex gradient or background image (`background-image`). The axe-core algorithm cannot know which pixel of the image lies directly beneath each character of text to measure mathematical contrast. It flags the element as `incomplete` and instructs the developer to manually review the color contrast.
</details>

### Exercise 5: Hard — Authoring an In-Page Accessibility Linter Script
**Problem**: Write a vanilla JavaScript function that you can paste into the DevTools console to instantly flag all images without `alt` attributes, all buttons without accessible names, and all form inputs without associated labels.

<details>
<summary>View Solution</summary>

```javascript
function runQuickA11ySanityCheck() {
  console.group('--- QUICK ACCESSIBILITY AUDIT ---');
  let issues = 0;

  // 1. Check Images for alt
  document.querySelectorAll('img').forEach((img, i) => {
    if (!img.hasAttribute('alt')) {
      console.error(`[A11y] Image without alt:`, img);
      issues++;
    }
  });

  // 2. Check Buttons for Accessible Name
  document.querySelectorAll('button').forEach((btn, i) => {
    const name = btn.innerText.trim() || btn.getAttribute('aria-label') || btn.getAttribute('title');
    if (!name) {
      console.error(`[A11y] Unnamed button:`, btn);
      issues++;
    }
  });

  // 3. Check Inputs for Labels
  document.querySelectorAll('input:not([type="hidden"]), select, textarea').forEach((input, i) => {
    const id = input.id;
    const hasLabel = id && document.querySelector(`label[for="${id}"]`);
    const hasAria = input.getAttribute('aria-label') || input.getAttribute('aria-labelledby');
    if (!hasLabel && !hasAria) {
      console.error(`[A11y] Unlabelled form control:`, input);
      issues++;
    }
  });

  console.log(`Audit finished: ${issues} violations found.`);
  console.groupEnd();
}

runQuickA11ySanityCheck();
```
</details>

### Exercise 6: Hard — Conducting a Functional VoiceOver Audit
**Problem**: Describe the step-by-step verification protocol to audit a collapsible accordion widget using Apple VoiceOver on macOS Safari.

<details>
<summary>View Solution</summary>

1. Open Safari and press `Command + F5` to start VoiceOver.
2. Navigate to the accordion trigger using `VO + Right Arrow`.
3. **Verify Initial Announcement**: VoiceOver must speak the button title, its role (*"button"*), and its collapsed state (*"collapsed"*).
4. Press `VO + Space` (or `Enter`) to activate the trigger.
5. **Verify State Mutation**: VoiceOver must announce the new state (*"expanded"*).
6. Press `VO + Right Arrow`: Focus must enter the newly revealed panel content seamlessly.
7. Press `Shift + Tab` back to the trigger and activate it again: VoiceOver must announce *"collapsed"*, and the panel content must disappear from reading traversal.
</details>

---

## 12. Summary / Cheat Sheet

- **The 30-40% Rule**: Automated tools (axe, Lighthouse) detect only ~35% of WCAG issues. Manual testing is mandatory.
- **Unplug the Mouse**: Verify keyboard Tab sequence, `:focus-visible` rings, `Enter`/`Space` activation, and `Esc` dismissal.
- **Screen Reader Hotkeys (NVDA/JAWS)**: `H` (Heading), `D` (Landmark), `K` (Link), `F` (Form), `T` (Table), `Ctrl` (Silence).
- **VoiceOver Hotkeys (macOS)**: `VO + Right/Left` (Read), `VO + Space` (Activate), `VO + U` (Rotor).
- **Continuous Integration**: Use `@axe-core/playwright` to prevent regressions in automated CI builds.
- **Links List Hygiene**: Eliminate generic "Read More" and "Click Here" link text.

---

## 13. Further Reading

- [axe-core Official GitHub Repository & Documentation](https://github.com/dequelabs/axe-core)
- [NVDA Official User Guide (NV Access)](https://www.nvaccess.org/files/nvda/documentation/userGuide.html)
- [Apple VoiceOver User Guide for Mac](https://support.apple.com/guide/voiceover/welcome/mac)
- [WebAIM: Testing with Screen Readers Quick Reference](https://webaim.org/articles/screenreader_testing/)

---

[Previous: 13 - Contrast, Motion & User Preferences (13-contrast-motion-and-user-preferences.md)](13-contrast-motion-and-user-preferences.md) | [Section Index](README.md) | [Next: 15 - Accessibility Audit Checklist & Triage ->](15-accessibility-audit-checklist.md)
