# React useActionState: Managing Form Actions, Pending States & Results in React 19

- **Topic**: `03-hooks`
- **React version**: React 19+
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Replaces legacy `useFormState` from Canary; core React 19 hook for action lifecycle management.

---

## 1. Concept Overview

Forms are the primary interactive primitive of the web. Historically, managing form submissions in React required significant boilerplate:
- Tracking loading spinners (`const [isSubmitting, setIsSubmitting] = useState(false)`).
- Tracking submission errors (`const [error, setError] = useState(null)`).
- Storing returned server data or validation messages.
- Preventing duplicate submissions and manually resetting form fields.

In React 19, **Actions** are first-class functions that handle asynchronous operations within Transitions. Functions passed to `<form action={fn}>` automatically receive form data and execute asynchronously.

`useActionState` is the standard React 19 hook that manages the full lifecycle of an Action:
1. It stores the **returned state** from the action (e.g. validation errors, success flags, or returned database records).
2. It provides a wrapped **action dispatcher** that can be passed directly to `<form action={...}>` or invoked imperatively.
3. It exposes a reactive **`isPending`** boolean indicating whether the asynchronous action is currently in flight.

```
                  <form action={formAction}>
                             │
                             ▼
                ┌─────────────────────────┐
                │ useActionState Trigger  │
                └────────────┬────────────┘
                             │
           ┌─────────────────┴─────────────────┐
           ▼                                   ▼
   [ During Submission ]               [ On Completion ]
   isPending = true                    isPending = false
   UI shows spinner / disables submit  state = nextState returned by action
```

### When to Use
- Managing form submissions with async validation or server persistence.
- Handling login, signup, checkout, and profile update forms.
- Coordinating error messages and success toasts returned from backend APIs.
- Replacing manual `try/catch/finally` loading state boilerplate.

### When NOT to Use
- **Simple Client State Toggles**: Use `useState` for UI-only flags (e.g., dropdown toggles, modal open/close).
- **External Real-time Subscriptions**: Use `useSyncExternalStore` for WebSockets or pub/sub sources.

---

## 2. Key Mechanics & Architecture

### API Signature
```ts
const [state, formAction, isPending] = useActionState<State, Payload>(
  action: (previousState: State, payload: Payload) => Promise<State> | State,
  initialState: State,
  permalink?: string
);
```

1. **`action`**: A function taking `(previousState, payload)` and returning the next state. When wired to a `<form>`, `payload` is standard browser `FormData`.
2. **`initialState`**: The value of `state` before the action has been executed for the first time.
3. **`permalink`** (Optional): A string URL used by SSR frameworks for progressive enhancement before JavaScript loads.
4. **`state`**: The current resolved state returned by the action.
5. **`formAction`**: The function to pass to `<form action={formAction}>` or `<button formAction={formAction}>`.
6. **`isPending`**: A boolean that is `true` while the action promise is resolving.

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Execution Phase** | Async Transition; updates queued in React's Transition Lane |
| **Pending State** | Built-in `isPending` boolean; no manual `useState` required |
| **Payload Typing** | Defaults to `FormData` when bound to `<form>`, or custom payload if invoked |
| **Progressive Enhancement**| Supported via the optional `permalink` parameter |
| **Reset Behavior** | Re-invoking the action overwrites `state` with the new action return value |
| **React Compiler Impact** | Fully supported; `action` callback dependencies are tracked safely |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
User newsletter subscription form with loading spinner and error display.

```tsx
import React, { useActionState } from 'react';

interface FormState {
  error: string | null;
  success: boolean;
}

async function subscribeNewsletterAction(
  prevState: FormState,
  formData: FormData
): Promise<FormState> {
  const email = formData.get('email') as string;

  // Simulate network delay
  await new Promise((r) => setTimeout(r, 800));

  if (!email || !email.includes('@')) {
    return { error: 'Please enter a valid email address.', success: false };
  }

  return { error: null, success: true };
}

export function NewsletterForm() {
  const [state, formAction, isPending] = useActionState(subscribeNewsletterAction, {
    error: null,
    success: false,
  });

  return (
    <form action={formAction} className="max-w-md p-4 border rounded-lg">
      <h3 className="font-bold mb-2">Subscribe to our Engineering Digest</h3>
      <div className="flex gap-2">
        <input
          name="email"
          type="email"
          placeholder="colleague@company.com"
          required
          disabled={isPending}
          className="flex-1 border px-3 py-1.5 rounded text-sm disabled:opacity-50"
        />
        <button
          type="submit"
          disabled={isPending}
          className="bg-zinc-900 text-white px-4 py-1.5 rounded text-sm font-medium disabled:opacity-50"
        >
          {isPending ? 'Joining...' : 'Subscribe'}
        </button>
      </div>

      {state.error && <p className="text-red-600 text-xs mt-2">{state.error}</p>}
      {state.success && <p className="text-emerald-600 text-xs mt-2">✓ Subscribed successfully!</p>}
    </form>
  );
}
```
Output:
```
// User submits invalid email: displays "Please enter a valid email address."
// User submits valid email: button shows "Joining..." then "✓ Subscribed successfully!"
```

### Example 2: Production / Real-World Integration
User Profile update form with Zod schema validation, field-level error messages, and disabled button states.

```tsx
import React, { useActionState } from 'react';

interface ProfileState {
  errors?: Record<string, string>;
  data?: { name: string; role: string };
  successMessage?: string;
}

async function updateProfileAction(
  _prev: ProfileState,
  formData: FormData
): Promise<ProfileState> {
  const name = (formData.get('name') as string)?.trim();
  const role = (formData.get('role') as string)?.trim();

  await new Promise((r) => setTimeout(r, 900));

  const fieldErrors: Record<string, string> = {};
  if (!name || name.length < 3) {
    fieldErrors.name = 'Full name must be at least 3 characters.';
  }
  if (!role) {
    fieldErrors.role = 'Role selection is mandatory.';
  }

  if (Object.keys(fieldErrors).length > 0) {
    return { errors: fieldErrors };
  }

  return {
    data: { name, role },
    successMessage: `Profile updated for ${name} (${role})!`,
  };
}

export function ProfileEditor() {
  const [state, formAction, isPending] = useActionState(updateProfileAction, {});

  return (
    <form action={formAction} className="max-w-md mx-auto p-6 bg-white border rounded-xl shadow-sm">
      <h2 className="text-lg font-bold mb-4">Edit Profile</h2>

      <div className="mb-4">
        <label className="block text-xs font-semibold mb-1">Full Name</label>
        <input
          name="name"
          defaultValue={state.data?.name ?? 'Alex Mercer'}
          disabled={isPending}
          className="w-full border px-3 py-2 rounded text-sm"
        />
        {state.errors?.name && (
          <span className="text-xs text-red-500 mt-1 block">{state.errors.name}</span>
        )}
      </div>

      <div className="mb-4">
        <label className="block text-xs font-semibold mb-1">Company Role</label>
        <select
          name="role"
          defaultValue={state.data?.role ?? 'Staff Engineer'}
          disabled={isPending}
          className="w-full border px-3 py-2 rounded text-sm bg-white"
        >
          <option value="Frontend Engineer">Frontend Engineer</option>
          <option value="Staff Engineer">Staff Engineer</option>
          <option value="Product Architect">Product Architect</option>
        </select>
        {state.errors?.role && (
          <span className="text-xs text-red-500 mt-1 block">{state.errors.role}</span>
        )}
      </div>

      <button
        type="submit"
        disabled={isPending}
        className="w-full bg-indigo-600 hover:bg-indigo-700 text-white font-medium py-2 rounded text-sm disabled:opacity-50"
      >
        {isPending ? 'Saving Profile...' : 'Save Changes'}
      </button>

      {state.successMessage && (
        <div className="mt-4 p-2 bg-emerald-50 text-emerald-700 text-xs rounded text-center">
          {state.successMessage}
        </div>
      )}
    </form>
  );
}
```
Output:
```
// Renders profile form; submitting with invalid data returns field errors cleanly;
// successful submit shows green confirmation banner.
```

### Example 3: Edge Case / Imperative Invocation without a Form
Using `useActionState` outside of a HTML `<form>` element by dispatching payloads imperatively.

```tsx
import React, { useActionState, startTransition } from 'react';

interface DeleteState {
  deletedId: string | null;
  error: string | null;
}

async function deleteRecordAction(
  _prev: DeleteState,
  recordId: string
): Promise<DeleteState> {
  await new Promise((r) => setTimeout(r, 700));

  if (recordId === 'admin_root') {
    return { deletedId: null, error: 'Cannot delete root admin account.' };
  }

  return { deletedId: recordId, error: null };
}

export function DeleteButton({ recordId }: { recordId: string }) {
  const [state, dispatchDelete, isPending] = useActionState(deleteRecordAction, {
    deletedId: null,
    error: null,
  });

  const handleDelete = () => {
    // Imperative invocation wrapped in transition
    startTransition(() => {
      dispatchDelete(recordId);
    });
  };

  if (state.deletedId === recordId) {
    return <span className="text-xs text-zinc-400 italic">Record deleted</span>;
  }

  return (
    <div>
      <button
        type="button"
        onClick={handleDelete}
        disabled={isPending}
        className="px-2 py-1 text-xs bg-red-50 text-red-600 hover:bg-red-100 rounded disabled:opacity-50"
      >
        {isPending ? 'Deleting...' : 'Delete'}
      </button>
      {state.error && <span className="text-xs text-red-600 block mt-1">{state.error}</span>}
    </div>
  );
}
```
Output:
```
// Button updates to "Deleting..." and gracefully transitions to "Record deleted".
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Forgetting the `previousState` first argument in the action signature.

```tsx
// ❌ WRONG: Missing previousState in action signature!
// React useActionState always passes (previousState, formData)!
async function wrongAction(formData: FormData) {
  // BUG: formData will actually be undefined or previousState!
  const query = formData.get('q'); // TypeError: formData.get is not a function!
  return { query };
}

// ✅ REMEDIATION: Always declare (previousState, payload) in the action handler!
async function correctAction(previousState: any, formData: FormData) {
  const query = formData.get('q') as string;
  return { query };
}
```
Output:
```
TypeError: formData.get is not a function
    at wrongAction (NewsletterForm.tsx:12)
```

### Example 5: Idiomatic Modern React 19 Pattern
Resetting a form automatically upon successful submission using React 19 form actions and ref.

```tsx
import React, { useActionState, useRef, useEffect } from 'react';

interface FeedbackState {
  sent: boolean;
  message?: string;
}

async function sendFeedbackAction(_prev: FeedbackState, formData: FormData): Promise<FeedbackState> {
  const comment = formData.get('comment') as string;
  await new Promise((r) => setTimeout(r, 600));

  return { sent: true, message: `Feedback received: "${comment.slice(0, 20)}..."` };
}

export function FeedbackBox() {
  const formRef = useRef<HTMLFormElement>(null);
  const [state, formAction, isPending] = useActionState(sendFeedbackAction, { sent: false });

  // Reset form inputs upon successful action completion
  useEffect(() => {
    if (state.sent) {
      formRef.current?.reset();
    }
  }, [state.sent]);

  return (
    <form ref={formRef} action={formAction} className="border p-4 rounded max-w-sm">
      <h4 className="font-bold text-sm mb-2">Send Feedback</h4>
      <textarea
        name="comment"
        required
        disabled={isPending}
        placeholder="Tell us what you think..."
        className="w-full border p-2 text-sm rounded mb-2"
      />
      <button
        type="submit"
        disabled={isPending}
        className="w-full bg-zinc-900 text-white py-1.5 text-xs font-semibold rounded disabled:opacity-50"
      >
        {isPending ? 'Transmitting...' : 'Send Feedback'}
      </button>

      {state.message && <p className="text-xs text-green-700 mt-2 font-medium">{state.message}</p>}
    </form>
  );
}
```
Output:
```
// Form clears input automatically when server returns successful response.
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Omitting `previousState` in the action signature** | React passes `(prev, payload)`; writing `(formData)` misaligns arguments. | Always declare `(prevState, formData)` in action handler. |
| **Using `useState` for loading flags when using actions** | Redundant state and synchronization bugs. | Rely directly on the third return value: `isPending`. |
| **Mutating `previousState` directly** | React relies on `Object.is` to detect state transitions. | Return a brand new object: `return { ...prev, error: null }`. |
| **Using `onSubmit={e => e.preventDefault()}` with actions** | Overriding form submission bypasses React 19 Action and Transition pipeline. | Use `<form action={formAction}>` directly without manual `e.preventDefault()`. |
| **Expecting `state` to update synchronously** | Actions are async Transitions; `state` updates only after the promise settles. | Render `isPending` state during intermediate loading periods. |
| **Passing non-serializable objects to Server Actions** | If the action is a Server Action (`'use server'`), payloads must be JSON serializable. | Send plain objects, primitives, or standard `FormData`. |

---

## 5. React Internals: Beneath the Abstraction

Inside `ReactFiberHooks.js`:
1. `useActionState` combines internal `useReducer` and `useTransition` mechanics.
2. The hook maintains:
   - An action dispatcher fiber node.
   - An internal transition boundary.
3. When `formAction(payload)` is triggered:
   - React invokes `startTransition`.
   - The fiber enters the **Transition Lane** (`isPending` becomes `true` immediately).
   - React awaits the resolution of `action(currentState, payload)`.
4. When the action resolves:
   - The returned value is dispatched to the internal reducer.
   - React commits the new state and toggles `isPending` to `false`.
5. If the action throws an unhandled rejection, React triggers error boundary reconciliation.

---

## 6. Performance, Bundling & React Compiler

- **Bundle Optimization**: Built directly into `react`; eliminates heavy form management dependencies (e.g. Formik, custom reducers).
- **Background Rendering**: Because actions run inside Transitions, slow network responses do not block high-priority typing or clicks in other components.
- **React Compiler**: The React Compiler memoizes form JSX and does not re-render unaffected form fields while `isPending` is toggling.

---

## 7. Accessibility (a11y) & Usability

- **Button Disabling**: While `isPending` is true, buttons should have `disabled={isPending}` and `aria-busy={isPending}`.
- **Error Announcement**: Ensure validation error containers have `role="alert"` or `aria-live="assertive"` so screen readers announce form failures immediately upon submission.

---

## 8. Testing & Verification Patterns

Testing form actions with Vitest and `@testing-library/react`:

```tsx
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { describe, it, expect } from 'vitest';
import React from 'react';
import { NewsletterForm } from './NewsletterForm';

describe('useActionState - NewsletterForm', () => {
  it('submits form and displays success state', async () => {
    const user = userEvent.setup();
    render(<NewsletterForm />);

    const input = screen.getByPlaceholderText(/colleague@company.com/i);
    const submitBtn = screen.getByRole('button', { name: /subscribe/i });

    await user.type(input, 'alex@acme.com');
    await user.click(submitBtn);

    // Verify pending state
    expect(screen.getByRole('button')).toHaveTextContent(/joining.../i);

    // Verify success state appears
    expect(await screen.findByText(/subscribed successfully!/i)).toBeInTheDocument();
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 17 / 18** | No native Action hooks. Used manual `onSubmit`, `useState`, and external form libraries. | Replace custom async submit handlers with `useActionState`. |
| **React Canary** | Previewed under the name `useFormState` in `react-dom`. | Deprecated in favor of `useActionState` in core `react`. |
| **React 19** | Standardized as `useActionState` imported directly from `'react'`. | Import from `'react'` (not `'react-dom'`). |

---

## 10. Security & Hardening

1. **Server Action Authentication**: When using Server Actions, verify the user's session and authorization tokens inside the action function; never trust client-submitted user IDs or role claims.
2. **CSRF Protection**: React 19 Server Actions include built-in cryptographic action IDs to prevent Cross-Site Request Forgery.
3. **Payload Sanitization**: Always validate and sanitize values extracted from `formData.get()` using libraries like Zod before database persistence.

---

## 11. Real-World Case Study

### Multi-Tenant Organization Invitation Manager

A SaaS admin console allowing team owners to send email invitations with role assignments. Moving to `useActionState` simplified error propagation, handled network latency cleanly, and removed 3 separate `useState` hooks.

```tsx
import React, { useActionState } from 'react';

interface InviteState {
  status: 'idle' | 'success' | 'error';
  message: string;
}

async function inviteTeamMemberAction(
  _prev: InviteState,
  formData: FormData
): Promise<InviteState> {
  const email = formData.get('email') as string;
  const role = formData.get('role') as string;

  await new Promise((r) => setTimeout(r, 800));

  if (!email || !email.includes('@')) {
    return { status: 'error', message: 'Valid corporate email required.' };
  }

  return { status: 'success', message: `Invitation dispatched to ${email} as ${role}.` };
}

export function OrgInviteWidget() {
  const [state, formAction, isPending] = useActionState(inviteTeamMemberAction, {
    status: 'idle',
    message: '',
  });

  return (
    <div className="p-4 border rounded-xl max-w-md">
      <h3 className="font-bold text-sm mb-3">Invite Team Member</h3>
      <form action={formAction} className="space-y-3">
        <input
          name="email"
          type="email"
          placeholder="colleague@domain.com"
          required
          disabled={isPending}
          className="w-full border p-2 rounded text-sm"
        />
        <select name="role" disabled={isPending} className="w-full border p-2 rounded text-sm bg-white">
          <option value="VIEWER">Viewer (Read-only)</option>
          <option value="DEVELOPER">Developer (Read/Write)</option>
          <option value="ADMIN">Admin (Full Control)</option>
        </select>
        <button
          type="submit"
          disabled={isPending}
          className="w-full bg-blue-600 text-white font-medium py-2 rounded text-sm disabled:opacity-50"
        >
          {isPending ? 'Sending Invitation...' : 'Send Invite'}
        </button>
      </form>

      {state.status === 'error' && (
        <div role="alert" className="mt-3 text-xs text-red-600 font-medium">
          {state.message}
        </div>
      )}
      {state.status === 'success' && (
        <div className="mt-3 text-xs text-emerald-600 font-medium">
          {state.message}
        </div>
      )}
    </div>
  );
}
```

---

## 12. Interview Q&A

<details>
<summary>1. What is the difference between `useActionState` and the legacy `useFormState` hook?</summary>

`useFormState` was an experimental hook in `react-dom` during React 18 / Canary.
In React 19, the React team renamed and promoted it to the core `'react'` package as `useActionState`.
Key differences:
1. `useActionState` is imported directly from `'react'`, not `'react-dom'`.
2. `useActionState` returns a 3-element tuple: `[state, formAction, isPending]`. In contrast, `useFormState` only returned `[state, formAction]`, requiring a separate `useFormStatus` call to inspect loading state.
3. `useActionState` can be invoked imperatively for non-form actions.
</details>

<details>
<summary>2. Why does the action callback in `useActionState` receive `(previousState, payload)` instead of just `(payload)`?</summary>

This follows the standard reducer pattern (`(state, action) => newState`). Receiving `previousState` enables cumulative workflows, such as paginated lists (appending new items to previous items), tracking attempt counters, or maintaining undo stacks across consecutive submissions.
</details>

<details>
<summary>3. How does `useActionState` interact with React Transitions?</summary>

Internally, `useActionState` executes the action within a React Transition. When `formAction` is invoked, React sets `isPending = true` within the Transition Lane. While the async action resolves, other user interactions (like typing in other inputs or clicking navigation tabs) remain completely responsive without freezing the browser thread.
</details>

<details>
<summary>4. What is the purpose of the third parameter, `permalink`?</summary>

The `permalink` parameter is an optional URL string used for Progressive Enhancement. In full-stack frameworks (like Next.js or React Router v7 framework mode), if a user submits a form before client-side JavaScript has finished downloading and hydrating, the browser falls back to a native HTTP POST request to the URL specified by `permalink`.
</details>

<details>
<summary>5. Can `useActionState` be used with Client Actions as well as Server Actions?</summary>

Yes. `useActionState` is completely runtime-agnostic. It works identically whether the action function is a client-side async function executing in the browser or a Server Action marked with `'use server'` executing in Node.js or edge runtimes.
</details>

<details>
<summary>6. How do you reset the state returned by `useActionState` to initial values?</summary>

`useActionState` does not have a dedicated `reset()` method. To reset the state, either:
1. Pass an explicit reset action payload that returns `initialState`.
2. Change the component's `key` prop to force React to remount the component and re-initialize the hook with `initialState`.
</details>

<details>
<summary>7. What happens if an action passed to `useActionState` throws an unhandled error?</summary>

If the action throws an error (e.g., network failure without `try/catch`), React catches the error and bubbles it up to the nearest React Error Boundary. If you want to handle errors gracefully in the UI without triggering an error boundary, catch the error inside the action function and return an error state object (e.g. `{ error: err.message }`).
</details>

<details>
<summary>8. How does `useActionState` work alongside `useOptimistic`?</summary>

They are complementary primitives. `useActionState` holds the authoritative server state and manages the async lifecycle. `useOptimistic` wraps the state from `useActionState` to render immediate UI changes before the action finishes.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Feedback Rating Submission Form</summary>

**Specification**: Build a rating feedback form that sends comments, shows a pending spinner, and displays confirmation text.

```tsx
import React, { useActionState } from 'react';

async function submitFeedback(_prev: any, formData: FormData) {
  const comment = formData.get('comment') as string;
  await new Promise((r) => setTimeout(r, 500));
  return { done: true, text: comment };
}

export function Feedback() {
  const [state, action, isPending] = useActionState(submitFeedback, { done: false, text: '' });

  return (
    <form action={action} className="p-3 border rounded max-w-xs">
      <input name="comment" required disabled={isPending} placeholder="Your thoughts..." className="border p-1 w-full text-sm" />
      <button type="submit" disabled={isPending} className="mt-2 bg-black text-white px-3 py-1 text-xs rounded">
        {isPending ? 'Sending...' : 'Submit'}
      </button>
      {state.done && <p className="text-xs text-green-600 mt-2">Saved: {state.text}</p>}
    </form>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Username Availability Checker</summary>

**Specification**: Form action that queries an API to verify username uniqueness and displays status badge.

```tsx
import React, { useActionState } from 'react';

async function checkUsername(_prev: any, formData: FormData) {
  const username = formData.get('username') as string;
  await new Promise((r) => setTimeout(r, 600));
  const isTaken = ['admin', 'root', 'moderator'].includes(username.toLowerCase());
  return { checked: true, isTaken, username };
}

export function UsernameChecker() {
  const [state, action, isPending] = useActionState(checkUsername, { checked: false });

  return (
    <form action={action} className="p-4 border rounded max-w-sm">
      <div className="flex gap-2">
        <input name="username" placeholder="Check handle..." className="border p-1.5 text-sm flex-1" />
        <button type="submit" disabled={isPending} className="bg-zinc-800 text-white px-3 text-xs rounded">
          {isPending ? 'Checking...' : 'Check'}
        </button>
      </div>
      {state.checked && (
        <p className={`text-xs mt-2 ${state.isTaken ? 'text-red-500' : 'text-green-500'}`}>
          {state.isTaken ? `Username @${state.username} is taken` : `Username @${state.username} is available!`}
        </p>
      )}
    </form>
  );
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Password Reset with Step Counter</summary>

**Specification**: Multi-attempt password reset form that tracks consecutive failure attempts in `previousState`.

```tsx
import React, { useActionState } from 'react';

interface AuthState {
  attempts: number;
  error?: string;
  success?: boolean;
}

async function resetPasswordAction(prev: AuthState, formData: FormData): Promise<AuthState> {
  const code = formData.get('code') as string;
  await new Promise((r) => setTimeout(r, 500));

  if (code !== '1234') {
    return { attempts: prev.attempts + 1, error: `Invalid code. Attempt ${prev.attempts + 1} of 3.` };
  }

  return { attempts: 0, success: true };
}

export function PasswordReset() {
  const [state, action, isPending] = useActionState(resetPasswordAction, { attempts: 0 });

  return (
    <form action={action} className="p-4 border rounded max-w-xs">
      <input name="code" placeholder="Enter code (1234)" disabled={isPending || state.attempts >= 3} className="border p-1.5 text-sm w-full" />
      <button type="submit" disabled={isPending || state.attempts >= 3} className="w-full mt-2 bg-blue-600 text-white p-1 text-xs rounded disabled:opacity-50">
        Verify Code
      </button>
      {state.error && <p className="text-red-600 text-xs mt-2">{state.error}</p>}
      {state.attempts >= 3 && <p className="text-red-700 font-bold text-xs mt-2">Account locked. Too many failed attempts.</p>}
      {state.success && <p className="text-green-600 text-xs mt-2">Code verified successfully!</p>}
    </form>
  );
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Dynamic Tag Creation Form</summary>

**Specification**: Action that appends new tag chips to an existing list stored in `previousState`.

```tsx
import React, { useActionState } from 'react';

async function addTagAction(prevTags: string[], formData: FormData): Promise<string[]> {
  const tag = (formData.get('tag') as string)?.trim();
  await new Promise((r) => setTimeout(r, 300));
  if (!tag || prevTags.includes(tag)) return prevTags;
  return [...prevTags, tag];
}

export function TagManager() {
  const [tags, action, isPending] = useActionState(addTagAction, ['react', 'vite']);

  return (
    <div className="p-4 border rounded max-w-sm">
      <div className="flex flex-wrap gap-1 mb-3">
        {tags.map((t) => (
          <span key={t} className="px-2 py-0.5 bg-zinc-100 rounded text-xs font-medium">#{t}</span>
        ))}
      </div>
      <form action={action} className="flex gap-2">
        <input name="tag" placeholder="Add tag..." disabled={isPending} className="border p-1 text-xs flex-1 rounded" />
        <button type="submit" disabled={isPending} className="bg-zinc-900 text-white px-2 py-1 text-xs rounded">
          {isPending ? 'Adding...' : 'Add'}
        </button>
      </form>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Checkout Billing Form with Multi-Field Server Validation</summary>

**Specification**: Implement an end-to-end checkout billing form that validates address, postal code, and payment method with field-level errors and accessible aria markers.

```tsx
import React, { useActionState } from 'react';

interface BillingErrors {
  address?: string;
  postal?: string;
  card?: string;
}

interface BillingResult {
  errors?: BillingErrors;
  orderId?: string;
}

async function processCheckoutAction(_prev: BillingResult, formData: FormData): Promise<BillingResult> {
  const address = formData.get('address') as string;
  const postal = formData.get('postal') as string;
  const card = formData.get('card') as string;

  await new Promise((r) => setTimeout(r, 1000));

  const errors: BillingErrors = {};
  if (!address || address.length < 5) errors.address = 'Valid delivery address required.';
  if (!postal || !/^\d{5}$/.test(postal)) errors.postal = 'Postal code must be 5 digits.';
  if (!card || card.replace(/\s/g, '').length < 16) errors.card = 'Card number must be 16 digits.';

  if (Object.keys(errors).length > 0) return { errors };

  return { orderId: `ORD-${Math.random().toString(36).substring(2, 9).toUpperCase()}` };
}

export function CheckoutForm() {
  const [result, action, isPending] = useActionState(processCheckoutAction, {});

  if (result.orderId) {
    return (
      <div className="p-6 bg-emerald-50 border border-emerald-200 rounded-xl text-center">
        <h3 className="font-bold text-emerald-800">Order Confirmed!</h3>
        <p className="text-sm text-emerald-600 mt-1">Reference: {result.orderId}</p>
      </div>
    );
  }

  return (
    <form action={action} className="p-6 border rounded-xl max-w-md space-y-4">
      <h2 className="font-bold text-base">Payment & Shipping</h2>
      <div>
        <label className="text-xs font-semibold block mb-1">Street Address</label>
        <input name="address" disabled={isPending} className="w-full border p-2 rounded text-sm" />
        {result.errors?.address && <span className="text-xs text-red-500">{result.errors.address}</span>}
      </div>
      <div>
        <label className="text-xs font-semibold block mb-1">ZIP / Postal Code</label>
        <input name="postal" maxLength={5} disabled={isPending} className="w-full border p-2 rounded text-sm" />
        {result.errors?.postal && <span className="text-xs text-red-500">{result.errors.postal}</span>}
      </div>
      <div>
        <label className="text-xs font-semibold block mb-1">Card Number</label>
        <input name="card" placeholder="1234 5678 9012 3456" disabled={isPending} className="w-full border p-2 rounded text-sm" />
        {result.errors?.card && <span className="text-xs text-red-500">{result.errors.card}</span>}
      </div>
      <button
        type="submit"
        disabled={isPending}
        className="w-full bg-indigo-600 text-white py-2 rounded text-sm font-semibold disabled:opacity-50"
      >
        {isPending ? 'Authorizing Payment...' : 'Complete Purchase'}
      </button>
    </form>
  );
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Optimistic Action Queue Coordinator</summary>

**Specification**: Combine `useActionState` and `useOptimistic` into a unified custom hook `useOptimisticAction` that encapsulates both layers.

```tsx
import React, { useActionState, useOptimistic, startTransition } from 'react';

export function useOptimisticAction<TState, TPayload>(
  action: (state: TState, payload: TPayload) => Promise<TState>,
  initialState: TState,
  optimisticReducer: (current: TState, payload: TPayload) => TState
) {
  const [state, formAction, isPending] = useActionState(action, initialState);
  const [optimisticState, setOptimistic] = useOptimistic(state, optimisticReducer);

  const dispatch = (payload: TPayload) => {
    startTransition(async () => {
      setOptimistic(payload);
      await formAction(payload as any);
    });
  };

  return { state: optimisticState, dispatch, isPending, formAction };
}
```
</details>
