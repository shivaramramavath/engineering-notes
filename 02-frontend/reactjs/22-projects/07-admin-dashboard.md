# Project 07: Enterprise Admin Dashboard (`admin-dashboard`)

> A mission-critical SaaS admin portal featuring high-performance data tables with TanStack Table v8, KPI metric cards with trend indicators, role-based access control (RBAC) route guards, CSV export, and batch action processing.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-07-admin-dashboard` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | TanStack Table v8 + URL SearchParams + Context RBAC |
| **Styling** | Tailwind CSS v4 + Radix UI Dropdown / Dialog |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Data Flow
User roles and permissions (`admin`, `editor`, `viewer`) control visibility of table operations and actions. The data grid uses TanStack Table headless hooks for sorting, column filtering, row selection, and client/server pagination.

```
[ User Auth Context (Roles / Permissions) ]
                    │
                    ▼
[ Admin Layout (Sidebar / Header / Breadcrumbs) ]
                    │
                    ▼
[ TanStack Table v8 Headless Instance ]
    ├── Multi-column sorting & global filter
    ├── Row selection state (Batch actions: Delete, Export)
    └── Pagination model synced with URL query params
```

### 1.2 Component Structure
```text
src/
├── types/
│   └── user.ts              # System user and audit log models
├── context/
│   └── AuthContext.tsx      # RBAC context and permission checks
├── components/
│   ├── MetricCards.tsx      # Stat summaries with percentage deltas
│   ├── UserTable.tsx        # TanStack Table component with selection
│   ├── TablePagination.tsx  # Page size selector and navigation controls
│   └── RoleGuard.tsx        # Component conditional on current user role
└── App.tsx                  # Dashboard view
```

---

## 2. Complete Project Source Code

### 2.1 Types (`src/types/user.ts`)
```ts
export type UserRole = 'ADMIN' | 'EDITOR' | 'VIEWER';

export interface SystemUser {
  id: string;
  name: string;
  email: string;
  role: UserRole;
  status: 'active' | 'suspended' | 'pending';
  lastLogin: string;
  department: string;
}

export interface MetricSummary {
  title: string;
  value: string;
  change: number; // e.g., +12.5%
  isPositive: boolean;
}
```

### 2.2 RBAC Guard (`src/context/AuthContext.tsx`, `src/components/RoleGuard.tsx`)
```tsx
// src/context/AuthContext.tsx
import React, { createContext, useContext, useState } from 'react';
import { UserRole } from '../types/user';

interface AuthContextType {
  currentUserRole: UserRole;
  switchRole: (role: UserRole) => void;
  canPerform: (action: 'CREATE' | 'UPDATE' | 'DELETE') => boolean;
}

const AuthContext = createContext<AuthContextType | undefined>(undefined);

export const AuthProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [currentUserRole, setCurrentUserRole] = useState<UserRole>('ADMIN');

  const canPerform = (action: 'CREATE' | 'UPDATE' | 'DELETE') => {
    if (currentUserRole === 'ADMIN') return true;
    if (currentUserRole === 'EDITOR' && action !== 'DELETE') return true;
    return false;
  };

  return (
    <AuthContext.Provider value={{ currentUserRole, switchRole: setCurrentUserRole, canPerform }}>
      {children}
    </AuthContext.Provider>
  );
};

export const useAuth = () => {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error('useAuth must be used within an AuthProvider');
  return ctx;
};
```

### 2.3 TanStack Table Grid (`src/components/UserTable.tsx`)
```tsx
import React, { useState } from 'react';
import {
  useReactTable,
  getCoreRowModel,
  getSortedRowModel,
  getFilteredRowModel,
  getPaginationRowModel,
  ColumnDef,
  flexRender,
  SortingState,
} from '@tanstack/react-table';
import { SystemUser } from '../types/user';
import { useAuth } from '../context/AuthContext';

interface UserTableProps {
  data: SystemUser[];
}

export const UserTable: React.FC<UserTableProps> = ({ data }) => {
  const [sorting, setSorting] = useState<SortingState>([]);
  const [globalFilter, setGlobalFilter] = useState('');
  const [rowSelection, setRowSelection] = useState({});
  const { canPerform } = useAuth();

  const columns: ColumnDef<SystemUser>[] = [
    {
      id: 'select',
      header: ({ table }) => (
        <input
          type="checkbox"
          checked={table.getIsAllPageRowsSelected()}
          onChange={table.getToggleAllPageRowsSelectedHandler()}
          className="rounded border-slate-300"
        />
      ),
      cell: ({ row }) => (
        <input
          type="checkbox"
          checked={row.getIsSelected()}
          disabled={!row.getCanSelect()}
          onChange={row.getToggleSelectedHandler()}
          className="rounded border-slate-300"
        />
      ),
    },
    {
      accessorKey: 'name',
      header: 'User',
      cell: (info) => (
        <div>
          <p className="font-medium text-slate-900">{info.getValue() as string}</p>
          <p className="text-xs text-slate-500">{info.row.original.email}</p>
        </div>
      ),
    },
    {
      accessorKey: 'role',
      header: 'Role',
      cell: (info) => (
        <span className="rounded-full bg-slate-100 px-2.5 py-0.5 text-xs font-semibold text-slate-700">
          {info.getValue() as string}
        </span>
      ),
    },
    {
      accessorKey: 'status',
      header: 'Status',
      cell: (info) => {
        const status = info.getValue() as string;
        const color =
          status === 'active'
            ? 'bg-emerald-50 text-emerald-700'
            : status === 'pending'
            ? 'bg-amber-50 text-amber-700'
            : 'bg-rose-50 text-rose-700';
        return <span className={`rounded px-2 py-0.5 text-xs font-semibold ${color}`}>{status}</span>;
      },
    },
    {
      id: 'actions',
      header: 'Actions',
      cell: ({ row }) => (
        <div className="flex items-center gap-2">
          {canPerform('UPDATE') && (
            <button className="text-xs text-blue-600 hover:underline">Edit</button>
          )}
          {canPerform('DELETE') && (
            <button className="text-xs text-rose-600 hover:underline">Revoke</button>
          )}
        </div>
      ),
    },
  ];

  const table = useReactTable({
    data,
    columns,
    state: { sorting, globalFilter, rowSelection },
    onSortingChange: setSorting,
    onGlobalFilterChange: setGlobalFilter,
    onRowSelectionChange: setRowSelection,
    getCoreRowModel: getCoreRowModel(),
    getSortedRowModel: getSortedRowModel(),
    getFilteredRowModel: getFilteredRowModel(),
    getPaginationRowModel: getPaginationRowModel(),
  });

  return (
    <div className="space-y-4">
      <div className="flex items-center justify-between">
        <input
          type="search"
          placeholder="Filter accounts..."
          value={globalFilter}
          onChange={(e) => setGlobalFilter(e.target.value)}
          className="rounded-lg border border-slate-300 px-3 py-1.5 text-sm focus:outline-blue-500"
        />
        <span className="text-xs text-slate-500">
          {Object.keys(rowSelection).length} of {data.length} selected
        </span>
      </div>

      <div className="overflow-x-auto rounded-xl border border-slate-200 bg-white shadow-xs">
        <table className="w-full text-left text-sm text-slate-600">
          <thead className="border-b bg-slate-50 text-xs font-semibold uppercase text-slate-500">
            {table.getHeaderGroups().map((headerGroup) => (
              <tr key={headerGroup.id}>
                {headerGroup.headers.map((header) => (
                  <th key={header.id} className="px-4 py-3">
                    {flexRender(header.column.columnDef.header, header.getContext())}
                  </th>
                ))}
              </tr>
            ))}
          </thead>
          <tbody className="divide-y divide-slate-100">
            {table.getRowModel().rows.map((row) => (
              <tr key={row.id} className="hover:bg-slate-50">
                {row.getVisibleCells().map((cell) => (
                  <td key={cell.id} className="px-4 py-3">
                    {flexRender(cell.column.columnDef.cell, cell.getContext())}
                  </td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
};
```

---

## 3. Testing Suite (`src/tests/UserTable.test.tsx`)

```tsx
import { render, screen } from '@testing-library/react';
import { UserTable } from '../components/UserTable';
import { AuthProvider } from '../context/AuthContext';
import { describe, it, expect } from 'vitest';

const mockUsers = [
  {
    id: 'u1',
    name: 'Alice Cooper',
    email: 'alice@company.com',
    role: 'ADMIN' as const,
    status: 'active' as const,
    lastLogin: '2026-11-20',
    department: 'Engineering',
  },
];

describe('Admin UserTable', () => {
  it('renders table rows and permission controls', () => {
    render(
      <AuthProvider>
        <UserTable data={mockUsers} />
      </AuthProvider>
    );

    expect(screen.getByText('Alice Cooper')).toBeInTheDocument();
    expect(screen.getByText('alice@company.com')).toBeInTheDocument();
    expect(screen.getByText('Revoke')).toBeInTheDocument();
  });
});
```

---

## 4. Build and Run Instructions

```bash
# Install dependencies
npm install @tanstack/react-table

# Start dev server
npm run dev
```
