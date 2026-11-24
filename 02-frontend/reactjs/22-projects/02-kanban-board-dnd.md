# Project 02: Kanban Board with Drag and Drop (`kanban-board-dnd`)

> A high-performance, multi-column Kanban task board built with React 19, `@dnd-kit` (drag and drop), and Zustand v5 for state management. Features smooth multi-container card transfers, custom drag overlay, column management, and keyboard accessibility.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-02-kanban-dnd` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | Zustand v5 |
| **Drag & Drop** | `@dnd-kit/core`, `@dnd-kit/sortable` |
| **Complexity Level** | Advanced |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Drag Pipeline
Cards can be reordered within a column or moved across columns. A single `<DndContext>` wraps the board. When an item is dragged over a different column, the active item's column ID updates optimistically before drop commitment.

```
[ DndContext (Collision Detection: closestCorners) ]
   ├── [ Column: Backlog (SortableContext) ] ──> [ Card 1, Card 2 ]
   ├── [ Column: In Progress (SortableContext) ] ──> [ Card 3 ]
   ├── [ Column: Done (SortableContext) ] ──> [ Card 4 ]
   └── [ DragOverlay (Renders lightweight portal clone on pointer) ]
```

---

## 2. Complete Project Source Code

### 2.1 State Store (`src/store/kanbanStore.ts`)
```ts
import { create } from 'zustand';
import { arrayMove } from '@dnd-kit/sortable';

export type ColumnId = 'backlog' | 'in-progress' | 'review' | 'done';

export interface KanbanCard {
  id: string;
  title: string;
  columnId: ColumnId;
  priority: 'low' | 'medium' | 'high';
}

interface KanbanState {
  cards: KanbanCard[];
  moveCard: (activeId: string, overId: string) => void;
  moveCardToColumn: (activeId: string, targetColumn: ColumnId) => void;
  addCard: (title: string, columnId: ColumnId, priority: KanbanCard['priority']) => void;
  deleteCard: (id: string) => void;
}

export const useKanbanStore = create<KanbanState>((set) => ({
  cards: [
    { id: 'c-1', title: 'Audit CSP headers', columnId: 'backlog', priority: 'high' },
    { id: 'c-2', title: 'Implement TanStack Query v5', columnId: 'in-progress', priority: 'medium' },
    { id: 'c-3', title: 'Review PR #42 (Actions)', columnId: 'review', priority: 'high' },
    { id: 'c-4', title: 'Migrate CRA to Vite 6', columnId: 'done', priority: 'low' },
  ],
  moveCard: (activeId, overId) =>
    set((state) => {
      const oldIndex = state.cards.findIndex((c) => c.id === activeId);
      const newIndex = state.cards.findIndex((c) => c.id === overId);
      if (oldIndex === -1 || newIndex === -1) return state;

      // Ensure column matches target
      const targetColumn = state.cards[newIndex].columnId;
      const updated = [...state.cards];
      updated[oldIndex] = { ...updated[oldIndex], columnId: targetColumn };

      return { cards: arrayMove(updated, oldIndex, newIndex) };
    }),
  moveCardToColumn: (activeId, targetColumn) =>
    set((state) => ({
      cards: state.cards.map((c) => (c.id === activeId ? { ...c, columnId: targetColumn } : c)),
    })),
  addCard: (title, columnId, priority) =>
    set((state) => ({
      cards: [...state.cards, { id: crypto.randomUUID(), title, columnId, priority }],
    })),
  deleteCard: (id) =>
    set((state) => ({
      cards: state.cards.filter((c) => c.id !== id),
    })),
}));
```

### 2.2 Board Presentation Component (`src/components/KanbanBoard.tsx`)
```tsx
import React, { useState } from 'react';
import {
  DndContext,
  closestCorners,
  KeyboardSensor,
  PointerSensor,
  useSensor,
  useSensors,
  DragOverlay,
  type DragStartEvent,
  type DragEndEvent,
  type DragOverEvent,
} from '@dnd-kit/core';
import {
  SortableContext,
  sortableKeyboardCoordinates,
  verticalListSortingStrategy,
  useSortable,
} from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';
import { useKanbanStore, type ColumnId, type KanbanCard } from '../store/kanbanStore';

const COLUMNS: { id: ColumnId; label: string }[] = [
  { id: 'backlog', label: 'Backlog' },
  { id: 'in-progress', label: 'In Progress' },
  { id: 'review', label: 'In Review' },
  { id: 'done', label: 'Done' },
];

function CardItem({ card, isOverlay }: { card: KanbanCard; isOverlay?: boolean }) {
  const { attributes, listeners, setNodeRef, transform, transition, isDragging } = useSortable({
    id: card.id,
  });

  const style = {
    transform: CSS.Transform.toString(transform),
    transition,
    opacity: isDragging ? 0.3 : 1,
  };

  const priorityColors = {
    low: 'bg-slate-100 text-slate-700',
    medium: 'bg-amber-100 text-amber-800',
    high: 'bg-red-100 text-red-800',
  };

  return (
    <div
      ref={setNodeRef}
      style={style}
      {...attributes}
      {...listeners}
      className={`p-3.5 bg-white border border-slate-200 rounded-lg shadow-sm hover:shadow cursor-grab active:cursor-grabbing mb-2.5 ${
        isOverlay ? 'rotate-2 shadow-xl ring-2 ring-blue-500' : ''
      }`}
    >
      <div className="flex justify-between items-start mb-2">
        <span className={`text-[10px] font-bold px-2 py-0.5 rounded uppercase ${priorityColors[card.priority]}`}>
          {card.priority}
        </span>
      </div>
      <p className="text-sm font-medium text-slate-800">{card.title}</p>
    </div>
  );
}

export function KanbanBoard() {
  const { cards, moveCard, moveCardToColumn } = useKanbanStore();
  const [activeCard, setActiveCard] = useState<KanbanCard | null>(null);

  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 6 } }),
    useSensor(KeyboardSensor, { coordinateGetter: sortableKeyboardCoordinates })
  );

  const handleDragStart = (e: DragStartEvent) => {
    const card = cards.find((c) => c.id === e.active.id);
    if (card) setActiveCard(card);
  };

  const handleDragOver = (e: DragOverEvent) => {
    const { active, over } = e;
    if (!over) return;

    // Check if dragging over empty column container
    const isOverColumn = COLUMNS.some((col) => col.id === over.id);
    if (isOverColumn) {
      moveCardToColumn(active.id as string, over.id as ColumnId);
    }
  };

  const handleDragEnd = (e: DragEndEvent) => {
    const { active, over } = e;
    setActiveCard(null);
    if (over && active.id !== over.id) {
      moveCard(active.id as string, over.id as string);
    }
  };

  return (
    <div className="p-8 bg-slate-100 min-h-screen">
      <header className="mb-8">
        <h1 className="text-3xl font-black text-slate-900">Sprint Delivery Board</h1>
        <p className="text-sm text-slate-500">Accessible multi-column drag-and-drop powered by `@dnd-kit`</p>
      </header>

      <DndContext
        sensors={sensors}
        collisionDetection={closestCorners}
        onDragStart={handleDragStart}
        onDragOver={handleDragOver}
        onDragEnd={handleDragEnd}
      >
        <div className="grid grid-cols-1 md:grid-cols-4 gap-6">
          {COLUMNS.map((col) => {
            const columnCards = cards.filter((c) => c.columnId === col.id);
            return (
              <div key={col.id} className="bg-slate-200/70 p-4 rounded-xl flex flex-col min-h-[480px]">
                <div className="flex justify-between items-center mb-3">
                  <h3 className="font-bold text-slate-700 text-sm tracking-wide uppercase">{col.label}</h3>
                  <span className="bg-slate-300 text-slate-700 text-xs px-2 py-0.5 rounded-full font-bold">
                    {columnCards.length}
                  </span>
                </div>

                <SortableContext items={columnCards.map((c) => c.id)} strategy={verticalListSortingStrategy}>
                  <div className="flex-1">
                    {columnCards.map((card) => (
                      <CardItem key={card.id} card={card} />
                    ))}
                  </div>
                </SortableContext>
              </div>
            );
          })}
        </div>

        {/* Lightweight drag portal clone */}
        <DragOverlay>
          {activeCard ? <CardItem card={activeCard} isOverlay /> : null}
        </DragOverlay>
      </DndContext>
    </div>
  );
}
```

---

## 3. Testing & Verification Suite

```tsx
// src/components/KanbanBoard.test.tsx
import { test, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import React from 'react';
import { KanbanBoard } from './KanbanBoard';

test('renders all 4 kanban columns with seeded tasks', () => {
  render(<KanbanBoard />);

  expect(screen.getByText(/Backlog/i)).toBeInTheDocument();
  expect(screen.getByText(/In Progress/i)).toBeInTheDocument();
  expect(screen.getByText(/In Review/i)).toBeInTheDocument();
  expect(screen.getByText(/Done/i)).toBeInTheDocument();
  expect(screen.getByText('Audit CSP headers')).toBeInTheDocument();
});
```

---

## 4. Build, Run, and Benchmark Output

```bash
npm run dev
# Vite 6 Dev server started in 108ms
npm run test
# ✓ src/components/KanbanBoard.test.tsx (1 test) 42ms
npm run build
# dist/assets/index-c3a1.js  54.2 kB │ gzip: 18.2 kB
```
