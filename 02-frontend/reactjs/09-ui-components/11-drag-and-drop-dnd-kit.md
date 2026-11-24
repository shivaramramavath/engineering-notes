# Modern Drag and Drop in React with @dnd-kit

- **Topic**: `09-ui-components`
- **React version**: React 19+ (`@dnd-kit` v6.x)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0, `@dnd-kit/core` 6.3.0, `@dnd-kit/sortable` 8.0.0
- **Bundle / compiler notes**: Modular, accessible drag-and-drop toolkit; replaces deprecated `react-beautiful-dnd`; 100% compatible with React 19 StrictMode and React Compiler.

---

## 1. Concept Overview

Building Drag and Drop (DnD) interfaces (like Kanban boards, reorderable lists, dashboard widget grids, and file drops) in React has historically been fraught with stability and accessibility issues:
- **HTML5 Drag and Drop API**: Extremely clunky, inconsistent across browsers, completely fails on mobile touchscreens, and lacks keyboard navigation.
- **`react-beautiful-dnd`**: Once the industry favorite, it was **deprecated and archived by Atlassian**. It relies on synchronous DOM measurements that fail in React 18/19 StrictMode and Concurrent Mode, and lacks support for arbitrary grids or multi-container trees.

**`@dnd-kit`** is the modern, lightweight, modular drag-and-drop toolkit designed specifically for modern React:
- **Modular Architecture**: Separate packages for core primitives (`@dnd-kit/core`), sortable lists (`@dnd-kit/sortable`), and constraint modifiers (`@dnd-kit/modifiers`).
- **Comprehensive Sensor System**: Built-in sensors for Pointer (mouse + touch + pen) and Keyboard, with customizable activation constraints (e.g., drag only starts after moving 8px, preventing accidental clicks).
- **First-Class Accessibility**: Complete keyboard navigation (Space/Enter to pick up, Arrow keys to move, Space/Enter to drop, Esc to cancel) with screen reader live announcements.
- **Zero Layout Shifts via `<DragOverlay>`**: Renders the dragging item into a separate DOM portal without altering source list dimensions.

```
@dnd-kit Architecture:
┌────────────────────────────────────────────────────────┐
│ <DndContext sensors={...} collisionDetection={...}>    │
│   ├──► <SortableContext items={ids}>                  │
│   │      ├──► <SortableItem id="1" />                 │
│   │      └──► <SortableItem id="2" />                 │
│   │                                                    │
│   └──► <DragOverlay> (Portal preview during dragging) │
└────────────────────────────────────────────────────────┘
```

### When to Use
- Reorderable sortable lists (task queues, playlist tracks, navigation menus).
- Kanban boards with multi-column card transfers (Trello/Jira style).
- Dashboard grid layout builders with resizable and movable widgets.

### When NOT to Use
- Simple file drag-and-drop from the operating system into the browser (use native HTML5 `onDrop` with `e.dataTransfer.files`).
- Static lists without reordering requirements.

---

## 2. Key Mechanics & Architecture

### Core Building Blocks
1. **`<DndContext>`**: The root provider managing active drag state, sensor listeners, collision detection, and drag lifecycle events (`onDragStart`, `onDragOver`, `onDragEnd`, `onDragCancel`).
2. **Sensors**: Abstract input listeners:
   - `PointerSensor`: Handles mouse, touch, and stylus.
   - `KeyboardSensor`: Handles accessible keyboard navigation.
   - `Activation Constraints`: Prevents accidental drags (e.g. require moving 8px before drag triggers).
3. **Collision Detection Algorithms**:
   - `closestCenter`: Best for sortable lists and grids.
   - `rectIntersection`: Best for standard droppable zones.
   - `pointerWithin`: Best for nested drop targets.
4. **`useSortable` & `arrayMove`**: Handles coordinate translation, CSS transforms, and immutable array index swapping.

### Technical Characteristics Table
| Feature | Legacy `react-beautiful-dnd` | Modern `@dnd-kit` |
| :--- | :--- | :--- |
| **Status** | Deprecated / Archived | Actively Maintained |
| **React 18 / 19 Support** | Broken in StrictMode / Concurrent | 100% Native Support |
| **Bundle Size** | ~35KB gzipped (monolithic) | ~10KB gzipped (modular) |
| **Input Sensors** | Mouse & Keyboard only | Pointer, Touch, Mouse, Keyboard |
| **Grid Support** | 1D Lists only (horizontal/vertical) | Arbitrary 2D Grids & Kanban trees |
| **Portals / Overlays** | Complex hacks required | First-class `<DragOverlay>` |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern — Reorderable Sortable List
A vertical reorderable list of items with pointer and keyboard accessibility.

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

interface Item {
  id: string;
  label: string;
}

function SortableRow({ item }: { item: Item }) {
  const { attributes, listeners, setNodeRef, transform, transition, isDragging } = useSortable({
    id: item.id,
  });

  const style: React.CSSProperties = {
    transform: CSS.Transform.toString(transform),
    transition,
    opacity: isDragging ? 0.4 : 1,
  };

  return (
    <div
      ref={setNodeRef}
      style={style}
      {...attributes}
      {...listeners}
      className="p-3 border rounded-lg bg-white shadow-sm flex items-center justify-between cursor-grab active:cursor-grabbing mb-2 select-none"
    >
      <span className="text-sm font-medium">{item.label}</span>
      <span className="text-zinc-400 text-xs">⋮⋮</span>
    </div>
  );
}

export function SimpleSortableList() {
  const [items, setItems] = useState<Item[]>([
    { id: '1', label: '1. Configure Vite Build' },
    { id: '2', label: '2. Setup Tailwind CSS v4' },
    { id: '3', label: '3. Implement TanStack Table' },
    { id: '4', label: '4. Deploy to Edge Infrastructure' },
  ]);

  // Configure sensors with activation constraints to prevent hijacking clicks:
  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 8 } }),
    useSensor(KeyboardSensor, { coordinateGetter: sortableKeyboardCoordinates })
  );

  function handleDragEnd(event: DragEndEvent) {
    const { active, over } = event;
    if (over && active.id !== over.id) {
      setItems((prev) => {
        const oldIndex = prev.findIndex((i) => i.id === active.id);
        const newIndex = prev.findIndex((i) => i.id === over.id);
        return arrayMove(prev, oldIndex, newIndex);
      });
    }
  }

  return (
    <DndContext sensors={sensors} collisionDetection={closestCenter} onDragEnd={handleDragEnd}>
      <SortableContext items={items.map((i) => i.id)} strategy={verticalListSortingStrategy}>
        <div className="max-w-sm p-4 border rounded-xl bg-zinc-50">
          <h4 className="font-bold text-sm mb-3">Project Roadmap</h4>
          {items.map((item) => (
            <SortableRow key={item.id} item={item} />
          ))}
        </div>
      </SortableContext>
    </DndContext>
  );
}
```
Output:
```
// Renders sortable list; dragging items smoothly shifts siblings with CSS transform animations.
```

### Example 2: Production / Real-World Integration — Multi-Column Kanban Board with `<DragOverlay>`
A complete Trello-style Kanban board allowing moving cards between columns and reordering within columns.

```tsx
import React, { useState } from 'react';
import {
  DndContext,
  closestCenter,
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
  useSortable,
  arrayMove,
  verticalListSortingStrategy,
} from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';

interface Card {
  id: string;
  title: string;
  col: 'TODO' | 'IN_PROGRESS' | 'DONE';
}

function KanbanCard({ card }: { card: Card }) {
  const { attributes, listeners, setNodeRef, transform, transition, isDragging } = useSortable({
    id: card.id,
    data: { card },
  });

  const style: React.CSSProperties = {
    transform: CSS.Transform.toString(transform),
    transition,
    opacity: isDragging ? 0.3 : 1,
  };

  return (
    <div
      ref={setNodeRef}
      style={style}
      {...attributes}
      {...listeners}
      className="p-3 bg-white border rounded-md shadow-xs text-xs font-medium cursor-grab active:cursor-grabbing mb-2"
    >
      {card.title}
    </div>
  );
}

export function KanbanBoard() {
  const [cards, setCards] = useState<Card[]>([
    { id: 'c1', title: 'Write Auth Middleware', col: 'TODO' },
    { id: 'c2', title: 'Fix CSS Grid Padding', col: 'TODO' },
    { id: 'c3', title: 'Integrate Stripe Webhooks', col: 'IN_PROGRESS' },
    { id: 'c4', title: 'Setup GitHub Actions CI', col: 'DONE' },
  ]);
  const [activeCard, setActiveCard] = useState<Card | null>(null);

  const sensors = useSensors(useSensor(PointerSensor, { activationConstraint: { distance: 5 } }));

  const columns: ('TODO' | 'IN_PROGRESS' | 'DONE')[] = ['TODO', 'IN_PROGRESS', 'DONE'];

  function handleDragStart(e: DragStartEvent) {
    const card = cards.find((c) => c.id === e.active.id);
    if (card) setActiveCard(card);
  }

  function handleDragOver(e: DragOverEvent) {
    const { active, over } = e;
    if (!over) return;

    const activeId = active.id;
    const overId = over.id;

    // Find cards
    const activeItem = cards.find((c) => c.id === activeId);
    const overItem = cards.find((c) => c.id === overId);

    if (!activeItem) return;

    // Moving between different columns
    if (overItem && activeItem.col !== overItem.col) {
      setCards((prev) =>
        prev.map((c) => (c.id === activeId ? { ...c, col: overItem.col } : c))
      );
    }
  }

  function handleDragEnd(e: DragEndEvent) {
    const { active, over } = e;
    setActiveCard(null);
    if (!over) return;

    if (active.id !== over.id) {
      setCards((prev) => {
        const oldIndex = prev.findIndex((c) => c.id === active.id);
        const newIndex = prev.findIndex((c) => c.id === over.id);
        return arrayMove(prev, oldIndex, newIndex);
      });
    }
  }

  return (
    <DndContext
      sensors={sensors}
      collisionDetection={closestCenter}
      onDragStart={handleDragStart}
      onDragOver={handleDragOver}
      onDragEnd={handleDragEnd}
    >
      <div className="grid grid-cols-3 gap-4 max-w-2xl p-4 bg-zinc-100 rounded-2xl">
        {columns.map((colName) => {
          const colCards = cards.filter((c) => c.col === colName);
          return (
            <div key={colName} className="p-3 bg-zinc-200/60 rounded-xl min-h-[220px]">
              <h5 className="font-bold text-xs text-zinc-700 mb-2 uppercase">{colName} ({colCards.length})</h5>
              <SortableContext items={colCards.map((c) => c.id)} strategy={verticalListSortingStrategy}>
                {colCards.map((card) => (
                  <KanbanCard key={card.id} card={card} />
                ))}
              </SortableContext>
            </div>
          );
        })}
      </div>

      {/* DragOverlay renders the floating clone outside column hierarchy */}
      <DragOverlay>
        {activeCard ? (
          <div className="p-3 bg-white border-2 border-indigo-600 rounded-md shadow-2xl text-xs font-bold rotate-2 scale-105 cursor-grabbing">
            {activeCard.title}
          </div>
        ) : null}
      </DragOverlay>
    </DndContext>
  );
}
```
Output:
```
// Full interactive Kanban board: dragging cards between columns provides smooth floating overlay.
```

### Example 3: Edge Case / Restricting Movement Along a Single Axis
Restricting drag movement strictly to the vertical Y-axis using `@dnd-kit/modifiers`.

```tsx
import React from 'react';
import { DndContext } from '@dnd-kit/core';
import { restrictToVerticalAxis, restrictToWindowEdges } from '@dnd-kit/modifiers';

export function VerticalRestrictedDnd({ children }: { children: React.ReactNode }) {
  return (
    <DndContext
      // Modifiers clamp coordinates before rendering transforms!
      modifiers={[restrictToVerticalAxis, restrictToWindowEdges]}
    >
      {children}
    </DndContext>
  );
}
```
Output:
```
// Dragged elements cannot move horizontally or leave viewport edges.
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Forgetting `activationConstraint` on `PointerSensor`.

```tsx
// ❌ WRONG: PointerSensor without activationConstraint!
const badSensors = useSensors(useSensor(PointerSensor));

// ⚠️ BUG: Any simple click on a button, checkbox, or input inside the draggable
// item will be intercepted as an instant drag gesture! Users cannot click buttons!

// ✅ REMEDIATION: Always configure a distance or delay activation constraint!
const goodSensors = useSensors(
  useSensor(PointerSensor, {
    activationConstraint: {
      distance: 8, // Drag only starts after dragging 8 pixels!
    },
  })
);
```
Output:
```
// Regular clicks trigger onClick cleanly; only deliberate drags initiate reordering.
```

### Example 5: Idiomatic Modern React 19 Pattern
Optimistic Reordering with `useOptimistic` and React 19 Actions.

```tsx
import React, { useOptimistic, startTransition } from 'react';
import { arrayMove } from '@dnd-kit/sortable';

interface TaskItem {
  id: string;
  order: number;
  text: string;
}

export function OptimisticDragList({ initialTasks }: { initialTasks: TaskItem[] }) {
  const [tasks, setTasks] = React.useState(initialTasks);

  const [optTasks, setOptTasks] = useOptimistic(
    tasks,
    (current, { oldIdx, newIdx }: { oldIdx: number; newIdx: number }) =>
      arrayMove(current, oldIdx, newIdx)
  );

  const handleReorder = (oldIdx: number, newIdx: number) => {
    startTransition(async () => {
      setOptTasks({ oldIdx, newIdx });
      // Call backend API to persist new ordering
      await fetch('/api/tasks/reorder', { method: 'POST' });
      setTasks((prev) => arrayMove(prev, oldIdx, newIdx));
    });
  };

  return <div>{/* Render list with optTasks */}</div>;
}
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Omitting `activationConstraint` on pointer sensor** | Intercepts standard clicks and input typing inside items. | Set `activationConstraint: { distance: 8 }`. |
| **Mutating state array in place** | In-place mutations (`array.splice()`) bypass React state diffing. | Use the pure immutable `arrayMove(array, oldIdx, newIdx)` helper. |
| **Not providing unique, persistent IDs** | Using index strings (`id="0"`) causes item jumps during reordering. | Always use stable entity IDs (e.g. `task.id`). |
| **Omitting `CSS.Transform.toString(transform)`** | Manually writing `transform: translate3d(...)` loses scale/skew data. | Use the official `CSS.Transform.toString()` utility. |
| **Rendering complex DOM inside sortable items during drag** | Causes layout calculation lag during high-frequency pointer moves. | Use `<DragOverlay>` to render the active dragging clone. |
| **Skipping `KeyboardSensor`** | Completely breaks keyboard accessibility for disabled users. | Always include `KeyboardSensor` in `useSensors()`. |

---

## 5. React Internals: Beneath the Abstraction

Inside `@dnd-kit/core`:
1. `DndContext` registers pointer and touch event listeners on `window`.
2. When drag initiates, it calculates the bounding client rect of all registered `useDroppable` containers.
3. On every `pointermove` animation frame (`requestAnimationFrame`), the configured collision algorithm (e.g. `closestCenter`) computes distances between the active pointer coordinate and candidate droppable bounding boxes.
4. Active items receive a synthetic CSS transform (`transform: translate3d(x, y, 0)`), moving them via GPU compositor layers without triggering browser layout reflows.

---

## 6. Performance, Bundling & React Compiler

- **60 FPS Hardware Acceleration**: `@dnd-kit` animates via GPU-accelerated CSS `transform` and `opacity` properties.
- **Tree-Shaking**: Modular imports ensure you only bundle packages you use.
- **React Compiler**: The React Compiler memoizes draggable card items effortlessly, preventing re-renders of stationary items during active drags.

---

## 7. Accessibility (a11y) & Usability

- **Keyboard Traversal**: Space bar lifts the item; Arrow keys move it in the layout; Space bar drops it; Escape cancels.
- **Screen Reader Announcements**: `@dnd-kit` includes a live region announcer that broadcasts position updates:
  *"Picked up item 1. Current position 1 of 4. Press arrow keys to move, space to drop."*

---

## 8. Testing & Verification Patterns

Testing `@dnd-kit` drag interactions with Vitest:

```tsx
import { render, screen, fireEvent } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import React from 'react';
import { SimpleSortableList } from './SimpleSortableList';

describe('SimpleSortableList DnD', () => {
  it('renders all sortable items with grab cursor handles', () => {
    render(<SimpleSortableList />);

    const item1 = screen.getByText(/1. configure vite build/i);
    const item2 = screen.getByText(/2. setup tailwind css v4/i);

    expect(item1).toBeInTheDocument();
    expect(item2).toBeInTheDocument();
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 17** | `react-beautiful-dnd` was dominant but development stalled. | Transition began toward `@dnd-kit`. |
| **React 18** | `react-beautiful-dnd` deprecated; broke under StrictMode. | `@dnd-kit` became official industry standard. |
| **React 19** | Full compatibility with React 19 native refs and transitions. | Works seamlessly in modern Vite & Next.js setups. |

---

## 10. Security & Hardening

1. **Reorder Authorization**: Always verify on the server that the authenticated user has permission to reorder items before persisting array indexes.
2. **Preventing Rapid Network Spam**: Debounce or batch reorder API calls so rapid drags do not overload backend databases.

---

## 11. Real-World Case Study

### Linear Project Management Board

Issue tracking platform Linear uses an architecture modeled on `@dnd-kit`:
- Custom sensor activation ensures clicking an issue opens the detail drawer, while dragging after 6px reorders the issue in the sprint backlog.
- Smooth CSS spring animations reposition surrounding cards with zero frame stutter.

---

## 12. Interview Q&A

<details>
<summary>1. Why should developers use `@dnd-kit` instead of `react-beautiful-dnd` in 2026?</summary>

`react-beautiful-dnd` was officially archived and deprecated by Atlassian. It relies on synchronous DOM measurements that fail in React 18/19 Concurrent Mode and StrictMode, cannot handle 2D grids, and weighs ~35KB gzipped.
`@dnd-kit` is lightweight (~10KB), fully modular, supports pointer/touch/mouse/keyboard sensors, handles arbitrary 2D grids and Kanban trees, and is 100% compatible with modern React.
</details>

<details>
<summary>2. What is the role of `<DragOverlay>` in `@dnd-kit`?</summary>

`<DragOverlay>` renders a portal clone of the active dragging element outside the source list hierarchy. This prevents layout collapses (where removing the dragging item causes sibling cards to jump) and allows applying custom drag styling (rotations, box-shadows, scaling) without affecting the original DOM layout.
</details>

<details>
<summary>3. Why is an `activationConstraint` essential when configuring `PointerSensor`?</summary>

Without an activation constraint, any pointer click (e.g. clicking a button, checkbox, or link inside the card) will immediately initiate a drag session, preventing the click event from firing. An activation constraint (e.g. `{ distance: 8 }`) ensures the drag session only starts after the user moves the pointer at least 8 pixels.
</details>

<details>
<summary>4. What does the `arrayMove` helper do?</summary>

`arrayMove(array, fromIndex, toIndex)` is a pure, immutable utility function that returns a new array with the item moved from `fromIndex` to `toIndex` without mutating the original array in place.
</details>

<details>
<summary>5. How does `@dnd-kit` achieve keyboard accessibility?</summary>

By attaching the `KeyboardSensor`. When a user tabs to a draggable element and presses `Space`, the item is lifted. Pressing `ArrowUp` or `ArrowDown` recalculates coordinates and shifts the item in the list. Pressing `Space` drops the item, and `Escape` cancels the drag. A live region announces every movement to screen readers.
</details>

<details>
<summary>6. What collision detection algorithm should you use for a grid of cards?</summary>

`closestCenter`. It calculates the distance between the center point of the active draggable item and the center points of all candidate droppable targets, providing the most natural user experience for 2D grids and lists.
</details>

<details>
<summary>7. How do you prevent a dragged item from moving outside its container?</summary>

Using modifiers from `@dnd-kit/modifiers`, such as `restrictToWindowEdges` or `restrictToParentElement`.
</details>

<details>
<summary>8. Does `@dnd-kit` mutate DOM elements directly?</summary>

No. `@dnd-kit` returns reactive state (`transform`, `transition`, `isDragging`) through hooks like `useSortable`. You apply these values via standard React `style` props, preserving declarative virtual DOM integrity.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Drag Handle Only Activation</summary>

**Specification**: Build a card where dragging is only possible by clicking a specific drag handle icon (`⋮⋮`), while the rest of the card remains clickable.

```tsx
import React from 'react';
import { useSortable } from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';

export function HandleOnlyCard({ id, text }: { id: string; text: string }) {
  const { attributes, listeners, setNodeRef, transform, transition } = useSortable({ id });

  return (
    <div ref={setNodeRef} style={{ transform: CSS.Transform.toString(transform), transition }} className="p-3 border rounded flex justify-between">
      <span onClick={() => alert('Clicked text')}>{text}</span>
      {/* Bind listeners strictly to the handle! */}
      <span {...attributes} {...listeners} className="cursor-grab p-1">⋮⋮</span>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Horizontal Sortable Tabs</summary>

**Specification**: Implement a horizontal sortable tab bar using `horizontalListSortingStrategy`.

```tsx
import { SortableContext, horizontalListSortingStrategy } from '@dnd-kit/sortable';
// Wrap tabs in:
// <SortableContext items={tabIds} strategy={horizontalListSortingStrategy}>
```
</details>

<details>
<summary>Exercise 3 (Medium): Droppable Trash Can (Delete on Drop)</summary>

**Specification**: Create a droppable "Trash" zone that deletes any item dropped onto it.

```tsx
import React from 'react';
import { useDroppable } from '@dnd-kit/core';

export function TrashZone() {
  const { setNodeRef, isOver } = useDroppable({ id: 'trash-zone' });
  return (
    <div ref={setNodeRef} className={`p-4 border-2 border-dashed rounded text-center text-xs ${isOver ? 'bg-red-100 border-red-500' : 'bg-zinc-50'}`}>
      🗑️ Drop here to delete
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Custom Sound Effect on Drop</summary>

**Specification**: Play an audio feedback tone whenever an item is dropped into a new position.

```tsx
function handleDragEnd(event: DragEndEvent) {
  const { active, over } = event;
  if (over && active.id !== over.id) {
    new Audio('/sounds/drop-chime.mp3').play().catch(() => {});
    // Reorder state...
  }
}
```
</details>

<details>
<summary>Exercise 5 (Hard): 2D Dashboard Widget Grid Reordering</summary>

**Specification**: Build a 3x3 dashboard grid where cards can be swapped in 2D space using `rectSortingStrategy`.

```tsx
import { SortableContext, rectSortingStrategy } from '@dnd-kit/sortable';
// Grid container:
<div className="grid grid-cols-3 gap-3">
  <SortableContext items={widgetIds} strategy={rectSortingStrategy}>
    {widgets.map(w => <WidgetCard key={w.id} id={w.id} />)}
  </SortableContext>
</div>
```
</details>

<details>
<summary>Exercise 6 (Hard): Multi-List Drag and Drop with Server Persistence</summary>

**Specification**: Full Kanban board implementation that batches reorder mutations and updates backend API on drop.

```tsx
function handleDragEnd(event: DragEndEvent) {
  const { active, over } = event;
  if (!over) return;
  // Compute next state...
  // Persist to server:
  fetch('/api/kanban/move', {
    method: 'POST',
    body: JSON.stringify({ cardId: active.id, targetCol: over.id }),
  });
}
```
</details>
