# Storybook 8 — Basic to Advanced

A comprehensive reference card for Storybook 8 with React 19 and Vite.

> **Target Version:** Storybook v8.5+  
> **Framework:** `@storybook/react-vite`  
> **Philosophy:** Isolated UI component development, automated documentation, and visual regression testing  

---

## Level 0 — Concepts

- **Component-Driven Development**: Developing UI components in complete isolation from complex backend APIs and routing layouts.
- **Component Story Format (CSF 3)**: Declarative, object-based story declarations with typed `Meta` and `StoryObj`.
- **Core Primitives Table**:

| Concept | Description |
| :--- | :--- |
| `Meta<typeof Component>` | Configures the component story catalog, parameters, and tags |
| `StoryObj<typeof Component>`| Defines a specific variant state of the component (e.g. Primary, Disabled) |
| `args` | Dynamic props passed to the story component |
| `argTypes` | Controls and documentation annotations for Storybook controls |
| `play` function | Automates user interactions (clicks, typing) for interaction testing |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npx storybook@latest init
```

### 1.2 Defining Stories (CSF 3 Format)
```tsx
// src/components/Button.stories.tsx
import type { Meta, StoryObj } from '@storybook/react';
import { Button } from './Button';

const meta: Meta<typeof Button> = {
  title: 'Components/Button',
  component: Button,
  parameters: {
    layout: 'centered',
  },
  tags: ['autodocs'],
  argTypes: {
    variant: {
      control: 'select',
      options: ['default', 'destructive', 'outline', 'ghost'],
    },
    size: {
      control: 'radio',
      options: ['sm', 'default', 'lg'],
    },
  },
};

export default meta;
type Story = StoryObj<typeof Button>;

export const Primary: Story = {
  args: {
    variant: 'default',
    children: 'Primary Action',
  },
};

export const Destructive: Story = {
  args: {
    variant: 'destructive',
    children: 'Delete Account',
  },
};

export const Disabled: Story = {
  args: {
    disabled: true,
    children: 'Disabled Button',
  },
};
```

---

## Level 2 — Intermediate: Automated Interaction Testing with `play()`

Storybook 8 can automatically execute interactions using `@storybook/test` (RTL + Vitest assertions):

```tsx
import { expect, userEvent, within } from '@storybook/test';

export const InteractiveLoginForm: StoryObj = {
  play: async ({ canvasElement }) => {
    const canvas = within(canvasElement);

    // Simulate typing into inputs
    await userEvent.type(canvas.getByLabelText(/email/i), 'user@example.com');
    await userEvent.type(canvas.getByLabelText(/password/i), 'secret123');

    // Click submit
    await userEvent.click(canvas.getByRole('button', { name: /sign in/i }));

    // Assert success feedback
    await expect(canvas.getByText(/welcome back/i)).toBeInTheDocument();
  },
};
```

---

## Level 3 — Advanced: Global Decorators for Themes and Providers

Wrap all stories in application providers (React Query, Tailwind, Theme):

```tsx
// .storybook/preview.tsx
import React from 'react';
import type { Preview } from '@storybook/react';
import '../src/index.css'; // Import Tailwind CSS v4!

const preview: Preview = {
  decorators: [
    (Story) => (
      <div className="font-sans antialiased">
        <Story />
      </div>
    ),
  ],
};

export default preview;
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Connecting Stories to Real Production Backends** | Stories fail when offline or on CI; tests are flaky. | Mock network requests inside Storybook using MSW addon (`msw-storybook-addon`). |
| **Writing Huge Stories that Render Entire Pages**| Hard to isolate visual regressions; defeats component-driven modularity. | Keep stories focused on individual UI components and features. |
| **Using Legacy CSF 2 Function Syntax** | Misses TypeScript autocompletion and CSF 3 auto-doc features. | Use modern CSF 3 object syntax (`export const Primary: Story = { ... }`). |
| **Omitting `autodocs` Tags** | Storybook cannot auto-generate visual component documentation tables. | Add `tags: ['autodocs']` to story `meta`. |
