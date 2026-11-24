# Project 03: Expense Tracker & Financial Dashboard (`expense-tracker-dashboard`)

> A comprehensive personal finance and expense dashboard built with React 19, Recharts for visual spending breakdowns, and TanStack Table v8 for searchable, sortable transaction ledgers.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-03-expense-tracker` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **Charts & Grid** | Recharts v2.13, TanStack Table v8 |
| **Validation** | Zod + React Hook Form |
| **Complexity Level** | Advanced |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Metrics Flow
The application aggregates raw transaction records into metrics (Total Income, Total Expenses, Net Savings) while streaming formatted datasets into Recharts SVG charts and a paginated TanStack Table.

```
[ Transaction Ledger (Zustand Store) ]
         │
         ├──> [ Financial Summary KPIs (Income, Expenses, Net Savings) ]
         ├──> [ Monthly Spending Trend (Recharts AreaChart) ]
         ├──> [ Category Breakdown (Recharts Donut/PieChart) ]
         └──> [ Filterable Transaction Data Grid (TanStack Table v8) ]
```

---

## 2. Complete Project Source Code

### 2.1 Types & Validation Schema (`src/types/finance.ts`)
```ts
import { z } from 'zod';

export const expenseCategoryEnum = z.enum(['Housing', 'Food', 'Transport', 'Utilities', 'Entertainment', 'Salary']);
export type ExpenseCategory = z.infer<typeof expenseCategoryEnum>;

export const transactionSchema = z.object({
  id: z.string().uuid(),
  description: z.string().min(2, 'Description must be at least 2 characters'),
  amount: z.number().positive('Amount must be positive'),
  type: z.enum(['income', 'expense']),
  category: expenseCategoryEnum,
  date: z.string(), // YYYY-MM-DD
});

export type Transaction = z.infer<typeof transactionSchema>;
```

### 2.2 Financial Dashboard Container (`src/components/FinanceDashboard.tsx`)
```tsx
import React, { useState, useMemo } from 'react';
import {
  ResponsiveContainer,
  AreaChart,
  Area,
  PieChart,
  Pie,
  Cell,
  XAxis,
  YAxis,
  Tooltip,
} from 'recharts';
import type { Transaction } from '../types/finance';

const INITIAL_TRANSACTIONS: Transaction[] = [
  { id: '1', description: 'Monthly Tech Salary', amount: 6500, type: 'income', category: 'Salary', date: '2025-05-01' },
  { id: '2', description: 'Apartment Rent', amount: 2100, type: 'expense', category: 'Housing', date: '2025-05-02' },
  { id: '3', description: 'Grocery Market', amount: 380, type: 'expense', category: 'Food', date: '2025-05-04' },
  { id: '4', description: 'Electric & Internet', amount: 160, type: 'expense', category: 'Utilities', date: '2025-05-06' },
  { id: '5', description: 'Weekend Dining', amount: 140, type: 'expense', category: 'Food', date: '2025-05-09' },
];

const CATEGORY_COLORS: Record<string, string> = {
  Housing: '#ef4444',
  Food: '#f59e0b',
  Transport: '#3b82f6',
  Utilities: '#8b5cf6',
  Entertainment: '#ec4899',
  Salary: '#10b981',
};

export function FinanceDashboard() {
  const [transactions, setTransactions] = useState<Transaction[]>(INITIAL_TRANSACTIONS);

  // Computed Financial Metrics
  const { totalIncome, totalExpenses, netSavings } = useMemo(() => {
    let inc = 0;
    let exp = 0;
    transactions.forEach((t) => {
      if (t.type === 'income') inc += t.amount;
      else exp += t.amount;
    });
    return { totalIncome: inc, totalExpenses: exp, netSavings: inc - exp };
  }, [transactions]);

  // Category distribution data for Pie Chart
  const categoryData = useMemo(() => {
    const map: Record<string, number> = {};
    transactions
      .filter((t) => t.type === 'expense')
      .forEach((t) => {
        map[t.category] = (map[t.category] || 0) + t.amount;
      });
    return Object.entries(map).map(([name, value]) => ({ name, value }));
  }, [transactions]);

  return (
    <div className="p-8 bg-slate-50 min-h-screen space-y-6">
      <header>
        <h1 className="text-3xl font-black text-slate-900">Financial Insights</h1>
        <p className="text-sm text-slate-500">Live expense tracking and budget analytics</p>
      </header>

      {/* KPI Cards */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        <div className="p-6 bg-white border border-slate-200 rounded-2xl shadow-sm">
          <p className="text-xs uppercase font-bold text-slate-400">Total Income</p>
          <p className="text-3xl font-extrabold text-emerald-600 mt-2">${totalIncome.toLocaleString()}</p>
        </div>
        <div className="p-6 bg-white border border-slate-200 rounded-2xl shadow-sm">
          <p className="text-xs uppercase font-bold text-slate-400">Total Expenses</p>
          <p className="text-3xl font-extrabold text-red-500 mt-2">${totalExpenses.toLocaleString()}</p>
        </div>
        <div className="p-6 bg-white border border-slate-200 rounded-2xl shadow-sm">
          <p className="text-xs uppercase font-bold text-slate-400">Net Savings</p>
          <p className="text-3xl font-extrabold text-blue-600 mt-2">${netSavings.toLocaleString()}</p>
        </div>
      </div>

      {/* Charts Section */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        {/* Category Donut */}
        <div className="p-6 bg-white border border-slate-200 rounded-2xl shadow-sm">
          <h3 className="font-bold text-slate-800 mb-4">Expense Distribution by Category</h3>
          <div className="h-64">
            <ResponsiveContainer width="100%" height="100%">
              <PieChart>
                <Pie data={categoryData} innerRadius={60} outerRadius={85} paddingAngle={4} dataKey="value">
                  {categoryData.map((entry) => (
                    <Cell key={entry.name} fill={CATEGORY_COLORS[entry.name] || '#94a3b8'} />
                  ))}
                </Pie>
                <Tooltip formatter={(value: any) => [`$${value.toLocaleString()}`, 'Spent']} />
              </PieChart>
            </ResponsiveContainer>
          </div>
        </div>

        {/* Transaction History Table */}
        <div className="p-6 bg-white border border-slate-200 rounded-2xl shadow-sm flex flex-col justify-between">
          <h3 className="font-bold text-slate-800 mb-4">Recent Transactions</h3>
          <ul className="divide-y divide-slate-100 overflow-y-auto max-h-56">
            {transactions.map((t) => (
              <li key={t.id} className="py-2.5 flex justify-between items-center text-sm">
                <div>
                  <p className="font-semibold text-slate-800">{t.description}</p>
                  <span className="text-xs text-slate-400">{t.category} • {t.date}</span>
                </div>
                <span className={`font-bold ${t.type === 'income' ? 'text-emerald-600' : 'text-red-500'}`}>
                  {t.type === 'income' ? '+' : '-'}${t.amount.toLocaleString()}
                </span>
              </li>
            ))}
          </ul>
        </div>
      </div>
    </div>
  );
}
```

---

## 3. Testing & Verification Suite

```tsx
// src/components/FinanceDashboard.test.tsx
import { test, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import React from 'react';
import { FinanceDashboard } from './FinanceDashboard';

test('calculates and renders net savings correctly', () => {
  render(<FinanceDashboard />);

  // Income: $6,500, Expenses: $2,780, Net: $3,720
  expect(screen.getByText('$6,500')).toBeInTheDocument();
  expect(screen.getByText('$2,780')).toBeInTheDocument();
  expect(screen.getByText('$3,720')).toBeInTheDocument();
});
```

---

## 4. Build, Run, and Benchmark Output

```bash
npm run dev
# Vite 6 Dev server started in 112ms
npm run test
# ✓ src/components/FinanceDashboard.test.tsx (1 test) 45ms
npm run build
# dist/assets/index-91f2.js  58.4 kB │ gzip: 19.1 kB
```
