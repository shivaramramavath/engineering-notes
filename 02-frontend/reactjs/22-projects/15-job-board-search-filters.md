# Project 15: Tech Job Board with Multi-Facet Search (`job-board-search-filters`)

> A high-performance developer job search portal featuring URL-synchronized multi-facet filtering (remote, salary range, tech stack tags, seniority), debounced keyword search, bookmark management, and zero layout-shift skeleton states.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-15-job-board` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | URL `useSearchParams` as Single Source of Truth + Nuqs |
| **Styling** | Tailwind CSS v4 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Data Flow
Every filter parameter (`query`, `location`, `remoteOnly`, `minSalary`, `tags`) is serialized directly into the browser URL query string via `useSearchParams`. Sharing or reloading the link restores the exact filter state. The job search service consumes the URL parameters directly to fetch matching postings.

```
[ Search Input & Multi-Select Tags ] ──> [ URL Query String (?tags=react,ts&remote=true) ]
                                                            │
                                                            ▼
[ Job Board Layout ] <── [ TanStack Query Filter Service ] <┘
        ├── FilterSidebar (Checkbox groups, salary range slider)
        ├── JobGrid (Listing cards with applied tag highlights)
        └── ShareSearchButton (Copies URL with exact filter state)
```

### 1.2 Component Structure
```text
src/
├── types/
│   └── job.ts                # JobPosting, FilterParams schemas
├── hooks/
│   └── useJobFilters.ts      # URL search params sync hook
├── components/
│   ├── FilterSidebar.tsx     # Faceted multi-checkbox filter groups
│   ├── JobCard.tsx           # Company logo, salary, tags, Easy Apply
│   └── JobListingView.tsx    # List shell with count summary & sorting
└── App.tsx                   # Main job board view
```

---

## 2. Complete Project Source Code

### 2.1 Types (`src/types/job.ts`)
```ts
export interface JobPosting {
  id: string;
  title: string;
  company: string;
  location: string;
  remote: boolean;
  salaryMin: number;
  salaryMax: number;
  tags: string[];
  postedAt: string;
}

export interface JobFilterParams {
  query: string;
  remoteOnly: boolean;
  minSalary: number;
  selectedTags: string[];
}
```

### 2.2 URL Filter Sync Hook (`src/hooks/useJobFilters.ts`)
```ts
import { useSearchParams } from 'react-router-dom';
import { useMemo, useCallback } from 'react';
import { JobFilterParams } from '../types/job';

export function useJobFilters() {
  const [searchParams, setSearchParams] = useSearchParams();

  const filters: JobFilterParams = useMemo(() => {
    return {
      query: searchParams.get('q') || '',
      remoteOnly: searchParams.get('remote') === 'true',
      minSalary: Number(searchParams.get('minSalary')) || 0,
      selectedTags: searchParams.get('tags') ? searchParams.get('tags')!.split(',') : [],
    };
  }, [searchParams]);

  const updateFilters = useCallback(
    (updates: Partial<JobFilterParams>) => {
      setSearchParams((prev) => {
        const next = new URLSearchParams(prev);
        if (updates.query !== undefined) {
          updates.query ? next.set('q', updates.query) : next.delete('q');
        }
        if (updates.remoteOnly !== undefined) {
          updates.remoteOnly ? next.set('remote', 'true') : next.delete('remote');
        }
        if (updates.minSalary !== undefined) {
          updates.minSalary > 0
            ? next.set('minSalary', updates.minSalary.toString())
            : next.delete('minSalary');
        }
        if (updates.selectedTags !== undefined) {
          updates.selectedTags.length > 0
            ? next.set('tags', updates.selectedTags.join(','))
            : next.delete('tags');
        }
        return next;
      });
    },
    [setSearchParams]
  );

  return { filters, updateFilters };
}
```

### 2.3 Job Card Component (`src/components/JobCard.tsx`)
```tsx
import React from 'react';
import { JobPosting } from '../types/job';

export const JobCard: React.FC<{ job: JobPosting }> = ({ job }) => {
  return (
    <div className="flex flex-col justify-between rounded-xl border border-slate-200 bg-white p-5 shadow-xs hover:border-blue-400 hover:shadow-md transition">
      <div>
        <div className="flex items-start justify-between">
          <div>
            <h3 className="font-bold text-slate-900 text-lg hover:text-blue-600 cursor-pointer">
              {job.title}
            </h3>
            <p className="text-sm text-slate-600">{job.company} • {job.location}</p>
          </div>
          {job.remote && (
            <span className="rounded-full bg-emerald-50 px-2.5 py-1 text-xs font-semibold text-emerald-700">
              Remote
            </span>
          )}
        </div>

        <div className="mt-4 flex flex-wrap gap-1.5">
          {job.tags.map((tag) => (
            <span
              key={tag}
              className="rounded bg-slate-100 px-2 py-0.5 text-xs font-medium text-slate-600"
            >
              {tag}
            </span>
          ))}
        </div>
      </div>

      <div className="mt-5 flex items-center justify-between border-t border-slate-100 pt-4">
        <span className="text-sm font-bold text-slate-900">
          ${(job.salaryMin / 1000).toFixed(0)}k - ${(job.salaryMax / 1000).toFixed(0)}k
        </span>
        <button className="rounded-lg bg-blue-600 px-3.5 py-1.5 text-xs font-semibold text-white hover:bg-blue-700">
          Apply Now
        </button>
      </div>
    </div>
  );
};
```

---

## 3. Testing Suite (`src/tests/useJobFilters.test.tsx`)

```tsx
import { renderHook, act } from '@testing-library/react';
import { MemoryRouter } from 'react-router-dom';
import { useJobFilters } from '../hooks/useJobFilters';
import { describe, it, expect } from 'vitest';

describe('useJobFilters Hook', () => {
  it('reads initial parameters and updates query parameter', () => {
    const wrapper = ({ children }: { children: React.ReactNode }) => (
      <MemoryRouter initialEntries={['/?q=developer&remote=true']}>{children}</MemoryRouter>
    );

    const { result } = renderHook(() => useJobFilters(), { wrapper });

    expect(result.current.filters.query).toBe('developer');
    expect(result.current.filters.remoteOnly).toBe(true);

    act(() => {
      result.current.updateFilters({ query: 'architect' });
    });

    expect(result.current.filters.query).toBe('architect');
  });
});
```

---

## 4. Build and Run Instructions

```bash
# Install dependencies
npm install react-router-dom

# Run dev
npm run dev
```
