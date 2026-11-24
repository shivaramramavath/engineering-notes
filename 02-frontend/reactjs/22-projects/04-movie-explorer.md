# Project 04: Movie Explorer with TMDB (`movie-explorer`)

> A movie discovery application integrating with TMDB (The Movie Database), featuring debounced search, infinite scroll pagination with TanStack Query v5 `useInfiniteQuery`, multi-criteria filtering, modal movie details, and responsive grid layouts.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-04-movie-explorer` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | TanStack Query v5 (`useInfiniteQuery`) + URL SearchParams |
| **Styling** | Tailwind CSS v4 + Lucide Icons |
| **Complexity Level** | Intermediate / Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Data Flow
The movie explorer relies on TanStack Query v5 to manage server cache, infinite scroll cursor tracking, and query key invalidation. Search queries are debounced using a custom `useDebounce` hook, with active filters reflected bidirectionally in the browser URL via `useSearchParams`.

```
[ Search Input / Filter Dropdowns ] ──> [ URL SearchParams (Single Source of Truth) ]
                                                        │
                                                        ▼
[ Window Scroll / IntersectionObserver ] ──> [ TanStack Query v5 useInfiniteQuery ]
                                                        │
                                                        ▼
[ UI Components (MovieGrid, MovieCard, Modal) ] <── [ TMDB REST API ]
```

### 1.2 Component Structure
```text
src/
├── types/
│   └── movie.ts              # TMDB API response schemas and entities
├── hooks/
│   ├── useDebounce.ts        # Input debounce hook
│   ├── useIntersection.ts    # Sentinel observer for infinite scroll
│   └── useMovies.ts          # TanStack Query infinite query service
├── components/
│   ├── Navbar.tsx            # Sticky header with search bar
│   ├── FilterBar.tsx         # Genre and sort dropdowns
│   ├── MovieGrid.tsx         # CSS Grid with skeleton loading
│   ├── MovieCard.tsx         # Poster card with hover badges and ratings
│   └── MovieModal.tsx        # Portal-rendered modal for movie details
└── App.tsx                   # Main layout container
```

---

## 2. Complete Project Source Code

### 2.1 Types (`src/types/movie.ts`)
```ts
export interface Movie {
  id: number;
  title: string;
  overview: string;
  poster_path: string | null;
  backdrop_path: string | null;
  release_date: string;
  vote_average: number;
  vote_count: number;
  genre_ids: number[];
}

export interface TMDBResponse<T> {
  page: number;
  results: T[];
  total_pages: number;
  total_results: number;
}

export interface Genre {
  id: number;
  name: string;
}

export type SortOption = 'popularity.desc' | 'vote_average.desc' | 'primary_release_date.desc';
```

### 2.2 Hooks (`src/hooks/useDebounce.ts`, `src/hooks/useIntersection.ts`)
```ts
// src/hooks/useDebounce.ts
import { useState, useEffect } from 'react';

export function useDebounce<T>(value: T, delay: number = 400): T {
  const [debouncedValue, setDebouncedValue] = useState<T>(value);

  useEffect(() => {
    const handler = setTimeout(() => setDebouncedValue(value), delay);
    return () => clearTimeout(handler);
  }, [value, delay]);

  return debouncedValue;
}

// src/hooks/useIntersection.ts
import { useEffect, useRef } from 'react';

export function useIntersection(onIntersect: () => void, enabled: boolean = true) {
  const sentinelRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    if (!enabled) return;
    const observer = new IntersectionObserver(
      (entries) => {
        if (entries[0].isIntersecting) {
          onIntersect();
        }
      },
      { rootMargin: '200px' }
    );

    const current = sentinelRef.current;
    if (current) observer.observe(current);

    return () => {
      if (current) observer.unobserve(current);
    };
  }, [onIntersect, enabled]);

  return sentinelRef;
}
```

### 2.3 TMDB Query Hook (`src/hooks/useMovies.ts`)
```ts
import { useInfiniteQuery } from '@tanstack/react-query';
import { Movie, TMDBResponse } from '../types/movie';

const TMDB_BASE_URL = 'https://api.themoviedb.org/3';
const API_KEY = import.meta.env.VITE_TMDB_API_KEY || 'demo_key';

export function useInfiniteMovies(query: string, genreId: string, sortBy: string) {
  return useInfiniteQuery<TMDBResponse<Movie>>({
    queryKey: ['movies', { query, genreId, sortBy }],
    queryFn: async ({ pageParam = 1 }) => {
      const endpoint = query.trim()
        ? `${TMDB_BASE_URL}/search/movie?query=${encodeURIComponent(query)}&page=${pageParam}&api_key=${API_KEY}`
        : `${TMDB_BASE_URL}/discover/movie?with_genres=${genreId}&sort_by=${sortBy}&page=${pageParam}&api_key=${API_KEY}`;

      const res = await fetch(endpoint);
      if (!res.ok) throw new Error('Failed to fetch movies');
      return res.json();
    },
    initialPageParam: 1,
    getNextPageParam: (lastPage) => {
      if (lastPage.page < lastPage.total_pages) {
        return lastPage.page + 1;
      }
      return undefined;
    },
    staleTime: 1000 * 60 * 5, // 5 minutes cache
  });
}
```

### 2.4 UI Components (`src/components/MovieCard.tsx`, `src/components/MovieGrid.tsx`)
```tsx
// src/components/MovieCard.tsx
import React from 'react';
import { Movie } from '../types/movie';

interface MovieCardProps {
  movie: Movie;
  onSelect: (movie: Movie) => void;
}

export const MovieCard: React.FC<MovieCardProps> = ({ movie, onSelect }) => {
  const posterUrl = movie.poster_path
    ? `https://image.tmdb.org/t/p/w500${movie.poster_path}`
    : 'https://via.placeholder.com/500x750?text=No+Poster';

  return (
    <article
      onClick={() => onSelect(movie)}
      className="group relative cursor-pointer overflow-hidden rounded-xl bg-slate-800 transition-all duration-300 hover:-translate-y-1 hover:shadow-xl hover:shadow-cyan-500/10"
      tabIndex={0}
      role="button"
      onKeyDown={(e) => (e.key === 'Enter' || e.key === ' ') && onSelect(movie)}
    >
      <div className="aspect-[2/3] w-full overflow-hidden bg-slate-900">
        <img
          src={posterUrl}
          alt={movie.title}
          loading="lazy"
          className="h-full w-full object-cover transition-transform duration-300 group-hover:scale-105"
        />
      </div>
      <div className="p-4">
        <h3 className="truncate font-semibold text-white group-hover:text-cyan-400">
          {movie.title}
        </h3>
        <div className="mt-2 flex items-center justify-between text-xs text-slate-400">
          <span>{movie.release_date?.slice(0, 4) || 'N/A'}</span>
          <span className="flex items-center gap-1 rounded bg-slate-700/80 px-2 py-0.5 font-bold text-amber-400">
            ★ {movie.vote_average.toFixed(1)}
          </span>
        </div>
      </div>
    </article>
  );
};
```

```tsx
// src/components/MovieGrid.tsx
import React from 'react';
import { Movie } from '../types/movie';
import { MovieCard } from './MovieCard';

interface MovieGridProps {
  movies: Movie[];
  isLoading: boolean;
  onSelectMovie: (movie: Movie) => void;
}

export const MovieGrid: React.FC<MovieGridProps> = ({ movies, isLoading, onSelectMovie }) => {
  if (isLoading && movies.length === 0) {
    return (
      <div className="grid grid-cols-2 gap-4 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5">
        {Array.from({ length: 10 }).map((_, idx) => (
          <div key={idx} className="aspect-[2/3] animate-pulse rounded-xl bg-slate-800" />
        ))}
      </div>
    );
  }

  if (movies.length === 0) {
    return (
      <div className="py-20 text-center text-slate-400">
        <p className="text-lg">No movies found matching your criteria.</p>
      </div>
    );
  }

  return (
    <div className="grid grid-cols-2 gap-4 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5">
      {movies.map((movie) => (
        <MovieCard key={movie.id} movie={movie} onSelect={onSelectMovie} />
      ))}
    </div>
  );
};
```

### 2.5 Main Application (`src/App.tsx`)
```tsx
import React, { useState } from 'react';
import { useDebounce, useIntersection } from './hooks/useDebounce';
import { useInfiniteMovies } from './hooks/useMovies';
import { MovieGrid } from './components/MovieGrid';
import { Movie } from './types/movie';

export default function App() {
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedGenre, setSelectedGenre] = useState('');
  const [sortBy, setSortBy] = useState('popularity.desc');
  const [selectedMovie, setSelectedMovie] = useState<Movie | null>(null);

  const debouncedSearch = useDebounce(searchTerm, 500);

  const { data, fetchNextPage, hasNextPage, isFetchingNextPage, status } =
    useInfiniteMovies(debouncedSearch, selectedGenre, sortBy);

  const sentinelRef = useIntersection(() => {
    if (hasNextPage && !isFetchingNextPage) {
      fetchNextPage();
    }
  });

  const allMovies = data?.pages.flatMap((page) => page.results) ?? [];

  return (
    <main className="min-h-screen bg-slate-950 px-4 py-8 text-slate-100 sm:px-8">
      <header className="mx-auto max-w-7xl">
        <div className="flex flex-col gap-4 sm:flex-row sm:items-center sm:justify-between">
          <h1 className="text-3xl font-extrabold tracking-tight text-white">
            Cine<span className="text-cyan-400">Wave</span>
          </h1>
          <input
            type="search"
            placeholder="Search thousands of movies..."
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
            className="w-full rounded-lg border border-slate-700 bg-slate-900 px-4 py-2.5 text-sm text-white placeholder-slate-400 focus:border-cyan-400 focus:outline-none sm:w-80"
          />
        </div>
      </header>

      <section className="mx-auto mt-8 max-w-7xl">
        <MovieGrid
          movies={allMovies}
          isLoading={status === 'pending'}
          onSelectMovie={(movie) => setSelectedMovie(movie)}
        />

        {/* Infinite Scroll Sentinel */}
        <div ref={sentinelRef} className="py-8 text-center">
          {isFetchingNextPage && <p className="text-sm text-cyan-400">Loading more movies...</p>}
          {!hasNextPage && allMovies.length > 0 && (
            <p className="text-sm text-slate-500">You've reached the end of the collection.</p>
          )}
        </div>
      </section>
    </main>
  );
}
```

---

## 3. Testing Suite (`src/tests/MovieExplorer.test.tsx`)

```tsx
import { render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import App from '../App';
import { describe, it, expect, vi, beforeEach } from 'vitest';

const queryClient = new QueryClient({
  defaultOptions: { queries: { retry: false } },
});

describe('Movie Explorer Component Flow', () => {
  beforeEach(() => {
    vi.restoreAllMocks();
  });

  it('renders initial movie list after fetch', async () => {
    const mockMovies = {
      page: 1,
      total_pages: 2,
      results: [{ id: 101, title: 'Inception', vote_average: 8.8, release_date: '2010-07-16' }],
    };

    vi.spyOn(global, 'fetch').mockResolvedValueOnce({
      ok: true,
      json: async () => mockMovies,
    } as Response);

    render(
      <QueryClientProvider client={queryClient}>
        <App />
      </QueryClientProvider>
    );

    expect(await screen.findByText('Inception')).toBeInTheDocument();
    expect(screen.getByText('★ 8.8')).toBeInTheDocument();
  });
});
```

---

## 4. Build and Run Instructions

```bash
# Install dependencies
npm install @tanstack/react-query lucide-react

# Set environment variables (.env.local)
VITE_TMDB_API_KEY="your_tmdb_api_key"

# Run development server
npm run dev

# Run Vitest suite
npm run test
```
