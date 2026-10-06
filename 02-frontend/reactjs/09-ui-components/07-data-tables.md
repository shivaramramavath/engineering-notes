# Data Tables

A data table looks simple until you need sorting, filtering, pagination, row selection, column visibility, and server-side data. **[TanStack Table](https://tanstack.com/table)** handles all of that logic, and renders **nothing**. It's headless: you get state and helpers, you write the markup. shadcn supplies the styled `<Table>` primitives to render into.

```bash
npm install @tanstack/react-table
npx shadcn@latest add table
```

```text
data + columns ──► useReactTable() ──► table instance (state + row models)
                                              │
                    you map header groups / rows / cells into <Table> markup
```

## Minimal table

```tsx
import {
  ColumnDef, flexRender, getCoreRowModel, useReactTable,
} from "@tanstack/react-table"
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table"

type User = { id: string; name: string; email: string; role: string }

const columns: ColumnDef<User>[] = [
  { accessorKey: "name", header: "Name" },
  { accessorKey: "email", header: "Email" },
  { accessorKey: "role", header: "Role" },
]

export function UsersTable({ data }: { data: User[] }) {
  const table = useReactTable({
    data,
    columns,
    getCoreRowModel: getCoreRowModel(),
    getRowId: (row) => row.id,
  })

  return (
    <Table>
      <TableHeader>
        {table.getHeaderGroups().map((hg) => (
          <TableRow key={hg.id}>
            {hg.headers.map((h) => (
              <TableHead key={h.id}>
                {h.isPlaceholder ? null : flexRender(h.column.columnDef.header, h.getContext())}
              </TableHead>
            ))}
          </TableRow>
        ))}
      </TableHeader>
      <TableBody>
        {table.getRowModel().rows.length ? (
          table.getRowModel().rows.map((row) => (
            <TableRow key={row.id}>
              {row.getVisibleCells().map((cell) => (
                <TableCell key={cell.id}>
                  {flexRender(cell.column.columnDef.cell, cell.getContext())}
                </TableCell>
              ))}
            </TableRow>
          ))
        ) : (
          <TableRow>
            <TableCell colSpan={columns.length} className="h-24 text-center">No results.</TableCell>
          </TableRow>
        )}
      </TableBody>
    </Table>
  )
}
```

- `columns` describes *what* each column is (accessor, header, cell renderer). `data` is the rows.
- `getCoreRowModel` is the required base. Features are opt-in by adding row models: `getSortedRowModel`, `getFilteredRowModel`, `getPaginationRowModel`.
- `flexRender` renders a header/cell definition that may be a string, a function, or a component.
- `getRowId` gives rows a stable identity (otherwise the index). That matters for selection and animations when data reorders.

## Stable references (the #1 gotcha)

`data` and `columns` must be **referentially stable** across renders. Creating them inline causes the table to see "new" data every render and can trigger infinite loops or reset state.

```tsx
// ✗ new array every render
useReactTable({ data: users.filter(Boolean), columns: [{ accessorKey: "name" }], ... })

// ✓ columns defined outside the component (or useMemo); data from state/query
const columns: ColumnDef<User>[] = [...]
const data = useMemo(() => users.filter(Boolean), [users])
```

Also avoid `data ?? []` inline for loading states: that creates a new empty array each render. Use a module-level `const EMPTY: User[] = []`.

## Custom cells

```tsx
const columns: ColumnDef<User>[] = [
  {
    accessorKey: "role",
    header: "Role",
    cell: ({ row }) => <Badge variant="secondary">{row.getValue<string>("role")}</Badge>,
  },
  {
    id: "actions",
    cell: ({ row }) => <RowActions user={row.original} />,
  },
]
```

`row.original` is the full data object. Columns without data (like actions) need an `id` instead of `accessorKey`.

## Sorting

```tsx
import { SortingState, getSortedRowModel } from "@tanstack/react-table"

const [sorting, setSorting] = useState<SortingState>([])

const table = useReactTable({
  data, columns,
  state: { sorting },
  onSortingChange: setSorting,
  getCoreRowModel: getCoreRowModel(),
  getSortedRowModel: getSortedRowModel(),
})
```

A sortable header:

```tsx
{
  accessorKey: "name",
  header: ({ column }) => (
    <Button
      variant="ghost"
      onClick={() => column.toggleSorting(column.getIsSorted() === "asc")}
    >
      Name <ArrowUpDown className="ml-2 h-4 w-4" />
    </Button>
  ),
}
```

Make the sort control a real `<button>` and expose the state to assistive tech with `aria-sort="ascending" | "descending"` on the `<th>`.

## Filtering

```tsx
const [columnFilters, setColumnFilters] = useState<ColumnFiltersState>([])

useReactTable({
  // ...
  state: { sorting, columnFilters },
  onColumnFiltersChange: setColumnFilters,
  getFilteredRowModel: getFilteredRowModel(),
})

<Input
  placeholder="Filter emails…"
  value={(table.getColumn("email")?.getFilterValue() as string) ?? ""}
  onChange={(e) => table.getColumn("email")?.setFilterValue(e.target.value)}
/>
```

For one search box over all columns use `globalFilter` and `onGlobalFilterChange`.

## Pagination

```tsx
useReactTable({
  // ...
  getPaginationRowModel: getPaginationRowModel(),
  initialState: { pagination: { pageSize: 10 } },
})

<Button onClick={() => table.previousPage()} disabled={!table.getCanPreviousPage()}>Previous</Button>
<Button onClick={() => table.nextPage()} disabled={!table.getCanNextPage()}>Next</Button>
<span>Page {table.getState().pagination.pageIndex + 1} of {table.getPageCount()}</span>
```

By default, changing the filter or data can reset the page index automatically (`autoResetPageIndex`). That's usually what you want when filtering; it can be surprising when you update data in place.

## Row selection

```tsx
const selectColumn: ColumnDef<User> = {
  id: "select",
  header: ({ table }) => (
    <Checkbox
      checked={table.getIsAllPageRowsSelected() || (table.getIsSomePageRowsSelected() && "indeterminate")}
      onCheckedChange={(v) => table.toggleAllPageRowsSelected(!!v)}
      aria-label="Select all"
    />
  ),
  cell: ({ row }) => (
    <Checkbox
      checked={row.getIsSelected()}
      onCheckedChange={(v) => row.toggleSelected(!!v)}
      aria-label="Select row"
    />
  ),
  enableSorting: false,
}
```

Enable with `state: { rowSelection }`, `onRowSelectionChange`, and read selected data via `table.getSelectedRowModel().rows`. With `getRowId` set, `rowSelection` is keyed by your IDs, so selection survives sorting and refetching. Without it, keys are indexes and the wrong rows get selected after data changes.

The select-all `Checkbox` is where the `"indeterminate"` state from [form controls](./06-form-controls.md) pays off.

## Column visibility

```tsx
const [columnVisibility, setColumnVisibility] = useState<VisibilityState>({})
// state: { columnVisibility }, onColumnVisibilityChange: setColumnVisibility

table.getAllColumns().filter((c) => c.getCanHide()).map((c) => (
  <DropdownMenuCheckboxItem
    key={c.id}
    checked={c.getIsVisible()}
    onCheckedChange={(v) => c.toggleVisibility(!!v)}
  >
    {c.id}
  </DropdownMenuCheckboxItem>
))
```

See [dropdowns and menus](./03-dropdowns-and-menus.md) for the checkbox-item pattern.

## Client-side vs server-side

Everything above processes data **in the browser**: fine up to a few thousand rows. Beyond that, the server should sort, filter, and paginate, and the table just displays what it's given. Tell TanStack Table not to do it itself:

```tsx
const [pagination, setPagination] = useState({ pageIndex: 0, pageSize: 20 })
const [sorting, setSorting] = useState<SortingState>([])

const { data, isPlaceholderData } = useQuery({
  queryKey: ["users", pagination, sorting],
  queryFn: () => fetchUsers({ ...pagination, sorting }),
  placeholderData: keepPreviousData,
})

const table = useReactTable({
  data: data?.rows ?? EMPTY,
  columns,
  rowCount: data?.total,              // total rows on the server
  state: { pagination, sorting },
  onPaginationChange: setPagination,
  onSortingChange: setSorting,
  manualPagination: true,
  manualSorting: true,
  getCoreRowModel: getCoreRowModel(),
})
```

- `manualPagination` / `manualSorting` / `manualFiltering` mean "I handle this; don't run the row model for it". You then **don't** add `getPaginationRowModel` etc.
- Provide the total via `rowCount` (or `pageCount` in older versions) so page controls know how many pages exist.
- Put the table state in the query key so [TanStack Query](../12-server-state/03-tanstack-query.md) refetches when it changes; `keepPreviousData` keeps old rows visible during the fetch instead of flashing empty. See [pagination](../12-server-state/07-pagination-and-infinite-queries.md).
- Consider syncing page/sort/filters to the URL ([search and URL state](../10-routing/06-search-filter-and-url-state.md)) so views are shareable.

## Loading, empty, and error states

Handle all four explicitly: loading (skeleton rows, not a blank table), empty ("No results" row, as above), error (message + retry), and refetching (dim the table with `isPlaceholderData` rather than replacing it). Layout shift when rows pop in is a common polish bug; fixed row heights and skeletons help.

## Large tables

Rendering thousands of DOM rows is slow regardless of how efficient your table logic is. Paginate, or [virtualize rows](../14-performance/04-virtualization.md) (TanStack Virtual pairs naturally with TanStack Table).

## Accessibility

- Use real table elements (shadcn's `Table` renders `<table>`, `<th>`, `<td>`), so screen readers get row/column navigation. Don't rebuild tables out of `div`s without ARIA grid roles.
- Sortable headers: a `<button>` inside `<th>`, plus `aria-sort`.
- Label selection checkboxes (`aria-label`), including *which* row when possible ("Select Ana Smith").
- Don't use a table for layout, and don't use a layout grid for tabular data.

## Common mistakes

- **Unstable `data`/`columns`** causing re-render loops or state resets.
- **No `getRowId`**, so selection/expansion sticks to row positions instead of records.
- **Adding `getPaginationRowModel` and `manualPagination` together** — pick one.
- **Forgetting to put table state in the query key** for server-side tables, so changing the page doesn't refetch.
- **Resetting to page 1 unexpectedly**, or *not* resetting after a filter change on server-side tables (you must do it yourself there).
- **Cell renderers defined inline with hooks inside** — cells are rendered via `flexRender`; keep hook-using logic in a proper component.
- **Client-side processing of huge datasets.**
- **Using `row.original` mutations** to edit data. Treat data as immutable and update the source.

## Quick summary

- TanStack Table is headless logic; shadcn `Table` is the markup. You connect them with `flexRender`.
- Define `columns` outside the component and keep `data` stable; set `getRowId`.
- Opt into features by adding row models (`sorted`, `filtered`, `pagination`) and controlling state with `state` + `on…Change`.
- For server-side tables use `manual*` flags, `rowCount`, and table state in your query key.
- Use real table semantics, label controls, and paginate or virtualize big data.

## Next

[08 — Toasts and notifications](./08-toasts-and-notifications.md)
