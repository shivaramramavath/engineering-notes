# @dnd-kit — Basic to Advanced

A comprehensive reference card for `@dnd-kit` (drag and drop toolkit) in React 19.

> **Target Version:** `@dnd-kit/core` v6.3+, `@dnd-kit/sortable` v8.0+  
> **Philosophy:** Lightweight, modular, accessible drag-and-drop primitives  
> **Performance:** Hardware-accelerated transforms, zero layout shifts with `<DragOverlay>`  

---

## Level 0 — Concepts

- **Modular Architecture**: Split into independent packages:
  - `@dnd-kit/core`: Fundamental context, collision detection, and sensor listeners.
  - `@dnd-kit/sortable`: Sortable list and grid algorithms.
  - `@dnd-kit/modifiers`: Restricts movement (e.g. restrict to vertical axis).
- **Sensors**: Abstract input devices (Mouse, Touch, Keyboard) with customizable activation constraints (e.g. require moving 8px before initiating drag, preserving tap/click events).
- **Core Primitives Table**:

| API | Type | Purpose |
| :--- | :--- | :--- |
| `<DndContext>` | Component | Central state provider managing drag lifecycle events |
| `useDraggable()` | Hook | Marks an element as draggable; returns ref and listeners |
| `useDroppable()` | Hook | Marks an element as a drop target |
| `<SortableContext>` | Component | Manages ordered arrays of sortable item IDs |
| `useSortable()` | Hook | Combines draggable and droppable for reorderable items |
| `<DragOverlay>` | Component | Renders a lightweight clone attached to the cursor during drag |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install @dnd-kit/core @dnd-kit/sortable @dnd-kit/utilities
```

### 1.2 Sortable List Implementation
```tsx
import React, { useState } from 'react';
import {
  DndContext,
  closestCenter,
  KeyboardSensor,
  PointerSensor,
  useSensor,
  useSensors,
  type DragEndEvent,
} from '@dnd-kit/core';
import {
  arrayMove,
  SortableContext,
  sortableKeyboardCoordinates,
  verticalListSortingStrategy,
  useSortable,
} from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';

function SortableItem({ id, title }: { id: string; title: string }) {
  const { attributes, listeners, setNodeRef, transform, transition, isDragging } = useSortable({ id });

  const style = {
    transform: CSS.Transform.toString(transform),
    transition,
    opacity: isDragging ? 0.5 : 1,
  };

  return (
    <div
      ref={setNodeRef}
      style={style}
      {...attributes}
      {...listeners}
      className="p-3 bg-white border rounded shadow-sm my-2 cursor-grab active:cursor-grabbing flex justify-between"
    >
      <span>{title}</span>
      <span className="text-gray-400">⠿</span>
    </div>
  );
}

export function SimpleSortableList() {
  const [items, setItems] = useState([
    { id: '1', title: 'Task Alpha' },
    { id: '2', title: 'Task Beta' },
    { id: '3', title: 'Task Gamma' },
  ]);

  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 8 } }),
    useSensor(KeyboardSensor, { coordinateGetter: sortableKeyboardCoordinates })
  );

  const handleDragEnd = (event: DragEndEvent) => {
    const { active, over } = event;
    if (over && active.id !== over.id) {
      setItems((prev) => {
        const oldIndex = prev.findIndex((i) => i.id === active.id);
        const newIndex = prev.findIndex((i) => i.id === over.id);
        return arrayMove(prev, oldIndex, newIndex);
      });
    }
  };

  return (
    <DndContext sensors={sensors} collisionDetection={closestCenter} onDragEnd={handleDragEnd}>
      <SortableContext items={items} strategy={verticalListSortingStrategy}>
        <div className="max-w-sm p-4 bg-gray-50 border rounded-xl">
          {items.map((item) => (
            <SortableItem key={item.id} id={item.id} title={item.title} />
          ))}
        </div>
      </SortableContext>
    </DndContext>
  );
}
```

---

## Level 2 — Intermediate: `<DragOverlay>` for Silky 60fps Performance

Using `<DragOverlay>` renders a portal clone that follows the pointer smoothly without layout shifting the underlying list:
```tsx
<DragOverlay>
  {activeItem ? <ItemCard title={activeItem.title} isOverlay /> : null}
</DragOverlay>
```

---

## Level 3 — Advanced: Multi-Container Kanban Boards

- Maintain separate `SortableContext` containers for each Kanban column (`Todo`, `In Progress`, `Done`).
- In `onDragOver`, detect when an item is dragged across column boundaries and transfer it between state arrays optimistically.

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Omitting `activationConstraint: { distance: 8 }`** | Clicks on buttons or inputs inside draggable items are mistaken for drag attempts. | Always configure a pointer activation constraint. |
| **Using `CSS.Translate.toString` instead of `Transform`**| Omits scale transforms, causing visual distortion during drag transitions. | Use `CSS.Transform.toString(transform)`. |
| **Omitting `KeyboardSensor`** | Violates WCAG accessibility standards; keyboard-only users cannot reorder items. | Always register `KeyboardSensor` with `sortableKeyboardCoordinates`. |
| **Directly Mutating Original Item Array** | Breaks React immutability and skips re-render reconciliation. | Always reorder using `@dnd-kit/sortable`'s pure `arrayMove()` utility. |
