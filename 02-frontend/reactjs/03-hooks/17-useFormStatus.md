# React useFormStatus: Contextual Form Submission State Without Prop Drilling

- **Topic**: `03-hooks`
- **React version**: React 19+
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Exported from `'react-dom'`; reads implicit Context from the parent `<form>`; zero bundle cost.

---

## 1. Concept Overview

In web applications, submit buttons and interactive form controls need to know when their parent form is in the process of submitting. Common requirements include:
- Disabling the submit button while an async request is in flight (`disabled={pending}`).
- Rendering a loading spinner or "Submitting..." label.
- Displaying real-time feedback about the data currently being transmitted.

In legacy React, sharing submission state between a form and deeply nested child controls required manual **prop drilling** or custom Context providers:
```
Legacy Approach (Prop Drilling):
<ParentForm> (holds `isSubmitting` state)
   └──► <Card> (forwards `isSubmitting`)
          └──► <Fieldset> (forwards `isSubmitting`)
                 └──► <SubmitButton isSubmitting={isSubmitting} />
```

`useFormStatus` is a native hook exported from `react-dom` that eliminates this prop drilling. Any component rendered *inside* a `<form>` can call `useFormStatus` to access the parent form's submission state directly.

```
React 19 Contextual Form Status:
<form action={submitAction}>
  <InputField />
  <Card>
    <ChildComponent>
      {/* Reads parent form status directly from React DOM internal context! */}
      <SubmitButton /> ◄── useFormStatus() -> { pending, data, method, action }
    </ChildComponent>
  </Card>
</form>
```

### When to Use
- Building reusable design system buttons (`<SubmitButton />`) that automatically disable and spin during parent form submission.
- Reading submitted `FormData` in real time to render optimistic previews inside child controls.
- Disabling child inputs or fieldsets while a parent form action is executing.

### When NOT to Use
- **Do NOT call `useFormStatus` inside the same component that renders the `<form>`**: It will always return `pending: false` because it reads context from an *ancestor* `<form>`.
- **Do NOT use for tracking non-form state**: Use `useTransition` or `useActionState` for standalone triggers outside a `<form>`.

---

## 2. Key Mechanics & Architecture

### API Signature
```ts
import { useFormStatus } from 'react-dom';

const { pending, data, method, action } = useFormStatus();
```

### Returned Properties Table
| Property | Type | Description |
| :--- | :--- | :--- |
| **`pending`** | `boolean` | `true` if the parent `<form>` is actively submitting an action; otherwise `false`. |
| **`data`** | `FormData \| null` | A `FormData` object containing the data submitted by the form. `null` when not pending. |
| **`method`** | `'get' \| 'post' \| null` | The HTTP method of the parent `<form>` (defaults to `'get'`). |
| **`action`** | `Function \| string \| null` | The function or URL string passed to the parent `<form action={...}>`. |

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Import Source** | Exported from `'react-dom'` (NOT `'react'`) |
| **Execution Phase** | Synchronous during render phase; subscribes to Form Context |
| **Ancestor Requirement** | Must be called inside a component rendered as a descendant of `<form>` |
| **SSR Support** | Fully supported in SSR and React Server Component trees |
| **Automatic Reset** | `pending` resets to `false` and `data` to `null` once the action settles |
| **React Compiler Impact** | Subscribes child to form context; unaffected by outer component memoization |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
Reusable design system `SubmitButton` component.

```tsx
import React from 'react';
import { useFormStatus } from 'react-dom';

export function SubmitButton({ label = 'Submit' }: { label?: string }) {
  // Must be called inside a child component within <form>!
  const { pending } = useFormStatus();

  return (
    <button
      type="submit"
      disabled={pending}
      className={`px-4 py-2 rounded text-sm font-semibold transition-colors ${
        pending ? 'bg-zinc-400 text-zinc-200 cursor-not-allowed' : 'bg-indigo-600 text-white hover:bg-indigo-700'
      }`}
    >
      {pending ? (
        <span className="flex items-center gap-2">
          <span className="w-3 h-3 border-2 border-white border-t-transparent rounded-full animate-spin" />
          Saving...
        </span>
      ) : (
        label
      )}
    </button>
  );
}

// Parent Form:
export function ContactForm() {
  async function handleContact(formData: FormData) {
    await new Promise((r) => setTimeout(r, 1000));
    console.log('Submitted message:', formData.get('message'));
  }

  return (
    <form action={handleContact} className="p-4 border rounded max-w-sm">
      <textarea name="message" placeholder="Your message..." required className="w-full border p-2 text-sm rounded mb-3" />
      <SubmitButton label="Send Message" />
    </form>
  );
}
```
Output:
```
// Clicking Send Message: button immediately shows "Saving..." with spinner and disables itself.
```

### Example 2: Production / Real-World Integration
Reading `data` to render live pending preview inside a child widget.

```tsx
import React from 'react';
import { useFormStatus } from 'react-dom';

function PendingUploadSummary() {
  const { pending, data } = useFormStatus();

  if (!pending || !data) return null;

  const fileName = (data.get('document') as File)?.name;
  const category = data.get('category') as string;

  return (
    <div className="p-3 bg-amber-50 border border-amber-200 rounded text-xs text-amber-900 mt-3 animate-pulse">
      <p className="font-semibold">Uploading in background:</p>
      <p>File: {fileName || 'Selected Document'}</p>
      <p>Category: {category}</p>
    </div>
  );
}

export function DocumentUploadForm() {
  async function uploadAction(formData: FormData) {
    await new Promise((r) => setTimeout(r, 1500));
  }

  return (
    <form action={uploadAction} className="p-6 border rounded-xl max-w-md">
      <h3 className="font-bold mb-3">Upload Compliance Document</h3>
      <div className="space-y-3">
        <input name="document" type="file" required className="block w-full text-xs" />
        <select name="category" className="w-full border p-2 text-sm rounded bg-white">
          <option value="KYC">KYC Identity Document</option>
          <option value="TAX">Tax Certification</option>
          <option value="AUDIT">Financial Audit</option>
        </select>
        <button type="submit" className="w-full bg-zinc-900 text-white py-2 rounded text-sm font-semibold">
          Upload
        </button>
      </div>

      <PendingUploadSummary />
    </form>
  );
}
```
Output:
```
// While uploadAction is running, PendingUploadSummary displays:
// "Uploading in background: File: passport.pdf, Category: KYC"
```

### Example 3: Edge Case / Nested Custom Fieldset Disabler
Disabling an entire nested section of inputs when the parent form submits.

```tsx
import React, { type ReactNode } from 'react';
import { useFormStatus } from 'react-dom';

export function FormFieldset({ children, legend }: { children: ReactNode; legend: string }) {
  const { pending } = useFormStatus();

  return (
    <fieldset disabled={pending} className="border p-4 rounded-lg mb-4 disabled:opacity-50 transition-opacity">
      <legend className="text-xs font-bold text-zinc-700 px-1">{legend}</legend>
      {children}
    </fieldset>
  );
}
```
Output:
```
// All inputs inside FormFieldset automatically lock and dim during parent form submission.
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Calling `useFormStatus` inside the component that renders `<form>`.

```tsx
// ❌ CRITICAL BUG: Calling useFormStatus in the same component declaring <form>
export function BrokenForm() {
  // ⚠️ BUG: pending will ALWAYS be false here!
  // useFormStatus looks UP the React tree for an ancestor <form>.
  // It cannot see the <form> rendered below in its own JSX return!
  const { pending } = useFormStatus();

  async function action(formData: FormData) {
    await new Promise((r) => setTimeout(r, 1000));
  }

  return (
    <form action={action}>
      <input name="test" />
      {/* pending is always false here! */}
      <button disabled={pending}>Submit ({pending ? 'Pending' : 'Idle'})</button>
    </form>
  );
}

// ✅ REMEDIATION: Extract the submit button (or inner contents) into a separate child component!
function ChildSubmitButton() {
  const { pending } = useFormStatus();
  return <button disabled={pending}>Submit ({pending ? 'Pending' : 'Idle'})</button>;
}

export function WorkingForm() {
  async function action(formData: FormData) {
    await new Promise((r) => setTimeout(r, 1000));
  }

  return (
    <form action={action}>
      <input name="test" />
      <ChildSubmitButton />
    </form>
  );
}
```
Output:
```
// WorkingForm correctly updates ChildSubmitButton from "Idle" to "Pending" upon submission.
```

### Example 5: Idiomatic Modern React 19 Pattern
Pairing `useFormStatus` with React 19 Action feedback in a complete checkout workflow.

```tsx
import React from 'react';
import { useFormStatus } from 'react-dom';
import { useActionState } from 'react';

function CheckoutSubmit() {
  const { pending } = useFormStatus();

  return (
    <button
      type="submit"
      disabled={pending}
      aria-busy={pending}
      className="w-full bg-emerald-600 hover:bg-emerald-700 text-white font-bold py-2.5 rounded-lg text-sm transition-all disabled:opacity-50"
    >
      {pending ? 'Processing Card...' : 'Pay $99.00'}
    </button>
  );
}

async function orderAction(_prev: any, formData: FormData) {
  await new Promise((r) => setTimeout(r, 1200));
  return { success: true, txn: 'TXN-90218' };
}

export function CheckoutWidget() {
  const [result, formAction] = useActionState(orderAction, { success: false });

  if (result.success) {
    return <div className="p-4 bg-emerald-100 text-emerald-800 rounded font-medium">Order confirmed! Reference: {result.txn}</div>;
  }

  return (
    <form action={formAction} className="border p-4 rounded-xl max-w-xs space-y-3">
      <h3 className="font-bold text-sm">Express Checkout</h3>
      <input name="cardholder" placeholder="Name on Card" required className="w-full border p-2 text-xs rounded" />
      <CheckoutSubmit />
    </form>
  );
}
```
Output:
```
// Clean separation of concerns: CheckoutSubmit handles pending UX; CheckoutWidget handles form action logic.
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Calling `useFormStatus` in the `<form>` component itself** | Reads ancestor context; cannot read the form rendered in its own return. | Extract button or controls into a child component. |
| **Importing from `'react'` instead of `'react-dom'`** | `useFormStatus` is DOM-specific and lives in `'react-dom'`. | Import: `import { useFormStatus } from 'react-dom'`. |
| **Mutating `data` directly** | `data` is an immutable snapshot of the submitted `FormData`. | Read values using `data.get('fieldName')`. |
| **Using `useFormStatus` outside of a `<form>`** | Returns default `{ pending: false, data: null, method: null, action: null }`. | Only call inside components rendered inside `<form>`. |
| **Using `e.preventDefault()` on form submit** | Bypasses React's action pipeline; `pending` will never toggle. | Use `<form action={...}>` without manual `e.preventDefault()`. |
| **Ignoring `aria-busy` on pending buttons** | Screen readers do not announce loading status automatically. | Add `aria-busy={pending}` to submit buttons. |

---

## 5. React Internals: Beneath the Abstraction

Inside `react-dom`:
1. When React mounts a `<form>`, it creates an internal **`FormContext`** provider.
2. When the user submits the form, React intercept the native DOM submit event and initiates an Action Transition.
3. React updates the `FormContext` value with:
   - `pending = true`
   - `data = new FormData(formElement)`
   - `method = formElement.method`
   - `action = formAction`
4. Any child component subscribing to `useFormStatus` immediately re-renders with the pending context.
5. Once the Action's async promise resolves, React resets the `FormContext` to idle and notifies subscribers.

---

## 6. Performance, Bundling & React Compiler

- **Zero Bundle Impact**: Provided directly by `react-dom`.
- **Targeted Re-renders**: Only components that invoke `useFormStatus` re-render when submission begins or ends. Static sibling components (labels, headers) do not re-render.
- **React Compiler**: The React Compiler preserves context subscriptions and memoizes child JSX efficiently.

---

## 7. Accessibility (a11y) & Usability

- **`aria-busy`**: Always set `aria-busy={pending}` on the submitting form or button so assistive technologies inform the user that a background process is active.
- **`disabled` vs `aria-disabled`**: While `disabled={pending}` prevents clicks, `aria-disabled="true"` keeps the button focusable so screen reader users do not lose their focus position unexpectedly.

---

## 8. Testing & Verification Patterns

Testing `useFormStatus` with Vitest and `@testing-library/react`:

```tsx
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { describe, it, expect } from 'vitest';
import React from 'react';
import { ContactForm } from './SubmitButton';

describe('useFormStatus - ContactForm', () => {
  it('disables button and displays pending text during submission', async () => {
    const user = userEvent.setup();
    render(<ContactForm />);

    const textarea = screen.getByPlaceholderText(/your message.../i);
    const submitBtn = screen.getByRole('button', { name: /send message/i });

    await user.type(textarea, 'Hello team!');
    await user.click(submitBtn);

    // Button should now be disabled and show Saving...
    expect(screen.getByRole('button')).toHaveTextContent(/saving.../i);
    expect(screen.getByRole('button')).toBeDisabled();
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 17 / 18** | Did not exist. Required manual Context or prop drilling for submit button loading states. | Migrate custom button loading props to `useFormStatus`. |
| **React Canary** | Introduced in `react-dom` experimental builds. | Retained identical signature in 19 stable. |
| **React 19** | Standard stable API in `react-dom`. | Use directly with React 19 Actions. |

---

## 10. Security & Hardening

1. **Inspecting Submitted `data`**: When displaying values from `data.get('field')` optimistically in the UI, ensure values are rendered as standard React text nodes rather than `dangerouslySetInnerHTML` to prevent XSS.
2. **Preventing Spam Submissions**: Always enforce `disabled={pending}` to prevent users from rapidly clicking submit multiple times before the server responds.

---

## 11. Real-World Case Study

### Enterprise Design System Button Library

A core design system team maintained a `<Button variant="submit">` used across 40 applications. Previously, every product team had to manually pass `isLoading={isSubmitting}`. Refactoring `<Button type="submit">` to internally call `useFormStatus()` eliminated 3,000+ lines of prop-drilling boilerplate across the company monorepo.

```tsx
import React, { type ButtonHTMLAttributes } from 'react';
import { useFormStatus } from 'react-dom';

interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  children: React.ReactNode;
}

export function DesignSystemButton({ children, type, disabled, ...props }: ButtonProps) {
  const formStatus = useFormStatus();
  const isFormPending = type === 'submit' && formStatus.pending;

  return (
    <button
      {...props}
      type={type}
      disabled={disabled || isFormPending}
      aria-busy={isFormPending}
      className="px-4 py-2 bg-indigo-600 text-white rounded font-medium disabled:opacity-50"
    >
      {isFormPending ? 'Processing...' : children}
    </button>
  );
}
```

---

## 12. Interview Q&A

<details>
<summary>1. Why does calling `useFormStatus` inside the component that renders `<form>` fail?</summary>

`useFormStatus` relies on React Context. React Context can only read values provided by an *ancestor* in the component tree. When a component returns `<form>`, the context provider is created for its *children*. The component itself sits above that context provider, so calling `useFormStatus` in that component returns the default idle status (`pending: false`). To read the form's status, the call must be placed in a child component nested inside the `<form>`.
</details>

<details>
<summary>2. What package exports `useFormStatus`?</summary>

`useFormStatus` is exported from `'react-dom'`, not `'react'`. This is because forms and form submission semantics are specific to the DOM renderer.
</details>

<details>
<summary>3. What data is available in the `data` property of `useFormStatus`?</summary>

The `data` property contains a browser `FormData` instance populated with all named fields of the parent form that was submitted. It is only non-null while `pending` is `true`. Once the action resolves, `data` resets to `null`.
</details>

<details>
<summary>4. Can `useFormStatus` track submissions triggered by `<button formAction={...}>`?</summary>

Yes. If a button specifies an overriding `formAction`, `useFormStatus` tracks that specific action and exposes its pending status to all child components of the form.
</details>

<details>
<summary>5. How does `useFormStatus` compare to `useActionState`'s `isPending`?</summary>

`useActionState` returns `isPending` at the location where the action is declared (usually the parent form component). `useFormStatus` exposes that pending status to deeply nested child components (like submit buttons or input fields) without requiring props to be passed down through intermediate components.
</details>

<details>
<summary>6. What happens if multiple forms are rendered on a page?</summary>

Each `useFormStatus` call binds to its closest ancestor `<form>` in the DOM tree hierarchy, ensuring complete isolation between multiple forms.
</details>

<details>
<summary>7. What is the value of `method` in `useFormStatus`?</summary>

It returns the HTTP method defined on the parent `<form>`, typically `'get'` or `'post'`. If unspecified, it defaults to `'get'`.
</details>

<details>
<summary>8. Does `useFormStatus` work with Server Components (RSC)?</summary>

No. `useFormStatus` is an interactive hook that tracks client-side form submission states. It must be used in a Client Component with the `'use client'` directive.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Form Submission Progress Bar</summary>

**Specification**: Build a linear indeterminate progress bar component that renders at the top of a form only while the form is pending.

```tsx
import React from 'react';
import { useFormStatus } from 'react-dom';

export function FormProgressBar() {
  const { pending } = useFormStatus();

  if (!pending) return null;

  return (
    <div className="w-full bg-zinc-200 h-1 overflow-hidden rounded">
      <div className="bg-indigo-600 h-full w-1/3 animate-[shimmer_1s_infinite]" />
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Contextual Input Disabler</summary>

**Specification**: Build an `AutoDisableInput` component that disables itself whenever its enclosing form is submitting.

```tsx
import React, { type InputHTMLAttributes } from 'react';
import { useFormStatus } from 'react-dom';

export function AutoDisableInput(props: InputHTMLAttributes<HTMLInputElement>) {
  const { pending } = useFormStatus();

  return (
    <input
      {...props}
      disabled={props.disabled || pending}
      className={`border p-2 rounded text-sm w-full ${pending ? 'bg-zinc-100 text-zinc-400' : 'bg-white'}`}
    />
  );
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Live Form Data Echo Display</summary>

**Specification**: Create a component inside a form that prints the values of the fields currently being sent while the action is pending.

```tsx
import React from 'react';
import { useFormStatus } from 'react-dom';

export function FormSubmissionEcho() {
  const { pending, data } = useFormStatus();

  if (!pending || !data) return null;

  const entries: [string, string][] = [];
  data.forEach((val, key) => {
    entries.push([key, String(val)]);
  });

  return (
    <div className="p-2 bg-zinc-100 border rounded text-xs font-mono mt-2">
      <p className="font-bold text-zinc-700">Payload in flight:</p>
      {entries.map(([k, v]) => (
        <div key={k}>{k}: {v}</div>
      ))}
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Conditional Submit Button Icon</summary>

**Specification**: Create a button that dynamically displays an airplane icon when idle, a spinning loader when pending, and disables clicks.

```tsx
import React from 'react';
import { useFormStatus } from 'react-dom';

export function FlightSubmitButton() {
  const { pending } = useFormStatus();

  return (
    <button
      type="submit"
      disabled={pending}
      className="flex items-center gap-2 bg-blue-600 text-white px-4 py-2 rounded text-sm font-semibold disabled:opacity-50"
    >
      {pending ? (
        <>
          <span className="w-3 h-3 border-2 border-white border-t-transparent rounded-full animate-spin" />
          <span>Booking Flight...</span>
        </>
      ) : (
        <>
          <span>✈️</span>
          <span>Book Flight</span>
        </>
      )}
    </button>
  );
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Multi-Form Conflict Resolver</summary>

**Specification**: Demonstrate two independent sibling forms on a single dashboard, verifying that submitting Form A triggers pending status only in Form A's children, while Form B remains idle.

```tsx
import React from 'react';
import { useFormStatus } from 'react-dom';

function SubButton({ name }: { name: string }) {
  const { pending } = useFormStatus();
  return (
    <button type="submit" disabled={pending} className="border px-3 py-1 text-xs rounded">
      {name} {pending ? '(Busy)' : '(Ready)'}
    </button>
  );
}

export function DualFormDashboard() {
  const mockAction = async () => new Promise((r) => setTimeout(r, 1000));

  return (
    <div className="flex gap-4 p-4 border rounded">
      <form action={mockAction} className="p-3 border rounded">
        <h4 className="font-bold text-xs mb-2">Form Alpha</h4>
        <SubButton name="Alpha Submit" />
      </form>

      <form action={mockAction} className="p-3 border rounded">
        <h4 className="font-bold text-xs mb-2">Form Beta</h4>
        <SubButton name="Beta Submit" />
      </form>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Accessible Announcer with `aria-live`</summary>

**Specification**: Create a live status announcer component that uses `useFormStatus` to inject polite screen reader announcements when a form starts and completes submission.

```tsx
import React, { useEffect, useState } from 'react';
import { useFormStatus } from 'react-dom';

export function FormScreenReaderAnnouncer() {
  const { pending } = useFormStatus();
  const [announcement, setAnnouncement] = useState('');

  useEffect(() => {
    if (pending) {
      setAnnouncement('Form submission started. Please wait.');
    } else if (announcement) {
      setAnnouncement('Form submission completed.');
      const t = setTimeout(() => setAnnouncement(''), 3000);
      return () => clearTimeout(t);
    }
  }, [pending]);

  return (
    <div role="status" aria-live="polite" className="sr-only">
      {announcement}
    </div>
  );
}
```
</details>
