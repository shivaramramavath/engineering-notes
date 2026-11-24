# TypeScript with React — Senior Interview Questions & Deep Dives

> Advanced typing patterns, polymorphic components with `asChild`, generic components, discriminated unions, and typing React 19 features.

---

## Question 1: How do you build a fully type-safe polymorphic component in React (e.g., `<Button as="a" href="...">` or using Radix `asChild`)?

### The Challenge
A polymorphic component should render as a `<button>` by default, but when `as="a"` is passed, TypeScript must:
1. Allow `href`, `target`, and anchor-specific attributes.
2. Disallow `disabled` or button-only attributes.
3. Correctly type the forwarded `ref` to `HTMLAnchorElement`.

### The Modern Solution: Radix UI `asChild` Slot Pattern
Instead of fragile polymorphic `as="component"` generic props that bloat TypeScript compilation times, the modern React 19 industry standard is the **Slot Pattern**:
```tsx
import * as React from 'react';
import { Slot } from '@radix-ui/react-slot';

export interface ButtonProps extends React.ComponentProps<'button'> {
  asChild?: boolean;
}

export const Button = ({ asChild = false, ref, className, ...props }: ButtonProps) => {
  const Comp = asChild ? Slot : 'button';
  return <Comp ref={ref} className={className} {...props} />;
};
```

### Usage
```tsx
// Renders <button> with full HTMLButtonElement typings
<Button onClick={() => alert('Clicked')}>Submit</Button>

// Merges props into child <a> with full HTMLAnchorElement typings!
<Button asChild>
  <a href="/login">Sign In</a>
</Button>
```

---

## Question 2: Why should you prefer Discriminated Unions over optional props when designing component interfaces?

### The Problem with Optional Props
```tsx
// Anti-pattern: Invalid combinations are allowed by TypeScript!
interface AlertProps {
  status: 'success' | 'error' | 'loading';
  errorMessage?: string;
  progressPercentage?: number;
}

// Bug: Compiles cleanly even though 'loading' shouldn't have an errorMessage!
<Alert status="loading" errorMessage="Database timeout" />
```

### The Discriminated Union Solution
```tsx
interface SuccessAlert {
  status: 'success';
  message: string;
}

interface ErrorAlert {
  status: 'error';
  errorMessage: string;
  retryAction: () => void;
}

interface LoadingAlert {
  status: 'loading';
  progressPercentage: number;
}

export type AlertProps = SuccessAlert | ErrorAlert | LoadingAlert;

export function Alert(props: AlertProps) {
  switch (props.status) {
    case 'success':
      return <div>Success: {props.message}</div>;
    case 'error':
      return <div>Error: {props.errorMessage} <button onClick={props.retryAction}>Retry</button></div>;
    case 'loading':
      return <div>Loading... {props.progressPercentage}%</div>;
  }
}
```
TypeScript enforces exhaustive checking and prevents impossible component states at compile time.
