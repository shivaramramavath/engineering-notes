# React Hook Form

React Hook Form (RHF) is the most widely used form library for React. It registers inputs as **uncontrolled** (so typing doesn't re-render your component), tracks validation and submission state, and integrates with schema libraries like Zod. This file covers the core API and the patterns you'll use in nearly every form.

## Prerequisites

[`00-controlled-and-uncontrolled-inputs.md`](./00-controlled-and-uncontrolled-inputs.md) and [`01-form-validation.md`](./01-form-validation.md)

Install:

```bash
npm install react-hook-form zod @hookform/resolvers
```

APIs shown here are stable across recent versions; check the RHF docs for the version you install, especially around TypeScript generics with Zod resolvers.

---

## Why a library

Hand-written forms need per-field state, touched tracking, error derivation, submit handling, and reset logic. RHF packages that, and its design avoids the biggest cost of controlled forms: **a re-render of the whole form on every keystroke**. Inputs are read from the DOM, and re-renders happen only where you subscribe to values.

---

## The basic form

```tsx
import { useForm } from "react-hook-form";

type FormValues = {
  email: string;
  password: string;
};

function LoginForm() {
  const {
    register,
    handleSubmit,
    formState: { errors, isSubmitting },
  } = useForm<FormValues>({
    defaultValues: { email: "", password: "" },
  });

  async function onSubmit(values: FormValues) {
    await login(values);
  }

  return (
    <form onSubmit={handleSubmit(onSubmit)} noValidate>
      <label htmlFor="email">Email</label>
      <input
        id="email"
        type="email"
        {...register("email", { required: "Email is required" })}
      />
      {errors.email && <p role="alert">{errors.email.message}</p>}

      <label htmlFor="password">Password</label>
      <input
        id="password"
        type="password"
        {...register("password", {
          required: "Password is required",
          minLength: { value: 8, message: "Use at least 8 characters" },
        })}
      />
      {errors.password && <p role="alert">{errors.password.message}</p>}

      <button disabled={isSubmitting}>
        {isSubmitting ? "Signing in…" : "Sign in"}
      </button>
    </form>
  );
}
```

The key pieces:

| API | Purpose |
|-----|---------|
| `useForm<T>({...})` | Creates the form instance; the generic types every field name |
| `register("name", rules?)` | Returns props (`name`, `ref`, `onChange`, `onBlur`) to spread on an input |
| `handleSubmit(onValid, onInvalid?)` | Prevents default, validates, then calls `onValid(values)` only if valid |
| `formState.errors` | Errors keyed by field name |
| `formState.isSubmitting` | `true` while your async `onSubmit` is running |
| `defaultValues` | Initial values (also the target of `reset()`); **set them for every field** |

`handleSubmit` awaits your function, so `isSubmitting` flips back automatically when it resolves or throws.

---

## Validation with Zod

Instead of per-field rules, hand RHF a schema through a **resolver**. This keeps validation in one place and gives you types from the schema ([`01-form-validation.md`](./01-form-validation.md)):

```tsx
import { useForm } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";
import { z } from "zod";

const schema = z
  .object({
    email: z.string().min(1, "Email is required").email("Enter a valid email"),
    password: z.string().min(8, "Use at least 8 characters"),
    confirmPassword: z.string(),
  })
  .refine((d) => d.password === d.confirmPassword, {
    message: "Passwords don't match",
    path: ["confirmPassword"],
  });

type FormValues = z.infer<typeof schema>;

function SignupForm() {
  const {
    register,
    handleSubmit,
    formState: { errors, isSubmitting },
  } = useForm<FormValues>({
    resolver: zodResolver(schema),
    defaultValues: { email: "", password: "", confirmPassword: "" },
  });

  return (
    <form onSubmit={handleSubmit(async (values) => { await signup(values); })} noValidate>
      <input {...register("email")} type="email" aria-invalid={!!errors.email} />
      {errors.email && <p role="alert">{errors.email.message}</p>}
      {/* password fields similarly */}
      <button disabled={isSubmitting}>Create account</button>
    </form>
  );
}
```

When schemas use transforms or coercion (`z.coerce.number()`), the **input** type and **output** type differ; newer Zod and resolver versions expose both types. If you hit type errors, consult the resolver's docs for your versions.

---

## When validation runs: `mode` and `reValidateMode`

```tsx
useForm({
  mode: "onTouched",          // when to validate first
  reValidateMode: "onChange", // after the first submit attempt, when to re-validate
});
```

| `mode` | First validation |
|--------|------------------|
| `"onSubmit"` (default) | On submit |
| `"onBlur"` | When a field loses focus |
| `"onChange"` | On every change (more re-renders) |
| `"onTouched"` | On first blur, then on every change — the "reward early, punish late" pattern |
| `"all"` | Both blur and change |

`"onTouched"` is a good default for user-facing forms. See the UX discussion in [`01-form-validation.md`](./01-form-validation.md).

---

## Useful form state

```tsx
const {
  formState: { errors, isSubmitting, isDirty, isValid, isSubmitSuccessful, touchedFields, dirtyFields },
} = useForm<FormValues>();
```

| Property | Meaning |
|----------|---------|
| `isDirty` | Any value differs from `defaultValues` — drive "unsaved changes" prompts |
| `isValid` | The form currently passes validation (only reliable with a non-default `mode`) |
| `isSubmitSuccessful` | The last submit completed without errors |

**RHF's `formState` is a Proxy**: you only subscribe to the properties you read in render. Reading `isDirty` costs a re-render when it changes; not reading it costs nothing. Don't destructure properties you don't use.

---

## Reading and setting values

```tsx
const { watch, getValues, setValue, reset, trigger, setFocus } = useForm<FormValues>();

const email = watch("email");           // subscribes this component: re-renders on change
const all = getValues();                // reads current values without subscribing
setValue("email", "ada@example.com", { shouldValidate: true, shouldDirty: true });
reset();                                // back to defaultValues
reset({ email: "new@example.com", password: "" });   // to a new set of defaults
await trigger("email");                 // run validation for a field (or all fields)
setFocus("password");
```

- **`watch`** re-renders the host component on every change of the watched field. For isolated subscriptions use **`useWatch`** inside a small child component, so only that child re-renders.
- **`getValues`** doesn't subscribe — use it in event handlers.
- Load existing data (editing a record) by calling `reset(data)` when it arrives, or pass `values` to `useForm`.

---

## Controlled components: `Controller`

`register` works with native inputs. **Custom components** that don't expose a ref or a native `onChange` (many UI-library selects, date pickers, sliders) need `Controller`, which bridges them to the form:

```tsx
import { Controller } from "react-hook-form";

<Controller
  name="country"
  control={control}
  render={({ field, fieldState }) => (
    <CountrySelect
      value={field.value}
      onChange={field.onChange}
      onBlur={field.onBlur}
      invalid={!!fieldState.error}
    />
  )}
/>
```

`field` carries `value`, `onChange`, `onBlur`, `name`, and `ref`; `fieldState` carries `error`, `isTouched`, and `isDirty`. Prefer `register` when it works — it's faster. Component-library field wrappers: [`../09-ui-components/06-form-controls.md`](../09-ui-components/06-form-controls.md).

---

## Sharing the form with nested components

For big forms split across components, avoid prop-drilling `register` and `errors`: use `FormProvider` and `useFormContext`:

```tsx
import { FormProvider, useForm, useFormContext } from "react-hook-form";

function AddressFields() {
  const { register, formState: { errors } } = useFormContext<FormValues>();
  return (
    <>
      <input {...register("street")} />
      {errors.street && <p>{errors.street.message}</p>}
    </>
  );
}

function Checkout() {
  const methods = useForm<FormValues>({ resolver: zodResolver(schema) });
  return (
    <FormProvider {...methods}>
      <form onSubmit={methods.handleSubmit(onSubmit)}>
        <AddressFields />
      </form>
    </FormProvider>
  );
}
```

This is also the foundation of multi-step forms ([`04-multi-step-forms.md`](./04-multi-step-forms.md)).

---

## A reusable field component

Remove repetition by wrapping label, input, and error message once:

```tsx
type FieldProps = ComponentProps<"input"> & {
  label: string;
  error?: string;
};

function Field({ label, error, id, ...inputProps }: FieldProps) {
  const generatedId = useId();                        // always call hooks unconditionally
  const inputId = id ?? generatedId;
  const errorId = `${inputId}-error`;
  return (
    <div>
      <label htmlFor={inputId}>{label}</label>
      <input
        id={inputId}
        aria-invalid={!!error}
        aria-describedby={error ? errorId : undefined}
        {...inputProps}
      />
      {error && <p id={errorId} role="alert">{error}</p>}
    </div>
  );
}

<Field label="Email" type="email" error={errors.email?.message} {...register("email")} />
```

In React 19 `ref` passes as a regular prop, so `register`'s ref reaches the `<input>` through `...inputProps`; older versions need `forwardRef` ([`../04-typescript-with-react/00-typing-components-and-props.md`](../04-typescript-with-react/00-typing-components-and-props.md)). Note that `useId` is called **unconditionally** and the fallback happens afterward; writing `id ?? useId()` would call the hook conditionally and violate the Rules of Hooks ([`../03-hooks/00-hook-rules.md`](../03-hooks/00-hook-rules.md)).

---

## Server errors and submission

Pushing server-side validation errors into the form (`setError`), handling failures, and double-submit protection are in [`05-form-submission-and-errors.md`](./05-form-submission-and-errors.md).

---

## Dynamic and multi-step forms

- Field arrays (`useFieldArray`) and conditional fields: [`03-dynamic-forms.md`](./03-dynamic-forms.md)
- Multi-step wizards with per-step validation (`trigger`): [`04-multi-step-forms.md`](./04-multi-step-forms.md)

---

## Alternatives

| Library | Notes |
|---------|-------|
| **TanStack Form** | Strong typing and framework-agnostic; a newer option |
| **Formik** | Older, controlled-by-default, re-renders more; common in legacy code |
| **Native `<form>` + `FormData`** | Perfectly fine for small forms ([`00-controlled-and-uncontrolled-inputs.md`](./00-controlled-and-uncontrolled-inputs.md)) |
| **React 19 form Actions** | Built-in `<form action>` with `useActionState` — pairs well with server frameworks ([`../15-concurrent-and-modern-react/05-react-19-features.md`](../15-concurrent-and-modern-react/05-react-19-features.md)) |

Quick reference: [`../24-cheatsheets/05-forms-rhf-zod.md`](../24-cheatsheets/05-forms-rhf-zod.md).

---

## Common mistakes

- **Omitting `defaultValues`** — fields start `undefined`, causing controlled/uncontrolled warnings and broken `isDirty`/`reset`.
- **Using `watch` at the top of a big form** — re-renders everything on each keystroke; use `useWatch` in a child.
- **Using `Controller` for plain inputs** — slower than `register`.
- **Spreading `register` and also passing your own `onChange`/`onBlur`** — you override RHF's handlers; wrap or use the option form.
- **Reading `formState` properties you don't need** — subscribes to updates unnecessarily.
- **Rules of Hooks violations in field wrappers** (conditional `useId`).
- **Forgetting `noValidate`** when using schema validation — the browser's native messages appear first.
- **Ignoring schema input/output type differences** with `coerce`/`transform` — leads to confusing type errors.
- **Not awaiting async work in `onSubmit`** — `isSubmitting` turns off early.

## Quick summary

- RHF registers inputs as uncontrolled, minimizing re-renders
- Core API: `useForm`, `register`, `handleSubmit`, `formState.errors`, `defaultValues`
- Plug in Zod with `zodResolver`; derive the form type with `z.infer`
- Choose `mode: "onTouched"` for good validation timing
- `watch` re-renders its host; `useWatch` isolates it; `getValues` doesn't subscribe
- Use `Controller` for custom components and `FormProvider` for nested fields
- Subscribe only to the `formState` you actually use

## Next

**[`03-dynamic-forms.md`](./03-dynamic-forms.md)** covers conditional fields, repeatable groups, and schema-driven forms.
