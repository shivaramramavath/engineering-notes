# Module 06: Accessibility Architecture (a11y) & WCAG 2.2

Welcome to **Module 06: Accessibility Architecture & WCAG 2.2**. Accessibility is not an optional polish or a retrofitted compliance layer; it is fundamental to the architecture of the World Wide Web. When semantic HTML is authored accurately, browsers automatically translate DOM nodes into the **Accessibility Tree (AOM)**, exposing native platform accessibility APIs to screen readers, braille displays, voice control engines, and switch access hardware.

This module provides exhaustive, production-grade technical training on building fully accessible web interfaces complying with **WCAG 2.2 Level AA**, applying the **First Rule of ARIA**, calculating **Accessible Names (AccName 1.2)**, managing complex keyboard focus, handling live region status updates, and mastering automated and manual screen reader testing.

---

## Module Roadmap & Navigation

| # | Note | Focus Area | Key Concepts & APIs |
|:---:|:---|:---|:---|
| **01** | [Accessibility Fundamentals & WCAG 2.2](01-accessibility-fundamentals-and-wcag.md) | Standards & Ethics | POUR principles, WCAG 2.2 AA new criteria, Conformance, ADA/EAA legal framework |
| **02** | [Semantic-First Architecture & ARIA Rules](02-semantic-first-and-the-aria-rules.md) | Foundations | First Rule of ARIA, implicit role mappings, redundant ARIA anti-patterns, ARIA design patterns |
| **03** | [ARIA Roles, States & Properties](03-aria-roles-states-and-properties.md) | ARIA Specification | Roles taxonomy (widget, landmark, structure), states vs properties, `aria-expanded`, `aria-selected` |
| **04** | [Accessible Names & The Accessibility Tree](04-accessible-names-and-the-a11y-tree.md) | Browser Internals | AccName 1.2 computation algorithm, `aria-labelledby`, `aria-label`, AOM DevTools inspection |
| **05** | [Keyboard Navigation & Focus Management](05-keyboard-navigation-and-focus.md) | Interaction | Sequential navigation, `tabindex` (-1, 0, positive trap), `:focus-visible`, focus traps, restoration |
| **06** | [Skip Links & Landmark Navigation](06-skip-links-and-landmarks.md) | Wayfinding | Bypass Blocks (WCAG 2.4.1), Skip-to-content links, landmark labeling (`aria-label` vs `aria-labelledby`) |
| **07** | [Alt Text Decision Tree & Complex Media](07-alt-text-decision-tree.md) | Visual Equity | W3C Alt Text Decision Tree, decorative images (`alt=""`), functional icons, charts & `<figcaption>` |
| **08** | [Accessible Forms & Error Announcement](08-accessible-forms-and-error-announcement.md) | Data Entry | Form labels, helper text, `aria-describedby`, `aria-invalid`, GDS Error Summary, focus shifting |
| **09** | [Accessible Tables & Grid Structures](09-accessible-tables-and-lists.md) | Data Presentation | `<table>`, `<caption>`, `scope="col|row"`, complex multi-tier headers (`id`/`headers`), CSS list-style bugs |
| **10** | [Accessible Dialogs, Popovers & Menus](10-accessible-dialogs-popovers-and-menus.md) | Overlay Components | `<dialog>` modal semantics, Popover API top layer, disclosure widgets, application menus (`role="menu"`) |
| **11** | [Live Regions & Dynamic Status Messages](11-live-regions-and-status-messages.md) | Asynchronous Feedback | `aria-live` (`polite`, `assertive`), `aria-atomic`, `aria-relevant`, `role="status"`, `role="alert"`, toast queues |
| **12** | [Accessible Multimedia, Audio & Captions](12-accessible-multimedia-and-captions.md) | Audio/Visual A11y | WCAG 1.2.x multimedia guidelines, WebVTT captions, synchronized transcripts, audio descriptions |
| **13** | [Contrast, Motion & User Preferences](13-contrast-motion-and-user-preferences.md) | Visual & Vestibular | WCAG contrast math (4.5:1, 3:1), non-text contrast, `prefers-reduced-motion`, `forced-colors` high contrast |
| **14** | [Testing with axe-core & Screen Readers](14-testing-with-axe-and-screen-readers.md) | Verification | Automated scanning limits (axe-core, Lighthouse), NVDA, JAWS, VoiceOver, TalkBack desktop & mobile testing |
| **15** | [Accessibility Audit Checklist & Triage](15-accessibility-audit-checklist.md) | Compliance Operations | Complete WCAG 2.2 AA audit checklist, issue remediation priority matrix, regression testing |

---

## Key Learning Outcomes

1. **Understand Browser Accessibility Internals**: Learn how browsers translate HTML tokens into DOM nodes, and how the platform Accessibility APIs (MSAA, UI Automation, IAccessible2, ATK, NSAccessibility) consume the Accessibility Object Model (AOM).
2. **Execute Semantic-First Markup**: Internalize the 5 Rules of ARIA. Avoid redundant ARIA overrides that destroy native browser ergonomics, keyboard event handling, and mobile screen reader behaviors.
3. **Master AccName 1.2**: Trace the exact computational steps screen readers execute when deriving an element's accessible name and description across competing attributes.
4. **Build Flawless Keyboard Experiences**: Eliminate positive `tabindex` anti-patterns, guarantee persistent high-contrast focus rings via `:focus-visible`, implement seamless modal focus traps, and restore focus to trigger elements upon dismissal.
5. **Comply with WCAG 2.2 Level AA**: Master every single criterion including the latest additions: 2.4.11 (Focus Appearance), 2.4.12 (Focus Not Obscured), 2.5.7 (Dragging Movements), 2.5.8 (Target Size Minimum 24x24px), 3.2.6 (Consistent Help), and 3.3.7/3.3.8 (Redundant Entry & Accessible Authentication).
6. **Operate Screen Readers Professionally**: Navigate web interfaces using NVDA (Windows), VoiceOver (macOS / iOS), and TalkBack (Android) like assistive technology users.

---

[Next: 01 - Accessibility Fundamentals & WCAG 2.2 ->](01-accessibility-fundamentals-and-wcag.md)
