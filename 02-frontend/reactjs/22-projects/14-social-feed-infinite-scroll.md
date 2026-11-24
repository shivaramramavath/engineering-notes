# Project 14: Virtualized Social Feed with Optimistic Likes (`social-feed-infinite-scroll`)

> A high-throughput social activity feed utilizing TanStack Virtual (`@tanstack/react-virtual`) for DOM virtualization, React 19 `useOptimistic` for instant like/bookmark toggles, and dynamic image dimensions with skeleton loading.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-14-social-feed` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | TanStack Virtual + React 19 `useOptimistic` + Server Actions |
| **Styling** | Tailwind CSS v4 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Data Flow
Rendering 1,000+ complex social posts causes severe DOM bloat and scroll jank. TanStack Virtual calculates dynamic element offsets and renders only items currently within the viewport window plus an overscan buffer. Post engagement (likes) employs `useOptimistic` for instantaneous user feedback.

```
[ Window / Scroll Container ]
             │
             ▼
[ TanStack Virtual: Virtualizer Window ]
  ──> Measures item heights
  ──> Renders only visible ~5-8 items with absolute positioning
             │
             ▼
[ Post Card Component ]
  ──> [ React 19 useOptimistic Like Counter ]
  ──> [ Async Server Action ]
```

### 1.2 Component Structure
```text
src/
├── types/
│   └── post.ts              # Social post, author, media types
├── components/
│   ├── FeedVirtualizer.tsx  # Dynamic height virtualized list container
│   ├── PostCard.tsx         # Author header, media carousel, engagement bar
│   └── OptimisticLike.tsx   # React 19 optimistic like button
└── App.tsx                  # Feed layout
```

---

## 2. Complete Project Source Code

### 2.1 Types (`src/types/post.ts`)
```ts
export interface Post {
  id: string;
  author: {
    name: string;
    handle: string;
    avatar: string;
  };
  content: string;
  image?: string;
  likes: number;
  isLiked: boolean;
  createdAt: string;
}
```

### 2.2 Feed Virtualizer (`src/components/FeedVirtualizer.tsx`)
```tsx
import React, { useRef } from 'react';
import { useVirtualizer } from '@tanstack/react-virtual';
import { Post } from '../types/post';
import { PostCard } from './PostCard';

interface FeedVirtualizerProps {
  posts: Post[];
  onToggleLike: (postId: string) => void;
}

export const FeedVirtualizer: React.FC<FeedVirtualizerProps> = ({ posts, onToggleLike }) => {
  const parentRef = useRef<HTMLDivElement | null>(null);

  const virtualizer = useVirtualizer({
    count: posts.length,
    getScrollElement: () => parentRef.current,
    estimateSize: () => 240, // Estimated card height
    overscan: 5,
  });

  return (
    <div
      ref={parentRef}
      className="h-screen w-full max-w-xl mx-auto overflow-y-auto border-x border-slate-200"
    >
      <div
        style={{
          height: `${virtualizer.getTotalSize()}px`,
          width: '100%',
          position: 'relative',
        }}
      >
        {virtualizer.getVirtualItems().map((virtualRow) => {
          const post = posts[virtualRow.index];
          return (
            <div
              key={post.id}
              data-index={virtualRow.index}
              ref={virtualizer.measureElement}
              style={{
                position: 'absolute',
                top: 0,
                left: 0,
                width: '100%',
                transform: `translateY(${virtualRow.start}px)`,
              }}
            >
              <PostCard post={post} onToggleLike={onToggleLike} />
            </div>
          );
        })}
      </div>
    </div>
  );
};
```

### 2.3 Post Card with Optimistic Like (`src/components/PostCard.tsx`)
```tsx
import React, { useOptimistic, startTransition } from 'react';
import { Post } from '../types/post';

interface PostCardProps {
  post: Post;
  onToggleLike: (postId: string) => void;
}

export const PostCard: React.FC<PostCardProps> = ({ post, onToggleLike }) => {
  const [optimisticState, setOptimisticState] = useOptimistic(
    { likes: post.likes, isLiked: post.isLiked },
    (current, update: boolean) => ({
      likes: update ? current.likes + 1 : current.likes - 1,
      isLiked: update,
    })
  );

  const handleLike = () => {
    const nextState = !optimisticState.isLiked;
    startTransition(async () => {
      setOptimisticState(nextState);
      await onToggleLike(post.id);
    });
  };

  return (
    <article className="border-b border-slate-200 bg-white p-4">
      <div className="flex items-center gap-3">
        <img src={post.author.avatar} alt="" className="h-10 w-10 rounded-full bg-slate-200" />
        <div>
          <span className="font-bold text-slate-900">{post.author.name}</span>
          <span className="ml-2 text-xs text-slate-500">@{post.author.handle}</span>
        </div>
      </div>
      <p className="mt-3 text-sm text-slate-800 leading-normal">{post.content}</p>
      {post.image && (
        <img
          src={post.image}
          alt=""
          className="mt-3 rounded-xl max-h-80 w-full object-cover border"
        />
      )}
      <div className="mt-3 flex items-center gap-6 text-xs text-slate-500">
        <button
          onClick={handleLike}
          className={`flex items-center gap-1.5 transition ${
            optimisticState.isLiked ? 'font-bold text-rose-500' : 'hover:text-slate-800'
          }`}
        >
          <span>♥</span>
          <span>{optimisticState.likes}</span>
        </button>
      </div>
    </article>
  );
};
```

---

## 3. Testing Suite (`src/tests/PostCard.test.tsx`)

```tsx
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { PostCard } from '../components/PostCard';
import { describe, it, expect, vi } from 'vitest';

const mockPost = {
  id: 'p1',
  author: { name: 'Dan', handle: 'dan_abramov', avatar: '/dan.jpg' },
  content: 'Optimistic UI makes apps feel instant!',
  likes: 42,
  isLiked: false,
  createdAt: '2026-11-20',
};

describe('PostCard Component', () => {
  it('optimistically increments likes on click', async () => {
    const onToggle = vi.fn();
    render(<PostCard post={mockPost} onToggleLike={onToggle} />);

    const likeBtn = screen.getByRole('button');
    expect(screen.getByText('42')).toBeInTheDocument();

    await userEvent.click(likeBtn);
    expect(screen.getByText('43')).toBeInTheDocument();
    expect(onToggle).toHaveBeenCalledWith('p1');
  });
});
```

---

## 4. Build and Run Instructions

```bash
# Install dependencies
npm install @tanstack/react-virtual

# Run dev
npm run dev
```
