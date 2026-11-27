# Accessibility Audit Checklist & Triage

| Metadata | Details |
|:---|:---|
| **Level** | Advanced |
| **Prerequisites** | `06-accessibility/01-accessibility-fundamentals-and-wcag.md` through `06-accessibility/14-testing-with-axe-and-screen-readers.md` |
| **Estimated Reading Time** | 32 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (WCAG 2.2 Level AA & VPAT 2.5 Standards) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | Enterprise VPAT Audits, axe-core 4.8, NVDA, JAWS, VoiceOver |

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

### What is an Accessibility Audit?
An **Accessibility Audit** is an exhaustive, systematic evaluation of a digital product against formal international accessibility benchmarks—specifically **WCAG 2.2 Level AA**. 

An audit is not a one-time subjective code review. It is an engineering assessment that evaluates raw HTML markup, CSS rendering, keyboard interaction flows, screen reader speech synthesis, and operating system accessibility API bridges.

### Why Structured Auditing and Triage Matter
Enterprise software contracts, public sector procurement (Section 508), and international commerce (European Accessibility Act) legally require proof of accessibility compliance in the form of a **VPAT (Voluntary Product Accessibility Template)** or **Accessibility Conformance Report (ACR)**.

Without a structured audit checklist and an objective triage severity matrix:
- Teams waste hundreds of hours debating whether an issue is a "bug" or a "feature request".
- Critical blocker issues (e.g., keyboard traps, unlabelled login forms) slip into production, exposing the company to devastating civil lawsuits under ADA Title III.
- Engineers attempt to solve non-issues with redundant, dangerous ARIA overrides.

---

## 2. Core Concepts

### The Master WCAG 2.2 Level AA Checklist Matrix
Every digital application targeting enterprise and legal compliance must be evaluated across all 55 Success Criteria comprising WCAG 2.2 Level A and Level AA:

```
+--------------------------------------------------------------------------+
|                  WCAG 2.2 LEVEL AA AUDIT CHECKLIST                       |
+--------------------------------------------------------------------------+
| CRITERION | LEVEL | SHORT NAME              | VERIFICATION TARGET        |
+-----------+-------+-------------------------+----------------------------+
| 1.1.1     | A     | Non-text Content        | All <img> have alt / alt=""|
| 1.2.1     | A     | Audio/Video Only        | Transcripts for podcasts   |
| 1.2.2     | A     | Captions (Prerecorded)  | WebVTT captions on video   |
| 1.2.3     | A     | Audio Description (Alt) | Audio description track    |
| 1.2.4     | AA    | Captions (Live)         | Real-time live broadcast CC|
| 1.2.5     | AA    | Audio Description       | Prerecorded audio desc     |
| 1.3.1     | A     | Info & Relationships    | Semantic tags, th, fieldset|
| 1.3.2     | A     | Meaningful Sequence     | DOM order matches visual   |
| 1.3.3     | A     | Sensory Characteristics | No "Click the green circle"|
| 1.3.4     | AA    | Orientation             | Works portrait & landscape |
| 1.3.5     | AA    | Identify Input Purpose  | autocomplete attributes    |
| 1.4.1     | A     | Use of Color            | Redundant non-color cues   |
| 1.4.2     | A     | Audio Control           | Pause/mute for audio > 3s  |
| 1.4.3     | AA    | Contrast (Minimum)      | >= 4.5:1 text, >= 3:1 large|
| 1.4.4     | AA    | Resize Text             | Zoom to 200% without loss  |
| 1.4.5     | AA    | Images of Text          | Use native HTML typography |
| 1.4.10    | AA    | Reflow                  | 320px width without h-scroll
| 1.4.11    | AA    | Non-text Contrast       | >= 3:1 UI borders & icons  |
| 1.4.12    | AA    | Text Spacing            | Line-height 1.5 override   |
| 1.4.13    | AA    | Content on Hover/Focus  | Dismissible, hoverable tips|
| 2.1.1     | A     | Keyboard                | 100% operable via keyboard |
| 2.1.2     | A     | No Keyboard Trap        | Focus never permanently stuck
| 2.1.4     | A     | Character Key Shortcuts | Remap single-key shortcuts |
| 2.2.1     | A     | Timing Adjustable       | Session timeouts extendable|
| 2.2.2     | A     | Pause, Stop, Hide       | Moving content pausable    |
| 2.3.1     | A     | Three Flashes           | No flashing > 3 per second |
| 2.4.1     | A     | Bypass Blocks           | Skip to main content link  |
| 2.4.2     | A     | Page Titled             | Unique, descriptive <title>|
| 2.4.3     | A     | Focus Order             | Logical sequential Tab flow|
| 2.4.4     | A     | Link Purpose (Context)  | Self-describing anchor text|
| 2.4.5     | AA    | Multiple Ways           | Search bar + sitemap / nav |
| 2.4.6     | AA    | Headings and Labels     | Descriptive headings & forms|
| 2.4.7     | AA    | Focus Visible           | Discernible focus indicator |
| 2.4.11    | AA    | Focus Not Obscured (Min)| Focused item not fully hidden
| 2.5.1     | A     | Pointer Gestures        | Single-pointer alternative  |
| 2.5.2     | A     | Pointer Cancellation    | Up-event activation        |
| 2.5.3     | A     | Label in Name           | AccName contains visual text|
| 2.5.7     | AA    | Dragging Movements      | Single-click button altern. |
| 2.5.8     | AA    | Target Size (Minimum)   | >= 24x24px or spaced offset|
| 3.1.1     | A     | Language of Page        | <html lang="en"> declared  |
| 3.1.2     | AA    | Language of Parts       | <span lang="fr"> for quotes |
| 3.2.1     | A     | On Focus                | Focusing item doesn't submit|
| 3.2.2     | A     | On Input                | Typing input doesn't submit |
| 3.2.3     | AA    | Consistent Navigation   | Nav order same across pages |
| 3.2.4     | AA    | Consistent Identification| Identical icons mean same  |
| 3.2.6     | A     | Consistent Help         | Help links in same position |
| 3.3.1     | A     | Error Identification    | Text errors identified      |
| 3.3.2     | A     | Labels or Instructions  | Clear form field guidance   |
| 3.3.3     | AA    | Error Suggestion        | Advice on how to fix errors |
| 3.3.4     | AA    | Error Prevention        | Confirm financial/legal data|
| 3.3.7     | A     | Redundant Entry         | Auto-populate repeated data |
| 3.3.8     | AA    | Accessible Auth (Min)   | No cognitive memory puzzles |
| 4.1.2     | A     | Name, Role, Value       | Correct AOM attributes      |
| 4.1.3     | AA    | Status Messages         | role="status" / live regions|
+--------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### The Four-Tier Issue Triage Hierarchy
When auditing software, defects are triaged into four objective priority buckets based on user impact:

```
+--------------------------------------------------------------------------+
|                        ACCESSIBILITY TRIAGE MATRIX                       |
+--------------------------------------------------------------------------+
| Severity     | Criteria                               | Release Blocker? |
+--------------+----------------------------------------+------------------+
| P0: Blocker  | Complete task barrier. Prevents user   | YES. Immediate   |
| (Critical)   | from completing core workflow.         | release halt.    |
|              | (e.g., Keyboard trap, unlabelled login)|                  |
+--------------+----------------------------------------+------------------+
| P1: High     | Significant barrier. Task can be       | YES for major    |
| (Severe)     | completed only with extreme difficulty | milestone deploy.|
|              | or confusing workaround.               |                  |
+--------------+----------------------------------------+------------------+
| P2: Medium   | Degraded experience. Causes annoyance  | NO. Scheduled for|
| (Moderate)   | or slower operation, but workflow is   | next sprint.     |
|              | achievable. (e.g., redundant speech).  |                  |
+--------------+----------------------------------------+------------------+
| P3: Low      | Minor polish issue. Technical violation| NO. Backlog.     |
| (Advisory)   | with minimal real-world user impact.   |                  |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### VPAT 2.5 Conformance Levels
When authoring an official Accessibility Conformance Report (VPAT), each WCAG criterion must be assigned one of five formal conformance status definitions:
1. **Supports**: The functionality of the product has at least one method that meets the criterion without known defects.
2. **Partially Supports**: Some functionality of the product does not meet the criterion (requires remediation details).
3. **Does Not Support**: The majority of product functionality fails the criterion.
4. **Not Applicable**: The criterion does not apply (e.g., 1.2.2 Captions on a site with zero video content).
5. **Not Evaluated**: The criterion was excluded from testing scope.

---

## 5. Practical Examples

### Example 1: Basic — Defect Triage Log Entry Template
The standard format engineering teams use to file actionable accessibility bug tickets:

```markdown
### [A11y Defect P0]: Keyboard Trap in Team Invitation Modal

- **WCAG Success Criteria**: 2.1.2 No Keyboard Trap (Level A), 2.4.3 Focus Order (Level A)
- **Component**: TeamInviteDialog.vue / TeamInviteModal.html
- **Environment**: Chrome 120 + NVDA 2024.1 (Windows 11), Safari 17 + VoiceOver (macOS)
- **User Impact**: 
  Keyboard-only and screen reader users cannot exit the invitation modal without refreshing 
  the browser window, causing complete loss of unsaved form state.

#### Steps to Reproduce:
1. Navigate to `/settings/team`.
2. Press `Tab` to "Invite Member" button and press `Enter`.
3. Modal dialog opens.
4. Press `Tab` through email input and invite button.
5. Press `Escape`: Modal does not close.
6. Press `Tab` past the Close button: Focus disappears into the document background.

#### Technical Root Cause:
Custom `<div>` used instead of native HTML `<dialog>`. Missing `keydown` listener for `Escape` 
and missing focus trap loop.

#### Remediation:
Refactor custom modal to native `<dialog>` and open with `dialog.showModal()`.
```

### Example 2: Real-World — Complete Page Audit Walkthrough Protocol
The systematic execution order followed by professional accessibility consultants:

```
STAGE 1: Automated Base Run (10 Minutes)
- Run axe DevTools / Lighthouse scan on target page.
- Log all color contrast failures, missing alt attributes, and duplicate IDs.

STAGE 2: Unplugged Keyboard Audit (15 Minutes)
- Verify Skip Link appears on first Tab.
- Tab through all interactive elements: confirm focus ring visibility.
- Test all buttons with Space/Enter.
- Open and close every modal: verify focus trap and restoration.
- Test 200% page zoom (`Ctrl + Plus`): verify no horizontal scroll at 320px width (1.4.10).

STAGE 3: Screen Reader Audit (25 Minutes)
- Launch NVDA (Windows) or VoiceOver (macOS).
- Open Headings List (Rotor): verify logical H1 -> H2 -> H3 outline.
- Open Landmarks List: verify all regions labeled and only one <main> exists.
- Fill out forms with deliberate errors: verify error summary receives focus and reads aloud.
- Test dynamic cart/search actions: verify aria-live status announcements.

STAGE 4: Report Synthesis & VPAT Compilation (10 Minutes)
- Map defects to WCAG 2.2 Success Criteria.
- Assign severity (P0 - P3).
- Document exact code locations and remediation recommendations.
```

### Example 3: Edge Case — Auditing WCAG 2.2 Target Size Minimum (2.5.8)
Measuring whether icon buttons in a compact toolbar meet the 24x24px physical hit target:

```html
<style>
  /* FAILING: 18x18px hit target with only 2px gap (Violates 2.5.8 Level AA) */
  .bad-toolbar button {
    width: 18px;
    height: 18px;
    margin-right: 2px;
  }

  /* PASSING: Visual icon is 16px, but button hit target is 28x28px (PASSES 2.5.8) */
  .good-toolbar button {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    min-width: 28px;
    min-height: 28px;
    padding: 6px;
    margin-right: 4px;
    background: transparent;
    border: 1px solid #767676;
    border-radius: 4px;
  }
</style>

<div role="toolbar" aria-label="Document Tools" class="good-toolbar">
  <button type="button" aria-label="Zoom in">
    <svg width="16" height="16" aria-hidden="true"><circle cx="8" cy="8" r="6"/></svg>
  </button>
  <button type="button" aria-label="Zoom out">
    <svg width="16" height="16" aria-hidden="true"><line x1="4" y1="8" x2="12" y2="8"/></svg>
  </button>
</div>
```

### Example 4: Failure Case — Incompetent Audit Triage Report
What an unusable, counter-productive audit report looks like:

```markdown
<!-- UNUSABLE BUG TICKET: Lacks WCAG mapping, impact, steps, and remediation -->
Title: "Website is not accessible"
Description: "I ran a tool and got a score of 72. Fix everything so we can get 100."
Severity: "Urgent"
Assignee: Frontend Team
```
*Why this fails*: It provides zero actionable technical reproduction steps, fails to cite WCAG criteria, and offers no remediation guidance.

### Example 5: Accessible Production-Grade — VPAT Table Excerpt
How an enterprise VPAT section is officially formatted:

```markdown
### Section 1194.22 / WCAG 2.2 Level AA Evaluation Table

| Criteria | Conformance Level | Remarks and Explanations |
|:---|:---:|:---|
| **1.1.1 Non-text Content** | Supports | All functional and informative images provide equivalent text alternatives via the `alt` attribute. Decorative imagery utilizes `alt=""`. |
| **1.4.3 Contrast (Minimum)**| Supports | Text styling adheres to a minimum 4.5:1 contrast ratio against backgrounds. Large text exceeds 3:1. Verified via axe-core and manual color sampling. |
| **2.1.1 Keyboard** | Supports | All navigation menus, modal dialogs, and form submission flows are fully operable using keyboard `Tab`, `Enter`, `Space`, and `Arrow` keys. |
| **2.4.7 Focus Visible** | Supports | Interactive controls feature a visible, high-contrast focus indicator using CSS `:focus-visible` with a 3:1 contrast ratio against adjacent surfaces. |
| **3.3.8 Accessible Auth** | Supports | Authentication flows permit copy-and-paste into password fields and natively support browser password managers and WebAuthn passkeys. |
```

---

## 6. Best Practices

1. **Follow the Four-Stage Audit Protocol**: Automated scan &rarr; Keyboard walkthrough &rarr; Screen reader test &rarr; Synthesis report.
2. **File Actionable Bug Tickets**: Every issue must cite the exact WCAG Success Criterion, reproduction steps, screen reader behavior, and concrete code remediation.
3. **Prioritize P0 Blockers Immediately**: Keyboard traps, missing form labels, and inaccessible logins must block production release deployments.
4. **Audit Third-Party Widgets**: Customer support chat widgets, cookie consent banners, and embedded video players must be audited; third-party code does not exempt your site from ADA liability.
5. **Retest After Every Remediation**: Never close an accessibility bug ticket based on code review alone; verify the fix using a real screen reader.
6. **Maintain an Ongoing Accessibility Conformance Report (ACR)**: Keep your VPAT updated quarterly to accelerate enterprise enterprise B2B sales cycles.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Impact | Correction |
|:---|:---|:---|:---|
| Auditing with only one browser engine | Critical | Misses engine-specific bugs (WebKit vs Blink) | Audit across Chrome, Firefox, and Safari |
| Ignoring mobile screen readers (TalkBack/iOS) | High | Leaves touch gesture accessibility broken | Test on physical iOS and Android devices |
| Over-reliance on overlay "a11y widgets" | Critical | Fails compliance; triggers federal lawsuits | Fix issues natively in HTML and CSS source |
| Filing subjective design complaints as WCAG bugs | Medium | Creates engineering friction and fatigue | Anchor all findings strictly to WCAG criteria |
| Omitting keyboard focus restoration checks | High | Users lose place in document after modals | Test focus state upon overlay dismissal |
| Marking criteria "Supports" without manual testing | High | Submits fraudulent VPAT; severe legal risk | Conduct rigorous manual verification |

---

## 8. Performance, Accessibility, SEO & Security Notes

### The "Accessibility Overlay" Fallback Trap
Third-party "accessibility overlay" scripts (floating toolbar widgets claiming to use "AI" to make sites compliant with one line of JavaScript) **do not satisfy WCAG requirements and do not protect against lawsuits**. Over 30% of all US federal ADA digital lawsuits in 2023-2025 specifically targeted companies using accessibility overlays. Real compliance requires semantic HTML authored correctly at the source code level.

### Auditing Security Boundaries
During an audit, verify that accessibility attributes are not vulnerable to XSS injection. Ensure that search query reflections inside `aria-label` or live region status messages are strictly sanitized.

---

## 9. Comparison / When to Use What

### Audit Severity Classification Guide

| Severity | Criteria Example | User Experience Impact | SLA for Fix |
|:---|:---|:---|:---|
| **P0 (Blocker)** | Keyboard trap in checkout modal | User cannot complete purchase | 24–48 hours (Hotfix) |
| **P1 (High)** | Credit card input lacks label | User struggles; requires guess | Current Sprint |
| **P2 (Medium)**| Breadcrumb missing `aria-current`| Functional, but context degraded| Next Sprint |
| **P3 (Low)** | Redundant role on `<nav role="nav">`| Zero user-facing barrier | Backlog |

---

## 10. Interview Questions

### Q1: What is a VPAT, and what is its role in software procurement?
<details>
<summary>View Answer</summary>

A **VPAT (Voluntary Product Accessibility Template)** is a standardized document created by the Information Technology Industry Council (ITI). It is used to produce an **Accessibility Conformance Report (ACR)** evaluating how thoroughly a digital product adheres to accessibility standards:
- Section 508 (US Federal government).
- WCAG 2.1 / 2.2 (Global web standards).
- EN 301 549 (European Union).

Enterprise B2B clients, universities, and government agencies legally mandate a completed VPAT before purchasing commercial software licenses to ensure the vendor's platform does not violate disability laws.
</details>

### Q2: Why are third-party "accessibility overlay" plugins widely rejected by accessibility professionals and disability advocates?
<details>
<summary>View Answer</summary>

Accessibility overlays (automated JS widgets injected via a `<script>` tag):
1. **Cannot fix underlying architecture**: They cannot retroactively repair broken DOM source order, write meaningful image descriptions, or fix complex custom keyboard widgets.
2. **Interfere with real assistive technology**: Blind users already have specialized screen readers (NVDA, JAWS). Overlays frequently inject buggy custom hotkeys that hijack native screen reader commands.
3. **Zero legal protection**: Hundreds of businesses with active overlay subscriptions have been sued and settled in federal court because courts evaluate the actual human experience, which remains broken.
True accessibility must be built into the semantic HTML and CSS source code.
</details>

### Q3: Explain the difference between WCAG 2.2 Criteria 2.4.11 (Focus Not Obscured - Minimum) and 2.4.12 (Focus Not Obscured - Enhanced).
<details>
<summary>View Answer</summary>

- **2.4.11 Focus Not Obscured (Minimum) (Level AA)**: When a user interface component receives keyboard focus, it must not be **entirely hidden** behind author-created content (such as sticky headers, floating footers, or non-modal chat widgets). At least a portion of the focused element must remain visible.
- **2.4.12 Focus Not Obscured (Enhanced) (Level AAA)**: When a component receives keyboard focus, **no portion of it whatsoever** may be obscured by author overlays.
</details>

### Q4: What is the recommended testing scope for an accessibility audit of a large web application?
<details>
<summary>View Answer</summary>

Rather than attempting to audit every single page in a 10,000-page application:
1. **Design System Components**: Audit all core primitive components (buttons, dialogs, inputs, accordions, tabs, tables).
2. **Core User Journeys**: Audit complete end-to-end paths (Homepage &rarr; Search &rarr; Product Selection &rarr; Checkout &rarr; Confirmation).
3. **Key Architectural Layouts**: Audit representative templates (Landing Page, Dashboard Shell, Form Submission, Help / Documentation, Error 404).
</details>

### Q5: How do you verify WCAG 1.4.10 (Reflow - Level AA)?
<details>
<summary>View Answer</summary>

To test Reflow:
1. Set the browser viewport width to **1280 CSS pixels** at **400% zoom** (which equates to an effective CSS layout width of **320 pixels** at 100% zoom).
2. Inspect the page: Content must reflow into a single vertical column without loss of information or functionality.
3. **Pass Criteria**: The user must **not** be required to scroll horizontally in two dimensions (except for complex data tables, maps, and code blocks which are explicitly exempt).
</details>

### Q6: What does WCAG 2.5.7 (Dragging Movements - Level AA) require?
<details>
<summary>View Answer</summary>

WCAG 2.5.7 (new in WCAG 2.2) mandates that any functionality that relies on a **dragging movement** (e.g., drag-and-drop kanban boards, slider thumbs, reordering lists) must have an **equivalent single-pointer alternative**, unless dragging is essential.

For example, a sortable list that supports dragging items must also provide "Move Up" and "Move Down" click buttons.
</details>

### Q7: What makes an accessibility bug report actionable for an engineering team?
<details>
<summary>View Answer</summary>

An actionable report must contain:
1. **Specific WCAG Criterion mapping** (e.g., *WCAG 1.3.1 Info and Relationships*).
2. **Defect Severity** (P0 Blocker, P1 High, etc.).
3. **Exact URL and DOM component locator**.
4. **Environment tested** (e.g., *Windows 11 + Chrome 120 + NVDA 2024.1*).
5. **Precise reproduction steps**.
6. **Expected vs Actual behavior** (with screen reader transcript).
7. **Concrete technical remediation recommendation** (e.g., *Wrap radios in `<fieldset>` + `<legend>`*).
</details>

### Q8: How should an auditor test an application for WCAG 2.5.8 (Target Size - Minimum)?
<details>
<summary>View Answer</summary>

1. Inspect interactive elements (buttons, links, icons) using DevTools Box Model.
2. Measure the physical clickable area: it must be at least **24 by 24 CSS pixels**.
3. **Spacing Exception**: If a target is smaller than 24x24px, it still passes if an imaginary 24px diameter circle centered over the target does not intersect any adjacent target.
4. **Inline Text Exception**: Links inside a running paragraph of text are explicitly exempt.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Classifying an A11y Defect
**Problem**: An auditor discovers that the "Submit Application" button on a credit card application form cannot be activated using the keyboard `Space` or `Enter` keys. Assign the WCAG Criterion and Triage Severity.

<details>
<summary>View Solution</summary>

- **WCAG Success Criterion**: **2.1.1 Keyboard (Level A)**
- **Triage Severity**: **P0 (Blocker / Critical)**
- **Justification**: Users relying on keyboards or assistive devices are completely blocked from completing the primary business workflow of the application.
</details>

### Exercise 2: Easy — Writing an Actionable Defect Title
**Problem**: Rewrite this vague bug title into an actionable engineering ticket title:
*"The menu is broken on my Mac"*

<details>
<summary>View Solution</summary>

**[A11y P1]: Header Navigation Submenu lacks aria-expanded and fails to open with VoiceOver on macOS Safari (WCAG 4.1.2)**
</details>

### Exercise 3: Medium — Remediation for a 2.5.8 Target Size Failure
**Problem**: A toolbar has icon buttons styled with `width: 16px; height: 16px; padding: 0;`. Write the CSS to enlarge the clickable hit target to 28x28px without enlarging the 16px visual SVG graphic.

<details>
<summary>View Solution</summary>

```css
.toolbar-icon-btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  /* Expands physical hit target to satisfy 2.5.8 */
  min-width: 28px;
  min-height: 28px;
  padding: 6px;
  background: transparent;
  border: 1px solid transparent;
  cursor: pointer;
}

.toolbar-icon-btn svg {
  width: 16px;
  height: 16px;
}

.toolbar-icon-btn:focus-visible {
  outline: 2px solid #005a9c;
}
```
</details>

### Exercise 4: Medium — Identifying WCAG 2.2 Obscured Focus (2.4.11)
**Problem**: A web app has a fixed bottom cookie banner (height 90px). When tabbing to the footer links, the focus ring is hidden behind the banner. How do you resolve this in CSS?

<details>
<summary>View Solution</summary>

```css
/* Tells browser scrolling mechanisms to offset focused items above the fixed banner */
html {
  scroll-padding-bottom: 100px; /* 90px banner + 10px buffer */
}

/* Ensure individual interactive elements also account for scroll margin */
:focus-visible {
  scroll-margin-bottom: 100px;
}
```
</details>

### Exercise 5: Hard — Writing a Full VPAT Criterion Assessment
**Problem**: Write an official VPAT assessment statement for a product that evaluated WCAG Success Criterion 3.3.8 (Accessible Authentication - Level AA).

<details>
<summary>View Solution</summary>

```markdown
### Criterion 3.3.8 Accessible Authentication (Minimum) (Level AA)
- **Conformance Status**: Supports
- **Remarks and Explanations**:
  The web application does not require cognitive function tests for authentication. 
  1. The login form supports standard browser password managers via native `autocomplete="username"` and `autocomplete="current-password"` attributes.
  2. Clipboard paste functionality is fully enabled on all credential inputs, permitting users to paste high-entropy passwords directly from external password vaults.
  3. Two-factor authentication (2FA) supports WebAuthn FIDO2 hardware security keys and biometric passkeys alongside traditional SMS / authenticator app codes.
```
</details>

### Exercise 6: Hard — Automated WCAG 2.2 Compliance Verifier Script
**Problem**: Write a vanilla JavaScript function that checks whether all interactive buttons on the page meet the WCAG 2.5.8 minimum target size of 24x24 pixels, logging any non-compliant elements with their dimensions.

<details>
<summary>View Solution</summary>

```javascript
function auditTargetSizes() {
  const interactiveElements = document.querySelectorAll('button, a, input, select');
  let failures = 0;

  interactiveElements.forEach((el, index) => {
    // Skip hidden elements
    if (el.offsetParent === null) return;

    const rect = el.getBoundingClientRect();
    const width = Math.round(rect.width);
    const height = Math.round(rect.height);

    // Target size must be at least 24x24 CSS pixels (unless inline link)
    const isInlineLink = el.tagName === 'A' && window.getComputedStyle(el).display === 'inline';

    if (!isInlineLink && (width < 24 || height < 24)) {
      console.warn(`[WCAG 2.5.8 Failure] Element #${index} (${el.tagName}) has target size of ${width}x${height}px (Minimum required: 24x24px):`, el);
      failures++;
    }
  });

  console.log(`Target Size Audit Complete: ${failures} non-compliant targets found.`);
}

auditTargetSizes();
```
</details>

---

## 12. Summary / Cheat Sheet

- **WCAG 2.2 AA**: Global legal benchmark comprising 55 Success Criteria.
- **Triage Matrix**:
  - **P0 (Blocker)**: Complete workflow blockage (Keyboard trap, unlabelled submit).
  - **P1 (High)**: Major barrier requiring awkward workaround.
  - **P2 (Medium)**: Degraded UX, redundant speech, missing secondary hint.
  - **P3 (Low)**: Minor markup polish.
- **VPAT Conformance Statuses**: Supports, Partially Supports, Does Not Support, Not Applicable.
- **Four Audit Stages**: Automated scan &rarr; Keyboard walkthrough &rarr; Screen reader test &rarr; Synthesis report.
- **WCAG 2.2 Key Criteria**:
  - `2.4.11`: Focus not obscured by sticky banners.
  - `2.5.7`: Dragging actions must have button alternatives.
  - `2.5.8`: Interactive hit targets must be $\ge 24\times 24\text{px}$.
  - `3.3.7`: Multi-step forms must not require redundant entry.
  - `3.3.8`: No cognitive memory puzzles for logins (allow paste).
- **Reject Overlays**: Third-party JS overlay widgets do not provide legal compliance. Fix code at the source.

---

## 13. Further Reading

- [W3C WCAG 2.2 Recommendation (Full Criteria List)](https://www.w3.org/TR/WCAG22/)
- [Information Technology Industry Council: VPAT Official Templates](https://www.itic.org/policy/accessibility/vpat)
- [W3C WAI: How to Meet WCAG (Quick Reference)](https://www.w3.org/WAI/WCAG22/quickref/)
- [Overlay Fact Sheet (Signed by Hundreds of A11y Experts)](https://overlayfactsheet.com/)

---

[Previous: 14 - Testing with axe-core & Screen Readers (14-testing-with-axe-and-screen-readers.md)](14-testing-with-axe-and-screen-readers.md) | [Section Index](README.md) | [Module Overview (README.md)](README.md)
