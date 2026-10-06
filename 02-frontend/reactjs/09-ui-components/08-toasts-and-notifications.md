# Toasts and Notifications

A toast is a small, temporary message that appears over the UI without blocking it: "Saved", "Link copied", "Failed to upload — Retry". shadcn's recommended toast is **[Sonner](https://sonner.emilkowal.ski/)**, which replaced shadcn's older Radix-based `toast` component.

```bash
npx shadcn@latest add sonner
```

## Setup

Render a single `<Toaster />` once, near the root:

```tsx
// App.tsx or main.tsx
import { Toaster } from "@/components/ui/sonner"

export default function App() {
  return (
    <>
      <RouterProvider router={router} />
      <Toaster />
    </>
  )
}
```

Then call `toast` from anywhere: components, event handlers, even outside React (query callbacks, API clients). No hook or context is required.

```tsx
import { toast } from "sonner"

toast("Event created")
toast.success("Profile saved")
toast.error("Could not save profile")
toast.info("Sync will run in the background")
toast.warning("Your session expires soon")
```

> shadcn's generated `sonner.tsx` reads the theme with `useTheme` from `next-themes`. That works in Next.js. In a Vite app without `next-themes`, replace it with your own theme source (see [theming and dark mode](../07-styling/04-theming-and-dark-mode.md)) or pass `theme` manually; otherwise toasts won't follow dark mode.

## Core patterns

### Description and actions

```tsx
toast("Message archived", {
  description: "It will be deleted after 30 days.",
  action: { label: "Undo", onClick: () => restoreMessage(id) },
})
```

An **undo toast** is often better than a confirm dialog for reversible actions: do it immediately, offer a way back. Reserve [AlertDialog](./02-dialogs-and-modals.md) for destructive actions that can't be undone.

### Promise toasts

```tsx
toast.promise(saveProject(values), {
  loading: "Saving project…",
  success: "Project saved",
  error: "Could not save project",
})
```

Sonner shows the loading state, then flips to success or error when the promise settles. Messages can be functions of the result/error for dynamic text.

### Updating a toast by id

```tsx
const id = toast.loading("Uploading…")
try {
  await upload(file)
  toast.success("Uploaded", { id })       // same id replaces the loading toast
} catch {
  toast.error("Upload failed", { id })
}
```

### Dismissing and options

```tsx
toast.dismiss(id)      // one
toast.dismiss()        // all

toast("Hello", { duration: 8000 })       // ms; use Infinity for sticky
```

Global defaults go on `<Toaster />`: `position="bottom-right"`, `richColors`, `closeButton`, `visibleToasts`, `duration`.

## Toasts with mutations

The usual home for toasts is the result of a server mutation ([TanStack Query mutations](../12-server-state/05-mutations.md)):

```tsx
const mutation = useMutation({
  mutationFn: updateProfile,
  onSuccess: () => toast.success("Profile updated"),
  onError: (err) => toast.error(getErrorMessage(err)),
})
```

If every mutation should report errors the same way, centralize it instead of repeating `onError`:

```tsx
const queryClient = new QueryClient({
  mutationCache: new MutationCache({
    onError: (error) => toast.error(getErrorMessage(error)),
  }),
})
```

Centralizing keeps messages consistent, but make sure errors a component already handles inline (like field validation errors) aren't also toasted. See [API error handling](../11-api-integration/05-api-error-handling.md).

## Choosing the right feedback

A toast is the wrong tool more often than people think.

| Situation | Use |
|---|---|
| Quick confirmation of a completed action ("Saved", "Copied") | **Toast** |
| Reversible action with undo | **Toast with action** |
| Form field validation errors | **Inline error** next to the field |
| Page-level status that should persist (offline, trial ending) | **Inline `Alert` / banner** |
| Error the user *must* act on to continue | **Inline alert or dialog** |
| Irreversible, destructive confirmation | **AlertDialog** |
| Background job finished while user was elsewhere | **Toast**, plus an inbox/notification list if it matters later |

Why the limits? Toasts disappear. A user who looks away misses them, and for anyone using a screen reader, magnifier, or who simply reads slowly, a short timer can make the message unreachable. **Never put information in a toast that the user can't recover elsewhere.**

## Accessibility

- Sonner renders toasts inside a live region so screen readers announce them without moving focus.
- Moving focus into a toast would interrupt the user, so don't. For anything requiring interaction, use a dialog or inline UI.
- Pause-on-hover and keyboard access to dismiss/action buttons are built in, but a hotkey to reach the toast region is not something to rely on; keep toast actions **optional**, like Undo, with another path to the same outcome.
- Don't convey status by color alone; use clear text (and an icon).
- Keep messages short and specific: "Couldn't save changes. Check your connection and try again", not "Error".

## Notification centers

If users need to review messages later (mentions, job completions, system alerts), a toast is only the *announcement*. The durable record lives in a notification list, usually a Popover or Sheet fed by server data. Don't try to make toasts persistent to fill that role.

## Common mistakes

- **Rendering `<Toaster />` multiple times** (or inside a route that remounts), causing duplicates or lost toasts.
- **Forgetting `<Toaster />` entirely.** `toast()` runs without error and nothing shows.
- **Toasting validation errors** instead of marking the fields.
- **Double reporting**: a component `onError` toast plus a global `MutationCache` toast for the same failure.
- **Vague messages** ("Something went wrong") with no next step.
- **Toast spam**: a toast per item in a bulk operation. Summarize ("12 items deleted").
- **Dark mode mismatch** because the generated wrapper depends on `next-themes`.
- **Relying on toasts for critical information** that disappears.
- **Starting a loading toast and never resolving it** — always settle by id or use `toast.promise`.

## Quick summary

- Sonner is shadcn's toast: one `<Toaster />` at the root, `toast()` anywhere.
- `toast.promise` and `toast.loading` + `id` cover async flows; `action` gives you Undo.
- Use toasts for brief, non-critical confirmations. Use inline alerts, field errors, or dialogs for anything the user must see or act on.
- Centralize mutation error toasts to stay consistent, without double reporting.
- Adapt the theme wiring when you're not on Next.js.

## Next

[09 — Design system](./09-design-system.md)
