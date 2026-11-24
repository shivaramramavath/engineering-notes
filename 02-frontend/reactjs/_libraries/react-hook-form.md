# React Hook Form — Basic to Advanced

A comprehensive reference card for `react-hook-form` v7 in React 19.

> **Target Version:** React Hook Form v7.54+  
> **Philosophy:** Uncontrolled components, isolated re-renders, high performance  
> **Schema Validation:** Integrates with Zod, Yup, Valibot via `@hookform/resolvers`  

---

## Level 0 — Concepts

- **Uncontrolled Component Architecture**: Rather than updating React state on every single keystroke (`value={val} onChange={setVal}`), React Hook Form registers DOM inputs via native refs (`ref={register().ref}`). Keystrokes mutate the input DOM directly, preventing component re-renders.
- **Subscriptions on Demand**: Components only re-render when explicitly watching fields (`useWatch`) or when submission errors trigger validation.
- **Core Primitives Table**:

| API | Type | Purpose |
| :--- | :--- | :--- |
| `useForm()` | Hook | Primary hook managing form state, registration, and submission |
| `register()` | Function | Binds an uncontrolled input to form state via ref and event listeners |
| `handleSubmit()` | Function | Validates form data before executing the success submission handler |
| `formState` | Object | Exposes `errors`, `isSubmitting`, `isDirty`, `isValid` flags |
| `<Controller />` | Component | Wrapper for controlled custom UI inputs (Radix, Select, Switch) |
| `useWatch()` | Hook | Subscribes to specific input values isolated from form re-renders |
| `useFieldArray()` | Hook | Manages dynamic, addable, sortable list fields |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install react-hook-form @hookform/resolvers zod
```

### 1.2 Basic Form with Native Inputs
```tsx
import React from 'react';
import { useForm } from 'react-hook-form';

interface FormData {
  username: string;
  email: string;
}

export function SimpleForm() {
  const {
    register,
    handleSubmit,
    formState: { errors, isSubmitting },
  } = useForm<FormData>();

  const onSubmit = async (data: FormData) => {
    console.log('Submitted data:', data);
  };

  return (
    <form onSubmit={handleSubmit(onSubmit)} className="space-y-4 max-w-sm">
      <div>
        <label className="block text-sm font-medium">Username</label>
        <input
          {...register('username', { required: 'Username is required', minLength: { value: 3, message: 'Min 3 chars' } })}
          className="border p-2 rounded w-full"
        />
        {errors.username && <p className="text-red-500 text-xs">{errors.username.message}</p>}
      </div>

      <div>
        <label className="block text-sm font-medium">Email</label>
        <input
          {...register('email', { required: 'Email is required' })}
          className="border p-2 rounded w-full"
        />
        {errors.email && <p className="text-red-500 text-xs">{errors.email.message}</p>}
      </div>

      <button
        type="submit"
        disabled={isSubmitting}
        className="px-4 py-2 bg-blue-600 text-white rounded font-medium disabled:opacity-50"
      >
        {isSubmitting ? 'Saving...' : 'Submit'}
      </button>
    </form>
  );
}
```

---

## Level 2 — Intermediate: Schema Validation with Zod

```tsx
import { useForm } from 'react-hook-form';
import { zodResolver } from '@hookform/resolvers/zod';
import { z } from 'zod';

const schema = z.object({
  email: z.string().email('Invalid email address'),
  age: z.number().min(18, 'Must be at least 18'),
});

type FormValues = z.infer<typeof schema>;

export function ZodValidatedForm() {
  const { register, handleSubmit, formState: { errors } } = useForm<FormValues>({
    resolver: zodResolver(schema),
  });

  return (
    <form onSubmit={handleSubmit((data) => console.log(data))}>
      <input {...register('email')} />
      {errors.email && <span>{errors.email.message}</span>}
      <input type="number" {...register('age', { valueAsNumber: true })} />
      {errors.age && <span>{errors.age.message}</span>}
      <button type="submit">Submit</button>
    </form>
  );
}
```

---

## Level 3 — Advanced: Controlled UI Primitives with `<Controller />`

For third-party components (e.g. Radix UI Slider, React Select, DatePickers) that do not expose a native input ref:
```tsx
import { Controller, useForm } from 'react-hook-form';
import * as Switch from '@radix-ui/react-switch';

export function ControlledSwitchForm() {
  const { control, handleSubmit } = useForm({ defaultValues: { notifications: true } });

  return (
    <form onSubmit={handleSubmit(console.log)}>
      <Controller
        name="notifications"
        control={control}
        render={({ field: { value, onChange } }) => (
          <Switch.Root checked={value} onCheckedChange={onChange}>
            <Switch.Thumb />
          </Switch.Root>
        )}
      />
    </form>
  );
}
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Using `watch()` at Component Root for Input Display**| Re-renders the entire component on every keystroke, defeating uncontrolled benefits. | Use `useWatch({ control, name })` inside an isolated child component. |
| **Omitting `defaultValues`** | Can cause inputs to switch between uncontrolled and controlled states. | Always provide complete `defaultValues` in `useForm`. |
| **Passing Strings for Number Inputs** | Zod schema validation fails with `"Expected number, received string"`. | Use `{ valueAsNumber: true }` in `register()`. |
| **Modifying Dynamic Lists Without `useFieldArray`** | Desynchronizes internal index keys, creating corrupted form states. | Always manage dynamic arrays with `useFieldArray({ control, name })`. |
