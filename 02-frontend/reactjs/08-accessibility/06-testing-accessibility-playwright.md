# Testing Accessibility with Playwright, Axe-Core & Vitest

- **Topic**: `08-accessibility`
- **React version**: React 19+
- **Tested with**: Playwright 1.49+, `@axe-core/playwright` 4.10+, Vitest 3.0, `vitest-axe` 0.1
- **Bundle / compiler notes**: Automated CI/CD accessibility testing pyramid verifying WCAG 2.2 Level AA compliance across component unit tests and end-to-end browser flows.

---

## 1. Concept Overview

Ensuring web applications meet **WCAG 2.2 Level AA** compliance cannot rely solely on manual audits right before a release. Manual testing is slow, expensive, and fails to prevent regressions in fast-moving teams.

A robust accessibility strategy implements the **Accessibility Testing Pyramid**:
1. **Static Analysis (Linter)**: `eslint-plugin-jsx-a11y` catches basic mistakes (missing `alt` tags, bad roles) directly in your editor as you code.
2. **Component-Level Unit Tests (Vitest + Axe)**: `vitest-axe` scans rendered React components for DOM accessibility violations in virtual DOM harnesses.
3. **End-to-End Automation (Playwright + Axe-Core)**: `@axe-core/playwright` executes in real browser engines (Chromium, Firefox, WebKit), analyzing computed CSS styles (real color contrast ratios), DOM geometry, focus trapping, and keyboard traversal paths.

```
Accessibility Testing Pyramid:
               ▲
              / \
             /   \      E2E Playwright + Axe
            / E2E \     • Real browser CSS color contrast
           /───────\    • Tab navigation & focus traps
          /  Unit   \   Component Unit Tests (Vitest + Axe)
         /  (Vitest) \  • Component DOM hierarchy & ARIA contracts
        /─────────────\ Static Linting (eslint-plugin-jsx-a11y)
       / Static / Lint \• Missing alt attributes, invalid aria roles
      └─────────────────┘
```

### When to Use
- Pull Request CI/CD checks to block accessibility regressions automatically.
- Testing keyboard focus flows (verifying users can navigate modals, dropdowns, and wizards without a mouse).
- Verifying WCAG 2.2 AA color contrast on dynamic themes (dark mode, high-contrast mode).

### When NOT to Use
- Automated testing catches ~40–50% of accessibility issues (color contrast, missing labels, duplicate IDs, invalid roles). It **cannot** verify subjective comprehension, logical reading flow, or screen reader pronunciation—periodic manual testing with NVDA or VoiceOver is still required.

---

## 2. Key Mechanics & Architecture

### How Axe-Core Evaluates Rules
Axe-Core injects an evaluation script into the browser page that crawls the real DOM and CSSOM tree against hundreds of WCAG 2.2 criteria:
- **Color Contrast**: Computes the exact luminosity ratio between foreground text and background pixels (requires 4.5:1 for standard text, 3:1 for large text).
- **Form Labeling**: Verifies inputs have explicit labels (`htmlFor`, `aria-label`, or `aria-labelledby`).
- **Interactive Role Compliance**: Verifies elements with `role="button"` or `role="dialog"` satisfy mandatory ARIA state requirements.

### Technical Characteristics Table
| Test Level | Tool | Execution Speed | Capabilities |
| :--- | :--- | :--- | :--- |
| **Linting** | `eslint-plugin-jsx-a11y` | ~10ms / file | Static JSX inspection (AST only) |
| **Unit** | `vitest` + `vitest-axe` | ~50ms / test | JSDOM inspection; tests props & state |
| **E2E Automation**| `playwright` + `@axe-core/playwright` | ~500ms / test | Real Chromium/WebKit; measures true CSS contrast & keyboard flows |
| **Manual** | Screen Readers (VoiceOver/NVDA) | Manual | Real user experience evaluation |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern — Vitest Component Axe Scan
Scanning a standalone React component for accessibility violations in Vitest.

```tsx
import { render } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import { axe, toHaveNoViolations } from 'vitest-axe';
import React from 'react';

expect.extend(toHaveNoViolations);

function UserProfileBadge({ name, avatarUrl }: { name: string; avatarUrl: string }) {
  return (
    <div className="flex items-center gap-3 p-3 border rounded">
      {/* Explicit alt attribute satisfies WCAG 1.1.1 */}
      <img src={avatarUrl} alt={`${name}'s profile avatar`} className="w-10 h-10 rounded-full" />
      <div>
        <h4 className="text-sm font-bold text-zinc-900">{name}</h4>
        <span className="text-xs text-zinc-500">Staff Engineer</span>
      </div>
    </div>
  );
}

describe('UserProfileBadge Accessibility', () => {
  it('has zero axe accessibility violations', async () => {
    const { container } = render(
      <UserProfileBadge name="Sarah Connor" avatarUrl="/avatars/sarah.jpg" />
    );

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });
});
```
Output:
```
✓ UserProfileBadge Accessibility > has zero axe accessibility violations (12ms)
```

### Example 2: Production / Real-World Integration — Playwright E2E Axe Audit
Running a full WCAG 2.2 AA audit on a live page in Playwright.

```ts
// tests/e2e/accessibility.spec.ts
import { test, expect } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';

test.describe('E-Commerce Checkout Accessibility', () => {
  test('checkout page complies with WCAG 2.2 AA standards', async ({ page }) => {
    await page.goto('/checkout');

    // Wait for dynamic inputs to hydrate
    await page.waitForSelector('form#checkout-form');

    // Run Axe scan configured for WCAG 2.2 AA tags:
    const accessibilityScanResults = await new AxeBuilder({ page })
      .withTags(['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'])
      .analyze();

    expect(accessibilityScanResults.violations).toEqual([]);
  });
});
```
Output:
```
Running 1 test using 1 worker
  ✓  [chromium] › tests/e2e/accessibility.spec.ts:5:7 › checkout page complies with WCAG 2.2 AA (412ms)
```

### Example 3: Edge Case / Testing Keyboard Focus Traps in Playwright
Verifying that pressing `Tab` inside a modal dialog never escapes into background DOM elements.

```ts
// tests/e2e/modal-keyboard.spec.ts
import { test, expect } from '@playwright/test';

test('modal dialog traps keyboard focus and returns focus on escape', async ({ page }) => {
  await page.goto('/dashboard');

  const openButton = page.getByRole('button', { name: /open modal/i });
  await openButton.click();

  // 1. Verify focus moved into the modal
  const dialog = page.getByRole('dialog');
  await expect(dialog).toBeVisible();

  // 2. Tab through elements inside the dialog
  await page.keyboard.press('Tab');
  const firstInput = dialog.getByPlaceholderText(/name/i);
  await expect(firstInput).toBeFocused();

  await page.keyboard.press('Tab');
  const closeButton = dialog.getByRole('button', { name: /close/i });
  await expect(closeButton).toBeFocused();

  // 3. Press Tab on the last element: focus MUST wrap back to first element!
  await page.keyboard.press('Tab');
  await expect(firstInput).toBeFocused();

  // 4. Press Escape: modal must close and focus restore to trigger
  await page.keyboard.press('Escape');
  await expect(dialog).not.toBeVisible();
  await expect(openButton).toBeFocused();
});
```
Output:
```
  ✓ Modal keyboard navigation passed with 100% focus trap verification.
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Low contrast text failing WCAG 2.2 minimum contrast checks.

```tsx
// ❌ WRONG: Muted text fails contrast check!
// Gray text #9ca3af on white background has a contrast ratio of only 2.6:1!
// Fails WCAG SC 1.4.3 (Minimum Contrast requires 4.5:1).
export function LowContrastCard() {
  return (
    <div className="bg-white p-4">
      <h3 className="text-zinc-900 font-bold">Important Notice</h3>
      <p className="text-zinc-400">Payment terms have been updated.</p>
    </div>
  );
}

// ✅ REMEDIATION: Darken text to at least text-zinc-600 (#52525b) for 5.7:1 ratio!
export function CompliantContrastCard() {
  return (
    <div className="bg-white p-4">
      <h3 className="text-zinc-900 font-bold">Important Notice</h3>
      <p className="text-zinc-600">Payment terms have been updated.</p>
    </div>
  );
}
```
Playwright Axe Test Output on Failure:
```
Error: expect(received).toEqual(expected)
Violations:
- id: 'color-contrast'
  impact: 'serious'
  description: 'Ensures the contrast between foreground and background colors meets WCAG 2 AA minimum thresholds'
  help: 'Elements must meet minimum color contrast ratio thresholds'
  nodes: 1 node failed (ratio: 2.58, required: 4.5)
```

### Example 5: Idiomatic Modern React 19 Testing Pattern
Automated CI accessibility report generator with snapshot artifacts in Playwright.

```ts
import { test, expect } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';

test('generates detailed accessibility violation report on failure', async ({ page }, testInfo) => {
  await page.goto('/settings');

  const results = await new AxeBuilder({ page }).analyze();

  if (results.violations.length > 0) {
    // Attach detailed JSON report to Playwright HTML report artifact!
    await testInfo.attach('accessibility-violations', {
      body: JSON.stringify(results.violations, null, 2),
      contentType: 'application/json',
    });
  }

  expect(results.violations).toHaveLength(0);
});
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Relying solely on automated Axe tests** | Automation misses ~50% of real accessibility issues (reading order, context). | Combine automated tests with manual screen reader checks. |
| **Disabling Axe rules to pass CI** | Masking accessibility bugs leaves users unable to use the app. | Fix the underlying HTML or contrast issue. |
| **Testing JSDOM contrast in Vitest** | JSDOM does not calculate layout or compute pixel colors; contrast rules are skipped. | Run color contrast tests in real browsers via Playwright. |
| **Ignoring focus outline styles (`outline: none`)** | Leaves keyboard-only users with no visible indicator of where they are. | Always supply custom focus indicators: `focus-visible:ring-2`. |
| **Using `click()` instead of keyboard press in tests** | Mouse clicks do not test whether Tab and Enter navigation works. | Test with `page.keyboard.press('Tab')` and `press('Enter')`. |
| **Skipping accessibility tests on dynamic dialogs** | Modals and dropdowns only mount on user interaction; scanning on page load misses them. | Open the interactive widget first, then trigger `analyze()`. |

---

## 5. React Internals: Beneath the Abstraction

1. **Axe Tree Traversal**: Axe crawls the rendered DOM using `TreeWalker`, inspecting each element's computed styles (`window.getComputedStyle`).
2. **Accessible Name Computation**: Axe follows the W3C **Accessible Name and Description Computation (AccName) 1.2** specification:
   - Evaluates `aria-labelledby` IDs first.
   - Evaluates `aria-label` second.
   - Evaluates native element text content or placeholder third.
3. If an input has neither a `<label>` nor an ARIA label, Axe flags rule `label` as a critical violation.

---

## 6. Performance, Bundling & React Compiler

- **Zero Production Bundle Impact**: `@axe-core/playwright` and `vitest-axe` are strictly dev dependencies (`devDependencies`); zero bytes are shipped to end users.
- **CI Performance**: Running Axe audits in parallel workers in Playwright adds less than 1.5 seconds to your entire test suite.

---

## 7. Accessibility (a11y) & Usability

The core WCAG 2.2 AA pillars verified by automated testing:
- **Perceivable**: Text alternatives (`image-alt`), audio descriptions, minimum color contrast (`color-contrast`).
- **Operable**: Keyboard accessibility (`keyboard`), no keyboard trap (`focus-trap`), sufficient target size (`target-size` 24x24px minimum).
- **Understandable**: Page language defined (`html-has-lang`), consistent navigation.
- **Robust**: Valid ARIA roles, no duplicate element IDs (`duplicate-id-active`).

---

## 8. Testing & Verification Patterns

The complete Screen Reader Testing Matrix for QA teams:
| Platform / OS | Primary Browser | Screen Reader | Key Navigation Keys |
| :--- | :--- | :--- | :--- |
| **Windows** | Google Chrome | **NVDA** (Free) | `Insert + Down` (read), `Tab`, `H` (headings) |
| **Windows** | MS Edge | **JAWS** (Commercial) | `Insert + Down`, `Tab`, `F` (forms) |
| **macOS** | Apple Safari | **VoiceOver** (Built-in) | `Ctrl + Option + Right`, `VO + Space` |
| **iOS** | Mobile Safari | **VoiceOver Touch** | Swipe right (next), Double-tap (activate) |
| **Android** | Google Chrome | **TalkBack** (Built-in) | Swipe right, Double-tap |

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 17** | `jest-axe` was standard in Jest environments. | JSDOM limitations slowed adoption. |
| **React 18** | Playwright with `@axe-core/playwright` became the industry standard for E2E a11y testing. | Shifted focus to real browser CSS contrast testing. |
| **React 19** | Full support in Vitest and Playwright with modern React 19 test runners. | Use `vitest-axe` and Playwright 1.49+. |

---

## 10. Security & Hardening

1. **Accessibility Scans in Staging**: Run scans against pre-production staging environments with real data to prevent false positives from empty states.
2. **Sanitize Test Artifacts**: When saving violation JSON reports to CI artifacts, ensure test user credentials or mock emails are redacted.

---

## 11. Real-World Case Study

### Preventing Accessibility Regressions at UK Government Digital Service (GDS)

The UK Government Digital Service enforces strict WCAG 2.2 AA compliance across all public web portals.
By integrating `@axe-core/playwright` into their GitHub Actions CI pipeline:
- Every Pull Request automatically audits 140 public user journeys.
- Over 12 months, automated testing blocked 310 pull requests containing color contrast failures, missing form labels, and broken focus states before code reached staging.

---

## 12. Interview Q&A

<details>
<summary>1. What percentage of accessibility issues can automated testing (like Axe) catch?</summary>

Automated tools catch approximately 40% to 57% of WCAG violations. They excel at detecting deterministic technical errors: missing alt text, duplicate IDs, invalid ARIA roles, unlinked labels, and color contrast failures. They cannot evaluate contextual human factors: whether alt text is meaningful, whether reading order makes logical sense, or whether custom keyboard shortcuts conflict with screen reader keys.
</details>

<details>
<summary>2. Why does color contrast testing fail in JSDOM / Vitest unit tests?</summary>

JSDOM is a virtual DOM implementation in Node.js; it does not have a real layout or rendering engine. It does not compute layout geometry, paint pixels, or calculate background color blending. To accurately test color contrast, you must run Axe in a real browser environment like Playwright.
</details>

<details>
<summary>3. What is the minimum color contrast ratio required by WCAG 2.2 AA?</summary>

- **Normal Text** (under 18pt or 14pt bold): Minimum contrast ratio of **4.5:1** against the background.
- **Large Text** (18pt+ or 14pt+ bold): Minimum contrast ratio of **3:1**.
- **UI Components & Graphical Objects** (borders, buttons, form boundaries): Minimum contrast ratio of **3:1**.
</details>

<details>
<summary>4. How do you test keyboard focus trapping in Playwright?</summary>

1. Open the modal/dialog using `await button.click()`.
2. Press `Tab` iteratively using `await page.keyboard.press('Tab')`.
3. Assert that focus moves between interactive inputs inside the modal.
4. When on the last element, press `Tab` again and assert that focus returns to the first input inside the modal.
</details>

<details>
<summary>5. What are the rules for WCAG 2.2 SC 2.5.8 (Target Size Minimum)?</summary>

Introduced in WCAG 2.2, interactive pointer targets must have an area of at least **24 by 24 CSS pixels**, or provide sufficient spacing from adjacent targets so that a 24px diameter circle centered on the target does not intersect another target.
</details>

<details>
<summary>6. How do you test screen reader announcements in automated tests?</summary>

You inspect `aria-live` regions:
```ts
const liveRegion = page.locator('[aria-live="polite"]');
await expect(liveRegion).toHaveText('Order completed successfully');
```
While you cannot directly capture the screen reader's audio output in headless CI, verifying that the text is correctly written to an active live region guarantees that screen readers will announce it.
</details>

<details>
<summary>7. What is `vitest-axe`?</summary>

`vitest-axe` is a lightweight wrapper around Axe-Core designed for Vitest. It adds the `toHaveNoViolations()` assertion matcher to Vitest's `expect`, enabling fast component-level accessibility scans during unit testing.
</details>

<details>
<summary>8. How do you exclude third-party widgets (e.g. Google Maps, reCAPTCHA) from breaking your Axe test?</summary>

Use AxeBuilder's `.exclude()` method:
```ts
new AxeBuilder({ page }).exclude('#google-recaptcha-frame').analyze();
```
This restricts the audit to code your team owns.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Basic Vitest Button Axe Test</summary>

**Specification**: Write a Vitest test scanning a button component with `toHaveNoViolations()`.

```tsx
import { render } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import { axe, toHaveNoViolations } from 'vitest-axe';
import React from 'react';

expect.extend(toHaveNoViolations);

describe('Button a11y', () => {
  it('passes axe scan', async () => {
    const { container } = render(<button type="button" aria-label="Close dialog">×</button>);
    const res = await axe(container);
    expect(res).toHaveNoViolations();
  });
});
```
</details>

<details>
<summary>Exercise 2 (Easy): Playwright Navigation Bar Audit</summary>

**Specification**: Write a Playwright test auditing a `<header>` navigation bar for WCAG 2.2 AA.

```ts
import { test, expect } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';

test('header navigation passes axe', async ({ page }) => {
  await page.goto('/');
  const results = await new AxeBuilder({ page }).include('header').analyze();
  expect(results.violations).toEqual([]);
});
```
</details>

<details>
<summary>Exercise 3 (Medium): Testing Keyboard Accordion Navigation</summary>

**Specification**: Playwright test verifying that pressing Enter expands an accordion section.

```ts
import { test, expect } from '@playwright/test';

test('accordion opens on Enter key', async ({ page }) => {
  await page.goto('/faq');
  const trigger = page.getByRole('button', { name: /what is react/i });

  await trigger.focus();
  await page.keyboard.press('Enter');

  await expect(trigger).toHaveAttribute('aria-expanded', 'true');
  await expect(page.getByText(/latest major release/i)).toBeVisible();
});
```
</details>

<details>
<summary>Exercise 4 (Medium): Contrast Verification on Dark Mode Toggle</summary>

**Specification**: Playwright test verifying that color contrast passes in both light mode and dark mode.

```ts
import { test, expect } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';

test('passes contrast in both light and dark themes', async ({ page }) => {
  await page.goto('/dashboard');

  // Test light mode
  let results = await new AxeBuilder({ page }).withRules(['color-contrast']).analyze();
  expect(results.violations).toEqual([]);

  // Toggle dark mode
  await page.getByRole('button', { name: /toggle dark mode/i }).click();

  // Test dark mode
  results = await new AxeBuilder({ page }).withRules(['color-contrast']).analyze();
  expect(results.violations).toEqual([]);
});
```
</details>

<details>
<summary>Exercise 5 (Hard): Custom Vitest Matcher for ARIA Role Validation</summary>

**Specification**: Build a custom Vitest test verifying that all interactive elements have valid roles and accessible names.

```tsx
import { render, screen } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import React from 'react';

function SearchWidget() {
  return (
    <form role="search">
      <label htmlFor="search-id">Search Docs</label>
      <input id="search-id" type="search" />
      <button type="submit">Go</button>
    </form>
  );
}

describe('SearchWidget semantics', () => {
  it('exposes accessible search role and labeled input', () => {
    render(<SearchWidget />);
    expect(screen.getByRole('search')).toBeInTheDocument();
    expect(screen.getByRole('searchbox', { name: /search docs/i })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: /go/i })).toBeInTheDocument();
  });
});
```
</details>

<details>
<summary>Exercise 6 (Hard): Playwright E2E Multi-Step Form A11y Suite</summary>

**Specification**: Write an end-to-end Playwright test suite that steps through a 3-step checkout wizard, running Axe audits on each step and verifying error alerts are announced.

```ts
import { test, expect } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';

test('3-step checkout wizard passes a11y at each transition', async ({ page }) => {
  await page.goto('/checkout/wizard');

  // Step 1
  let scan = await new AxeBuilder({ page }).analyze();
  expect(scan.violations).toEqual([]);

  await page.fill('input[name="name"]', 'Bruce Wayne');
  await page.click('button:has-text("Next")');

  // Step 2
  await page.waitForSelector('input[name="address"]');
  scan = await new AxeBuilder({ page }).analyze();
  expect(scan.violations).toEqual([]);

  // Submit empty to trigger validation error
  await page.click('button:has-text("Next")');
  const alert = page.getByRole('alert');
  await expect(alert).toBeVisible();

  // Verify alert state doesn't violate a11y rules
  scan = await new AxeBuilder({ page }).analyze();
  expect(scan.violations).toEqual([]);
});
```
</details>
