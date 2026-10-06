# Command Palette

A command palette is a searchable list opened with a shortcut (usually `Ctrl/Cmd + K`) that lets users jump to pages and run actions by typing. The same underlying component also powers **searchable selects (comboboxes)**.

shadcn's `Command` wraps **[cmdk](https://github.com/pacocoursey/cmdk)**, an unstyled React component that handles filtering, keyboard navigation, and ARIA for you.

```bash
npx shadcn@latest add command
```

## Basic palette in a dialog

```tsx
import { useEffect, useState } from "react"
import { useNavigate } from "react-router"
import {
  CommandDialog, CommandEmpty, CommandGroup, CommandInput,
  CommandItem, CommandList, CommandSeparator, CommandShortcut,
} from "@/components/ui/command"

export function CommandPalette() {
  const [open, setOpen] = useState(false)
  const navigate = useNavigate()

  useEffect(() => {
    const onKeyDown = (e: KeyboardEvent) => {
      if (e.key === "k" && (e.metaKey || e.ctrlKey)) {
        e.preventDefault()
        setOpen((o) => !o)
      }
    }
    document.addEventListener("keydown", onKeyDown)
    return () => document.removeEventListener("keydown", onKeyDown)
  }, [])

  function run(action: () => void) {
    setOpen(false)
    action()
  }

  return (
    <CommandDialog open={open} onOpenChange={setOpen}>
      <CommandInput placeholder="Type a command or search…" />
      <CommandList>
        <CommandEmpty>No results found.</CommandEmpty>

        <CommandGroup heading="Navigate">
          <CommandItem onSelect={() => run(() => navigate("/dashboard"))}>Dashboard</CommandItem>
          <CommandItem onSelect={() => run(() => navigate("/settings"))}>
            Settings <CommandShortcut>⌘,</CommandShortcut>
          </CommandItem>
        </CommandGroup>

        <CommandSeparator />

        <CommandGroup heading="Actions">
          <CommandItem onSelect={() => run(createProject)}>New project</CommandItem>
        </CommandGroup>
      </CommandList>
    </CommandDialog>
  )
}
```

Mount `<CommandPalette />` **once** near the app root (inside the router, since it uses `useNavigate`).

- `preventDefault()` on the shortcut stops the browser's own handling of the key combo.
- Toggling with `o => !o` lets the same shortcut close it.
- `CommandDialog` is a regular Radix [Dialog](./02-dialogs-and-modals.md) with a `Command` inside, so you get focus trapping, `Esc`, and portal behavior.
- Close *then* act in `run()`. If the action opens another dialog, you don't want two modals fighting over focus.

## How filtering works

As the user types, cmdk scores each item's text against the search and **reorders and hides** items. Arrow keys move the highlighted item; `Enter` triggers its `onSelect`.

- Matching uses the item's text content by default. To match different text, set `value`.
- To match extra terms without showing them, use `keywords`:

```tsx
<CommandItem value="settings" keywords={["preferences", "config", "account"]}>
  Settings
</CommandItem>
```

Typing "prefs" or "config" now finds Settings.

- Items whose group has no matching items hide the group heading automatically.

### Custom or disabled filtering

```tsx
<Command filter={(value, search, keywords) => /* return 0..1 */ 1}>
<Command shouldFilter={false}>   {/* you filter; cmdk only renders */}
```

## Async search (server-side)

For results from an API, turn off client-side filtering. Otherwise cmdk will filter your already-filtered server results against the same text and may hide valid hits.

```tsx
function UserSearch({ onPick }: { onPick: (u: User) => void }) {
  const [query, setQuery] = useState("")
  const debounced = useDebouncedValue(query, 250)

  const { data = [], isFetching } = useQuery({
    queryKey: ["users", "search", debounced],
    queryFn: () => searchUsers(debounced),
    enabled: debounced.length > 1,
  })

  return (
    <Command shouldFilter={false}>
      <CommandInput value={query} onValueChange={setQuery} placeholder="Search users…" />
      <CommandList>
        {isFetching && <CommandLoading>Searching…</CommandLoading>}
        {!isFetching && debounced.length > 1 && data.length === 0 && (
          <CommandEmpty>No users found.</CommandEmpty>
        )}
        {data.map((u) => (
          <CommandItem key={u.id} value={String(u.id)} onSelect={() => onPick(u)}>
            {u.name}
          </CommandItem>
        ))}
      </CommandList>
    </Command>
  )
}
```

- `useDebouncedValue` is your own small hook (see [custom hooks](../03-hooks/09-custom-hooks.md)).
- Query keys include the search text so [TanStack Query](../12-server-state/03-tanstack-query.md) caches per term.
- Give items a stable `value` (an ID) so highlighting survives re-renders when results change.
- `CommandLoading` is exported by cmdk; shadcn's `command.tsx` may not re-export it, so import it from `cmdk` or add the wrapper yourself if your generated file lacks it.

## Combobox: searchable select

shadcn doesn't ship a single "Combobox" component; it's a **Popover + Command** composition:

```tsx
export function FrameworkCombobox({ value, onChange }: Props) {
  const [open, setOpen] = useState(false)

  return (
    <Popover open={open} onOpenChange={setOpen}>
      <PopoverTrigger asChild>
        <Button variant="outline" role="combobox" aria-expanded={open} className="w-56 justify-between">
          {frameworks.find((f) => f.value === value)?.label ?? "Select framework…"}
        </Button>
      </PopoverTrigger>
      <PopoverContent className="w-56 p-0">
        <Command>
          <CommandInput placeholder="Search…" />
          <CommandList>
            <CommandEmpty>No framework found.</CommandEmpty>
            <CommandGroup>
              {frameworks.map((f) => (
                <CommandItem
                  key={f.value}
                  value={f.value}
                  onSelect={(v) => { onChange(v === value ? "" : v); setOpen(false) }}
                >
                  {f.label}
                </CommandItem>
              ))}
            </CommandGroup>
          </CommandList>
        </Command>
      </PopoverContent>
    </Popover>
  )
}
```

Use this instead of [Select](./03-dropdowns-and-menus.md) when the list is long enough that scrolling is painful. Note that cmdk lowercases `onSelect`'s value argument in some versions; if your values are case-sensitive IDs, close over the item (`onSelect={() => onChange(f.value)}`) instead of trusting the parameter.

## Designing the command list

A palette is only as good as its commands. Keep them as **data**, not hardcoded JSX:

```tsx
type Command = {
  id: string
  label: string
  group: "Navigate" | "Actions"
  keywords?: string[]
  shortcut?: string
  run: () => void
}
```

Build the list in one place (or let features register commands) and render by group. Good palettes include navigation, creation actions ("New project"), and toggles (theme). Poor ones dump every button in the app.

## Accessibility notes

- cmdk uses a listbox pattern with `aria-activedescendant`: focus stays in the input while the highlighted option changes. That's intended — don't move DOM focus onto items.
- Make sure there's also a **visible** way to open it (a search button in the header). A hidden shortcut helps power users but isn't discoverable.
- Avoid shortcuts that collide with browser or screen reader keys. `Ctrl/Cmd+K` is conventional but is also "focus the address bar" in some browsers, hence `preventDefault()`.
- Give `CommandDialog` a title for screen readers if your shadcn version supports it (it wraps a Dialog, so the title requirement from [02](./02-dialogs-and-modals.md) applies).

## Common mistakes

- **Leaving client filtering on for server results**, causing results to disappear.
- **No debounce** on async search → a request per keystroke.
- **Opening a second dialog before closing the palette.** Close first, then act.
- **Registering the key listener without cleanup**, stacking handlers on every render or hot reload.
- **Mounting the palette in many places.** One instance, one shortcut listener.
- **Unstable item `value`s** (array indexes) so selection jumps when results change.
- **Using `onSelect`'s argument as an ID** without checking how cmdk transforms it.
- **Giant static lists.** cmdk renders all items; thousands of rows need server search or virtualization.

## Quick summary

- shadcn `Command` = cmdk: filtering, keyboard nav, ARIA built in.
- `CommandDialog` + a global `Ctrl/Cmd+K` listener (with cleanup and `preventDefault`) = command palette.
- Use `keywords` to widen matches; `shouldFilter={false}` for server-side search with debounce.
- Combobox = Popover + Command.
- Model commands as data and close the palette before running an action.

## Next

[06 — Form controls](./06-form-controls.md)
