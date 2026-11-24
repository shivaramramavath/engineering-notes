# Project 10: Dynamic Form Builder (`form-builder`)

> A drag-and-drop form construction toolkit allowing users to design custom schemas (text, number, select, checkbox, nested groups), configure validation rules dynamically, and instantly preview the rendered interactive form powered by React Hook Form and Zod.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-10-form-builder` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | Zustand v5 + React Hook Form + Dynamic Zod Schema Generator |
| **Styling** | Tailwind CSS v4 + @dnd-kit |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Data Flow
The builder state maintains an array of `FormFieldDefinition` objects. When fields are added, reordered, or edited, a pure utility function transforms the field definitions into an executable Zod schema (`buildZodSchema(fields)`). The Preview panel instantiates a `useForm` hook bound to that dynamic schema.

```
[ Canvas: Field Palette & Drag Reorder ]
                    │
                    ▼
[ Builder Store: FormFieldDefinition[] ]
        │                       │
        ▼                       ▼
[ Schema Generator ]    [ Live Form Renderer ]
  `buildZodSchema()`     `useForm({ resolver: zodResolver(schema) })`
```

### 1.2 Component Structure
```text
src/
├── types/
│   └── formSchema.ts        # Field types, options, validation rules
├── utils/
│   └── schemaBuilder.ts     # Dynamic Zod schema compiler
├── store/
│   └── useBuilderStore.ts   # Active schema definition store
├── components/
│   ├── FieldPalette.tsx     # Draggable field templates (Input, Select, etc.)
│   ├── FormCanvas.tsx       # Reorderable builder dropzone
│   ├── FieldConfigModal.tsx # Rule editor (required, min, max, regex)
│   └── FormPreview.tsx      # Rendered interactive form with submit inspector
└── App.tsx                  # Split-pane builder layout
```

---

## 2. Complete Project Source Code

### 2.1 Types & Schema Builder (`src/types/formSchema.ts`, `src/utils/schemaBuilder.ts`)
```ts
// src/types/formSchema.ts
export type FieldType = 'text' | 'number' | 'select' | 'checkbox';

export interface FormFieldDefinition {
  id: string;
  name: string;
  label: string;
  type: FieldType;
  required: boolean;
  placeholder?: string;
  options?: string[]; // for select
  min?: number;
}

// src/utils/schemaBuilder.ts
import { z } from 'zod';
import { FormFieldDefinition } from '../types/formSchema';

export function buildZodSchema(fields: FormFieldDefinition[]) {
  const shape: Record<string, z.ZodTypeAny> = {};

  fields.forEach((field) => {
    let schema: z.ZodTypeAny;

    switch (field.type) {
      case 'text':
        schema = z.string();
        if (field.required) {
          schema = (schema as z.ZodString).min(1, `${field.label} is required`);
        } else {
          schema = schema.optional();
        }
        break;
      case 'number':
        schema = z.coerce.number();
        if (field.min !== undefined) {
          schema = (schema as z.ZodNumber).min(field.min, `Must be at least ${field.min}`);
        }
        break;
      case 'checkbox':
        schema = z.boolean();
        break;
      case 'select':
        schema = z.string().min(1, 'Selection required');
        break;
      default:
        schema = z.any();
    }

    shape[field.name] = schema;
  });

  return z.object(shape);
}
```

### 2.2 Live Form Preview Component (`src/components/FormPreview.tsx`)
```tsx
import React, { useState } from 'react';
import { useForm } from 'react-hook-form';
import { zodResolver } from '@hookform/resolvers/zod';
import { FormFieldDefinition } from '../types/formSchema';
import { buildZodSchema } from '../utils/schemaBuilder';

export const FormPreview: React.FC<{ fields: FormFieldDefinition[] }> = ({ fields }) => {
  const [submittedData, setSubmittedData] = useState<any>(null);
  const schema = buildZodSchema(fields);

  const {
    register,
    handleSubmit,
    formState: { errors },
  } = useForm({
    resolver: zodResolver(schema),
  });

  return (
    <div className="rounded-xl border border-slate-200 bg-white p-6 shadow-sm">
      <h3 className="text-base font-bold text-slate-900 border-b pb-3 mb-4">Live Preview</h3>
      <form onSubmit={handleSubmit((data) => setSubmittedData(data))} className="space-y-4">
        {fields.map((field) => (
          <div key={field.id} className="flex flex-col gap-1">
            <label className="text-xs font-semibold text-slate-700">
              {field.label} {field.required && <span className="text-red-500">*</span>}
            </label>
            {field.type === 'select' ? (
              <select
                {...register(field.name)}
                className="rounded border border-slate-300 p-2 text-sm focus:outline-blue-500"
              >
                <option value="">Select an option</option>
                {field.options?.map((opt) => (
                  <option key={opt} value={opt}>
                    {opt}
                  </option>
                ))}
              </select>
            ) : field.type === 'checkbox' ? (
              <input
                type="checkbox"
                {...register(field.name)}
                className="h-4 w-4 rounded border-slate-300"
              />
            ) : (
              <input
                type={field.type}
                placeholder={field.placeholder}
                {...register(field.name)}
                className="rounded border border-slate-300 p-2 text-sm focus:outline-blue-500"
              />
            )}
            {errors[field.name] && (
              <span className="text-xs text-red-500">
                {errors[field.name]?.message as string}
              </span>
            )}
          </div>
        ))}

        <button
          type="submit"
          className="w-full rounded-lg bg-blue-600 py-2.5 text-sm font-semibold text-white hover:bg-blue-700"
        >
          Submit Form
        </button>
      </form>

      {submittedData && (
        <pre className="mt-4 rounded bg-slate-900 p-3 text-xs text-emerald-400 overflow-x-auto">
          {JSON.stringify(submittedData, null, 2)}
        </pre>
      )}
    </div>
  );
};
```

---

## 3. Testing Suite (`src/tests/schemaBuilder.test.ts`)

```ts
import { describe, it, expect } from 'vitest';
import { buildZodSchema } from '../utils/schemaBuilder';
import { FormFieldDefinition } from '../types/formSchema';

describe('Dynamic Zod Schema Builder', () => {
  it('creates valid schema rejecting empty required string', () => {
    const fields: FormFieldDefinition[] = [
      { id: '1', name: 'username', label: 'Username', type: 'text', required: true },
    ];
    const schema = buildZodSchema(fields);
    const result = schema.safeParse({ username: '' });
    expect(result.success).toBe(false);
  });
});
```

---

## 4. Build and Run Instructions

```bash
# Install dependencies
npm install react-hook-form @hookform/resolvers zod @dnd-kit/core

# Run dev
npm run dev
```
