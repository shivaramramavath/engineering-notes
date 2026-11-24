# TanStack Table v8 — Basic to Advanced

A comprehensive reference card for `@tanstack/react-table` v8 (formerly React Table) in React 19.

> **Target Version:** TanStack Table v8.20+  
> **React Version:** 19.0.0+  
> **Architecture:** Headless, 100% UI Agnostic  

---

## Level 0 — Concepts

- **Headless Architecture**: TanStack Table contains zero HTML, CSS, or UI elements. It is an algorithmic state engine that computes columns, row models, sorting indices, and pagination states, letting you render table markup using Tailwind, shadcn/ui, or plain HTML.
- **Pipeline Architecture**: Data flows through a series of modular row models: `getCoreRowModel` -> `getFilteredRowModel` -> `getSortedRowModel` -> `getPaginationRowModel`.
- **Core Primitives Table**:

| API | Type | Purpose |
| :--- | :--- | :--- |
| `useReactTable()` | Hook | Central table hook returning the table instance |
| `createColumnHelper()` | Utility | Type-safe column definition factory |
| `flexRender()` | Function | Renders cell/header values (strings, numbers, or React components) |
| `getCoreRowModel()` | Function | Mandatory baseline row model processor |
| `getSortedRowModel()` | Function | Enables multi-column client-side sorting |
| `getPaginationRowModel()` | Function | Enables client-side page slicing |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install @tanstack/react-table
```

### 1.2 Defining Columns and Rendering the Table
```tsx
import React, { useMemo } from 'react';
import {
  useReactTable,
  getCoreRowModel,
  flexRender,
  createColumnHelper,
} from '@tanstack/react-table';

interface User {
  id: string;
  name: string;
  email: string;
  role: string;
}

const columnHelper = createColumnHelper<User>();

const columns = [
  columnHelper.accessor('name', {
    header: 'Name',
    cell: (info) => <span className="font-semibold">{info.getValue()}</span>,
  }),
  columnHelper.accessor('email', {
    header: 'Email',
  }),
  columnHelper.accessor('role', {
    header: 'Role',
    cell: (info) => (
      <span className="px-2 py-0.5 rounded text-xs bg-blue-100 text-blue-800">
        {info.getValue()}
      </span>
    ),
  }),
];

export function BasicTable({ data }: { data: User[] }) {
  const table = useReactTable({
    data,
    columns,
    getCoreRowModel: getCoreRowModel(),
  });

  return (
    <div className="border rounded-lg overflow-x-auto">
      <table className="w-full text-sm text-left">
        <thead className="bg-gray-50 border-b text-gray-700">
          {table.getHeaderGroups().map((headerGroup) => (
            <tr key={headerGroup.id}>
              {headerGroup.headers.map((header) => (
                <th key={header.id} className="p-3">
                  {flexRender(header.column.columnDef.header, header.getContext())}
                </th>
              ))}
            </tr>
          ))}
        </thead>
        <tbody className="divide-y">
          {table.getRowModel().rows.map((row) => (
            <tr key={row.id} className="hover:bg-gray-50">
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

---

## Level 2 — Intermediate: Sorting & Pagination

```tsx
import {
  useReactTable,
  getCoreRowModel,
  getSortedRowModel,
  getPaginationRowModel,
  SortingState,
} from '@tanstack/react-table';

export function SortableTable({ data, columns }: any) {
  const [sorting, setSorting] = React.useState<SortingState>([]);

  const table = useReactTable({
    data,
    columns,
    state: { sorting },
    onSortingChange: setSorting,
    getCoreRowModel: getCoreRowModel(),
    getSortedRowModel: getSortedRowModel(),
    getPaginationRowModel: getPaginationRowModel(),
    initialState: { pagination: { pageSize: 10 } },
  });

  return (
    <div>
      {/* Table JSX with sorting indicators on headers */}
      {/* Pagination controls: table.previousPage(), table.nextPage() */}
    </div>
  );
}
```

---

## Level 3 — Advanced: Virtualization & Server-Side Filtering

- Combine TanStack Table with **TanStack Virtual (`useVirtualizer`)** for 100,000+ rows.
- Set `manualPagination: true` and `manualSorting: true` to bypass client models and drive table state via URL search parameters or server APIs.

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Defining `columns` Inline in Component** | Recreates column definitions on every render, resetting column widths and state. | Declare `columns` outside component or wrap in `useMemo`. |
| **Rendering Cell Values without `flexRender`** | Strings fail to render if cell definitions return JSX or custom render functions. | Always call `flexRender(cell.column.columnDef.cell, cell.getContext())`. |
| **Missing Unique Key on Table Rows** | React warns on reconciliation and breaks row animations. | Use `row.id` as React `key`. |
| **Unbounded Client-Side Sorting on 500k Rows** | Freezes the browser's main thread. | Enable `manualSorting: true` and push sorting to database indexes. |
