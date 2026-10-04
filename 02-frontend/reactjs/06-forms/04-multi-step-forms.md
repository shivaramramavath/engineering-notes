# Multi-Step Forms

Long forms are easier to complete when split into steps: details → address → payment → review. The hard part isn't the UI; it's **keeping one body of data across steps, validating each step before moving on, and not losing anything when the user goes back**. This file shows a clean architecture using one form instance, per-step schemas, and `trigger`.

## Prerequisites

[`02-react-hook-form.md`](./02-react-hook-form.md) (especially `FormProvider`) and [`03-dynamic-forms.md`](./03-dynamic-forms.md) (what happens to unmounted fields)

---

## The design decision: one form, many steps

There are two broad approaches:

| Approach | How | Trade-off |
|----------|-----|-----------|
| **One form instance for the whole flow** | A single `useForm` with the combined schema; steps are views over it | Data is always in one place; back/forward is trivial; cross-step rules are easy |
| **A separate form per step** | Each step has its own `useForm`; results merge into a shared state | Independent steps and schemas; you manage the merge yourself |

Prefer **one form instance** unless steps are truly independent (different pages, different owners). The rest of this file uses that approach.

---

## 1. Schemas: one per step, combined for the whole

Define each step's schema, then merge them:

```tsx
import { z } from "zod";

const detailsSchema = z.object({
  firstName: z.string().min(1, "First name is required"),
  lastName: z.string().min(1, "Last name is required"),
  email: z.string().email("Enter a valid email"),
});

const addressSchema = z.object({
  street: z.string().min(1, "Street is required"),
  city: z.string().min(1, "City is required"),
  postalCode: z.string().min(3, "Enter a postal code"),
});

const checkoutSchema = detailsSchema.merge(addressSchema);
type CheckoutValues = z.infer<typeof checkoutSchema>;
```

The combined schema validates the final submit; the per-step schemas (or just their field names) drive step validation. (`.merge` and `.extend` are the classic Zod ways to combine object schemas; confirm names for your Zod version.)

---

## 2. Step configuration

Describe each step's UI and which fields it owns:

```tsx
const steps = [
  { id: "details", title: "Your details", fields: ["firstName", "lastName", "email"] },
  { id: "address", title: "Address", fields: ["street", "city", "postalCode"] },
  { id: "review", title: "Review", fields: [] },
] as const satisfies readonly { id: string; title: string; fields: readonly (keyof CheckoutValues)[] }[];
```

`satisfies` verifies every listed field is a real key ([`../04-typescript-with-react/04-utility-types.md`](../04-typescript-with-react/04-utility-types.md)).

---

## 3. The wizard component

One `useForm` with `FormProvider`. The step components read the shared form with `useFormContext`:

```tsx
import { useState } from "react";
import { useForm, FormProvider, useFormContext } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";

function CheckoutWizard() {
  const [stepIndex, setStepIndex] = useState(0);

  const methods = useForm<CheckoutValues>({
    resolver: zodResolver(checkoutSchema),
    mode: "onTouched",
    defaultValues: {
      firstName: "", lastName: "", email: "",
      street: "", city: "", postalCode: "",
    },
  });

  const step = steps[stepIndex];
  const isLast = stepIndex === steps.length - 1;

  async function next() {
    // Validate only this step's fields
    const valid = await methods.trigger([...step.fields]);
    if (valid) setStepIndex((i) => i + 1);
  }

  function back() {
    setStepIndex((i) => Math.max(0, i - 1));
  }

  async function onSubmit(values: CheckoutValues) {
    await submitOrder(values);
  }

  return (
    <FormProvider {...methods}>
      <form onSubmit={methods.handleSubmit(onSubmit)} noValidate>
        <ol aria-label="Progress">
          {steps.map((s, i) => (
            <li key={s.id} aria-current={i === stepIndex ? "step" : undefined}>
              {s.title}
            </li>
          ))}
        </ol>

        {step.id === "details" && <DetailsStep />}
        {step.id === "address" && <AddressStep />}
        {step.id === "review" && <ReviewStep />}

        <div>
          {stepIndex > 0 && <button type="button" onClick={back}>Back</button>}
          {isLast ? (
            <button type="submit" disabled={methods.formState.isSubmitting}>Place order</button>
          ) : (
            <button type="button" onClick={next}>Next</button>
          )}
        </div>
      </form>
    </FormProvider>
  );
}
```

```tsx
function DetailsStep() {
  const { register, formState: { errors } } = useFormContext<CheckoutValues>();
  return (
    <fieldset>
      <legend>Your details</legend>
      <input {...register("firstName")} placeholder="First name" />
      {errors.firstName && <p role="alert">{errors.firstName.message}</p>}
      {/* lastName, email */}
    </fieldset>
  );
}
```

How it works:

- **`trigger(fieldNames)`** runs validation for just those fields and resolves to `true` if they pass. The user can only advance when the **current** step is valid.
- **"Next" is `type="button"`**, not a submit button. Only the last step submits. (Pressing Enter inside a step would otherwise submit the whole form prematurely; handle Enter by calling `next` in a step-level `onKeyDown` or by making each step's form handle `onSubmit` → `next` on non-final steps.)
- Because there's **one form instance**, values entered on earlier steps live in the form state even while those step components are unmounted — **provided `shouldUnregister` is `false`** (the default). Don't set `shouldUnregister: true` for wizards, or going to the next step discards the previous step's data ([`03-dynamic-forms.md`](./03-dynamic-forms.md)).
- The **final** submit runs the **combined** schema, so cross-step rules and any step the user skipped (via direct URL, for instance) are still enforced.

### Review step

```tsx
function ReviewStep() {
  const { getValues } = useFormContext<CheckoutValues>();
  const values = getValues();
  return (
    <dl>
      <dt>Name</dt><dd>{values.firstName} {values.lastName}</dd>
      <dt>Email</dt><dd>{values.email}</dd>
      <dt>Address</dt><dd>{values.street}, {values.city} {values.postalCode}</dd>
    </dl>
  );
}
```

Add "Edit" links that set the step index back to the relevant step.

---

## 4. Keeping the step in the URL

State-only steps are lost on refresh and can't be linked to. Store the step in the **URL** (a path segment or search param) so refresh, browser Back/Forward, and deep links work:

```tsx
// /checkout?step=address   (React Router)
const [params, setParams] = useSearchParams();
const stepId = params.get("step") ?? steps[0].id;
const stepIndex = Math.max(0, steps.findIndex((s) => s.id === stepId));

function goTo(index: number) {
  setParams({ step: steps[index].id });   // pushes a history entry
}
```

Guard against skipping ahead: if someone loads `?step=review` with an empty form, validate prior steps and redirect to the first invalid one. More on URL state: [`../10-routing/06-search-filter-and-url-state.md`](../10-routing/06-search-filter-and-url-state.md).

---

## 5. Persisting progress

For long forms, don't lose the user's work on an accidental refresh or tab close:

- **Session storage / local storage:** save `getValues()` on change (debounced) and restore it as `defaultValues`. Don't store sensitive data (card numbers, passwords) there ([`../19-production/05-security.md`](../19-production/05-security.md)).
- **Server-side draft:** save each step as a draft via an API call so users can resume on another device. Often the right choice for lengthy applications.
- Clear the saved data after a successful submit.

```tsx
const saved = sessionStorage.getItem("checkout-draft");
const methods = useForm<CheckoutValues>({
  defaultValues: saved ? JSON.parse(saved) : emptyValues,
});

useEffect(() => {
  const sub = methods.watch((values) =>
    sessionStorage.setItem("checkout-draft", JSON.stringify(values))
  );
  return () => sub.unsubscribe();
}, [methods]);
```

Validate parsed data before trusting it; stored JSON can be stale or edited ([`../04-typescript-with-react/05-advanced-typing-patterns.md`](../04-typescript-with-react/05-advanced-typing-patterns.md)).

---

## 6. Variable steps

If later steps depend on earlier answers (show "Company details" only for business accounts), derive the **list of active steps** from the current values and keep a step **id** (not an index) in state:

```tsx
const accountType = useWatch({ control: methods.control, name: "accountType" });
const activeSteps = steps.filter((s) => s.when?.(accountType) ?? true);
```

Use a discriminated-union schema so the combined validation matches the active steps ([`03-dynamic-forms.md`](./03-dynamic-forms.md)).

---

## 7. Accessibility and UX

- **Move focus to the new step's heading** (or the first field) when the step changes; otherwise keyboard and screen reader users stay on the "Next" button that just disappeared. Use a ref on the heading with `tabIndex={-1}` and call `focus()` in an effect keyed on the step ([`../08-accessibility/02-keyboard-and-focus-management.md`](../08-accessibility/02-keyboard-and-focus-management.md)).
- **Announce the step** ("Step 2 of 3: Address") with a visible title and `aria-current="step"` on the progress indicator.
- **On failed "Next", focus the first invalid field** (RHF's `shouldFocusError` does this on submit; with `trigger`, call `setFocus` on the first error).
- Provide a **Back** button that never loses data.
- Show a **summary/review** step before the irreversible action.
- Warn before leaving with unsaved progress ([`05-form-submission-and-errors.md`](./05-form-submission-and-errors.md)).

---

## 8. Testing the flow

Test the user journey rather than internals: fill step one, click Next, assert step two shows, go Back, assert values remain, submit, assert the payload ([`../18-testing-and-debugging/02-component-testing-with-rtl.md`](../18-testing-and-debugging/02-component-testing-with-rtl.md)).

---

## Common mistakes

- **Separate `useForm` per step with manual merging** when one shared form would be simpler.
- **Setting `shouldUnregister: true`** — earlier steps' data vanishes when their fields unmount.
- **"Next" as a submit button** — submits the whole form (or the combined schema fails) on step one.
- **Validating the whole schema on "Next"** — shows errors for steps the user hasn't reached; use `trigger` with the step's fields.
- **Only validating per step, never the combined schema on submit** — steps can be skipped via URL or stale state.
- **Using a step index in the URL or state when steps can change** — use stable ids.
- **Not moving focus on step change** — a significant accessibility failure.
- **Storing sensitive data in local/session storage** drafts.
- **Forgetting a Back path that preserves values** — users abandon forms that lose their data.

## Quick summary

- Prefer one `useForm` for the whole flow, shared with `FormProvider`; steps are views over it
- Define per-step schemas, merge them for the final submit
- `trigger(step.fields)` validates just the current step before advancing; only the last step submits
- Keep `shouldUnregister` off so data survives unmounting
- Put the step in the URL and guard against skipping ahead
- Persist progress carefully (not sensitive data), clear it after success
- Manage focus and announcements on every step change

## Next

**[`05-form-submission-and-errors.md`](./05-form-submission-and-errors.md)** covers what happens after the user presses submit: pending state, server errors, and accessible feedback.
