# Project 09: Offline-First Notes PWA (`notes-pwa-offline`)

> A Progressive Web App (PWA) note-taking system built with Dexie.js (IndexedDB wrapper), Service Worker caching, bidirectional background synchronization, and network status detection (`useSyncExternalStore`).

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-09-notes-pwa` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | Dexie.js (IndexedDB) + `useSyncExternalStore` (Online/Offline) |
| **Styling** | Tailwind CSS v4 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Data Flow
All note creations, modifications, and deletions write directly to client-side IndexedDB tables via Dexie.js first. When network connectivity changes, `useSyncExternalStore` triggers background synchronization of un-replicated records with the remote backend.

```
[ User Action: Create / Edit Note ]
               │
               ▼
   [ Dexie.js (IndexedDB Local DB) ]  <── Immediate, Zero-Latency Storage
               │
               ▼ (Status: 'pending_sync')
   [ Online Event Detected ]
               │
               ▼
   [ Background Sync Queue ] ───────────> [ REST Sync Endpoint ]
                                                 │
                                                 ▼
   [ Update Record Status to 'synced' ] <────────┘
```

### 1.2 Component Structure
```text
src/
├── db/
│   └── database.ts           # Dexie IndexedDB schema and stores
├── hooks/
│   ├── useNetworkStatus.ts   # useSyncExternalStore online status
│   └── useLiveNotes.ts       # dexie-react-hooks live queries
├── components/
│   ├── SyncBadge.tsx         # Online / Offline / Syncing indicator
│   ├── NoteSidebar.tsx       # Search and note list
│   └── NoteEditor.tsx        # Title, tags, and auto-saving body
└── App.tsx                   # Main PWA layout
```

---

## 2. Complete Project Source Code

### 2.1 Dexie Database Schema (`src/db/database.ts`)
```ts
import Dexie, { Table } from 'dexie';

export interface NoteRecord {
  id?: number;
  uuid: string;
  title: string;
  content: string;
  tags: string[];
  updatedAt: number;
  syncStatus: 'synced' | 'pending';
}

export class NotesDatabase extends Dexie {
  notes!: Table<NoteRecord, number>;

  constructor() {
    super('OfflineNotesDB');
    this.version(1).stores({
      notes: '++id, uuid, title, updatedAt, syncStatus, *tags',
    });
  }
}

export const db = new NotesDatabase();
```

### 2.2 Network Status Hook (`src/hooks/useNetworkStatus.ts`)
```ts
import { useSyncExternalStore } from 'react';

function subscribe(callback: () => void) {
  window.addEventListener('online', callback);
  window.addEventListener('offline', callback);
  return () => {
    window.removeEventListener('online', callback);
    window.removeEventListener('offline', callback);
  };
}

export function useNetworkStatus(): boolean {
  return useSyncExternalStore(
    subscribe,
    () => navigator.onLine,
    () => true // SSR fallback
  );
}
```

### 2.3 Note Editor with Auto-Save (`src/components/NoteEditor.tsx`)
```tsx
import React, { useState, useEffect } from 'react';
import { NoteRecord, db } from '../db/database';

interface NoteEditorProps {
  note: NoteRecord;
}

export const NoteEditor: React.FC<NoteEditorProps> = ({ note }) => {
  const [title, setTitle] = useState(note.title);
  const [content, setContent] = useState(note.content);

  useEffect(() => {
    setTitle(note.title);
    setContent(note.content);
  }, [note.uuid]);

  const handleUpdate = async (newTitle: string, newContent: string) => {
    setTitle(newTitle);
    setContent(newContent);

    if (note.id) {
      await db.notes.update(note.id, {
        title: newTitle,
        content: newContent,
        updatedAt: Date.now(),
        syncStatus: 'pending',
      });
    }
  };

  return (
    <div className="flex flex-col h-full p-6 bg-white">
      <input
        type="text"
        value={title}
        onChange={(e) => handleUpdate(e.target.value, content)}
        placeholder="Note Title"
        className="text-2xl font-bold border-b pb-2 focus:outline-none focus:border-blue-500"
      />
      <textarea
        value={content}
        onChange={(e) => handleUpdate(title, e.target.value)}
        placeholder="Start writing in markdown..."
        className="flex-1 mt-4 resize-none border-none focus:outline-none text-slate-700 font-mono text-sm leading-relaxed"
      />
    </div>
  );
};
```

---

## 3. Testing Suite (`src/tests/useNetworkStatus.test.ts`)

```ts
import { describe, it, expect } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { useNetworkStatus } from '../hooks/useNetworkStatus';

describe('useNetworkStatus Hook', () => {
  it('returns true when navigator.onLine is true', () => {
    const { result } = renderHook(() => useNetworkStatus());
    expect(result.current).toBe(true);
  });
});
```

---

## 4. Build and Run Instructions

```bash
# Install dependencies
npm install dexie dexie-react-hooks

# Build PWA
npm run build
```
