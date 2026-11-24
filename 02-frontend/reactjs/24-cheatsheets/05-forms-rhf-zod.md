# Forms with React Hook Form & Zod Cheatsheet

> Quick-reference guide for schema validation, controlled inputs, error handling, and form submission with React Hook Form v7 and Zod.

---

## 1. Schema Definition with Zod

```ts
import { z } from 'zod';

export const userRegistrationSchema = z.object({
  username: z.string().min(3, 'Username must be at least 3 chars').max(20),
  email: z.string().email('Invalid email address'),
  password: z.string().min(8, 'Password must be at least 8 chars'),
  confirmPassword: z.string(),
  age: z.coerce.number().min(18, 'Must be at least 18'),
  acceptTerms: z.literal(true, {
    errorMap: () => ({ message: 'You must accept terms' }),
  }),
}).refine((data) => data.password === data.confirmPassword, {
  message: "Passwords don't match",
  path: ['confirmPassword'],
});

export type UserRegistrationInput = z.infer<typeof userRegistrationSchema>;
```

---

## 2. React Hook Form Integration

```tsx
import { useForm } from 'react-hook-form';
import { zodResolver } from '@hookform/resolvers/zod';
import { userRegistrationSchema, UserRegistrationInput } from './schema';

export function RegistrationForm() {
  const {
    register,
    handleSubmit,
    formState: { errors, isSubmitting },
    reset,
  } = useForm<UserRegistrationInput>({
    resolver: zodResolver(userRegistrationSchema),
    defaultValues: { username: '', email: '', age: 18 },
  });

  const onSubmit = async (data: UserRegistrationInput) => {
    await submitApi(data);
    reset();
  };

  return (
    <form onSubmit={handleSubmit(onSubmit)} className="space-y-4">
      <div>
        <label>Username</label>
        <input {...register('username')} />
        {errors.username && <p className="error">{errors.username.message}</p>}
      </div>

      <div>
        <label>Email</label>
        <input type="email" {...register('email')} />
        {errors.email && <p className="error">{errors.email.message}</p>}
      </div>

      <button type="submit" disabled={isSubmitting}>
        {isSubmitting ? 'Registering...' : 'Register'}
      </button>
    </form>
  );
}
```

---

## 3. Controlled Custom Primitives (`Controller`)

```tsx
import { Controller, useForm } from 'react-hook-form';
import { Switch } from '@radix-ui/react-switch';

<Controller
  name="notificationsEnabled"
  control={control}
  render={({ field }) => (
    <Switch checked={field.value} onCheckedChange={field.onChange} />
  )}
/>
```
