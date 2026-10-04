# Legacy Component Patterns

Before hooks (React 16.8), sharing logic between components required three patterns: **render props**, **higher-order components (HOCs)**, and **container/presentational** splits. Hooks and composition replaced most of their uses, but you'll still meet them in older codebases, in some libraries, and in interviews. This file explains each, shows how to migrate it to modern code, and notes the few cases where it's still the right tool.

## Prerequisites

[`../03-hooks/09-custom-hooks.md`](../03-hooks/09-custom-hooks.md), [`01-composition-patterns.md`](./01-composition-patterns.md), and [`../04-typescript-with-react/03-generics.md`](../04-typescript-with-react/03-generics.md) (for the typed HOC example)

---

## 1. Render props

A component takes a **function** (as a prop, usually `children` or `render`) and calls it with data to decide what to render. The component owns the logic; the caller owns the UI.

```tsx
type MouseTrackerProps = {
  children: (position: { x: number; y: number }) => ReactNode;
};

function MouseTracker({ children }: MouseTrackerProps) {
  const [position, setPosition] = useState({ x: 0, y: 0 });

  useEffect(() => {
    const onMove = (e: MouseEvent) => setPosition({ x: e.clientX, y: e.clientY });
    window.addEventListener("mousemove", onMove);
    return () => window.removeEventListener("mousemove", onMove);
  }, []);

  return <>{children(position)}</>;
}

<MouseTracker>
  {({ x, y }) => <p>Mouse at {x}, {y}</p>}
</MouseTracker>
```

### Modern replacement: a custom hook

```tsx
function useMousePosition() {
  const [position, setPosition] = useState({ x: 0, y: 0 });
  useEffect(() => {
    const onMove = (e: MouseEvent) => setPosition({ x: e.clientX, y: e.clientY });
    window.addEventListener("mousemove", onMove);
    return () => window.removeEventListener("mousemove", onMove);
  }, []);
  return position;
}

function Cursor() {
  const { x, y } = useMousePosition();
  return <p>Mouse at {x}, {y}</p>;
}
```

No extra component, no nesting, and several hooks combine without "callback pyramids":

```tsx
// Render props nest
<Mouse>{(pos) => <Theme>{(theme) => <User>{(user) => /* … */}</User>}</Theme>}</Mouse>

// Hooks stay flat
const pos = useMousePosition();
const theme = useTheme();
const user = useUser();
```

### Where render props are still used

- **Libraries that must render per-item content**, giving data for each item: virtualized lists (`renderItem`), Downshift, Formik's `<Field>`, React Hook Form's `<Controller render={...}>`, Table cell renderers.
- **Passing data from parent to children in the JSX tree** when a hook can't (the data exists only inside a component, such as per-row state).
- **Legacy class components** that can't call hooks.

A function passed as `children` is called **"function as children"**; it's the same pattern.

---

## 2. Higher-order components (HOCs)

A HOC is a **function that takes a component and returns a new, enhanced component**:

```tsx
function withLoading<P extends object>(
  Component: React.ComponentType<P>
) {
  return function WithLoading({
    isLoading,
    ...props
  }: P & { isLoading: boolean }) {
    if (isLoading) return <Spinner />;
    return <Component {...(props as P)} />;
  };
}

const UserListWithLoading = withLoading(UserList);

<UserListWithLoading isLoading={loading} users={users} />
```

Typical HOC uses: injecting props (`connect` from older Redux, `withRouter`), authorization guards (`withAuth`), logging and analytics wrappers, theming, and adding a loading state as above.

### Modern replacement

- **Injecting data or behavior** → a custom hook inside the component.
- **Guards** → a wrapper component or route guard ([`../10-routing/04-route-protection.md`](../10-routing/04-route-protection.md)).

```tsx
// HOC: withAuth(Dashboard)
// Modern: a guard component
function RequireAuth({ children }: { children: ReactNode }) {
  const { user } = useAuth();
  if (!user) return <Navigate to="/login" replace />;
  return <>{children}</>;
}

<RequireAuth><Dashboard /></RequireAuth>
```

### Why HOCs fell out of favor

| Problem | Explanation |
|---------|-------------|
| **Wrapper hell** | Many HOCs stack into deep trees: `withA(withB(withC(Component)))`, which clutters DevTools |
| **Prop collisions** | Two HOCs may inject a prop of the same name; one silently overwrites the other |
| **Implicit dependencies** | The wrapped component relies on props that appear "from nowhere", hard to see and to type |
| **Static members and refs lost** | Static properties and `ref` don't pass through without extra work (`hoist-non-react-statics`, `forwardRef`) |
| **Defined in render = bug** | Creating a HOC inside a component makes a new component type every render, resetting state ([`../02-state-and-rendering/05-state-preservation-and-reset.md`](../02-state-and-rendering/05-state-preservation-and-reset.md)) |
| **Typing is painful** | Generic wrapper types are verbose and error-prone |

Rules if you must write one: **apply HOCs outside components** (at module level), **pass unrelated props through**, **set a `displayName`** for DevTools, and don't mutate the wrapped component.

### Where HOCs are still used

- Wrapping with `React.memo` — technically a HOC ([`../14-performance/02-memoization.md`](../14-performance/02-memoization.md)).
- Library APIs (`connect`, `withTranslation`, `withProfiler`, `withErrorBoundary`) that predate hooks and remain for compatibility.
- **Error boundaries**, which still require a class component or a wrapper around one ([`../15-concurrent-and-modern-react/04-error-boundaries.md`](../15-concurrent-and-modern-react/04-error-boundaries.md)).

---

## 3. Container / presentational components

An older guideline for separating **data and behavior** from **display**:

- **Container (smart):** fetches data, manages state, passes it down. Little or no markup.
- **Presentational (dumb):** receives props and renders UI. No data fetching, no state beyond UI details.

```tsx
// Presentational: pure UI, easy to test and reuse
function UserListView({ users, onSelect }: { users: User[]; onSelect: (id: string) => void }) {
  return (
    <ul>
      {users.map((u) => (
        <li key={u.id} onClick={() => onSelect(u.id)}>{u.name}</li>
      ))}
    </ul>
  );
}

// Container: owns data and behavior
function UserListContainer() {
  const [users, setUsers] = useState<User[]>([]);
  useEffect(() => { fetchUsers().then(setUsers); }, []);
  return <UserListView users={users} onSelect={navigateToUser} />;
}
```

### Modern replacement: hooks

Hooks let one component use logic **without a separate container component**:

```tsx
function UserList() {
  const { users } = useUsers();     // data logic in a hook
  const navigate = useNavigate();
  return <UserListView users={users} onSelect={(id) => navigate(`/users/${id}`)} />;
}
```

The *idea* — keep display logic separate from data logic, so UI is easy to test and preview — survives. The *rigid two-component split* usually doesn't. Its descendants:

- **Hooks** for logic, **components** for UI.
- **Presentational components in a design system or Storybook**, fed by data from the feature code ([`../09-ui-components/09-design-system.md`](../09-ui-components/09-design-system.md)).
- **Data libraries** (TanStack Query) replacing hand-written containers ([`../12-server-state/03-tanstack-query.md`](../12-server-state/03-tanstack-query.md)).

The original author of the pattern has since said it's no longer recommended as a rule. Use the split when it genuinely helps, not as dogma.

---

## 4. Migration cheat sheet

| Legacy | Modern |
|--------|--------|
| Render prop component that only shares logic | Custom hook |
| Render prop that supplies per-item UI (list renderers, form fields) | Keep it (still idiomatic) |
| HOC injecting props | Custom hook |
| HOC guarding access | Guard component or route-level guard |
| HOC wrapping for memoization | `React.memo` (or the React Compiler — [`../15-concurrent-and-modern-react/06-react-compiler.md`](../15-concurrent-and-modern-react/06-react-compiler.md)) |
| Container component fetching data | Hook or data library |
| Class component lifecycle logic | Function component with effects ([`../02-state-and-rendering/04-component-lifecycle.md`](../02-state-and-rendering/04-component-lifecycle.md)) |
| `cloneElement` to inject props | Context (compound components) |

---

## 5. Reading legacy code

When you meet these patterns in a codebase:

1. **Don't rewrite them for fun.** Working code isn't a bug. Migrate when you're changing that code anyway.
2. **Migrate leaf-first.** Convert a HOC to a hook, keep a thin HOC wrapper for existing callers, then remove callers one by one.
3. **Check the TypeScript types** of HOCs carefully; they often hide `any`.
4. **Watch for class components**: hooks can't be used in them, so a render prop or HOC may be the only bridge until the class is converted.

---

## Interview angle

Common questions: *What are HOCs and what problems do they have? When would you use a render prop instead of a hook? How do hooks replace container/presentational?* Answers are summarized in [`../23-interview/01-hooks.md`](../23-interview/01-hooks.md) and [`../23-interview/00-react-fundamentals.md`](../23-interview/00-react-fundamentals.md).

---

## Common mistakes

- **Writing new HOCs for logic reuse** — a custom hook is simpler, more composable, and easier to type.
- **Creating a HOC inside a component body** — makes a new component type every render and resets state.
- **Not passing unrelated props through** in a HOC — breaks the wrapped component.
- **Nesting many render props** — use hooks to flatten.
- **Treating container/presentational as a mandatory rule** — split when it helps testing or reuse, not by default.
- **Rewriting working legacy code wholesale** — migrate incrementally while touching it.
- **Losing `ref`s and static properties** through HOCs — forward refs and hoist statics, or avoid the HOC.

## Quick summary

- Render props pass a function as `children`/`render`; HOCs wrap a component to enhance it; container/presentational splits logic from UI
- Custom hooks replace most uses: they're flatter, easier to compose, and easier to type
- Render props remain idiomatic for per-item rendering (virtualized lists, form fields); HOCs survive in `memo`, error boundaries, and older libraries
- HOC pitfalls: wrapper hell, prop collisions, lost refs and statics, defining them inside render
- Migrate incrementally, leaf-first, while you're already changing the code

## Next

You've finished component design. Continue to **[`../06-forms/README.md`](../06-forms/README.md)** to apply these patterns to forms, the most common reusable component family. For how Radix/shadcn put them into practice, see [`../09-ui-components/00-shadcn-ui.md`](../09-ui-components/00-shadcn-ui.md).
