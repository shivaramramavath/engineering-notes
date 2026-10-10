# Forms

Forms are where typed UI meets untyped reality: every browser value is a string, users type nonsense, and the server must re-check everything. A good form setup has one **schema** that defines the valid shape, derives the TypeScript type from it, validates on the client for quick feedback, and validates again on the server. This note covers controlled and uncontrolled inputs, typing form state and handlers, schema validation, form libraries, and React 19's form actions.

**Prerequisites:**
- [Event types](./01-event-types.md)
- [Hooks](./02-hooks.md)
- [Schema validation](../15-runtime-validation/01-schema-validation.md) and [Zod](../15-runtime-validation/02-zod.md)

---

## Controlled inputs

A **controlled** input's value lives in React state. The input displays the state, and each change updates it.

```tsx
function NameForm() {
  const [name, setName] = useState("");

  return (
    <form onSubmit={(e) => { e.preventDefault(); save(name); }}>
      <input value={name} onChange={(e) => setName(e.target.value)} />
      <button type="submit">Save</button>
    </form>
  );
}
```

Controlled inputs give you the current value on every keystroke (for live validation, formatting, or dependent fields), at the cost of a re-render per change.

### A form state object

For several fields, keep one typed object:

```tsx
interface SignupValues {
  email: string;
  age: string;            // inputs produce strings, so keep the raw form value as a string
  plan: "free" | "pro";
  acceptTerms: boolean;
}

const initialValues: SignupValues = { email: "", age: "", plan: "free", acceptTerms: false };

function SignupForm() {
  const [values, setValues] = useState<SignupValues>(initialValues);

  function setField<K extends keyof SignupValues>(key: K, value: SignupValues[K]) {
    setValues((prev) => ({ ...prev, [key]: value }));
  }

  return (
    <form>
      <input value={values.email} onChange={(e) => setField("email", e.target.value)} />
      <input value={values.age} onChange={(e) => setField("age", e.target.value)} />
      <select value={values.plan} onChange={(e) => setField("plan", e.target.value as SignupValues["plan"])}>
        <option value="free">Free</option>
        <option value="pro">Pro</option>
      </select>
      <input
        type="checkbox"
        checked={values.acceptTerms}
        onChange={(e) => setField("acceptTerms", e.target.checked)}
      />
    </form>
  );
}
```

`setField<K extends keyof SignupValues>(key: K, value: SignupValues[K])` ties each field name to its value type: `setField("acceptTerms", "yes")` is an error. Compare with the common shortcut of one generic `handleChange` using `e.target.name`:

```tsx
function handleChange(e: ChangeEvent<HTMLInputElement>) {
  setValues((prev) => ({ ...prev, [e.target.name]: e.target.value }));   // computed key: loses type safety
}
```

It is shorter, but TypeScript cannot check that `name` is a real field or that `value` has the right type. Prefer the typed `setField` for forms you care about.

Notes on specific inputs:

- **Numbers:** the raw value is a string. Keep it as a string in form state and convert when validating. Parsing on every keystroke fights the user (typing `1.` or an empty field).
- **Checkboxes:** use `checked`, not `value`.
- **Selects:** the value is a `string`. Narrow or validate it if your state is a union.
- **Files:** use `e.target.files?.[0]`, and store the `File` outside the form-value object if you serialize it ([event types](./01-event-types.md)).

## Uncontrolled inputs and `FormData`

An **uncontrolled** input keeps its own value in the DOM. You read it on submit, using `FormData`:

```tsx
function handleSubmit(e: FormEvent<HTMLFormElement>) {
  e.preventDefault();
  const data = new FormData(e.currentTarget);
  const raw = Object.fromEntries(data);         // { [k: string]: FormDataEntryValue }, each string | File
  const result = SignupSchema.safeParse(raw);
  // ...
}

<form onSubmit={handleSubmit}>
  <input name="email" type="email" defaultValue="" />
  <input name="age" type="number" />
  <button type="submit">Sign up</button>
</form>
```

Each input needs a `name`, and you use `defaultValue` (not `value`) for initial values. Uncontrolled forms re-render less and are simpler when you only need values at submit time. They also work with native browser validation and with server actions ([below](#react-19-form-actions)).

`Object.fromEntries(data)` produces strings (and `File`s), so a schema with coercion turns them into the real types.

**Do not switch an input between controlled and uncontrolled.** Passing `value={undefined}` first and a string later triggers a warning. Always give controlled inputs a defined initial value (`""`, not `undefined`).

## Validate with a schema

Define the shape once, derive the type, and validate both on the client and the server ([trust boundaries](../15-runtime-validation/00-trust-boundaries.md)):

```tsx
import { z } from "zod";

const SignupSchema = z.object({
  email: z.string().email("Enter a valid email"),
  age: z.coerce.number().int().min(18, "You must be at least 18"),
  plan: z.enum(["free", "pro"]),
  acceptTerms: z.literal(true, { errorMap: () => ({ message: "You must accept the terms" }) }),
});

type SignupInput = z.input<typeof SignupSchema>;     // what the form produces (strings, before coercion)
type SignupData = z.output<typeof SignupSchema>;     // what you get after validation (age is a number)
```

(Zod 4 customizes error messages with `{ error: ... }` instead of `errorMap`. Check the [Zod note](../15-runtime-validation/02-zod.md) for the version differences.)

The **input** type and **output** type differ when you use coercion or defaults. Use `z.input` for the form's raw values and `z.output` for the validated data your submit function receives.

Collect errors into a map by field:

```tsx
type FieldErrors = Partial<Record<keyof SignupInput, string>>;

function validate(values: unknown): { data: SignupData } | { errors: FieldErrors } {
  const result = SignupSchema.safeParse(values);
  if (result.success) return { data: result.data };

  const errors: FieldErrors = {};
  for (const issue of result.error.issues) {
    const key = issue.path[0] as keyof SignupInput | undefined;
    if (key && !errors[key]) errors[key] = issue.message;     // first message per field
  }
  return { errors };
}
```

Show errors next to their fields, tie them to inputs for screen readers, and only show a field's error after the user has touched it (or tried to submit):

```tsx
const emailId = useId();

<label htmlFor={emailId}>Email</label>
<input
  id={emailId}
  value={values.email}
  onChange={(e) => setField("email", e.target.value)}
  aria-invalid={Boolean(errors.email)}
  aria-describedby={errors.email ? `${emailId}-error` : undefined}
/>
{errors.email && <p id={`${emailId}-error`} role="alert">{errors.email}</p>}
```

`useId` generates stable, unique ids ([hooks](./02-hooks.md)).

## Form libraries

For anything beyond a few fields, a library handles touched state, errors, dirty tracking, array fields, and performance. **React Hook Form** is widely used, with resolvers that plug in a schema:

```tsx
import { useForm } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";

type FormValues = z.input<typeof SignupSchema>;

function SignupForm() {
  const {
    register,
    handleSubmit,
    formState: { errors, isSubmitting },
  } = useForm<FormValues>({ resolver: zodResolver(SignupSchema) });

  const onSubmit = async (data: FormValues) => {
    await api("POST /signup", { body: data });
  };

  return (
    <form onSubmit={handleSubmit(onSubmit)}>
      <input {...register("email")} />
      {errors.email && <p>{errors.email.message}</p>}

      <input type="number" {...register("age")} />
      {errors.age && <p>{errors.age.message}</p>}

      <button disabled={isSubmitting}>Sign up</button>
    </form>
  );
}
```

`register("email")` is typed against `FormValues`, so `register("emial")` is an error. The generics, resolver typing, and how input/output types (with coercion or transforms) are reflected depend on the versions of React Hook Form, the resolver package, and Zod, and have changed across releases, so follow the current documentation of each. The principle stays the same: one schema, types derived from it, and the same schema reused on the server.

## React 19 form actions

React 19 lets a `<form>` take a function as its `action`. React calls it with the form's `FormData`, manages pending state, and resets uncontrolled fields after a successful submission:

```tsx
async function signup(formData: FormData) {
  const result = SignupSchema.safeParse(Object.fromEntries(formData));
  if (!result.success) return;
  await api("POST /signup", { body: result.data });
}

<form action={signup}>
  <input name="email" />
  <button type="submit">Sign up</button>
</form>
```

Related hooks (check your React and `@types/react` versions):

- **`useFormStatus()`** (from `react-dom`): inside a component rendered **within** the form, tells you whether the form is submitting (`pending`), for disabling buttons and showing spinners.
- **`useActionState(action, initialState)`**: wraps an action so its **return value** becomes state, which is how you return validation errors to the form. The action receives the previous state and the `FormData`.

```tsx
type State = { errors?: FieldErrors; ok?: boolean };

async function signupAction(prev: State, formData: FormData): Promise<State> {
  const result = SignupSchema.safeParse(Object.fromEntries(formData));
  if (!result.success) return { errors: toFieldErrors(result.error) };
  await save(result.data);
  return { ok: true };
}

const [state, formAction, pending] = useActionState(signupAction, {});
<form action={formAction}> ... {state.errors?.email && <p>{state.errors.email}</p>} </form>
```

In frameworks with server actions (the Next.js App Router), the action can run **on the server**, which means validation happens where it cannot be bypassed ([Next.js](./09-nextjs.md)).

## Submitting

- **Prevent default** in `onSubmit` handlers (`e.preventDefault()`), unless you use an `action`.
- **Disable the submit button while submitting** to prevent double submits, and handle the failure path: re-enable, show an error.
- **Use `type="button"`** on buttons that should not submit the form. A `<button>` inside a form defaults to `type="submit"`.
- **Handle server errors:** map the API's error response onto fields (validation issues) or a form-level message ([error response types](../16-type-safe-apis/04-error-response-types.md)).
- **Never trust the client.** The server must validate again, because client validation can be bypassed.

## Accessibility and UX

- Every input needs a **label** (`<label htmlFor>` or wrapping). Placeholders are not labels.
- Use the right `type` (`email`, `tel`, `number`, `date`) and `autoComplete` values for better keyboards and autofill.
- Tie error messages to inputs with `aria-describedby`, and mark invalid fields with `aria-invalid`.
- Do not rely on color alone to show errors.
- Preserve user input when validation fails.

## Common mistakes

- Storing numeric inputs as numbers in form state and fighting the user's typing.
- Using `value` instead of `checked` for checkboxes.
- Switching between controlled and uncontrolled by passing `undefined` as the value.
- A generic `handleChange` with `[e.target.name]` that no longer type-checks values.
- Validating only on the client.
- Forgetting `e.preventDefault()`, so the page reloads.
- Confusing the schema's input and output types, and treating coerced numbers as strings (or the reverse).
- Using `z.coerce.boolean()` for checkboxes or `"true"/"false"` strings, where any non-empty string becomes `true`.
- Showing every error immediately, before the user has interacted.
- A `<button>` without `type`, submitting the form unexpectedly.
- Missing labels and `aria` associations.

## Debugging

- Log `Object.fromEntries(new FormData(form))` to see the raw strings you are validating.
- Print the schema's `issues` (path and message) to see exactly which field fails and why.
- If the form submits unexpectedly, check button `type` and enter-key behavior on inputs.
- If an input "will not type", it is controlled with a value that never updates, or its `onChange` is missing.
- If a controlled/uncontrolled warning appears, find the input whose value is `undefined` on the first render.
- For React Hook Form type errors, hover `useForm`'s generic and compare the form values type with the schema's input type.

## Quick summary

- Controlled inputs keep value in state. Uncontrolled inputs keep it in the DOM and are read via `FormData`. Never switch between them.
- Keep raw form values as strings, type form state with a `setField<K extends keyof T>` helper, and use `checked` for checkboxes.
- Define one schema, derive types with `z.input` (raw) and `z.output` (validated), and validate on both client and server.
- Use a form library for non-trivial forms, and React 19 actions (`useActionState`, `useFormStatus`) or server actions for submission.
- Handle submitting state, server errors, and accessibility (labels, `aria-invalid`, `aria-describedby`).

**Next:** [Generic and polymorphic components](./06-generic-and-polymorphic-components.md)
