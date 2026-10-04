# 06 — Forms

Forms are where most real-world React complexity lives: state for many fields, validation, error messages, submission, loading states, dynamic fields, multi-step flows, and file uploads. This chapter goes from the two basic input models to production-grade patterns with React Hook Form and Zod.

Examples are TSX. Typing techniques come from [`../04-typescript-with-react/`](../04-typescript-with-react/README.md).

## Prerequisites

- [`../01-fundamentals/07-forms-basics.md`](../01-fundamentals/07-forms-basics.md) — the basic controlled-input pattern
- [`../03-hooks/01-useState.md`](../03-hooks/01-useState.md), [`../03-hooks/04-useRef.md`](../03-hooks/04-useRef.md)
- [`../04-typescript-with-react/05-advanced-typing-patterns.md`](../04-typescript-with-react/05-advanced-typing-patterns.md) (schemas and `z.infer`)

## What you'll be able to do after this chapter

- Choose between controlled and uncontrolled inputs for each situation
- Validate on the client with native rules and Zod schemas, and understand why the server must validate too
- Build forms with React Hook Form and share one schema for types and validation
- Build dynamic forms: conditional fields, repeatable field groups, schema-driven fields
- Split a long form into steps without losing data
- Handle pending states, server errors, and double submits, and make errors accessible
- Upload files with previews, validation, and progress

## Reading order

| # | File | What it covers | Prerequisite |
|---|------|----------------|--------------|
| 00 | [00-controlled-and-uncontrolled-inputs.md](./00-controlled-and-uncontrolled-inputs.md) | The two input models and when to use each | Basic forms |
| 01 | [01-form-validation.md](./01-form-validation.md) | Native, manual, and Zod validation; validation timing | 00 |
| 02 | [02-react-hook-form.md](./02-react-hook-form.md) | The standard library for forms; Zod resolver | 01 |
| 03 | [03-dynamic-forms.md](./03-dynamic-forms.md) | Conditional fields, field arrays, schema-driven forms | 02 |
| 04 | [04-multi-step-forms.md](./04-multi-step-forms.md) | Wizards with per-step validation | 02 |
| 05 | [05-form-submission-and-errors.md](./05-form-submission-and-errors.md) | Pending state, server errors, accessible error UI | 02 |
| 06 | [06-file-upload.md](./06-file-upload.md) | File inputs, previews, validation, progress | 00 |

`03`, `04`, `05`, and `06` are independent of each other once you've read `02` (and `00` for `06`).

## Boundary with other chapters

- **Basic controlled input and submit:** [`../01-fundamentals/07-forms-basics.md`](../01-fundamentals/07-forms-basics.md). This chapter assumes it.
- **Controlled/uncontrolled as a *component API* design:** [`../05-component-design/00-component-api-design.md`](../05-component-design/00-component-api-design.md). `00` here covers *form inputs*.
- **UI building blocks (select, checkbox, combobox, date picker):** [`../09-ui-components/06-form-controls.md`](../09-ui-components/06-form-controls.md).
- **Sending data and mutations:** [`../12-server-state/05-mutations.md`](../12-server-state/05-mutations.md).
- **React 19 form Actions (`useActionState`, `useFormStatus`):** [`../15-concurrent-and-modern-react/05-react-19-features.md`](../15-concurrent-and-modern-react/05-react-19-features.md). `05` here shows how they fit.
- **Accessibility of forms in depth:** [`../08-accessibility/`](../08-accessibility/README.md).

## Exercises

1. **Both models.** Build a search box twice: once controlled (live character count) and once uncontrolled (read with `FormData` on submit). Note what each costs and gives you.
2. **Hand-rolled validation.** Add validation to a signup form with `useState` only: errors shown after a field is touched, submit blocked while invalid.
3. **Zod schema.** Define one schema for a registration form (including "passwords must match") and derive the TypeScript type from it.
4. **React Hook Form.** Rebuild exercise 2 with React Hook Form and `zodResolver`.
5. **Field array.** An invoice form where you can add, remove, and reorder line items, with a computed total.
6. **Wizard.** A three-step checkout (details → address → review) that validates each step before continuing and keeps data when going back.
7. **Server errors.** Simulate a "username taken" 409 response and show it under the right field; disable the submit button while pending.
8. **Uploader.** An avatar uploader with type and size checks, an image preview, and a progress bar.

## Next

**[`../07-styling/README.md`](../07-styling/README.md)** covers how to style what you've built.
