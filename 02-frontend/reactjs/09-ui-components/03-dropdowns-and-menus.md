# Dropdowns and Menus

"Dropdown" covers four different components that look similar and behave differently. Picking the right one is most of the work.

| Component | Use it for | Holds a value? |
|---|---|---|
| **DropdownMenu** | A list of *actions* (Edit, Duplicate, Delete) | No — items run callbacks |
| **Select** | Choosing *one value* from a fixed list | Yes |
| **Popover** | Arbitrary floating content (filters, date picker, small forms) | Whatever you put in it |
| **ContextMenu** | Actions on right-click / long-press | No |

The test: **does picking an option change a value, or trigger an action?** Value → Select. Action → DropdownMenu.

```bash
npx shadcn@latest add dropdown-menu select popover context-menu
```

## DropdownMenu

```tsx
import {
  DropdownMenu, DropdownMenuContent, DropdownMenuItem, DropdownMenuLabel,
  DropdownMenuSeparator, DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu"

export function UserMenu() {
  return (
    <DropdownMenu>
      <DropdownMenuTrigger asChild>
        <Button variant="ghost">Account</Button>
      </DropdownMenuTrigger>

      <DropdownMenuContent align="end">
        <DropdownMenuLabel>My account</DropdownMenuLabel>
        <DropdownMenuSeparator />
        <DropdownMenuItem asChild>
          <Link to="/settings">Settings</Link>
        </DropdownMenuItem>
        <DropdownMenuItem onSelect={logout}>Log out</DropdownMenuItem>
      </DropdownMenuContent>
    </DropdownMenu>
  )
}
```

What you get for free: roving focus with arrow keys, `Home`/`End`, typeahead (type "L" to jump to "Log out"), `Esc` to close and return focus to the trigger, collision-aware positioning, correct `role="menu"` / `menuitem` semantics.

### `onSelect`, not `onClick`

Radix items fire `onSelect` for both mouse and keyboard activation. By default the menu closes afterwards. To keep it open, prevent the default:

```tsx
<DropdownMenuItem onSelect={(e) => { e.preventDefault(); toggleSomething() }}>
  Toggle
</DropdownMenuItem>
```

### Links in menus

Wrap a router `Link` with `asChild` so the item is a real anchor (middle-click and "open in new tab" work):

```tsx
<DropdownMenuItem asChild><Link to="/billing">Billing</Link></DropdownMenuItem>
```

### Checkbox and radio items

For view options and sort order — stateful items inside a menu:

```tsx
<DropdownMenuCheckboxItem checked={showArchived} onCheckedChange={setShowArchived}>
  Show archived
</DropdownMenuCheckboxItem>

<DropdownMenuRadioGroup value={sort} onValueChange={setSort}>
  <DropdownMenuRadioItem value="newest">Newest</DropdownMenuRadioItem>
  <DropdownMenuRadioItem value="oldest">Oldest</DropdownMenuRadioItem>
</DropdownMenuRadioGroup>
```

Submenus use `DropdownMenuSub`, `DropdownMenuSubTrigger`, and `DropdownMenuSubContent`. Keep nesting to one level; deeper menus are hard to use with a mouse and worse on touch.

### Icon-only triggers need a label

```tsx
<Button variant="ghost" size="icon" aria-label="Open row actions">
  <MoreHorizontal />
</Button>
```

Without `aria-label`, a screen reader announces just "button".

## Select

Use `Select` when the user picks one value from a known set:

```tsx
import {
  Select, SelectContent, SelectItem, SelectTrigger, SelectValue,
} from "@/components/ui/select"

<Select value={role} onValueChange={setRole}>
  <SelectTrigger className="w-48">
    <SelectValue placeholder="Choose a role" />
  </SelectTrigger>
  <SelectContent>
    <SelectItem value="admin">Admin</SelectItem>
    <SelectItem value="editor">Editor</SelectItem>
    <SelectItem value="viewer">Viewer</SelectItem>
  </SelectContent>
</Select>
```

- It's **not a native `<select>`**. The change event is `onValueChange(value: string)`, not `onChange(event)`. This matters with form libraries: see [06-form-controls](./06-form-controls.md).
- Values are strings. Convert numbers/IDs yourself.
- For long or mobile-first lists, a native `<select>` is sometimes the better UX — it gets the platform picker for free. shadcn's styled `NativeSelect`-style approach or a plain `<select>` with Tailwind classes is a valid choice.
- Need search? A Select can't filter. Use the combobox pattern: [Popover + Command](./05-command-palette.md).

## Popover

A floating panel anchored to a trigger, for content that isn't a list of menu items:

```tsx
<Popover>
  <PopoverTrigger asChild><Button variant="outline">Filters</Button></PopoverTrigger>
  <PopoverContent className="w-72" align="start">
    <div className="space-y-3">
      <Label htmlFor="min">Min price</Label>
      <Input id="min" type="number" />
    </div>
  </PopoverContent>
</Popover>
```

Unlike a menu, a popover doesn't take over arrow-key navigation: Tab moves through its contents. It's non-modal by default (clicking outside closes it) and returns focus to the trigger when closed. Use it for date pickers, filter forms, and small inline editors.

## ContextMenu

Same item components as DropdownMenu, opened by right-click or long-press:

```tsx
<ContextMenu>
  <ContextMenuTrigger className="rounded border p-8">Right-click me</ContextMenuTrigger>
  <ContextMenuContent>
    <ContextMenuItem onSelect={copy}>Copy</ContextMenuItem>
    <ContextMenuItem onSelect={rename}>Rename</ContextMenuItem>
  </ContextMenuContent>
</ContextMenu>
```

Never make a context menu the **only** way to reach an action — it's undiscoverable and awkward on keyboards and touch. Mirror the actions in a visible "⋯" DropdownMenu.

## Positioning props

`DropdownMenuContent`, `PopoverContent`, and `SelectContent` position with Radix Popper:

```tsx
<DropdownMenuContent side="bottom" align="end" sideOffset={4} />
```

- `side`: `top | right | bottom | left` (preferred side; flips on collision)
- `align`: `start | center | end`
- `sideOffset` / `alignOffset`: gap in px

## Common mistakes

- **Menu for a value, Select for an action.** Selecting "Delete" from a Select makes no sense semantically or for assistive tech.
- **`onClick` on items** instead of `onSelect`, losing keyboard/menu-close behavior consistency.
- **Rendering a Dialog inside a menu item** — it unmounts with the menu. Lift state out; see [02-dialogs-and-modals](./02-dialogs-and-modals.md).
- **Missing `asChild` on custom triggers.** Without it Radix renders its own `<button>` around yours, producing a button-in-button.
- **Using `register()` on Select.** It's not a native input. Use `Controller`.
- **Icon-only triggers without `aria-label`.**
- **Huge item lists in a Select/Menu.** They render all items. For hundreds of options, use a searchable combobox with virtualization.

## Quick summary

- Action → `DropdownMenu`. Value → `Select`. Free-form content → `Popover`. Right-click → `ContextMenu`.
- Radix gives you keyboard navigation, typeahead, focus return, and collision handling.
- Use `onSelect` on items; `preventDefault()` keeps the menu open.
- `asChild` makes triggers and items render as your own element (links, custom buttons).
- Select uses `onValueChange` with string values — not a native change event.

## Next

[04 — Tabs](./04-tabs.md)
