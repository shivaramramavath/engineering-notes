# 05 — Component Design

This module explores component architecture, design patterns, and engineering principles for building robust, scalable, and accessible React design systems. It covers the full progression of component patterns—from clean API design and compound components to modern headless architectures, Radix/shadcn `asChild` slots, the State Reducer pattern, and domain service hooks.

---

## What you'll be able to do after this module

- **Design Intuitive Component APIs**: Build props interfaces with strict type-safety, sensible defaults, and minimal prop bloat.
- **Master Composition Over Inheritance**: Replace fragile props configuration with component composition, avoiding the boolean flag trap.
- **Build Compound Components**: Implement compound component systems (`Select`, `Accordion`, `Tabs`) sharing state implicitly via React Context.
- **Understand Historical React Patterns**: Recognize and modernize legacy Higher-Order Components (HOCs), Render Props, and Mixins.
- **Architect Headless UI Primitives**: Decouple state, keyboard navigation, and WAI-ARIA accessibility from visual styling using custom hooks and prop getters.
- **Implement Slots & the `asChild` Pattern**: Build flexible components matching Radix UI and shadcn/ui conventions, eliminating unnecessary wrapper DOM nodes.
- **Invert Control with State Reducers**: Allow consumers to intercept, customize, or veto internal component state transitions.
- **Organize Code with Hooks as Services**: Decouple domain business logic, payment pipelines, and network requests from UI presentation with Dependency Injection.

---

## Module Syllabus

| # | Note | Description | Key Focus |
|---|------|-------------|:---:|
| 00 | [00-component-api-design.md](./00-component-api-design.md) | Component API principles, prop naming conventions, polymorphic contracts | React 19 / Clean Code |
| 01 | [01-composition-patterns.md](./01-composition-patterns.md) | Composition vs inheritance, containment slots, specialization patterns | Composition |
| 02 | [02-compound-components.md](./02-compound-components.md) | Compound components pattern, internal context, subcomponent exports | Context & State |
| 03 | [03-legacy-component-patterns.md](./03-legacy-component-patterns.md) | Historical patterns: HOCs, Mixins, Render Props migration to modern hooks | Migration |
| 04 | [04-headless-and-render-props.md](./04-headless-and-render-props.md) | Headless architecture, prop getters, keyboard interaction, WAI-ARIA 1.2 | Headless Architecture |
| 05 | [05-slots-and-asChild-pattern.md](./05-slots-and-asChild-pattern.md) | Radix UI / shadcn `<Slot>`, `asChild` convention, event & ref composition | Slot Composition |
| 06 | [06-state-reducer-pattern.md](./06-state-reducer-pattern.md) | Inverting state control, action types, vetoing transitions, Downshift pattern | Inversion of Control |
| 07 | [07-hooks-as-services.md](./07-hooks-as-services.md) | Domain logic encapsulation, Dependency Injection via Context, unit testability | Architecture & DI |

---

## Architectural Comparison Matrix

| Pattern | Best Use Case | Coupling Level | Consumer Control | Example Library |
| :--- | :--- | :---: | :---: | :--- |
| **Simple Styled Component** | Atoms (Badges, Avatars) | High | Low | Tailwind UI |
| **Compound Components** | Multi-part Widgets (Select, Tabs) | Medium | Medium | Radix UI, Headless UI |
| **Headless Hooks** | Complex State & Keyboard (Combobox) | Zero | 100% | React Aria, Downshift |
| **Slots (`asChild`)** | Swapping HTML / Router elements | Zero | High | Radix UI, shadcn/ui |
| **State Reducer** | Intercepting state rules (Wizards, Menus) | Low | Maximum | Downshift |
| **Hooks as Services** | Domain rules, Payments, Auth | Zero | High | Enterprise Architecture |

---

## Boundary with Other Modules

- **React 19 Native Refs**: Read [`04-typescript-with-react/06-typing-refs-and-props-in-19.md`](../04-typescript-with-react/06-typing-refs-and-props-in-19.md) for typing refs without `forwardRef`.
- **Form Architecture**: Read [`06-forms/`](../06-forms/README.md) for form validation with Zod and React 19 Form Actions.
- **Accessibility Contracts**: Read [`08-accessibility/`](../08-accessibility/README.md) for WCAG 2.2 guidelines, focus management, and ARIA roles.
- **UI Primitives in Practice**: Read [`09-ui-components/`](../09-ui-components/README.md) for real-world implementations of tables, tooltips, and floating menus.

---

## Practical Exercises

1. **Compound Card System**: Build a `<Card>` component with `<Card.Header>`, `<Card.Body>`, and `<Card.Footer>` sharing subtle border context.
2. **Headless Disclosure**: Implement `useDisclosure` returning `getTriggerProps` and `getPanelProps` with full ARIA linkage.
3. **AsChild Navigation Item**: Implement a navigation item supporting `asChild` that correctly passes down active styles to Next.js or React Router links.
4. **Rate-Limited Toggle**: Use the State Reducer pattern to build a toggle button that vetoes clicks after 5 activations.
5. **Injected Cart Service**: Create a shopping cart component consuming an injected `CartService` and verify it in Vitest with a mock service.

---

## Next Steps

Advance to **[06-forms/README.md](../06-forms/README.md)** to master form validation, React 19 Form Actions, Zod 4, and TanStack Form.
