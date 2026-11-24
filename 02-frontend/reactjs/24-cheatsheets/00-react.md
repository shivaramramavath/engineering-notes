# React Cheatsheet

> Quick-reference guide for modern React 19 fundamentals, JSX patterns, component structures, ref props, and common syntax.

---

## 1. Component Structure & Typing (React 19)

```tsx
import React, { type ComponentProps } from 'react';

interface CardProps extends ComponentProps<'article'> {
  title: string;
  badge?: string;
  variant?: 'default' | 'highlight';
}

// React 19: `ref` is a regular prop (no forwardRef required!)
export function Card({ title, badge, variant = 'default', ref, children, ...props }: CardProps) {
  return (
    <article ref={ref} className={`card ${variant}`} {...props}>
      <header>
        <h3>{title}</h3>
        {badge && <span className="badge">{badge}</span>}
      </header>
      <div>{children}</div>
    </article>
  );
}
```

---

## 2. Common JSX Patterns

### Conditional Rendering
```tsx
// Ternary
{isLoggedIn ? <UserAvatar /> : <LoginButton />}

// Logical AND (Warning: check length > 0, never {items.length && ...} which renders '0')
{items.length > 0 && <ItemList items={items} />}

// Early Return
if (isLoading) return <Spinner />;
if (error) return <ErrorMessage message={error.message} />;
```

### List Rendering
```tsx
<ul>
  {users.map((user) => (
    // Always use unique stable keys, never array index!
    <li key={user.id}>{user.name}</li>
  ))}
</ul>
```

---

## 3. Portals & Error Boundaries

### `createPortal`
```tsx
import { createPortal } from 'react-dom';

export function ModalPortal({ children }: { children: React.ReactNode }) {
  const mount = document.getElementById('modal-root') || document.body;
  return createPortal(children, mount);
}
```

### Error Boundary (React Component Pattern)
```tsx
import { Component, type ReactNode } from 'react';

interface Props { children: ReactNode; fallback?: ReactNode }
interface State { hasError: boolean }

export class ErrorBoundary extends Component<Props, State> {
  state: State = { hasError: false };

  static getDerivedStateFromError() {
    return { hasError: true };
  }

  componentDidCatch(error: Error) {
    console.error('Captured error:', error);
  }

  render() {
    if (this.state.hasError) {
      return this.props.fallback || <h2>Something went wrong.</h2>;
    }
    return this.props.children;
  }
}
```
