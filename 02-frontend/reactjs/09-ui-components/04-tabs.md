# Tabs

Tabs let users switch between related views of the **same context** without leaving the page: a profile's "Overview / Activity / Settings", a code sample's "npm / pnpm / yarn".

```bash
npx shadcn@latest add tabs
```

## Basic usage

```tsx
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs"

export function ProjectTabs() {
  return (
    <Tabs defaultValue="overview">
      <TabsList>
        <TabsTrigger value="overview">Overview</TabsTrigger>
        <TabsTrigger value="activity">Activity</TabsTrigger>
        <TabsTrigger value="settings">Settings</TabsTrigger>
      </TabsList>

      <TabsContent value="overview">…</TabsContent>
      <TabsContent value="activity">…</TabsContent>
      <TabsContent value="settings">…</TabsContent>
    </Tabs>
  )
}
```

Each `TabsTrigger` is linked to the `TabsContent` with the same `value`. Radix handles the ARIA wiring (`role="tablist"`, `tab`, `tabpanel`, `aria-selected`, `aria-controls`) and keyboard behavior:

- `←` / `→` move between tabs (`↑` / `↓` when `orientation="vertical"`)
- `Home` / `End` jump to first/last
- `Tab` moves focus **into the active panel**, not to the next tab — only one tab is in the tab order at a time

## Controlled tabs

```tsx
const [tab, setTab] = useState("overview")

<Tabs value={tab} onValueChange={setTab}>…</Tabs>
```

Use controlled mode when something other than a click needs to switch tabs (a "View activity" link elsewhere on the page, restoring state from a URL).

## Keeping the active tab in the URL

If a user refreshes, shares a link, or hits Back, they usually expect the same tab. That's [URL state](../10-routing/06-search-filter-and-url-state.md):

```tsx
import { useSearchParams } from "react-router"

const TABS = ["overview", "activity", "settings"] as const
type TabValue = (typeof TABS)[number]

export function ProjectTabs() {
  const [params, setParams] = useSearchParams()
  const raw = params.get("tab")
  const tab: TabValue = TABS.includes(raw as TabValue) ? (raw as TabValue) : "overview"

  return (
    <Tabs value={tab} onValueChange={(v) => setParams({ tab: v }, { replace: true })}>
      {/* … */}
    </Tabs>
  )
}
```

Validate the param — users can type anything into a URL. `replace: true` avoids filling the history stack with every tab click; drop it if you want Back to step through tabs.

## Tabs vs routes

Both look like tabs. They're different tools:

| | `Tabs` component | Route-based tabs (`<NavLink>`) |
|---|---|---|
| Switching | Client state, same route | Navigates to a different URL |
| Deep-linkable | Only if you sync to URL | Yes, naturally |
| Code splitting per tab | Manual | Automatic with route lazy loading |
| Data loading per tab | Manual | Route loaders |
| Semantics | `tablist` (a widget) | Navigation links |

Rule of thumb: **if each tab is a page-like view with its own data and URL, use routes** ([nested routes](../10-routing/01-nested-routes-and-layouts.md)) and style the links like tabs. If it's a small view toggle in the same context, use `Tabs`.

## Panels are unmounted when inactive

By default Radix only renders the **active** panel's content. Switching tabs unmounts the old panel, so its local state (scroll position, form input, expanded rows) is lost.

Options when that hurts:

```tsx
// Keep it mounted, hide with CSS
<TabsContent value="settings" forceMount className="data-[state=inactive]:hidden">
  <SettingsForm />
</TabsContent>
```

or lift the state into the parent / a store so it survives remounting. Prefer lifting state; `forceMount` keeps everything alive, including expensive effects and subscriptions.

The flip side is a benefit: inactive panels that are **not** mounted don't run effects or fetch. A data-heavy tab only loads when first opened.

## Automatic vs manual activation

By default, arrowing to a tab activates it immediately. If activating is expensive (it triggers a fetch or heavy render), switch to manual so users arrow to a tab and press `Enter`/`Space` to commit:

```tsx
<Tabs defaultValue="overview" activationMode="manual">
```

## Styling the active state

Radix exposes state as data attributes. Style with Tailwind variants:

```tsx
<TabsTrigger className="data-[state=active]:bg-background data-[state=active]:shadow">
```

Don't track "active" in your own state just for styling — read the attribute.

## Common mistakes

- **Tabs for sequential steps.** A wizard needs "Next/Back" and validation between steps, not free switching. See [multi-step forms](../06-forms/04-multi-step-forms.md).
- **Tabs as primary site navigation.** That's a nav bar with links; use routes.
- **Mismatched `value`s.** A trigger and content whose values differ silently show nothing.
- **Losing form state on switch** because panels unmount. Lift state or use `forceMount` deliberately.
- **Unvalidated URL param** driving `value`, leaving no panel visible for `?tab=banana`.
- **Too many tabs.** Past five or six, or on narrow screens, they overflow. Consider a Select or a vertical tab list.
- **Putting a heading structure inside the TabsList** — it should contain only triggers.

## Quick summary

- `Tabs` → `TabsList` → `TabsTrigger` + `TabsContent`, linked by `value`.
- Radix provides ARIA roles and arrow-key navigation; only the active tab is in the Tab order.
- Controlled `value` / `onValueChange` when other UI or the URL must drive it.
- Sync to search params for shareable, refresh-safe tabs, and validate the value.
- Page-like views with their own data → routes. Small same-context toggles → `Tabs`.
- Inactive panels unmount by default; lift state or `forceMount` if you need persistence.

## Next

[05 — Command palette](./05-command-palette.md)
