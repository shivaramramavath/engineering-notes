# Dynamic Forms

Real forms rarely have a fixed list of fields. Fields appear when another answer demands them, users add as many rows as they need (phone numbers, invoice line items), and sometimes the whole form is described by data from a server. This file covers conditional fields, repeatable field groups with `useFieldArray`, dependent fields, and schema-driven forms.

## Prerequisites

[`02-react-hook-form.md`](./02-react-hook-form.md) and [`01-form-validation.md`](./01-form-validation.md)

---

## 1. Conditional fields

Show a field only when another field's value calls for it. Subscribe to the controlling value with `useWatch` (or `watch`) and render conditionally:

```tsx
import { useForm, useWatch } from "react-hook-form";

type FormValues = {
  contactMethod: "email" | "phone";
  email?: string;
  phone?: string;
};

function ContactForm() {
  const { register, control, handleSubmit } = useForm<FormValues>({
    defaultValues: { contactMethod: "email" },
  });

  const method = useWatch({ control, name: "contactMethod" });

  return (
    <form onSubmit={handleSubmit(onSubmit)}>
      <select {...register("contactMethod")}>
        <option value="email">Email</option>
        <option value="phone">Phone</option>
      </select>

      {method === "email" && <input type="email" {...register("email")} />}
      {method === "phone" && <input type="tel" {...register("phone")} />}

      <button>Send</button>
    </form>
  );
}
```

### What happens to hidden fields?

By default, when an input unmounts, RHF **keeps its value** in form state (and still submits it). That's often wrong: a user who picks "phone" shouldn't submit a stale email. Options:

- **`shouldUnregister: true`** in `useForm` — unmounted fields' values are removed. Simple, but also drops values in multi-step flows where earlier steps unmount ([`04-multi-step-forms.md`](./04-multi-step-forms.md)).
- **Clear specific fields** when the condition changes: `setValue("email", undefined)` or `unregister("email")`.
- **Strip them at submit** by parsing with the schema (below), which removes values not valid for the chosen branch.

### Conditional validation: discriminated unions

Express "required only when…" in the schema, not with `if`s scattered around:

```tsx
import { z } from "zod";

const schema = z.discriminatedUnion("contactMethod", [
  z.object({
    contactMethod: z.literal("email"),
    email: z.string().email("Enter a valid email"),
  }),
  z.object({
    contactMethod: z.literal("phone"),
    phone: z.string().regex(/^\+?[0-9 ()-]{7,}$/, "Enter a valid phone number"),
  }),
]);

type FormValues = z.infer<typeof schema>;
```

Each branch only requires its own fields, and the resolved type narrows by `contactMethod` ([`../04-typescript-with-react/05-advanced-typing-patterns.md`](../04-typescript-with-react/05-advanced-typing-patterns.md)). Accessing `errors.email` in the form requires narrowing in TypeScript; that friction is a good reason to keep each branch in its own small component.

---

## 2. Dependent fields

One field's options depend on another (country → state/province; category → subcategory). Watch the parent, derive the child's options, and **reset the child** when the parent changes so an invalid selection can't linger:

```tsx
const country = useWatch({ control, name: "country" });
const states = statesByCountry[country] ?? [];

useEffect(() => {
  setValue("state", "");          // clear when the country changes
}, [country, setValue]);

<select {...register("state")}>
  <option value="">Select…</option>
  {states.map((s) => <option key={s} value={s}>{s}</option>)}
</select>
```

Resetting in an effect is acceptable here because the form library holds state outside React's render flow; if you control the state yourself, prefer resetting inside the `onChange` handler ([`../03-hooks/03-you-might-not-need-an-effect.md`](../03-hooks/03-you-might-not-need-an-effect.md)). If the options come from the server, fetch them with a data library keyed on the parent value ([`../12-server-state/03-tanstack-query.md`](../12-server-state/03-tanstack-query.md)).

---

## 3. Repeatable groups: `useFieldArray`

For "add another" lists — phone numbers, invoice lines, team members — use `useFieldArray`:

```tsx
import { useForm, useFieldArray } from "react-hook-form";

type InvoiceValues = {
  customer: string;
  items: { description: string; quantity: number; price: number }[];
};

function InvoiceForm() {
  const { register, control, handleSubmit, formState: { errors } } = useForm<InvoiceValues>({
    defaultValues: {
      customer: "",
      items: [{ description: "", quantity: 1, price: 0 }],
    },
  });

  const { fields, append, remove, move } = useFieldArray({ control, name: "items" });

  return (
    <form onSubmit={handleSubmit(onSubmit)}>
      <input {...register("customer")} placeholder="Customer" />

      {fields.map((field, index) => (
        <div key={field.id}>
          <input {...register(`items.${index}.description` as const)} placeholder="Description" />
          <input
            type="number"
            {...register(`items.${index}.quantity` as const, { valueAsNumber: true })}
          />
          <input
            type="number"
            step="0.01"
            {...register(`items.${index}.price` as const, { valueAsNumber: true })}
          />
          {errors.items?.[index]?.description && (
            <p role="alert">{errors.items[index]?.description?.message}</p>
          )}
          <button type="button" onClick={() => remove(index)}>Remove</button>
          <button type="button" onClick={() => index > 0 && move(index, index - 1)}>↑</button>
        </div>
      ))}

      <button
        type="button"
        onClick={() => append({ description: "", quantity: 1, price: 0 })}
      >
        Add item
      </button>
      <button>Save invoice</button>
    </form>
  );
}
```

Key rules:

- **Use `field.id` as the React `key`**, not the index. RHF generates a stable id per row; index keys make inputs swap values on removal or reorder ([`../01-fundamentals/05-lists-and-keys.md`](../01-fundamentals/05-lists-and-keys.md)).
- **Register with the path** `` `items.${index}.description` ``, so RHF builds a nested array in the form values.
- **Give new rows complete default objects** to `append`. Appending `{}` yields undefined fields.
- Use `valueAsNumber: true` for numeric inputs (or `z.coerce.number()` in the schema).
- Buttons that aren't submit buttons need `type="button"`, or they submit the form.
- Operations: `append`, `prepend`, `insert`, `remove`, `swap`, `move`, `update`, `replace`.

### Validating arrays

```tsx
const schema = z.object({
  customer: z.string().min(1, "Customer is required"),
  items: z
    .array(
      z.object({
        description: z.string().min(1, "Description is required"),
        quantity: z.number().min(1, "At least 1"),
        price: z.number().min(0),
      })
    )
    .min(1, "Add at least one item"),
});
```

Array-level errors (like "Add at least one item") appear on `errors.items?.root` (or `errors.items?.message`, depending on the version). Check the RHF docs for the exact location in the version you use.

### Computed values: totals

Subscribe with `useWatch` in a **small child component** so only the total re-renders when a row changes:

```tsx
function Total({ control }: { control: Control<InvoiceValues> }) {
  const items = useWatch({ control, name: "items" });
  const total = items.reduce((sum, i) => sum + (i.quantity || 0) * (i.price || 0), 0);
  return <p>Total: {total.toFixed(2)}</p>;
}
```

Compute derived values; don't store them in state ([`../02-state-and-rendering/02-state-structure-and-lifting.md`](../02-state-and-rendering/02-state-structure-and-lifting.md)). Money math: keep amounts in integer minor units (cents) in real code to avoid floating-point errors.

### Nested arrays

An array inside an array (sections with questions) needs a `useFieldArray` per nested level, each in its own child component receiving its parent's `index`. Keep nesting shallow — deep nesting gets painful ([`../02-state-and-rendering/02-state-structure-and-lifting.md`](../02-state-and-rendering/02-state-structure-and-lifting.md)).

---

## 4. Schema-driven forms

When the form's structure comes from **data** (a CMS, a survey builder, an admin-configured form), describe the fields as configuration and render them generically:

```tsx
type FieldConfig =
  | { name: string; label: string; type: "text" | "email" | "number"; required?: boolean }
  | { name: string; label: string; type: "select"; options: { value: string; label: string }[]; required?: boolean }
  | { name: string; label: string; type: "checkbox" };

const fields: FieldConfig[] = [
  { name: "fullName", label: "Full name", type: "text", required: true },
  { name: "role", label: "Role", type: "select", options: [{ value: "dev", label: "Developer" }, { value: "pm", label: "Manager" }] },
  { name: "newsletter", label: "Subscribe", type: "checkbox" },
];

function DynamicForm({ fields }: { fields: FieldConfig[] }) {
  const { register, handleSubmit } = useForm();

  return (
    <form onSubmit={handleSubmit(console.log)}>
      {fields.map((f) => {
        switch (f.type) {
          case "select":
            return (
              <label key={f.name}>
                {f.label}
                <select {...register(f.name, { required: f.required })}>
                  {f.options.map((o) => <option key={o.value} value={o.value}>{o.label}</option>)}
                </select>
              </label>
            );
          case "checkbox":
            return (
              <label key={f.name}>
                <input type="checkbox" {...register(f.name)} /> {f.label}
              </label>
            );
          default:
            return (
              <label key={f.name}>
                {f.label}
                <input type={f.type} {...register(f.name, { required: f.required })} />
              </label>
            );
        }
      })}
      <button>Submit</button>
    </form>
  );
}
```

Notes:

- The config is a **discriminated union**, so the `switch` is type-safe and exhaustive.
- Build the **validation schema from the config** too (map each field to a Zod type), so rules and UI come from one source.
- Form values lose their static types (`Record<string, unknown>`); validate at the boundary rather than trusting them ([`../04-typescript-with-react/05-advanced-typing-patterns.md`](../04-typescript-with-react/05-advanced-typing-patterns.md)).
- **Never render unvetted HTML from config**, and treat server-supplied config as untrusted input for validation purposes.
- Consider whether you need this. Schema-driven forms are powerful for form builders; for an app with a handful of known forms, plain components are easier to read, test, and customize.

---

## 5. Accessibility for dynamic changes

- When fields appear or disappear, **screen reader users must be told**. Put newly revealed fields right after the controlling input in DOM order, and consider an `aria-live` region for significant changes.
- After "Add item", move **focus** to the new row's first input; after "Remove", move focus to a sensible neighbor (or the Add button), not to `<body>` ([`../08-accessibility/02-keyboard-and-focus-management.md`](../08-accessibility/02-keyboard-and-focus-management.md)).
- Give remove buttons an accessible name that includes the row: `aria-label={`Remove item ${index + 1}`}`.

---

## Common mistakes

- **Using the array index as the key** in `useFieldArray` — use `field.id`.
- **Forgetting that hidden fields keep their values** — they get submitted; unregister, clear, or let the schema strip them.
- **Appending `{}` or partial objects** — undefined fields and uncontrolled warnings.
- **Calling `watch` at the top of a large form** — use `useWatch` in a child for isolated re-renders.
- **Missing `type="button"`** on add/remove controls — each click submits the form.
- **Forgetting `valueAsNumber` (or coercion)** — numbers arrive as strings.
- **Conditional "required" logic scattered in components** — express it in the schema with a discriminated union.
- **Not resetting dependent fields** when their parent changes — invalid combinations get submitted.
- **Losing focus on remove/add** — keyboard users end up at the top of the page.
- **Over-engineering schema-driven forms** where a few plain components would do.

## Quick summary

- Conditional fields: `useWatch` the controller, render conditionally, and decide what happens to hidden values
- Put conditional rules in the schema using `z.discriminatedUnion`
- Dependent fields: derive options from the parent and reset the child when the parent changes
- Repeatable groups: `useFieldArray`, `field.id` as key, indexed `register` paths, complete defaults for new rows
- Isolate computed totals with `useWatch` in a child component
- Schema-driven forms: render from a typed config, generate validation from it, treat the config as data to validate
- Manage focus and announcements when fields appear or disappear

## Next

**[`04-multi-step-forms.md`](./04-multi-step-forms.md)** splits a long form into steps, validating each one before the user moves on.
