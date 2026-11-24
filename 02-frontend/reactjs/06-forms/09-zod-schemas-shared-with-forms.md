# Sharing Zod Schemas Between Client Forms and Server Actions

- **Topic**: `06-forms`
- **React version**: React 19+ (Zod 3.x / Zod 4)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0, Zod 3.24
- **Bundle / compiler notes**: Single source of truth for runtime validation and static TypeScript types across full-stack boundaries.

---

## 1. Concept Overview

In full-stack React applications, a classic source of bugs and code duplication is the divergence between **client-side form validation** and **server-side API validation**:
- The client-side form checks that passwords are 8+ characters.
- The server endpoint checks that passwords are 10+ characters.
- A change to database validation requires manually updating TypeScript interfaces, form resolvers, and API handlers.

**Zod** is a TypeScript-first schema declaration and validation library. By sharing Zod schemas across client and server boundaries, you establish a **Single Source of Truth**:
1. **Runtime Validation**: Validates client-side form inputs and server-side HTTP/Server Action payloads using the exact same rules.
2. **Static Typing**: TypeScript types are inferred directly from the schema via `z.infer<typeof Schema>`, eliminating duplicate interface declarations.
3. **Coercion & Transformations**: Bridges the gap between raw browser strings (`FormData` always yields strings) and typed primitives (numbers, booleans, dates) using `z.coerce`.

```
                        Shared Zod Schema
                  (e.g., schemas/auth.schema.ts)
                                │
               ┌────────────────┴────────────────┐
               ▼                                 ▼
       Client-Side Forms                Server Actions / APIs
  • Instant UI error feedback       • Reject malicious payloads
  • TanStack Form / React Hook Form • Database persistence
  • Types derived: z.infer<Schema>  • Same error shape: flatten()
```

### When to Use
- All forms submitting to backend APIs or React 19 Server Actions.
- Validating browser `FormData` where numeric and boolean fields arrive as raw strings.
- Complex cross-field validation: Password confirmations, date ranges, conditional billing fields.

### When NOT to Use
- Purely visual UI state (e.g. tracking whether an accordion is open or which tab is highlighted).
- Simple string length checks where HTML5 native constraints (`minlength="3"`, `required`) suffice.

---

## 2. Key Mechanics & Architecture

### The Zod Coercion & Parse Pipeline
HTML forms submit data via `FormData`, where every value is either a `string` or a `File`. If an input is `<input type="number" name="age" />`, `formData.get('age')` returns `"25"` (a string).

Using `z.coerce.number()` or `z.coerce.boolean()` ensures that Zod safely casts the string into the expected primitive during schema parsing:
```ts
export const UserSchema = z.object({
  name: z.string().min(2, 'Name must be at least 2 characters'),
  age: z.coerce.number().min(18, 'Must be at least 18 years old'),
  newsletter: z.coerce.boolean(),
});
```

### Error Flattening with `flatten()`
Zod provides `.flatten()` to transform nested Zod error issues into a clean, flat map of field errors that map directly to input `name` attributes:
```ts
const result = UserSchema.safeParse(data);
if (!result.success) {
  const { fieldErrors } = result.error.flatten();
  // fieldErrors: { name: ['Name too short'], age: ['Must be 18'] }
}
```

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Type Derivation** | `type User = z.infer<typeof UserSchema>` |
| **Parsing Methods** | `safeParse()` (non-throwing) vs `parse()` (throws `ZodError`) |
| **Type Coercion** | Supported via `z.coerce.number()`, `z.coerce.date()`, `z.coerce.boolean()` |
| **Cross-Field Checks** | Supported via `.refine()` and `.superRefine()` |
| **Error Mapping** | `result.error.flatten().fieldErrors` directly populates form UI |
| **Bundle Impact** | ~12KB gzipped; shared between client and server packages in monorepos |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
Defining a shared schema and deriving its static TypeScript interface.

```tsx
import { z } from 'zod';

export const LoginSchema = z.object({
  email: z.string().email('Please enter a valid email address'),
  password: z.string().min(8, 'Password must be at least 8 characters'),
  rememberMe: z.coerce.boolean().optional(),
});

// Single Source of Truth for TypeScript:
export type LoginInput = z.infer<typeof LoginSchema>;

// Example usage in pure function:
const input = { email: 'alex@example.com', password: 'supersecretpassword', rememberMe: 'true' };
const validation = LoginSchema.safeParse(input);

if (validation.success) {
  console.log('Parsed data:', validation.data);
  // validation.data.rememberMe is boolean `true`!
}
```
Output:
```
Parsed data: { email: 'alex@example.com', password: 'supersecretpassword', rememberMe: true }
```

### Example 2: Production / Real-World Integration
Sharing a Zod schema between a React 19 Client Form and a Server Action.

```tsx
// 1. Shared Schema File (e.g. shared/schemas/booking.ts)
import { z } from 'zod';

export const BookingSchema = z
  .object({
    guestName: z.string().min(3, 'Guest name is required'),
    rooms: z.coerce.number().min(1).max(5),
    checkIn: z.coerce.date(),
    checkOut: z.coerce.date(),
  })
  .refine((data) => data.checkOut > data.checkIn, {
    message: 'Check-out date must be after check-in date',
    path: ['checkOut'], // Attach error to checkOut field!
  });

export type BookingInput = z.infer<typeof BookingSchema>;

// 2. React 19 Client Component consuming the schema
import React, { useActionState } from 'react';

interface BookingState {
  errors?: Record<string, string[] | undefined>;
  success?: boolean;
}

async function bookReservationAction(_prev: BookingState, formData: FormData): Promise<BookingState> {
  // Convert FormData to plain object
  const rawData = Object.fromEntries(formData.entries());

  // Validate using shared Zod schema!
  const result = BookingSchema.safeParse(rawData);

  if (!result.success) {
    return { errors: result.error.flatten().fieldErrors };
  }

  // At this point, result.data is 100% type-safe BookingInput!
  await new Promise((r) => setTimeout(r, 600)); // Call backend API

  return { success: true };
}

export function HotelBookingForm() {
  const [state, formAction, isPending] = useActionState(bookReservationAction, {});

  if (state.success) {
    return <div className="p-4 bg-emerald-100 text-emerald-800 rounded font-bold">Reservation confirmed!</div>;
  }

  return (
    <form action={formAction} className="p-6 border rounded-xl max-w-sm space-y-3">
      <h3 className="font-bold text-sm">Book Hotel Stay</h3>

      <div>
        <label className="text-xs font-semibold block mb-1">Guest Name</label>
        <input name="guestName" className="border p-2 text-xs w-full rounded" />
        {state.errors?.guestName && <span className="text-xs text-red-500">{state.errors.guestName[0]}</span>}
      </div>

      <div>
        <label className="text-xs font-semibold block mb-1">Number of Rooms (1-5)</label>
        <input name="rooms" type="number" defaultValue="1" className="border p-1.5 text-xs w-20 rounded" />
        {state.errors?.rooms && <span className="text-xs text-red-500">{state.errors.rooms[0]}</span>}
      </div>

      <div className="grid grid-cols-2 gap-2">
        <div>
          <label className="text-xs font-semibold block mb-1">Check In</label>
          <input name="checkIn" type="date" className="border p-1.5 text-xs w-full rounded" />
        </div>
        <div>
          <label className="text-xs font-semibold block mb-1">Check Out</label>
          <input name="checkOut" type="date" className="border p-1.5 text-xs w-full rounded" />
        </div>
      </div>
      {state.errors?.checkOut && <span className="text-xs text-red-500 block">{state.errors.checkOut[0]}</span>}

      <button
        type="submit"
        disabled={isPending}
        className="w-full bg-blue-600 text-white py-2 rounded text-xs font-bold disabled:opacity-50"
      >
        {isPending ? 'Validating & Booking...' : 'Reserve Room'}
      </button>
    </form>
  );
}
```
Output:
```
// Client and server share exact date refinement and validation logic;
// checkOut error appears directly under date picker.
```

### Example 3: Edge Case / Complex Password Confirmation with `.refine()`
Cross-field equality check with path-targeted error messages.

```tsx
import { z } from 'zod';

export const PasswordResetSchema = z
  .object({
    password: z.string().min(8, 'Password must be at least 8 characters'),
    confirmPassword: z.string(),
  })
  .refine((values) => values.password === values.confirmPassword, {
    message: 'Passwords do not match',
    path: ['confirmPassword'], // Targets confirmPassword field!
  });

// Testing the schema:
const invalidTest = PasswordResetSchema.safeParse({
  password: 'correcthorsebatterystaple',
  confirmPassword: 'differentpassword',
});

if (!invalidTest.success) {
  const errors = invalidTest.error.flatten().fieldErrors;
  console.log(errors);
  // Output: { confirmPassword: ['Passwords do not match'] }
}
```
Output:
```json
{
  "confirmPassword": ["Passwords do not match"]
}
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Using standard `z.number()` instead of `z.coerce.number()` on `FormData`.

```tsx
// ❌ WRONG: z.number() expects a JavaScript number primitive!
const BadSchema = z.object({
  quantity: z.number(),
});

// In React 19 form actions:
const formData = new FormData();
formData.set('quantity', '5'); // FormData always produces strings!

const result = BadSchema.safeParse(Object.fromEntries(formData));
// ⚠️ FAILS! result.error: Expected number, received string!

// ✅ REMEDIATION: Use z.coerce.number() to parse incoming form strings!
const GoodSchema = z.object({
  quantity: z.coerce.number().min(1),
});
const goodResult = GoodSchema.safeParse(Object.fromEntries(formData));
// ✅ SUCCESS: goodResult.data.quantity is numeric 5!
```
Output:
```
{ success: true, data: { quantity: 5 } }
```

### Example 5: Idiomatic Modern React 19 Pattern
Typed form action handler with reusable schema validator helper.

```tsx
import { z } from 'zod';

export function validateActionData<T extends z.ZodTypeAny>(
  schema: T,
  formData: FormData
): { success: true; data: z.infer<T> } | { success: false; errors: Record<string, string[] | undefined> } {
  const raw = Object.fromEntries(formData.entries());
  const parsed = schema.safeParse(raw);

  if (!parsed.success) {
    return { success: false, errors: parsed.error.flatten().fieldErrors };
  }

  return { success: true, data: parsed.data };
}
```
Output:
```
// Reusable helper returns 100% typed data or flattened field errors cleanly.
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Using `z.number()` without coercion on FormData** | `formData.get()` always returns a string; `z.number()` throws type mismatch. | Use `z.coerce.number()`. |
| **Duplicating TypeScript interfaces manually** | Maintaining separate `interface User` and `UserSchema` leads to desync. | Derive types via `z.infer<typeof UserSchema>`. |
| **Refining cross-field checks without specifying `path`** | Attaches error to the root form object rather than the specific input field. | Always add `path: ['fieldName']` in `.refine()`. |
| **Catching `ZodError` with `parse()` inside actions** | `parse()` throws exceptions, triggering Error Boundaries instead of form errors. | Use `safeParse()` and return `{ errors: error.flatten().fieldErrors }`. |
| **Importing client React code in shared schema files** | Pollutes backend/server action packages with DOM types. | Keep shared schemas in a pure TypeScript file with zero React imports. |
| **Using `z.coerce.boolean()` with checkbox unchecked state** | An unchecked HTML checkbox is omitted from `FormData` entirely. | Use `z.preprocess((val) => val === 'on' \|\| val === true, z.boolean())`. |

---

## 5. React Internals: Beneath the Abstraction

1. **Serialization Safety**: Zod schemas run synchronously during the render phase or within action transitions.
2. When parsing `FormData`, `Object.fromEntries(formData.entries())` creates a plain JavaScript dictionary.
3. Zod applies structural transformation (stripping unknown keys, casting primitives, running refinements).
4. The output is a frozen, validated data object safe for direct database insertion or state commits.

---

## 6. Performance, Bundling & React Compiler

- **Tree-Shaking**: Zod v3/v4 is modular; only the validators you instantiate (`z.string()`, `z.number()`) are bundled.
- **Fast Path Validation**: `safeParse()` executes in microseconds for typical 10-field schemas.
- **React Compiler**: The React Compiler recognizes derived Zod types and ensures validation helper outputs do not trigger redundant re-renders.

---

## 7. Accessibility (a11y) & Usability

- **Field-Targeted Errors**: By utilizing `path: ['fieldName']` in Zod refinements, errors can be mapped to individual DOM elements with matching `id` and `aria-describedby` attributes.
- **Screen Reader Announcements**: Render flattened Zod errors inside elements with `role="alert"` so assistive technology users hear validation mistakes immediately.

---

## 8. Testing & Verification Patterns

Unit testing Zod schemas with Vitest:

```tsx
import { describe, it, expect } from 'vitest';
import { BookingSchema } from './booking';

describe('BookingSchema validation', () => {
  it('accepts valid check-in and check-out dates', () => {
    const input = {
      guestName: 'John Doe',
      rooms: '2',
      checkIn: '2026-06-01',
      checkOut: '2026-06-05',
    };
    const result = BookingSchema.safeParse(input);
    expect(result.success).toBe(true);
  });

  it('rejects check-out date prior to check-in', () => {
    const input = {
      guestName: 'John Doe',
      rooms: '1',
      checkIn: '2026-06-10',
      checkOut: '2026-06-05',
    };
    const result = BookingSchema.safeParse(input);
    expect(result.success).toBe(false);

    if (!result.success) {
      const errors = result.error.flatten().fieldErrors;
      expect(errors.checkOut).toContain('Check-out date must be after check-in date');
    }
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 17 / 18** | Zod used primarily with React Hook Form resolvers (`@hookform/resolvers/zod`). | Validated synthetic event objects. |
| **React 19** | Zod used natively in Form Actions to validate `FormData` directly. | Use `z.coerce` for seamless FormData parsing. |

---

## 10. Security & Hardening

1. **SQL / NoSQL Injection Prevention**: Zod strips un-whitelisted properties by default (`.strip()`), preventing attackers from injecting extra fields (e.g. `role: 'ADMIN'`) into database updates.
2. **XSS Protection**: Validate that text inputs do not contain unescaped HTML entities or script tags.

---

## 11. Real-World Case Study

### FinTech User Onboarding KYC Pipeline

A digital banking platform needed to collect user tax IDs, physical addresses, and passport data across a 4-step wizard.
Previously, client and server code maintained separate validation rules, leading to a 4.2% failure rate where client forms accepted data that the backend banking API rejected.

By creating a shared npm package (`@acme/kyc-schemas`) containing shared Zod schemas:
- Client forms validated inputs with instant feedback.
- Server Actions validated payloads before transmission to KYC microservices.
- Rejection desync dropped from 4.2% to **0.00%**.

---

## 12. Interview Q&A

<details>
<summary>1. Why should you share Zod schemas between client forms and backend APIs?</summary>

Sharing Zod schemas establishes a Single Source of Truth for both runtime validation and static TypeScript types. It prevents validation divergence (where the client allows data the server rejects), eliminates duplicate interface definitions via `z.infer`, and ensures consistent error messages across the stack.
</details>

<details>
<summary>2. Why is `z.coerce` critical when validating React 19 `FormData`?</summary>

HTML forms submitted via `FormData` represent all inputs as strings (e.g. `<input type="number">` returns `"123"`). Standard `z.number()` fails because it expects a JavaScript numeric type. `z.coerce.number()` automatically casts the incoming string into a number during parsing.
</details>

<details>
<summary>3. What is the difference between `schema.parse()` and `schema.safeParse()`?</summary>

`schema.parse()` throws a `ZodError` exception if validation fails.
`schema.safeParse()` returns a discriminated union: either `{ success: true, data: T }` or `{ success: false, error: ZodError }`. In React form actions, `safeParse()` is strongly preferred because it avoids throwing exceptions that trigger Error Boundaries.
</details>

<details>
<summary>4. How does `error.flatten()` help in React form components?</summary>

`error.flatten()` transforms complex, deeply nested Zod error issues into a simple, flat object: `{ formErrors: string[], fieldErrors: Record<string, string[]> }`. The keys of `fieldErrors` match your input `name` attributes, allowing one-to-one mapping in JSX: `state.errors?.email?.[0]`.
</details>

<details>
<summary>5. How do you implement cross-field validation with Zod?</summary>

Using `.refine()` or `.superRefine()` on the parent schema object. For example, comparing `password` and `confirmPassword`. Always specify `path: ['targetField']` in the options so Zod assigns the error message to the intended input rather than the root form.
</details>

<details>
<summary>6. How do you handle unchecked checkboxes in `FormData` with Zod?</summary>

In standard HTML, unchecked checkboxes are omitted from `FormData` (evaluating to `undefined` or `null`). To handle this, use `z.preprocess()` to convert missing values to `false`:
```ts
z.preprocess((val) => val === 'on' || val === true, z.boolean())
```
</details>

<details>
<summary>7. Can Zod transform data during validation?</summary>

Yes. Methods like `.transform()`, `.trim()`, and `.toLowerCase()` allow you to sanitize and format data during parsing (e.g., trimming whitespace from usernames or converting email addresses to lowercase).
</details>

<details>
<summary>8. How do you integrate a shared Zod schema with React Hook Form or TanStack Form?</summary>

- For React Hook Form: use `@hookform/resolvers/zod` with `zodResolver(Schema)`.
- For TanStack Form: use `@tanstack/zod-form-adapter` with `zodValidator()`.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Basic Signup Schema with Inferred Type</summary>

**Specification**: Define a Zod schema for user registration and export its inferred TypeScript type.

```tsx
import { z } from 'zod';

export const SignupSchema = z.object({
  username: z.string().min(3, 'Username must be at least 3 characters'),
  email: z.string().email('Invalid email address'),
  password: z.string().min(8, 'Password must be at least 8 characters'),
});

export type SignupInput = z.infer<typeof SignupSchema>;
```
</details>

<details>
<summary>Exercise 2 (Easy): Coerced Product Inventory Schema</summary>

**Specification**: Schema that coerces `price` (number) and `inStock` (boolean) from form strings.

```tsx
import { z } from 'zod';

export const ProductSchema = z.object({
  title: z.string().min(1, 'Title is required'),
  price: z.coerce.number().positive('Price must be greater than 0'),
  inStock: z.preprocess((val) => val === 'on' || val === 'true', z.boolean()),
});
```
</details>

<details>
<summary>Exercise 3 (Medium): Address Zip Code & Country Regex Validator</summary>

**Specification**: Schema validating US ZIP codes (5 digits) or UK Postcodes with custom regex.

```tsx
import { z } from 'zod';

export const AddressSchema = z.object({
  country: z.enum(['US', 'UK']),
  postalCode: z.string(),
}).superRefine((data, ctx) => {
  if (data.country === 'US' && !/^\d{5}$/.test(data.postalCode)) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, message: 'US ZIP must be 5 digits', path: ['postalCode'] });
  }
  if (data.country === 'UK' && !/^[A-Z]{1,2}\d[A-Z\d]? ?\d[A-Z]{2}$/i.test(data.postalCode)) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, message: 'Invalid UK Postcode', path: ['postalCode'] });
  }
});
```
</details>

<details>
<summary>Exercise 4 (Medium): Password Strength Refinement</summary>

**Specification**: Enforce that passwords contain at least one uppercase letter, one digit, and one special character.

```tsx
import { z } from 'zod';

export const StrongPasswordSchema = z.string()
  .min(8, 'At least 8 characters')
  .regex(/[A-Z]/, 'Must contain at least one uppercase letter')
  .regex(/[0-9]/, 'Must contain at least one number')
  .regex(/[^A-Za-z0-9]/, 'Must contain at least one special character');
```
</details>

<details>
<summary>Exercise 5 (Hard): Dynamic Multi-Step Checkout Schema</summary>

**Specification**: Build a discriminated union schema where selecting payment method `'CREDIT_CARD'` requires card details, while `'PAYPAL'` requires only email.

```tsx
import { z } from 'zod';

const CardPaymentSchema = z.object({
  method: z.literal('CREDIT_CARD'),
  cardNumber: z.string().regex(/^\d{16}$/, 'Card must be 16 digits'),
  cvv: z.string().regex(/^\d{3,4}$/, 'CVV must be 3-4 digits'),
});

const PaypalPaymentSchema = z.object({
  method: z.literal('PAYPAL'),
  paypalEmail: z.string().email('Invalid PayPal email'),
});

export const PaymentMethodSchema = z.discriminatedUnion('method', [
  CardPaymentSchema,
  PaypalPaymentSchema,
]);
```
</details>

<details>
<summary>Exercise 6 (Hard): Action Validator Helper with Sanitization</summary>

**Specification**: Create a production helper that strips HTML tags from string fields and returns typed data or field error maps.

```tsx
import { z } from 'zod';

export function sanitizeAndValidate<T extends z.ZodRawShape>(
  schema: z.ZodObject<T>,
  formData: FormData
) {
  const raw = Object.fromEntries(formData.entries());

  // Sanitize simple string tags
  const sanitized: Record<string, any> = {};
  for (const [k, v] of Object.entries(raw)) {
    sanitized[k] = typeof v === 'string' ? v.replace(/<[^>]*>?/gm, '') : v;
  }

  const result = schema.safeParse(sanitized);
  if (!result.success) {
    return { ok: false as const, errors: result.error.flatten().fieldErrors };
  }
  return { ok: true as const, data: result.data };
}
```
</details>
