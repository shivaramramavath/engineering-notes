# Dialogs and Modals

A modal dialog interrupts the user to ask for focused input or confirmation. Getting one *right* means handling more than a centered `div` with a backdrop:

- Trap focus inside while open, restore it to the trigger on close
- Close on `Esc`
- Lock background scroll
- Make the rest of the page inert for assistive tech
- Render above everything (portal)
- Label it for screen readers

Radix's `Dialog` does all of this; shadcn styles it. You supply content and state.

```bash
npx shadcn@latest add dialog alert-dialog sheet
```

## Basic dialog

```tsx
import {
  Dialog, DialogContent, DialogDescription, DialogFooter,
  DialogHeader, DialogTitle, DialogTrigger, DialogClose,
} from "@/components/ui/dialog"
import { Button } from "@/components/ui/button"

export function InviteDialog() {
  return (
    <Dialog>
      <DialogTrigger asChild>
        <Button>Invite member</Button>
      </DialogTrigger>

      <DialogContent>
        <DialogHeader>
          <DialogTitle>Invite a teammate</DialogTitle>
          <DialogDescription>They'll get an email with a link to join.</DialogDescription>
        </DialogHeader>

        {/* body */}

        <DialogFooter>
          <DialogClose asChild>
            <Button variant="outline">Cancel</Button>
          </DialogClose>
          <Button>Send invite</Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  )
}
```

The parts are a **compound component** ([see 05-component-design](../05-component-design/02-compound-components.md)): `Dialog` owns the open state and shares it through context with `Trigger`, `Content`, and `Close`.

`DialogTitle` is required for accessibility — it becomes the dialog's accessible name. Radix logs a console warning if it's missing. If the design has no visible title, hide it visually (`className="sr-only"`) rather than omitting it.

## Controlled vs uncontrolled

**Uncontrolled** (above): Radix tracks `open` internally. Fine for simple triggers.

**Controlled**: you hold the state. Required when something *other than the trigger* opens or closes the dialog — after an async save, from a menu item, from a keyboard shortcut.

```tsx
function EditProfile() {
  const [open, setOpen] = useState(false)

  async function handleSave(values: ProfileValues) {
    await updateProfile(values)
    setOpen(false)           // close only on success
  }

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogTrigger asChild><Button>Edit</Button></DialogTrigger>
      <DialogContent>
        <DialogHeader><DialogTitle>Edit profile</DialogTitle></DialogHeader>
        <ProfileForm onSubmit={handleSave} />
      </DialogContent>
    </Dialog>
  )
}
```

`onOpenChange` fires for **every** close path: Esc, overlay click, the X button, `DialogClose`. Route them all through your state setter.

## What actually happens when it opens

```text
click trigger
   ↓
open = true
   ↓
Content mounts into a Portal at <body>
   ↓
Overlay rendered, scroll locked, outside content marked inert
   ↓
Focus moves into Content (first focusable, or what you specify)
   ↓
Tab / Shift+Tab cycle inside Content
   ↓
Esc / overlay click / Close → onOpenChange(false)
   ↓
Content unmounts, focus returns to the trigger
```

Two consequences you'll feel in real code:

1. **Content is unmounted when closed** (unless you pass `forceMount`). Local state inside the dialog body resets every time it opens. State held in the *parent* survives.
2. Because it's portaled, the dialog is **outside your component's DOM tree**. CSS selectors based on ancestors won't reach it, but React context and events still bubble through the React tree.

## Dialog with a form

```tsx
<Dialog open={open} onOpenChange={setOpen}>
  <DialogContent>
    <DialogHeader><DialogTitle>New project</DialogTitle></DialogHeader>
    <form onSubmit={handleSubmit(onSubmit)} className="space-y-4">
      {/* fields */}
      <DialogFooter>
        <Button type="submit" disabled={isSubmitting}>Create</Button>
      </DialogFooter>
    </form>
  </DialogContent>
</Dialog>
```

Put the `<form>` *inside* `DialogContent` and keep the submit button inside the form, so Enter submits and the button is a real `type="submit"`. Because the content unmounts on close, React Hook Form state created inside it resets for free. See [06-forms](../06-forms/README.md).

## Dialog vs AlertDialog

| | `Dialog` | `AlertDialog` |
|---|---|---|
| Use for | Forms, details, anything the user may dismiss | Confirmations that need an explicit decision |
| Overlay click closes | Yes | **No** |
| Esc closes | Yes | Yes (acts as Cancel) |
| Initial focus | First focusable | The Cancel button |
| ARIA role | `dialog` | `alertdialog` |

```tsx
<AlertDialog>
  <AlertDialogTrigger asChild>
    <Button variant="destructive">Delete project</Button>
  </AlertDialogTrigger>
  <AlertDialogContent>
    <AlertDialogHeader>
      <AlertDialogTitle>Delete this project?</AlertDialogTitle>
      <AlertDialogDescription>This can't be undone.</AlertDialogDescription>
    </AlertDialogHeader>
    <AlertDialogFooter>
      <AlertDialogCancel>Cancel</AlertDialogCancel>
      <AlertDialogAction onClick={handleDelete}>Delete</AlertDialogAction>
    </AlertDialogFooter>
  </AlertDialogContent>
</AlertDialog>
```

Focusing Cancel first is deliberate: pressing Enter by reflex shouldn't destroy data. Use AlertDialog for destructive or irreversible actions only; overusing confirmations trains users to click through them. For reversible actions, prefer doing it and offering **Undo** in a [toast](./08-toasts-and-notifications.md).

## Sheet (side panel)

`Sheet` is a Dialog that slides in from an edge. Same behavior, different layout:

```tsx
<Sheet>
  <SheetTrigger asChild><Button variant="outline">Filters</Button></SheetTrigger>
  <SheetContent side="right">
    <SheetHeader><SheetTitle>Filters</SheetTitle></SheetHeader>
    {/* controls */}
  </SheetContent>
</Sheet>
```

Good for mobile navigation, filter panels, and detail views where a centered modal feels heavy. shadcn's `Drawer` (built on Vaul) is the touch-friendly bottom-sheet variant.

## Opening a dialog from a menu item

The classic trap: putting `<Dialog>` *inside* a `<DropdownMenuItem>`. When the item is selected the menu closes and unmounts, taking the dialog with it. Lift the state **outside** the menu:

```tsx
function RowActions({ project }: { project: Project }) {
  const [deleteOpen, setDeleteOpen] = useState(false)

  return (
    <>
      <DropdownMenu>
        <DropdownMenuTrigger asChild>
          <Button variant="ghost" size="icon" aria-label="Row actions">⋯</Button>
        </DropdownMenuTrigger>
        <DropdownMenuContent>
          <DropdownMenuItem onSelect={() => setDeleteOpen(true)}>Delete</DropdownMenuItem>
        </DropdownMenuContent>
      </DropdownMenu>

      <AlertDialog open={deleteOpen} onOpenChange={setDeleteOpen}>
        {/* content */}
      </AlertDialog>
    </>
  )
}
```

The dialog is a sibling of the menu, so closing the menu doesn't unmount it. If you still see focus or pointer-events oddities after the menu closes, check Radix's issue tracker for your versions — this interaction has had version-specific bugs.

## Common mistakes

- **Missing `DialogTitle`** → console warning and an unnamed dialog for screen readers.
- **Dialog nested in a menu item** (see above).
- **Closing before an async action finishes**, then showing an error after the dialog is gone. Close in the success path.
- **Rendering the dialog per row in a long list.** Hundreds of closed dialogs are cheap (content isn't mounted), but hundreds of *controlled* ones with their own state add up. Lift one dialog above the list and pass it the selected item.
- **Modal for everything.** Inline editing, popovers, and expandable rows often beat a modal. A modal blocks the page; use it when you really need the user's attention.
- **Long content without scrolling.** Constrain `DialogContent` with `max-h-[85vh] overflow-y-auto` so small screens don't clip the footer.
- **Z-index fights.** Portaled content stacks by DOM order and z-index; if a toast or popover appears *behind* a dialog, check the stacking order of their portals.

## Quick summary

- Radix Dialog handles focus trap, `Esc`, scroll lock, inert background, and portal. Don't rebuild these.
- Always include `DialogTitle`; add `DialogDescription` when there's explanatory text.
- Go controlled (`open` + `onOpenChange`) when anything besides the trigger opens or closes it.
- Content unmounts on close, so internal state resets.
- `AlertDialog` for destructive confirmations; `Sheet` for side panels.
- Never nest a dialog inside a menu item — lift the state outside.

## Next

[03 — Dropdowns and menus](./03-dropdowns-and-menus.md)
