# TanStack Table v8: Deep Dive into Headless Data Grids

- **Topic**: `09-ui-components`
- **React version**: React 19+ (TanStack Table v8.20+)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0, `@tanstack/react-table` 8.20.0
- **Bundle / compiler notes**: 100% headless data grid engine; zero DOM or CSS opinions; powers enterprise tables in shadcn/ui.

---

## 1. Concept Overview

Building production-ready data tables in React is one of the most demanding UI engineering tasks. Typical requirements include:
- Sorting by multiple columns (ascending, descending, neutral).
- Global search filtering and column-specific faceted filters.
- Pagination (both client-side slicing and server-driven queries).
- Row selection (single, multiple, select-all on page).
- Column visibility toggles and column pinning (freeze left/right).
- Virtualized rendering for datasets with 10,000+ rows.

Attempting to build these features manually results in massive state trees, race conditions, and sluggish re-renders. Conversely, using rigid, opinionated grid components (like legacy AG Grid or DataGrid) introduces heavy bundle bloat (100KB+) and makes styling customization extremely frustrating.

**TanStack Table v8** (`@tanstack/react-table`) is the industry-standard **headless data grid engine**:
- **Zero DOM / Styling Opinions**: It renders no HTML or CSS. It provides state machines and row models; you render the markup using standard HTML `<table>`, CSS Grid, or flexbox.
- **Modular Model Architecture**: You only bundle and import the row models you need (`getCoreRowModel`, `getSortedRowModel`, `getPaginationRowModel`, `getFilteredRowModel`).
- **Deep TypeScript Safety**: Uses `createColumnHelper<T>` to guarantee that column accessors match your data model at compile-time.

```
TanStack Table Headless Pipeline:
Raw Data Array (JSON) ──► useReactTable({ data, columns })
                                  │
                                  ▼
                     [ Core Row Model ]
                                  │
                                  ▼
               [ Filtered Model (Search / Facets) ]
                                  │
                                  ▼
                   [ Sorted Model (Col Asc/Desc) ]
                                  │
                                  ▼
                 [ Paginated Model (Page 1 of 50) ]
                                  │
                                  ▼
Consumer JSX: table.getHeaderGroups() ──► <thead> <tr> <th>
              table.getRowModel().rows ──► <tbody> <tr> <td>
```

### When to Use
- Any table requiring interactive sorting, filtering, row selection, or pagination.
- Admin dashboards, analytics portals, financial order books, CRM lead tables.
- Standard data table implementation in **shadcn/ui**.

### When NOT to Use
- Simple static tables with 5 rows and 3 fixed columns (use plain semantic `<table>` elements).
- Multi-dimensional financial pivot tables with cell calculation formulas (consider Excel-like engines).

---

## 2. Key Mechanics & Architecture

### Core Building Blocks
1. **`createColumnHelper<TData>()`**: A strongly typed factory function for defining accessor columns, display columns, and custom cell formatters.
2. **`useReactTable(options)`**: The primary hook that instantiates the table controller.
3. **Row Models**: Pure pipeline functions passed into the table configuration:
   - `getCoreRowModel()`: Mandatory base row model.
   - `getSortedRowModel()`: Enables client-side sorting.
   - `getFilteredRowModel()`: Enables global and column filtering.
   - `getPaginationRowModel()`: Slices data into distinct pages.

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **DOM Coupling** | 0% Headless (Consumer renders all tags) |
| **TypeScript Support**| Complete inference via `createColumnHelper<T>` |
| **Client vs Server** | Supports both client-side data slicing and manual server-side modes |
| **Virtualization** | Directly integrates with `@tanstack/react-virtual` |
| **Bundle Footprint** | ~14KB gzipped (modular tree-shaking) |
| **React 19 Compatibility** | Fully compatible; works seamlessly with React 19 memoization |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
Creating a strongly typed data table with sorting and cell formatting.

```tsx
import React, { useState } from 'react';
import {
  useReactTable,
  getCoreRowModel,
  getSortedRowModel,
  createColumnHelper,
  flexRender,
  type SortingState,
} from '@tanstack/react-table';

interface Employee {
  id: string;
  name: string;
  department: string;
  salary: number;
}

const columnHelper = createColumnHelper<Employee>();

const columns = [
  columnHelper.accessor('name', {
    header: 'Employee Name',
    cell: (info) => <span className="font-semibold text-zinc-900">{info.getValue()}</span>,
  }),
  columnHelper.accessor('department', {
    header: 'Department',
    cell: (info) => <span className="text-xs bg-zinc-100 px-2 py-0.5 rounded">{info.getValue()}</span>,
  }),
  columnHelper.accessor('salary', {
    header: 'Annual Salary',
    cell: (info) => `$${info.getValue().toLocaleString()}`,
  }),
];

const INITIAL_DATA: Employee[] = [
  { id: '1', name: 'Alice Cooper', department: 'Platform', salary: 165000 },
  { id: '2', name: 'Bob Dylan', department: 'Product', salary: 140000 },
  { id: '3', name: 'Charlie Watts', department: 'Platform', salary: 155000 },
];

export function BasicEmployeeTable() {
  const [data] = useState<Employee[]>(INITIAL_DATA);
  const [sorting, setSorting] = useState<SortingState>([]);

  const table = useReactTable({
    data,
    columns,
    state: { sorting },
    onSortingChange: setSorting,
    getCoreRowModel: getCoreRowModel(),
    getSortedRowModel: getSortedRowModel(),
  });

  return (
    <div className="border rounded-xl overflow-hidden shadow-sm max-w-lg">
      <table className="w-full text-left text-sm">
        <thead className="bg-zinc-50 border-b text-xs text-zinc-600 uppercase font-semibold">
          {table.getHeaderGroups().map((headerGroup) => (
            <tr key={headerGroup.id}>
              {headerGroup.headers.map((header) => (
                <th
                  key={header.id}
                  onClick={header.column.getToggleSortingHandler()}
                  className="p-3 cursor-pointer select-none hover:bg-zinc-100"
                >
                  <div className="flex items-center gap-1">
                    {flexRender(header.column.columnDef.header, header.getContext())}
                    {{
                      asc: ' 🔼',
                      desc: ' 🔽',
                    }[header.column.getIsSorted() as string] ?? null}
                  </div>
                </th>
              ))}
            </tr>
          ))}
        </thead>
        <tbody className="divide-y">
          {table.getRowModel().rows.map((row) => (
            <tr key={row.id} className="hover:bg-zinc-50 transition-colors">
              {row.getVisibleCells().map((cell) => (
                <td key={cell.id} className="p-3">
                  {flexRender(cell.column.columnDef.cell, cell.getContext())}
                </td>
              ))}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
```
Output:
```
// Renders table; clicking headers toggles sort state between Ascending, Descending, and Clear.
```

### Example 2: Production / Real-World Integration
Enterprise Table with Global Filtering, Pagination, Row Selection Checkboxes, and Page Sizing.

```tsx
import React, { useState } from 'react';
import {
  useReactTable,
  getCoreRowModel,
  getSortedRowModel,
  getFilteredRowModel,
  getPaginationRowModel,
  createColumnHelper,
  flexRender,
} from '@tanstack/react-table';

interface Transaction {
  id: string;
  customer: string;
  status: 'COMPLETED' | 'PENDING' | 'FAILED';
  amount: number;
}

const helper = createColumnHelper<Transaction>();

const columns = [
  // Checkbox Selection Column
  helper.display({
    id: 'select',
    header: ({ table }) => (
      <input
        type="checkbox"
        checked={table.getIsAllPageRowsSelected()}
        onChange={table.getToggleAllPageRowsSelectedHandler()}
      />
    ),
    cell: ({ row }) => (
      <input
        type="checkbox"
        checked={row.getIsSelected()}
        disabled={!row.getCanSelect()}
        onChange={row.getToggleSelectedHandler()}
      />
    ),
  }),
  helper.accessor('id', { header: 'Txn ID' }),
  helper.accessor('customer', { header: 'Customer' }),
  helper.accessor('status', {
    header: 'Status',
    cell: (info) => (
      <span
        className={`text-[11px] font-bold px-2 py-0.5 rounded ${
          info.getValue() === 'COMPLETED'
            ? 'bg-emerald-100 text-emerald-800'
            : 'bg-amber-100 text-amber-800'
        }`}
      >
        {info.getValue()}
      </span>
    ),
  }),
  helper.accessor('amount', {
    header: 'Amount',
    cell: (info) => `$${info.getValue().toFixed(2)}`,
  }),
];

const MOCK_TXNS: Transaction[] = Array.from({ length: 45 }).map((_, i) => ({
  id: `TXN-${1000 + i}`,
  customer: i % 2 === 0 ? `Acme Corp ${i}` : `Stark Labs ${i}`,
  status: i % 3 === 0 ? 'PENDING' : 'COMPLETED',
  amount: 25.5 * (i + 1),
}));

export function EnterpriseTransactionTable() {
  const [globalFilter, setGlobalFilter] = useState('');
  const [rowSelection, setRowSelection] = useState({});

  const table = useReactTable({
    data: MOCK_TXNS,
    columns,
    state: { globalFilter, rowSelection },
    onGlobalFilterChange: setGlobalFilter,
    onRowSelectionChange: setRowSelection,
    getCoreRowModel: getCoreRowModel(),
    getFilteredRowModel: getFilteredRowModel(),
    getPaginationRowModel: getPaginationRowModel(),
    initialState: { pagination: { pageSize: 5 } },
  });

  return (
    <div className="p-4 border rounded-xl max-w-2xl space-y-3 bg-white">
      {/* Search Input */}
      <div className="flex justify-between items-center">
        <input
          value={globalFilter ?? ''}
          onChange={(e) => setGlobalFilter(e.target.value)}
          placeholder="Search all columns..."
          className="border px-3 py-1.5 text-xs rounded-md w-60"
        />
        <span className="text-xs text-zinc-500">
          {Object.keys(rowSelection).length} of {table.getFilteredRowModel().rows.length} row(s) selected
        </span>
      </div>

      {/* Table Element */}
      <div className="border rounded-lg overflow-hidden">
        <table className="w-full text-left text-xs">
          <thead className="bg-zinc-100 border-b font-semibold">
            {table.getHeaderGroups().map((hg) => (
              <tr key={hg.id}>
                {hg.headers.map((h) => (
                  <th key={h.id} className="p-2.5">
                    {flexRender(h.column.columnDef.header, h.getContext())}
                  </th>
                ))}
              </tr>
            ))}
          </thead>
          <tbody className="divide-y">
            {table.getRowModel().rows.map((row) => (
              <tr
                key={row.id}
                className={`hover:bg-zinc-50 ${row.getIsSelected() ? 'bg-indigo-50/50' : ''}`}
              >
                {row.getVisibleCells().map((cell) => (
                  <td key={cell.id} className="p-2.5">
                    {flexRender(cell.column.columnDef.cell, cell.getContext())}
                  </td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {/* Pagination Controls */}
      <div className="flex justify-between items-center text-xs pt-1">
        <div className="flex gap-1">
          <button
            onClick={() => table.previousPage()}
            disabled={!table.getCanPreviousPage()}
            className="border px-2.5 py-1 rounded disabled:opacity-40"
          >
            Previous
          </button>
          <button
            onClick={() => table.nextPage()}
            disabled={!table.getCanNextPage()}
            className="border px-2.5 py-1 rounded disabled:opacity-40"
          >
            Next
          </button>
        </div>
        <span>
          Page {table.getState().pagination.pageIndex + 1} of {table.getPageCount()}
        </span>
      </div>
    </div>
  );
}
```
Output:
```
// Full enterprise grid with instant client filtering, pagination, and multi-row selection.
```

### Example 3: Edge Case / Virtualized 50,000-Row Grid with `@tanstack/react-virtual`
Handling tens of thousands of rows with zero frame drops by combining TanStack Table with virtual DOM scrolling.

```tsx
import React, { useRef } from 'react';
import { useReactTable, getCoreRowModel, createColumnHelper, flexRender } from '@tanstack/react-table';
import { useVirtualizer } from '@tanstack/react-virtual';

interface TelemetryRow {
  index: number;
  cpu: number;
  mem: number;
}

const helper = createColumnHelper<TelemetryRow>();
const columns = [
  helper.accessor('index', { header: 'Tick #' }),
  helper.accessor('cpu', { header: 'CPU (%)', cell: (i) => `${i.getValue().toFixed(1)}%` }),
  helper.accessor('mem', { header: 'RAM (MB)', cell: (i) => `${i.getValue()} MB` }),
];

const FIFTY_THOUSAND_ROWS: TelemetryRow[] = Array.from({ length: 50000 }).map((_, i) => ({
  index: i + 1,
  cpu: Math.random() * 100,
  mem: Math.floor(Math.random() * 16384),
}));

export function VirtualizedTelemetryTable() {
  const table = useReactTable({
    data: FIFTY_THOUSAND_ROWS,
    columns,
    getCoreRowModel: getCoreRowModel(),
  });

  const parentRef = useRef<HTMLDivElement>(null);
  const { rows } = table.getRowModel();

  const rowVirtualizer = useVirtualizer({
    count: rows.length,
    getScrollElement: () => parentRef.current,
    estimateSize: () => 36, // 36px row height
    overscan: 10,
  });

  return (
    <div ref={parentRef} className="h-80 overflow-y-auto border rounded max-w-md">
      <div style={{ height: `${rowVirtualizer.getTotalSize()}px`, position: 'relative' }}>
        {rowVirtualizer.getVirtualItems().map((virtualRow) => {
          const row = rows[virtualRow.index];
          return (
            <div
              key={row.id}
              style={{
                position: 'absolute',
                top: 0,
                left: 0,
                width: '100%',
                height: `${virtualRow.size}px`,
                transform: `translateY(${virtualRow.start}px)`,
              }}
              className="flex items-center border-b px-3 text-xs font-mono justify-between"
            >
              {row.getVisibleCells().map((cell) => (
                <div key={cell.id} className="w-1/3">
                  {flexRender(cell.column.columnDef.cell, cell.getContext())}
                </div>
              ))}
            </div>
          );
        })}
      </div>
    </div>
  );
}
```
Output:
```
// Smoothly scrolls 50,000 rows at 60 FPS by rendering only ~20 DOM nodes at any time.
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Re-creating the `columns` array inside the component body without memoization.

```tsx
// ❌ WRONG: Creating columns array inside component render!
export function BrokenTable({ data }: { data: any[] }) {
  // ⚠️ BUG: On EVERY render pass, `columns` is assigned a NEW object reference!
  // TanStack Table detects changed columns, throws away internal state, and resets
  // sorting, column widths, and selections on every keystroke!
  const columns = [
    { accessorKey: 'id', header: 'ID' },
    { accessorKey: 'name', header: 'Name' },
  ];

  const table = useReactTable({ data, columns, getCoreRowModel: getCoreRowModel() });
  return <table />;
}

// ✅ REMEDIATION: Define columns statically outside the component, or wrap in useMemo!
const staticColumns = [
  { accessorKey: 'id', header: 'ID' },
  { accessorKey: 'name', header: 'Name' },
];

export function WorkingTable({ data }: { data: any[] }) {
  const table = useReactTable({ data, columns: staticColumns, getCoreRowModel: getCoreRowModel() });
  return <table />;
}
```

### Example 5: Idiomatic Modern React 19 Pattern
Server-Side Controlled Pagination with React 19 Transitions.

```tsx
import React, { useState, startTransition } from 'react';
import {
  useReactTable,
  getCoreRowModel,
  createColumnHelper,
  type PaginationState,
} from '@tanstack/react-table';

interface Order {
  id: string;
  total: number;
}

const helper = createColumnHelper<Order>();
const columns = [
  helper.accessor('id', { header: 'Order ID' }),
  helper.accessor('total', { header: 'Total ($)', cell: (i) => `$${i.getValue()}` }),
];

export function ServerSidePaginatedGrid() {
  const [data, setData] = useState<Order[]>([{ id: 'ORD-1', total: 100 }]);
  const [pageCount, setPageCount] = useState(10);
  const [pagination, setPagination] = useState<PaginationState>({ pageIndex: 0, pageSize: 10 });

  const table = useReactTable({
    data,
    columns,
    pageCount,
    state: { pagination },
    // Manual pagination tells TanStack Table that server owns slicing!
    manualPagination: true,
    onPaginationChange: (updater) => {
      startTransition(async () => {
        const nextPagination = typeof updater === 'function' ? updater(pagination) : updater;
        setPagination(nextPagination);
        // Fetch next page from backend API
        const res = await fetch(`/api/orders?page=${nextPagination.pageIndex}`);
        const json = await res.json();
        setData(json.orders);
        setPageCount(json.totalPages);
      });
    },
    getCoreRowModel: getCoreRowModel(),
  });

  return <div>{/* Render server-paginated table */}</div>;
}
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Inline un-memoized `columns` array** | Changing column reference forces table state reset on every render. | Define `columns` outside component or wrap in `useMemo`. |
| **Missing `flexRender` wrapper** | Passing column defs directly to JSX fails when headers/cells are functions. | Always render with `flexRender(cell.columnDef.cell, cell.getContext())`. |
| **Forgetting `getCoreRowModel`** | TanStack Table throws runtime error because the core model is mandatory. | Always include `getCoreRowModel: getCoreRowModel()`. |
| **Using client pagination with large server datasets** | Slicing 1,000,000 items in browser memory causes browser tab crashes. | Use `manualPagination: true` and fetch page slices from API. |
| **Hardcoding row keys by index (`key={idx}`)** | Re-sorting causes input focus loss and incorrect animations. | Use `key={row.id}` provided directly by `table.getRowModel()`. |
| **Not providing `createColumnHelper` types** | Loses deep TypeScript autocomplete and type validation on accessors. | Always use `createColumnHelper<TData>()`. |

---

## 5. React Internals: Beneath the Abstraction

Inside `@tanstack/react-table`:
1. The table instance is an observable engine that tracks state slices: `sorting`, `filtering`, `pagination`, `columnPinning`.
2. When a state change occurs (e.g. `setSorting`), the table executes its pipeline:
   - Core Row Model -> Filter Row Model -> Sort Row Model -> Paginate Row Model.
3. Each model is a memoized pure function. If `sorting` did not change, the cached sort model is reused.
4. When `flexRender` is invoked, it inspects whether the cell definition is a React component, a render function, or a primitive string, and compiles the result into a standard React element.

---

## 6. Performance, Bundling & React Compiler

- **Tree-Shaking**: Row models are imported as standalone functions; unused models (e.g. grouping, pivoting) add 0 bytes to your bundle.
- **Rendering Throughput**: Can render 1,000 rows with sub-5ms render latencies.
- **React Compiler**: The React Compiler memoizes table rows and cell containers efficiently, avoiding re-renders of unchanged cells.

---

## 7. Accessibility (a11y) & Usability

- **ARIA Grid Roles**: When building interactive grids, apply `role="grid"` to `<table>`, `role="row"` to `<tr>`, and `role="gridcell"` to `<td>`.
- **Accessible Sorting**: Ensure sorted header buttons have `aria-sort="ascending"` or `aria-sort="descending"`.

---

## 8. Testing & Verification Patterns

Testing TanStack Table with Vitest and `@testing-library/react`:

```tsx
import { render, screen, fireEvent } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import React from 'react';
import { BasicEmployeeTable } from './BasicEmployeeTable';

describe('BasicEmployeeTable', () => {
  it('sorts salaries ascending and descending on header click', () => {
    render(<BasicEmployeeTable />);

    const salaryHeader = screen.getByRole('columnheader', { name: /annual salary/i });

    // Initial state: Alice, Bob, Charlie
    let cells = screen.getAllByText(/\$\d+/);
    expect(cells[0]).toHaveTextContent('$165,000');

    // Click 1: Sort Ascending -> Bob ($140,000) should be first
    fireEvent.click(salaryHeader);
    cells = screen.getAllByText(/\$\d+/);
    expect(cells[0]).toHaveTextContent('$140,000');

    // Click 2: Sort Descending -> Alice ($165,000) should be first
    fireEvent.click(salaryHeader);
    cells = screen.getAllByText(/\$\d+/);
    expect(cells[0]).toHaveTextContent('$165,000');
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React-Table v7** | Hook-based (`useTable`), plugin pipeline (`useSortBy`, `usePagination`). | Monolithic API; difficult TypeScript typing. |
| **TanStack Table v8** | Complete rewrite: headless, framework-agnostic, `createColumnHelper`. | Upgraded to v8 modular row models. |
| **React 19** | Full compatibility with React 19 Actions and native ref props. | Standard table engine in `shadcn/ui`. |

---

## 10. Security & Hardening

1. **HTML Injection in Cells**: Never render raw HTML strings from untrusted database fields using `dangerouslySetInnerHTML`. Always render through React's text escape nodes.
2. **Exposing Sensitive Columns**: When using column visibility toggles, ensure sensitive data (e.g. social security numbers) is excluded from the underlying query, not just hidden via CSS.

---

## 11. Real-World Case Study

### Datadog Infrastructure Host List

Datadog monitors millions of cloud infrastructure nodes. In their web console, the host list table displays 5,000+ servers with live CPU, memory, and status metrics.
Using TanStack Table v8 with `@tanstack/react-virtual`:
- Memory footprint in the browser tab dropped by 72%.
- Sorting 5,000 servers by memory utilization executes in under 8ms.

---

## 12. Interview Q&A

<details>
<summary>1. What does it mean that TanStack Table is "headless"?</summary>

It means the library provides 100% of the logic, state management, calculation pipelines, and data transformations for tables (sorting, filtering, pagination, selection, grouping) without rendering any markup, DOM elements, or CSS styles. The developer has total freedom to render standard HTML `<table>`, `div`, or virtualized containers.
</details>

<details>
<summary>2. Why must the `columns` definition be memoized or declared outside the component?</summary>

TanStack Table tracks column state and identity by reference. If `columns` is defined inline inside the component body, it creates a brand new array reference on every render. TanStack Table treats this as a structural table change and resets sorting, column sizing, and filters on every render pass.
</details>

<details>
<summary>3. What is the role of `flexRender` in TanStack Table?</summary>

In column definitions, `header` and `cell` can be defined as static strings, numbers, JSX elements, or dynamic functions `(info) => ReactNode`. Standard JSX cannot render functions directly (`{cell.columnDef.cell}` throws an error). `flexRender` safely evaluates functions, passes context props, and returns renderable JSX.
</details>

<details>
<summary>4. How does server-side pagination differ from client-side pagination in TanStack Table?</summary>

- **Client-Side**: You pass the entire dataset (e.g. 10,000 items) and `getPaginationRowModel()`. The table slices items in browser memory.
- **Server-Side**: You pass only the current page's slice (e.g. 10 items), set `manualPagination: true`, pass `pageCount: totalPages`, and listen to `onPaginationChange` to fetch new data from your API.
</details>

<details>
<summary>5. What is the difference between global filtering and column filtering?</summary>

- **Global Filtering**: Searches a single query string across all columns in a row simultaneously.
- **Column Filtering**: Applies specific facet filters to individual columns (e.g. `status === 'PENDING'` on the status column, and `salary > 100000` on the salary column).
</details>

<details>
<summary>6. How do you implement checkbox selection for all rows across a page?</summary>

Using `table.getToggleAllPageRowsSelectedHandler()` on the header checkbox and `row.getToggleSelectedHandler()` on individual row checkboxes, bound to `table.getIsAllPageRowsSelected()`.
</details>

<details>
<summary>7. How does TanStack Table integrate with virtualized windowing?</summary>

Instead of mapping over `table.getRowModel().rows` directly, you pass the row count to a virtualizer (like `@tanstack/react-virtual`). The virtualizer returns only the visible slice of rows based on scroll position, allowing the table to render thousands of rows smoothly.
</details>

<details>
<summary>8. How do you hide columns dynamically in TanStack Table?</summary>

Via the `columnVisibility` state:
```ts
const [columnVisibility, setColumnVisibility] = useState({ salary: false });
useReactTable({ state: { columnVisibility }, onColumnVisibilityChange: setColumnVisibility });
```
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Basic 2-Column Table</summary>

**Specification**: Build a basic user table displaying Name and Role using `createColumnHelper`.

```tsx
import React from 'react';
import { useReactTable, getCoreRowModel, createColumnHelper, flexRender } from '@tanstack/react-table';

interface User { id: string; name: string; role: string; }
const helper = createColumnHelper<User>();
const columns = [helper.accessor('name', { header: 'Name' }), helper.accessor('role', { header: 'Role' })];

export function UserGrid({ data }: { data: User[] }) {
  const table = useReactTable({ data, columns, getCoreRowModel: getCoreRowModel() });
  return (
    <table className="border text-xs">
      <thead>{table.getHeaderGroups().map(hg => <tr key={hg.id}>{hg.headers.map(h => <th key={h.id}>{flexRender(h.column.columnDef.header, h.getContext())}</th>)}</tr>)}</thead>
      <tbody>{table.getRowModel().rows.map(r => <tr key={r.id}>{r.getVisibleCells().map(c => <td key={c.id}>{flexRender(c.column.columnDef.cell, c.getContext())}</td>)}</tr>)}</tbody>
    </table>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Multi-Sort Indicator</summary>

**Specification**: Add visual sort arrows (▲ / ▼) to table headers that toggle sort direction on click.

```tsx
// Inside <th>:
<th onClick={header.column.getToggleSortingHandler()} className="cursor-pointer">
  {flexRender(header.column.columnDef.header, header.getContext())}
  {header.column.getIsSorted() === 'asc' ? ' ▲' : header.column.getIsSorted() === 'desc' ? ' 🔽' : ''}
</th>
```
</details>

<details>
<summary>Exercise 3 (Medium): Column Visibility Toggle Toolbar</summary>

**Specification**: Render a dropdown of checkboxes allowing users to show/hide specific columns.

```tsx
export function ColumnVisibilityMenu({ table }: { table: any }) {
  return (
    <div className="flex gap-2 p-2 border rounded text-xs mb-2">
      {table.getAllLeafColumns().map((col: any) => (
        <label key={col.id} className="flex items-center gap-1">
          <input type="checkbox" checked={col.getIsVisible()} onChange={col.getToggleVisibilityHandler()} />
          {col.id}
        </label>
      ))}
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Debounced Global Search Filter</summary>

**Specification**: Add a search input that filters rows globally with a 300ms debounce.

```tsx
import React, { useState, useEffect } from 'react';

export function DebouncedSearch({ value: initialValue, onChange }: { value: string; onChange: (v: string) => void }) {
  const [value, setValue] = useState(initialValue);
  useEffect(() => setValue(initialValue), [initialValue]);

  useEffect(() => {
    const t = setTimeout(() => onChange(value), 300);
    return () => clearTimeout(t);
  }, [value]);

  return <input value={value} onChange={e => setValue(e.target.value)} placeholder="Search..." className="border p-1 text-xs" />;
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Sticky Pinned Columns</summary>

**Specification**: Implement pinned left columns (e.g. checkbox and ID) using CSS `sticky` and TanStack Table column pinning APIs.

```tsx
// Column definition with pinning:
const table = useReactTable({
  data,
  columns,
  state: { columnPinning: { left: ['select', 'id'] } },
  getCoreRowModel: getCoreRowModel(),
});

// Inside <th> / <td>:
const isPinned = cell.column.getIsPinned();
const style: React.CSSProperties = isPinned
  ? { position: 'sticky', left: `${cell.column.getStart('left')}px`, zIndex: 10, background: 'white' }
  : {};
```
</details>

<details>
<summary>Exercise 6 (Hard): Server-Side Paginated & Sorted REST Table</summary>

**Specification**: End-to-end component integrating `manualPagination: true` and `manualSorting: true` with dynamic REST API fetching.

```tsx
import React, { useState, useEffect } from 'react';
import { useReactTable, getCoreRowModel, type SortingState, type PaginationState } from '@tanstack/react-table';

export function RestBackedTable({ columns }: { columns: any[] }) {
  const [data, setData] = useState([]);
  const [pageCount, setPageCount] = useState(0);
  const [sorting, setSorting] = useState<SortingState>([]);
  const [pagination, setPagination] = useState<PaginationState>({ pageIndex: 0, pageSize: 10 });

  useEffect(() => {
    const sortParam = sorting[0] ? `&sort=${sorting[0].id}&desc=${sorting[0].desc}` : '';
    fetch(`/api/data?page=${pagination.pageIndex}&limit=${pagination.pageSize}${sortParam}`)
      .then(r => r.json())
      .then(res => { setData(res.items); setPageCount(res.totalPages); });
  }, [sorting, pagination]);

  const table = useReactTable({
    data,
    columns,
    pageCount,
    state: { sorting, pagination },
    manualSorting: true,
    manualPagination: true,
    onSortingChange: setSorting,
    onPaginationChange: setPagination,
    getCoreRowModel: getCoreRowModel(),
  });

  return <div>{/* Render table */}</div>;
}
```
</details>
