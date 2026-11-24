# Project 06: Blog with Markdown & Syntax Highlighting (`blog-with-markdown`)

> A developer publishing platform and blog reader featuring Markdown AST parsing via unified / remark / rehype, syntax highlighting with Prism / Shiki, dynamic Table of Contents generation with active section spy, and calculated reading time estimates.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-06-markdown-blog` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | React State + IntersectionObserver for Active TOC |
| **Styling** | Tailwind Typography (`@tailwindcss/typography`) |
| **Complexity Level** | Intermediate |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Data Flow
Raw markdown text is parsed into structured HTML with syntax-highlighted code blocks. An AST extraction pass extracts all heading nodes (`h2`, `h3`) with slugified anchors to populate the Table of Contents. An `IntersectionObserver` tracks which heading is currently in viewport view to update the reading progress bar and highlight the active TOC item.

```
[ Raw Markdown Input / Remote MD ]
                │
                ▼
  [ remark-parse / remark-gfm ] ──> [ Extract Heading Hierarchy for TOC ]
                │
                ▼
  [ rehype-highlight / prism ]  ──> [ AST to React Elements Rendering ]
                │
                ▼
  [ IntersectionObserver ]      ──> [ Active Section Spy & Reading Progress ]
```

### 1.2 Component Structure
```text
src/
├── types/
│   └── post.ts              # Post metadata and heading AST types
├── utils/
│   ├── readingTime.ts       # Words-per-minute calculator
│   └── extractHeadings.ts   # Regex / AST heading tree extractor
├── hooks/
│   └── useScrollSpy.ts      # Active heading tracker
├── components/
│   ├── TableOfContents.tsx  # Nested links with smooth scrolling
│   ├── MarkdownRenderer.tsx # Custom code block with copy button
│   ├── ReadingProgress.tsx  # Top fixed scroll progress bar
│   └── ArticleHeader.tsx    # Author, date, reading time badge
└── App.tsx                  # Post reader view
```

---

## 2. Complete Project Source Code

### 2.1 Types & Utilities (`src/types/post.ts`, `src/utils/readingTime.ts`)
```ts
// src/types/post.ts
export interface HeadingItem {
  id: string;
  text: string;
  level: number;
}

export interface BlogPost {
  title: string;
  publishedAt: string;
  author: string;
  content: string;
}

// src/utils/readingTime.ts
export function calculateReadingTime(text: string, wordsPerMinute: number = 200): number {
  const words = text.trim().split(/\s+/).length;
  return Math.ceil(words / wordsPerMinute);
}
```

### 2.2 Heading Extractor & Scroll Spy Hook (`src/hooks/useScrollSpy.ts`)
```ts
import { useState, useEffect } from 'react';

export function useScrollSpy(headingIds: string[]): string {
  const [activeId, setActiveId] = useState<string>('');

  useEffect(() => {
    const observer = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            setActiveId(entry.target.id);
          }
        });
      },
      { rootMargin: '0% 0% -70% 0%' }
    );

    headingIds.forEach((id) => {
      const el = document.getElementById(id);
      if (el) observer.observe(el);
    });

    return () => observer.disconnect();
  }, [headingIds]);

  return activeId;
}
```

### 2.3 Table of Contents & Article Components (`src/components/TableOfContents.tsx`)
```tsx
import React from 'react';
import { HeadingItem } from '../types/post';

interface TableOfContentsProps {
  headings: HeadingItem[];
  activeId: string;
}

export const TableOfContents: React.FC<TableOfContentsProps> = ({ headings, activeId }) => {
  return (
    <nav className="sticky top-8 rounded-xl border border-slate-200 bg-slate-50 p-4">
      <h4 className="text-xs font-bold uppercase tracking-wider text-slate-500">On This Page</h4>
      <ul className="mt-3 space-y-2 text-sm">
        {headings.map((heading) => (
          <li
            key={heading.id}
            style={{ paddingLeft: `${(heading.level - 2) * 12}px` }}
          >
            <a
              href={`#${heading.id}`}
              className={`block transition-colors ${
                activeId === heading.id
                  ? 'font-semibold text-blue-600'
                  : 'text-slate-600 hover:text-slate-900'
              }`}
            >
              {heading.text}
            </a>
          </li>
        ))}
      </ul>
    </nav>
  );
};
```

### 2.4 Code Block with Copy Button (`src/components/CodeBlock.tsx`)
```tsx
import React, { useState } from 'react';

interface CodeBlockProps {
  language: string;
  code: string;
}

export const CodeBlock: React.FC<CodeBlockProps> = ({ language, code }) => {
  const [copied, setCopied] = useState(false);

  const handleCopy = async () => {
    await navigator.clipboard.writeText(code);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div className="relative my-4 overflow-hidden rounded-lg bg-slate-900 text-slate-100">
      <div className="flex items-center justify-between border-b border-slate-800 px-4 py-2 text-xs text-slate-400">
        <span>{language}</span>
        <button
          onClick={handleCopy}
          className="rounded px-2 py-0.5 hover:bg-slate-800 hover:text-white"
        >
          {copied ? 'Copied!' : 'Copy'}
        </button>
      </div>
      <pre className="overflow-x-auto p-4 text-sm font-mono">
        <code>{code}</code>
      </pre>
    </div>
  );
};
```

---

## 3. Testing Suite (`src/tests/readingTime.test.ts`)

```ts
import { describe, it, expect } from 'vitest';
import { calculateReadingTime } from '../utils/readingTime';

describe('Reading Time Calculator', () => {
  it('calculates 1 minute for under 200 words', () => {
    const text = 'Hello world this is a test post that has very few words.';
    expect(calculateReadingTime(text)).toBe(1);
  });

  it('calculates correct minutes for long text', () => {
    const words = new Array(500).fill('word').join(' ');
    expect(calculateReadingTime(words, 200)).toBe(3);
  });
});
```

---

## 4. Build and Run Instructions

```bash
# Install dependencies
npm install @tailwindcss/typography

# Run dev server
npm run dev
```
