# React Forms, React Hook Form, and React 19 Server Actions in TypeScript

> Typing form handling in React and TypeScript: controlled vs uncontrolled inputs, Zod schema validation with React Hook Form, React 19 Server Actions, and `useActionState`.

| Metadata | Details |
| :--- | :--- |
| **Category** | React and Frontend Typing |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [15-runtime-validation/02-zod.md](../15-runtime-validation/02-zod.md), [19-react-and-frontend/01-event-types.md](./01-event-types.md) |
| **TypeScript Baseline** | TS 5.0+, React 19 / @types/react 19 |
| **Tested tsconfig** | `"strict": true, "jsx": "react-jsx", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Form handling is the primary vector for user data entry in web applications. Without strong typing, form architectures suffer from:
1. **Field Name Drift**: Typos like `register('emial')` fail silently at runtime, failing to capture user input.
2. **Type Deserialization Issues**: HTML inputs always submit string values (`"42"` or `""`), but backend APIs expect numbers, booleans, or dates.
3. **Validation Sync Failures**: Maintaining separate client validation logic and backend validation rules results in inconsistent error messages.

Modern TypeScript form architectures solve this using two primary paradigms:
- **Client-Side Heavy**: **React Hook Form (RHF)** with **Zod resolver**, deriving form value types directly from the schema (`z.infer<typeof FormSchema>`).
- **Server-Side Integrated (React 19)**: Native form actions with **`useActionState`**, progressively enhancing standard HTML `<form>` submissions with server-side validation.

```
+-----------------------------------------------------------+
|               Type-Safe React Form Pipeline               |
|                                                           |
|  Zod Schema: z.object({ email: z.string().email() })      |
|                               |                           |
|                        z.infer<Schema>                    |
|                               v                           |
|  useForm<FormValues>({ resolver: zodResolver(Schema) })   |
|                               |                           |
|                 register('email') // Autocompleted!       |
|                               v                           |
|  onSubmit: (data: FormValues) => Promise<void>            |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. Controlled vs Uncontrolled Forms
- **Controlled (useState)**: Component state drives input value via `value={state}` and `onChange`. Causes re-renders on every keystroke.
- **Uncontrolled (Refs / RHF)**: DOM manages form values; React reads values on submit via `ref` or `FormData`. Vastly superior render performance.

### 2. React Hook Form Type Safety
RHF leverages TypeScript mapped types to ensure input field names passed to `register()` must be valid paths into the form schema:
```typescript
import { useForm } from 'react-hook-form';
import { zodResolver } from '@hookform/resolvers/zod';
import { z } from 'zod';

const schema = z.object({
  username: z.string().min(3),
  age: z.number().min(18)
});

type FormValues = z.infer<typeof schema>;

const { register, handleSubmit, formState: { errors } } = useForm<FormValues>({
  resolver: zodResolver(schema)
});
```

### 3. React 19 Server Actions
A form action is an asynchronous function that can be passed directly to `<form action={serverAction}>`:
```typescript
async function handleAction(prevState: State, formData: FormData): Promise<State> {
  'use server';
  const email = formData.get('email');
  // ...
}
```

---

## 3. How It Works Internally

Inside React Hook Form's `register(name)`:
TypeScript uses a recursive template literal type `Path<TFieldValues>` to calculate all valid dot-notation paths (e.g. `'user.address.street'`).
If you pass a typo `'user.addres'`, TypeScript rejects the call at compile time with `TS2345`.

---

## 4. Syntax & API Reference

### React 19 Form Hook Signatures
```typescript
import { useActionState } from 'react';
import { useFormStatus } from 'react-dom';

// Check if parent form is submitting (inside child button):
const { pending, data, method, action } = useFormStatus();
```

---

## 5. Practical Examples

### Example 1: Basic Uncontrolled Form with FormData Parsing
```typescript
import type { FormEvent } from 'react';

export function SimpleLoginForm() {
  const handleSubmit = (e: FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const formData = new FormData(e.currentTarget);
    const email = formData.get('email') as string;
    const password = formData.get('password') as string;
    console.log({ email, password });
  };

  return (
    <form onSubmit={handleSubmit}>
      <input name="email" type="email" required />
      <input name="password" type="password" required />
      <button type="submit">Log In</button>
    </form>
  );
}
```

### Example 2: Real-World React Hook Form with Zod Validation
```typescript
import { useForm } from 'react-hook-form';
import { zodResolver } from '@hookform/resolvers/zod';
import { z } from 'zod';

export const RegistrationSchema = z.object({
  email: z.string().email(),
  password: z.string().min(8),
  confirmPassword: z.string()
}).refine((data) => data.password === data.confirmPassword, {
  message: 'Passwords must match',
  path: ['confirmPassword']
});

export type RegistrationFormValues = z.infer<typeof RegistrationSchema>;

export function RegistrationForm() {
  const {
    register,
    handleSubmit,
    formState: { errors, isSubmitting }
  } = useForm<RegistrationFormValues>({
    resolver: zodResolver(RegistrationSchema)
  });

  const onSubmit = async (data: RegistrationFormValues) => {
    await fetch('/api/register', { method: 'POST', body: JSON.stringify(data) });
  };

  return (
    <form onSubmit={handleSubmit(onSubmit)}>
      <input {...register('email')} placeholder="Email" />
      {errors.email && <p className="error">{errors.email.message}</p>}

      <input {...register('password')} type="password" placeholder="Password" />
      {errors.password && <p className="error">{errors.password.message}</p>}

      <input {...register('confirmPassword')} type="password" placeholder="Confirm" />
      {errors.confirmPassword && <p className="error">{errors.confirmPassword.message}</p>}

      <button type="submit" disabled={isSubmitting}>Register</button>
    </form>
  );
}
```

### Example 3: Edge Case - Nested Dynamic Form Arrays (FieldArray)
```typescript
import { useForm, useFieldArray } from 'react-hook-form';

interface TeamFormValues {
  teamName: string;
  members: { name: string; role: 'lead' | 'dev' }[];
}

export function TeamForm() {
  const { register, control, handleSubmit } = useForm<TeamFormValues>({
    defaultValues: { teamName: '', members: [{ name: '', role: 'dev' }] }
  });

  const { fields, append, remove } = useFieldArray({ control, name: 'members' });

  return (
    <form onSubmit={handleSubmit(console.log)}>
      <input {...register('teamName')} placeholder="Team Name" />
      {fields.map((field, index) => (
        <div key={field.id}>
          <input {...register(`members.${index}.name` as const)} />
          <button type="button" onClick={() => remove(index)}>Remove</button>
        </div>
      ))}
      <button type="button" onClick={() => append({ name: '', role: 'dev' })}>Add Member</button>
    </form>
  );
}
```

### Example 4: Failure Case - Typo in `register()` Key
```typescript
const { register } = useForm<{ email: string }>();

// @ts-expect-error TS2345: Argument of type '"emaiil"' is not assignable to parameter of type '"email"'.
register('emaiil');
```

### Example 5: Idiomatic React 19 Submit Button with `useFormStatus`
```typescript
import { useFormStatus } from 'react-dom';

export function SubmitButton({ label = 'Submit' }: { label?: string }) {
  // Automatically detects if the enclosing <form> is in an active transition!
  const { pending } = useFormStatus();

  return (
    <button type="submit" disabled={pending}>
      {pending ? 'Saving...' : label}
    </button>
  );
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Number Inputs Yield Strings**:
   In native HTML, `<input type="number" />` yields a string in `e.target.value`. In Zod, use `z.coerce.number()` or RHF's `{ valueAsNumber: true }` to convert `"42"` to `42`.
2. **`useFormStatus` Must Be Inside a Child Component**:
   `useFormStatus()` reads context from the enclosing `<form>`. Calling `useFormStatus` inside the same component that renders `<form>` returns `pending: false` because the hook is outside the form's context! Always isolate submit buttons into child components.
3. **Checkbox Value Gotcha**:
   In HTML forms, an unchecked checkbox sends nothing in `FormData`, while a checked checkbox sends `"on"`. In Zod, model checkboxes as `z.boolean()` or `z.enum(['on']).optional()`.
4. **Form Resetting with Async Default Values**:
   Calling `reset()` immediately after fetching data requires passing the new values directly: `reset(data)` or setting `values` in `useForm({ values })`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **`useFormStatus` at Root** | `useFormStatus()` in `<Form>` component | Call inside child `<SubmitButton>` | `pending` is always false |
| **Untyped `FormData.get()`** | `const name = fd.get('name')` | Validate with Zod schema | Returns `FormDataEntryValue | null` |
| **Controlled State for 50 Inputs** | 50 `useState()` hooks in one form | React Hook Form uncontrolled refs | Severe typing lag on slow devices |
| **Forgetting `as const` on Field Array Path** | `register(\`items.${i}.val\`)` | `register(\`items.${i}.val\` as const)` | TS2345 template string widening |
| **Silent Form Submission Failure** | Missing `<button type="submit">` | Explicit submit buttons | Pressing enter fails in some browsers |
| **No Server-Side Validation** | Client-only Zod validation | Re-parse `FormData` on server action | Malicious actors bypass client JS |

---

## 8. Performance & Type-Checker Complexity

- **Uncontrolled Inputs**: React Hook Form isolates re-renders to the specific input being updated. A form with 100 inputs updates at 60 FPS without re-rendering sibling components.
- **Path Recursion**: RHF's `Path<T>` type can encounter recursion limits if your schema contains recursive types. Break recursive schemas into shallow levels.

---

## 9. Compiler Internals

When calling `register('email')`:
TypeScript calculates `Path<FormValues>`, which evaluates to `'email' | 'password'`. If the argument matches, the return type is inferred as `UseFormRegisterReturn<'email'>`, injecting `onChange`, `onBlur`, `name`, and `ref`.

---

## 10. Best Practices & Design Guidelines

1. **Derive Form Types from Zod**: Use `type FormData = z.infer<typeof Schema>` as single source of truth.
2. **Isolate Submit Buttons**: Use `useFormStatus` in separate child components.
3. **Use Uncontrolled Forms for Performance**: Prefer React Hook Form for forms with 5+ inputs.
4. **Validate on the Server**: Always re-validate in Server Actions.

---

## 11. Real-World Case Study

### Multi-Step Checkout Address Form with Server Action
A checkout form that submits via React 19 Server Action with progressive enhancement:

```typescript
import { useActionState } from 'react';
import { SubmitButton } from './SubmitButton';

interface AddressState {
  errors?: Record<string, string[]>;
  success?: boolean;
}

async function saveAddressAction(prevState: AddressState, formData: FormData): Promise<AddressState> {
  const street = formData.get('street') as string;
  const zip = formData.get('zip') as string;

  if (!zip || zip.length < 5) {
    return { errors: { zip: ['Invalid postal code'] } };
  }
  return { success: true };
}

export function ShippingAddressForm() {
  const [state, formAction] = useActionState(saveAddressAction, {});

  return (
    <form action={formAction}>
      <div>
        <label>Street Address</label>
        <input name="street" required />
      </div>
      <div>
        <label>Postal Code</label>
        <input name="zip" required />
        {state.errors?.zip && <p className="error">{state.errors.zip[0]}</p>}
      </div>
      <SubmitButton label="Save Address" />
    </form>
  );
}
```

---

## 12. Interview Questions

<details>
<summary>1. Why is React Hook Form significantly faster than controlled `useState` forms?</summary>

RHF uses uncontrolled components backed by DOM refs. Instead of re-rendering the entire form component on every keystroke, RHF registers inputs directly with DOM event listeners, updating component state only during validation errors or submission.
</details>

<details>
<summary>2. How does `zodResolver` bridge Zod schemas to React Hook Form types?</summary>

`zodResolver(schema)` runs `schema.safeParse(data)` during validation, mapping any `ZodError` into the error format expected by RHF (`Record<string, { message: string }>`), while preserving compile-time type inference for form inputs.
</details>

<details>
<summary>3. Why must `useFormStatus()` be used inside a child component of `<form>`?</summary>

`useFormStatus` uses React Context provided by the parent `<form>`. A component that renders `<form>` cannot read context provided by that same form; it must be called within a descendant child component.
</details>

<details>
<summary>4. How do you handle number conversion with inputs in TypeScript forms?</summary>

HTML inputs store values as strings. In Zod, use `z.coerce.number()`, or in RHF use `{ valueAsNumber: true }` in `register()`.
</details>

<details>
<summary>5. What are React 19 Server Actions?</summary>

Server Actions are asynchronous functions marked with `'use server'` that execute on the server. They can be passed directly to the `action` attribute of `<form>`, supporting progressive enhancement even before JavaScript has loaded in the browser.
</details>

<details>
<summary>6. How do you type a dynamic list of nested inputs in React Hook Form?</summary>

Using `useFieldArray<FormValues>({ control, name: 'items' })`. Each item in the array receives a unique `field.id` for use as a React key.
</details>

<details>
<summary>7. What is the difference between `formState.isSubmitting` and `useFormStatus().pending`?</summary>

`isSubmitting` is an RHF property tracking client-side form submission. `useFormStatus().pending` is a React DOM property tracking whether a React 19 Server Action / Transition is actively pending.
</details>

<details>
<summary>8. How do you handle file uploads in Server Actions?</summary>

By reading files from `formData`:
```typescript
const file = formData.get('file') as File;
const buffer = await file.arrayBuffer();
```
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Zod Form Schema</summary>

**Problem:** Define a Zod schema for a newsletter signup form requiring `email: string` and optional `subscribedToMarketing: boolean`.

**Solution:**
```typescript
import { z } from 'zod';

export const NewsletterSchema = z.object({
  email: z.string().email(),
  subscribedToMarketing: z.boolean().default(false)
});

export type NewsletterForm = z.infer<typeof NewsletterSchema>;
```
</details>

<details>
<summary>Exercise 2 (Easy): FormData Extractor Helper</summary>

**Problem:** Write a helper that extracts a required string field from `FormData`, throwing an error if missing.

**Solution:**
```typescript
export function getRequiredString(fd: FormData, key: string): string {
  const val = fd.get(key);
  if (typeof val !== 'string' || !val.trim()) {
    throw new Error(`Missing required field: ${key}`);
  }
  return val.trim();
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Password Confirmation Zod Refinement</summary>

**Problem:** Create a Zod schema validating that `newPassword` and `confirmPassword` are identical, attaching the error to `confirmPassword`.

**Solution:**
```typescript
import { z } from 'zod';

export const ResetPasswordSchema = z.object({
  newPassword: z.string().min(8),
  confirmPassword: z.string()
}).refine((val) => val.newPassword === val.confirmPassword, {
  message: 'Passwords do not match',
  path: ['confirmPassword']
});
```
</details>

<details>
<summary>Exercise 4 (Medium): Custom Submit Button with Loading Indicator</summary>

**Problem:** Create a submit button using `useFormStatus` that disables itself and shows a spinner when pending.

**Solution:**
```typescript
import { useFormStatus } from 'react-dom';

export function LoadingSubmit() {
  const { pending } = useFormStatus();
  return (
    <button type="submit" disabled={pending}>
      {pending ? 'Submitting...' : 'Submit'}
    </button>
  );
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe FormData Validator</summary>

**Problem:** Write a function `validateFormData<T>(schema: z.ZodType<T>, formData: FormData): { success: true; data: T } | { success: false; errors: Record<string, string[]> }`.

**Solution:**
```typescript
import { z } from 'zod';

export function validateFormData<T>(
  schema: z.ZodType<T>,
  formData: FormData
) {
  const raw: Record<string, unknown> = {};
  formData.forEach((value, key) => { raw[key] = value; });
  const result = schema.safeParse(raw);

  if (result.success) {
    return { success: true as const, data: result.data };
  }
  return {
    success: false as const,
    errors: result.error.flatten().fieldErrors
  };
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Multi-Step Form State Machine with Actions</summary>

**Problem:** Create a 2-step form state machine handler using `useActionState` that transitions from `STEP_1` to `STEP_2` upon valid initial input.

**Solution:**
```typescript
export type StepState =
  | { step: 1; email?: string }
  | { step: 2; email: string; code?: string };

export async function stepAction(prev: StepState, formData: FormData): Promise<StepState> {
  if (prev.step === 1) {
    const email = formData.get('email') as string;
    return { step: 2, email };
  }
  return prev;
}
```
</details>
