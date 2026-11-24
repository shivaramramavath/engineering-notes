# 06 — Forms & Validation

This module provides a comprehensive guide to building forms in React. Covering fundamental input paradigms (controlled vs uncontrolled), validation strategies, production form libraries (React Hook Form, TanStack Form), file uploads, and modern React 19 Form Actions integrated with shared Zod schemas.

---

## What you'll be able to do after this module

- **Choose Between Controlled & Uncontrolled Inputs**: Understand performance trade-offs, native DOM inputs, and React 19 automatic form resets.
- **Implement Robust Validation**: Build synchronous, debounced asynchronous, and cross-field validation rules.
- **Master React Hook Form**: Utilize ref-based uncontrolled form subscriptions for lightweight, high-performance SPAs.
- **Leverage TanStack Form**: Build complex enterprise forms with granular field-level reactivity and zero whole-form re-renders.
- **Manage Dynamic Form Arrays**: Handle dynamic add/remove rows, nested fields, and multi-step wizard funnels.
- **Handle File Uploads**: Build accessible drag-and-drop file inputs, validate MIME types, and stream files via `FormData`.
- **Implement React 19 Form Actions**: Wire `<form action={fn}>` and `<button formAction={fn}>` natively without `e.preventDefault()`.
- **Share Zod Schemas Across Boundaries**: Maintain a Single Source of Truth for validation rules between client forms and Server Actions.

---

## Module Syllabus

| # | Note | Description | Focus Area |
|---|------|-------------|:---:|
| 00 | [00-controlled-and-uncontrolled-inputs.md](./00-controlled-and-uncontrolled-inputs.md) | State-driven inputs vs DOM refs, `defaultValue`, performance differences | Foundations |
| 01 | [01-form-validation.md](./01-form-validation.md) | Validation strategies, timing (change vs blur vs submit), error display | Validation |
| 02 | [02-react-hook-form.md](./02-react-hook-form.md) | `useForm`, `register`, `Controller`, `@hookform/resolvers`, performance | Libraries |
| 03 | [03-dynamic-forms.md](./03-dynamic-forms.md) | Dynamic field arrays, conditional fields, schema-driven form generation | Dynamic Forms |
| 04 | [04-multi-step-forms.md](./04-multi-step-forms.md) | Multi-step wizards, state persistence across steps, validation gates | Wizards |
| 05 | [05-form-submission-and-errors.md](./05-form-submission-and-errors.md) | Submission handling, network errors, field error mapping, accessibility | Error Handling |
| 06 | [06-file-upload.md](./06-file-upload.md) | File inputs, drag-and-drop, progress indicators, multipart uploads | File Uploads |
| 07 | [07-react-19-form-actions.md](./07-react-19-form-actions.md) | Native `<form action={fn}>`, button `formAction`, auto-reset, transitions | React 19 Actions |
| 08 | [08-tanstack-form-overview.md](./08-tanstack-form-overview.md) | Granular reactivity, headless architecture, deep type safety | TanStack Form |
| 09 | [09-zod-schemas-shared-with-forms.md](./09-zod-schemas-shared-with-forms.md) | Shared Zod schemas, type coercion, cross-field `.refine()`, error flattening | Zod Integration |

---

## Form Architecture Comparison

| Feature | Controlled State | React Hook Form | TanStack Form | React 19 Native Actions |
| :--- | :---: | :---: | :---: | :---: |
| **Primary Mechanism** | `useState` per input | Uncontrolled refs | Observable store | Native `FormData` + Actions |
| **Typing Re-render** | Whole Form | Isolated | Granular `<form.Field>` | Zero (Uncontrolled) |
| **External Dependencies** | None | ~9KB | ~5KB | None |
| **Validation Adapters** | Manual | Zod / Yup / Joi | Zod / Valibot / ArkType | Manual safeParse / Zod |
| **Auto Form Reset** | Manual | `reset()` | `form.reset()` | Automatic on success |
| **Ideal Project** | Small forms (1-3 fields) | High-performance SPAs | Large enterprise forms | Standard full-stack web apps |

---

## Boundary with Other Modules

- **React 19 Action Hooks**: Read [`03-hooks/16-useActionState.md`](../03-hooks/16-useActionState.md) and [`03-hooks/17-useFormStatus.md`](../03-hooks/17-useFormStatus.md) for hook lifecycle details.
- **Component Design & Slots**: Read [`05-component-design/05-slots-and-asChild-pattern.md`](../05-component-design/05-slots-and-asChild-pattern.md) for custom submit buttons with `asChild`.
- **Accessibility & ARIA**: Read [`08-accessibility/`](../08-accessibility/README.md) for accessible error announcements and `aria-describedby`.
- **API Integration**: Read [`11-api-integration/`](../11-api-integration/README.md) for network layer error boundaries and retry logic.

---

## Practical Exercises

1. **Native React 19 Action Form**: Build a contact form with `<form action={fn}>` and verify inputs auto-reset after submission.
2. **Dual-Button Form**: Implement a post editor with separate "Draft" and "Publish" buttons using `formAction`.
3. **Zod Validated Registration**: Validate an account creation form with shared Zod schema and target errors to fields via `flatten()`.
4. **TanStack Form Dynamic Team Inviter**: Build a multi-member invitation form with dynamic add/remove row operations.
5. **Drag-and-Drop File Uploader**: Implement a multi-file upload box that previews file names and sizes before submitting via `FormData`.

---

## Next Steps

Advance to **[07-styling/README.md](../07-styling/README.md)** to master Tailwind CSS v4, CSS Modules, zero-runtime styling, and theme systems.
