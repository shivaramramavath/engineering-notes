# TypeScript with React Cheatsheet

> Quick-reference guide for typing props, event handlers, HTML attributes, hooks, and polymorphic components in TypeScript 5.7+ and React 19.

---

## 1. Common Prop Types

```tsx
interface Props {
  // Primitives
  title: string;
  count: number;
  isActive: boolean;

  // React elements
  children: React.ReactNode;          // Anything renderable (strings, JSX, numbers, null)
  element: React.ReactElement;        // Strictly a single JSX element
  style?: React.CSSProperties;        // Inline styles object

  // Functions / Callbacks
  onClick: (id: string) => void;
  onFilterChange: (filters: Record<string, string>) => Promise<void>;

  // Dispatch / State setters
  setState: React.Dispatch<React.SetStateAction<number>>;
}
```

---

## 2. Event Handler Types

| Event | Type | Common Target Property |
| :--- | :--- | :--- |
| Click / Mouse | `React.MouseEvent<HTMLButtonElement>` | `e.clientX`, `e.currentTarget` |
| Form Submit | `React.FormEvent<HTMLFormElement>` | `e.preventDefault()` |
| Input Change | `React.ChangeEvent<HTMLInputElement>` | `e.target.value`, `e.target.checked` |
| Textarea Change| `React.ChangeEvent<HTMLTextAreaElement>` | `e.target.value` |
| Keydown / Up | `React.KeyboardEvent<HTMLInputElement>` | `e.key === 'Enter'`, `e.code` |
| Focus / Blur | `React.FocusEvent<HTMLInputElement>` | `e.relatedTarget` |

---

## 3. Extending Native Elements (`ComponentProps`)

```tsx
// Extend any standard HTML tag
interface CustomButtonProps extends React.ComponentProps<'button'> {
  isLoading?: boolean;
}

export function CustomButton({ isLoading, children, disabled, ...props }: CustomButtonProps) {
  return (
    <button disabled={disabled || isLoading} {...props}>
      {isLoading ? 'Loading...' : children}
    </button>
  );
}
```

---

## 4. Hook Typings

```tsx
// useState with union / null
const [user, setUser] = useState<User | null>(null);

// useRef attached to DOM node
const inputRef = useRef<HTMLInputElement>(null);

// useRef mutable variable (no DOM)
const timerRef = useRef<number | null>(null);
```
