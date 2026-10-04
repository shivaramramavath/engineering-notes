# Conditional Rendering

Real UIs change with the data: a spinner while loading, an error message on failure, an admin button only for admins. In React there's no special template syntax for this — you use ordinary JavaScript (`if`, ternary, `&&`) to decide what JSX to return.

## Prerequisites

[`02-components-and-props.md`](./02-components-and-props.md)

---

## `if` and early return

The clearest option for whole-component decisions:

```jsx
function Profile({ user, isLoading }) {
  if (isLoading) return <Spinner />;
  if (!user) return <p>User not found.</p>;

  return (
    <div>
      <h1>{user.name}</h1>
      <p>{user.email}</p>
    </div>
  );
}
```

Handling the "special" cases first and returning early keeps the main path flat and readable. A component may return `null` to render nothing.

---

## Ternary operator

Use a ternary to choose between two pieces of JSX **inside** markup:

```jsx
<button>{isLoggedIn ? "Log out" : "Log in"}</button>

{isEditing ? <EditForm /> : <ReadOnlyView />}
```

Nested ternaries get unreadable quickly. If you need a third branch, switch to `if`, a variable, or a lookup object (below).

---

## Logical AND (`&&`)

Use `&&` when you want to render something **or nothing**:

```jsx
{isAdmin && <AdminPanel />}
{errors.length > 0 && <ErrorList errors={errors} />}
```

If the left side is falsy, React skips the right side and renders nothing.

### The `0` pitfall

`0` is falsy, but React **renders** the number `0`:

```jsx
{count && <Badge count={count} />}   // ❌ shows "0" when count is 0
{count > 0 && <Badge count={count} />} // ✅ boolean on the left
```

Always make the left side an actual boolean (`> 0`, `!!value`, `Boolean(value)`). The same applies to empty strings only partially (`""` renders nothing, but be explicit anyway) and to `NaN`, which renders as `NaN`.

---

## Assigning JSX to variables

For longer or multi-branch logic, compute the content before the `return`:

```jsx
function StatusBadge({ status }) {
  let badge;
  if (status === "success") badge = <span className="green">Done</span>;
  else if (status === "error") badge = <span className="red">Failed</span>;
  else badge = <span className="gray">Pending</span>;

  return <div>{badge}</div>;
}
```

---

## Lookup objects for many cases

When branching on one value with several options, an object map is cleaner than a chain of conditions:

```jsx
const icons = {
  success: <CheckIcon />,
  error: <XIcon />,
  warning: <AlertIcon />,
};

function StatusIcon({ status }) {
  return icons[status] ?? null;
}
```

---

## Conditional attributes and classes

Conditions apply to props too:

```jsx
<button disabled={isSubmitting} className={isActive ? "tab active" : "tab"}>
  Save
</button>
```

For many conditional classes, a helper such as `clsx` keeps things tidy (see [`../07-styling/05-styling-approaches-compared.md`](../07-styling/05-styling-approaches-compared.md)).

---

## Rendering vs hiding

Conditionally **not rendering** a component is different from hiding it with CSS:

```jsx
{show && <Panel />}                        // removed from the DOM, state is lost
<Panel style={{ display: show ? "block" : "none" }} /> // stays mounted, state kept
```

When a component stops rendering, React **unmounts** it and discards its state. When it renders again, it starts fresh. Use CSS hiding only when you need to preserve state (a half-filled form tab, a scroll position). The rules are explained in [`../02-state-and-rendering/05-state-preservation-and-reset.md`](../02-state-and-rendering/05-state-preservation-and-reset.md).

---

## Handling loading, error, and empty states

Most real data-driven UIs have four states, and all should be handled deliberately:

```jsx
if (isLoading) return <Skeleton />;
if (error) return <ErrorMessage error={error} />;
if (items.length === 0) return <EmptyState />;
return <ItemList items={items} />;
```

More on this pattern in [`../12-server-state/02-loading-and-error-states.md`](../12-server-state/02-loading-and-error-states.md).

---

## Common mistakes

- **`{count && <X />}`** — renders `0` when `count` is `0`; compare explicitly.
- **Deeply nested ternaries** — extract to `if`, a variable, or a lookup object.
- **Assuming hidden means preserved** — conditional rendering unmounts and resets state.
- **Forgetting the loading/error/empty cases** — leaves users with blank screens.
- **Early `return` before a hook** — hooks must run in the same order every render; place all hooks above any early return (see [`../03-hooks/00-hook-rules.md`](../03-hooks/00-hook-rules.md)).

## Quick summary

- Use `if`/early return for whole-component branching, ternary for either/or inside JSX, `&&` for show-or-nothing
- Make the left side of `&&` a real boolean to avoid rendering `0`
- Use variables or lookup objects when there are more than two branches
- Not rendering unmounts and resets state; CSS hiding keeps it
- Handle loading, error, and empty states explicitly

## Next

**[`05-lists-and-keys.md`](./05-lists-and-keys.md)** covers rendering collections of data.
