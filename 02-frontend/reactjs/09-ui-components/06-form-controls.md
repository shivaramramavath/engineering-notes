# Form Controls

shadcn gives you styled versions of the usual form pieces: `Input`, `Textarea`, `Label`, `Checkbox`, `RadioGroup`, `Switch`, `Select`, `Slider`. Styling them is easy. The part that trips people up is **wiring them to a form library**, because some of them are not native inputs.

```bash
npx shadcn@latest add input textarea label checkbox radio-group switch select
```

## Two kinds of controls

| Type | Controls | Wiring |
|---|---|---|
| **Native-backed** | `Input`, `Textarea` | `register("name")` works (they forward a ref to a real `<input>`) |
| **Custom (Radix)** | `Checkbox`, `RadioGroup`, `Switch`, `Select`, `Slider` | Use `Controller`; `register` doesn't work |

Custom controls render a `<button role="checkbox">`, not an `<input type="checkbox">`. There's no native `onChange` event with `e.target.checked`; they expose their own callbacks:

| Control | Value prop | Change callback | Value type |
|---|---|---|---|
| Checkbox | `checked` | `onCheckedChange` | `boolean \| "indeterminate"` |
| Switch | `checked` | `onCheckedChange` | `boolean` |
| RadioGroup | `value` | `onValueChange` | `string` |
| Select | `value` | `onValueChange` | `string` |
| Slider | `value` | `onValueChange` | `number[]` (always an array) |

Knowing this table solves most "my checkbox isn't updating the form" bugs.

## Accessible label and error pattern

Every control needs a programmatic label; every error needs to be tied to its field.

```tsx
<div className="space-y-2">
  <Label htmlFor="email">Email</Label>
  <Input
    id="email"
    type="email"
    aria-invalid={!!errors.email}
    aria-describedby={errors.email ? "email-error" : undefined}
    {...register("email")}
  />
  {errors.email && (
    <p id="email-error" role="alert" className="text-sm text-destructive">
      {errors.email.message}
    </p>
  )}
</div>
```

- `htmlFor` + `id` link label to input. Clicking the label focuses the input and screen readers announce it.
- `aria-invalid` and `aria-describedby` connect the error message to the field. shadcn's input styles respond to `aria-invalid` (red ring), so you don't need extra classes.
- A placeholder is **not** a label — it disappears on input.

## Full example with React Hook Form + Zod

```tsx
import { useForm, Controller } from "react-hook-form"
import { zodResolver } from "@hookform/resolvers/zod"
import { z } from "zod"

const schema = z.object({
  name: z.string().min(2, "At least 2 characters"),
  role: z.enum(["admin", "editor", "viewer"], { message: "Choose a role" }),
  notifications: z.boolean(),
  terms: z.literal(true, { message: "You must accept the terms" }),
})
type FormValues = z.infer<typeof schema>

export function InviteForm() {
  const {
    register, control, handleSubmit, formState: { errors, isSubmitting },
  } = useForm<FormValues>({
    resolver: zodResolver(schema),
    defaultValues: { name: "", role: undefined, notifications: true, terms: false as unknown as true },
  })

  return (
    <form onSubmit={handleSubmit(async (v) => { await invite(v) })} className="space-y-6" noValidate>
      {/* Input: register works */}
      <div className="space-y-2">
        <Label htmlFor="name">Name</Label>
        <Input id="name" aria-invalid={!!errors.name} {...register("name")} />
        {errors.name && <p className="text-sm text-destructive">{errors.name.message}</p>}
      </div>

      {/* Select: Controller */}
      <div className="space-y-2">
        <Label htmlFor="role">Role</Label>
        <Controller
          name="role"
          control={control}
          render={({ field }) => (
            <Select value={field.value} onValueChange={field.onChange}>
              <SelectTrigger id="role" aria-invalid={!!errors.role} onBlur={field.onBlur}>
                <SelectValue placeholder="Choose a role" />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="admin">Admin</SelectItem>
                <SelectItem value="editor">Editor</SelectItem>
                <SelectItem value="viewer">Viewer</SelectItem>
              </SelectContent>
            </Select>
          )}
        />
        {errors.role && <p className="text-sm text-destructive">{errors.role.message}</p>}
      </div>

      {/* Switch: Controller */}
      <div className="flex items-center gap-2">
        <Controller
          name="notifications"
          control={control}
          render={({ field }) => (
            <Switch id="notifications" checked={field.value} onCheckedChange={field.onChange} />
          )}
        />
        <Label htmlFor="notifications">Email notifications</Label>
      </div>

      {/* Checkbox: Controller */}
      <div className="flex items-center gap-2">
        <Controller
          name="terms"
          control={control}
          render={({ field }) => (
            <Checkbox
              id="terms"
              checked={field.value}
              onCheckedChange={(checked) => field.onChange(checked === true)}
            />
          )}
        />
        <Label htmlFor="terms">I accept the terms</Label>
      </div>
      {errors.terms && <p className="text-sm text-destructive">{errors.terms.message}</p>}

      <Button type="submit" disabled={isSubmitting}>Send invite</Button>
    </form>
  )
}
```

Points worth noting:

- `field.onChange` can be passed straight to `onValueChange` and `onCheckedChange` because RHF's `onChange` accepts a raw value as well as an event.
- The `Checkbox` callback is narrowed with `checked === true` because it may report `"indeterminate"`, which isn't a boolean.
- Zod's error-message options (`{ message: ... }`) have changed between Zod major versions; if yours differs, check the Zod docs for how to set custom messages on `enum` and `literal`.
- Typing `defaultValues` for a `z.literal(true)` field is awkward. Many teams use `z.boolean().refine((v) => v, "...")` instead, which keeps the type a plain `boolean`.
- `noValidate` on the form lets your schema, not the browser's tooltips, own validation messages.

Full React Hook Form coverage lives in [06-forms/02-react-hook-form](../06-forms/02-react-hook-form.md) and [form validation](../06-forms/01-form-validation.md).

## The shadcn `Form` / `Field` components

shadcn also ships helper components that wrap RHF's `Controller` and automatically wire ids, labels, `aria-describedby`, and error messages. The helpers and their recommended names have changed across releases, so check [the current docs](https://ui.shadcn.com/docs/components/form) for your version before copying. Underneath they do exactly what the manual example above does; understanding the manual version means you can use any of them.

## Control-specific notes

**Input**
- `type` matters: `email`, `tel`, `number`, `password` select the right mobile keyboard and autofill behavior. Add `autoComplete` (`email`, `current-password`, `one-time-code`).
- `type="number"` returns strings from `register` unless you use `valueAsNumber: true`: `register("age", { valueAsNumber: true })`.
- File inputs are uncontrolled — see [file upload](../06-forms/06-file-upload.md).

**Select**
- Placeholder only shows when `value` is `undefined` or `""`-ish; pass `undefined` rather than a made-up string for "no selection yet".
- Radix Select renders a hidden native select for native form submission, but when using RHF you read the value from state, not from `FormData`.

**RadioGroup**
- Values are strings; the group is the controlled unit, not each radio.
- Always include a group label (`aria-labelledby` or a `<fieldset>/<legend>`).

**Slider**
- Value is an **array** (`[50]`, or `[20, 80]` for a range). Adapt: `value={[field.value]} onValueChange={([v]) => field.onChange(v)}`.

**Switch vs Checkbox**
- Switch = takes effect **immediately** (a setting). Checkbox = a choice **submitted later** with the form.

## Common mistakes

- **`register()` on Radix controls.** Nothing updates. Use `Controller`.
- **Missing `id`/`htmlFor`**, so labels aren't associated.
- **Treating `onCheckedChange`'s value as a boolean** and storing `"indeterminate"` in form state.
- **Slider array confusion** — storing `[50]` where the schema expects a number.
- **Number inputs yielding strings** and failing validation.
- **Placeholders as labels.**
- **Errors shown only by color.** Include text, and link it via `aria-describedby`.
- **Uncontrolled → controlled warnings** from giving `Select`/`Checkbox` an `undefined` value then a defined one. Set sensible `defaultValues`.

## Quick summary

- `Input`/`Textarea` work with `register`; Radix-based controls need `Controller`.
- Each control has its own value/callback names (`checked`/`onCheckedChange`, `value`/`onValueChange`).
- Label with `htmlFor`/`id`; link errors with `aria-invalid` + `aria-describedby`.
- Checkbox can be `"indeterminate"`; Slider is always an array; number inputs need `valueAsNumber`.
- shadcn's form helpers are conveniences over the same `Controller` pattern.

## Next

[07 — Data tables](./07-data-tables.md)
