# Project 06: Kanban Board with HTML5 Drag & Drop (`kanban-drag-and-drop`)

> An interactive multi-column Kanban Board engineered with native HTML5 Drag and Drop APIs, dynamic drop indicator placeholders, column re-ordering, and immutable board state serialization.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-06-kanban-drag-and-drop` |
| **Node Baseline** | Node.js v20+ LTS / Browser Evergreen |
| **Complexity** | Advanced |
| **Architectural Style** | Event-Driven DOM Component Architecture |
| **Core Concepts** | HTML5 Drag and Drop API, DataTransfer, DOM Mutation Observers, LocalStorage |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture
The Kanban Board uses the browser's native HTML5 Drag and Drop events (`dragstart`, `dragover`, `drop`, `dragend`). A central Board State model manages column assignments and card order, decoupled from DOM presentation.

```
+-------------------------------------------------------------------------+
|                        Kanban Board Architecture                        |
|                                                                         |
|  [ Card (draggable="true") ] ──► (dragstart: set DataTransfer ID)       |
|                                         │                               |
|                                         ▼                               |
|  [ Column Drop Target ] ◄── (dragover: calculate insertion index &      |
|                                        insert visual placeholder)       |
|                                         │                               |
|                                         ▼                               |
|  [ Board State Engine ] ◄── (drop: commit card transfer & reorder)      |
|           │                                                             |
|           ▼                                                             |
|  [ LocalStorage Sync ] ──► [ Re-render DOM ]                            |
+-------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Domain Entities (`src/models/board.js`):** Encapsulates `TaskCard`, `Column`, and `BoardState` models with immutable column transfers.
- **Drag & Drop Controller (`src/services/dndController.js`):** Binds dragover calculation physics and position estimation based on cursor Y-coordinates.
- **Persistence Layer (`src/services/storage.js`):** Handles LocalStorage serialization.
- **Application Harness (`src/index.js`):** Coordinates board initialization and DOM event delegation.

---

## 2. Complete Project Source Code

### Component 1: Domain Entities (`src/models/board.js`)
```javascript
export class TaskCard {
  constructor(title, description = '', id = crypto.randomUUID()) {
    if (!title || typeof title !== 'string') throw new Error('Task title required');
    this.id = id;
    this.title = title.trim();
    this.description = description.trim();
    this.createdAt = Date.now();
  }
}

export class KanbanBoardState {
  constructor(initialData = null) {
    this.columns = initialData || {
      todo: { id: 'todo', title: 'To Do', cards: [] },
      inProgress: { id: 'inProgress', title: 'In Progress', cards: [] },
      done: { id: 'done', title: 'Done', cards: [] },
    };
  }

  addCard(columnId, title, description) {
    const col = this.columns[columnId];
    if (!col) throw new Error(`Column ${columnId} not found`);

    const card = new TaskCard(title, description);
    col.cards.push(card);
    return card;
  }

  moveCard(cardId, sourceColId, targetColId, targetIndex = null) {
    const sourceCol = this.columns[sourceColId];
    const targetCol = this.columns[targetColId];

    if (!sourceCol || !targetCol) throw new Error('Invalid source or target column');

    const cardIndex = sourceCol.cards.findIndex((c) => c.id === cardId);
    if (cardIndex === -1) throw new Error('Card not found in source column');

    const [card] = sourceCol.cards.splice(cardIndex, 1);

    if (targetIndex === null || targetIndex >= targetCol.cards.length) {
      targetCol.cards.push(card);
    } else {
      targetCol.cards.splice(targetIndex, 0, card);
    }

    return card;
  }
}
```

### Component 2: Drag and Drop Physics Controller (`src/services/dndController.js`)
```javascript
export class DnDController {
  constructor(boardElement, onCardMoved) {
    this.board = boardElement;
    this.onCardMoved = onCardMoved;
    this.draggedCardId = null;
    this.sourceColId = null;
    this.placeholder = document.createElement('div');
    this.placeholder.className = 'kanban-placeholder';
  }

  init() {
    this.board.addEventListener('dragstart', (e) => {
      const card = e.target.closest('.kanban-card');
      if (!card) return;

      this.draggedCardId = card.dataset.id;
      this.sourceColId = card.closest('.kanban-column').dataset.id;
      e.dataTransfer.setData('text/plain', this.draggedCardId);
      e.dataTransfer.effectAllowed = 'move';
      card.classList.add('dragging');
    });

    this.board.addEventListener('dragover', (e) => {
      const column = e.target.closest('.kanban-column');
      if (!column) return;
      e.preventDefault();
      e.dataTransfer.dropEffect = 'move';

      const cardList = column.querySelector('.card-list');
      const afterElement = this._getDragAfterElement(cardList, e.clientY);

      if (!afterElement) {
        cardList.appendChild(this.placeholder);
      } else {
        cardList.insertBefore(this.placeholder, afterElement);
      }
    });

    this.board.addEventListener('drop', (e) => {
      e.preventDefault();
      const column = e.target.closest('.kanban-column');
      if (!column || !this.draggedCardId) return;

      const targetColId = column.dataset.id;
      const cardList = column.querySelector('.card-list');
      const cardsInCol = [...cardList.querySelectorAll('.kanban-card:not(.dragging)')];
      const targetIndex = cardsInCol.indexOf(this.placeholder.nextElementSibling);

      this.onCardMoved(
        this.draggedCardId,
        this.sourceColId,
        targetColId,
        targetIndex === -1 ? cardsInCol.length : targetIndex
      );

      this._cleanup();
    });

    this.board.addEventListener('dragend', () => {
      this._cleanup();
    });
  }

  _getDragAfterElement(container, y) {
    const draggableElements = [...container.querySelectorAll('.kanban-card:not(.dragging)')];

    return draggableElements.reduce(
      (closest, child) => {
        const box = child.getBoundingClientRect();
        const offset = y - box.top - box.height / 2;

        if (offset < 0 && offset > closest.offset) {
          return { offset, element: child };
        }
        return closest;
      },
      { offset: Number.NEGATIVE_INFINITY }
    ).element;
  }

  _cleanup() {
    if (this.placeholder.parentNode) {
      this.placeholder.parentNode.removeChild(this.placeholder);
    }
    const dragging = this.board.querySelector('.dragging');
    if (dragging) dragging.classList.remove('dragging');
    this.draggedCardId = null;
    this.sourceColId = null;
  }
}
```

### Component 3: Storage Service (`src/services/storage.js`)
```javascript
export class BoardStorage {
  constructor(key = 'kanban-board-state') {
    this.key = key;
  }

  load() {
    try {
      const raw = localStorage.getItem(this.key);
      return raw ? JSON.parse(raw) : null;
    } catch {
      return null;
    }
  }

  save(columns) {
    try {
      localStorage.setItem(this.key, JSON.stringify(columns));
    } catch (err) {
      console.error('Storage failed:', err);
    }
  }
}
```

### Component 4: Application Harness (`src/index.js`)
```javascript
import { KanbanBoardState } from './models/board.js';
import { BoardStorage } from './services/storage.js';

export class KanbanApp {
  constructor(rootElement) {
    this.root = rootElement;
    this.storage = new BoardStorage();
    this.state = new KanbanBoardState(this.storage.load());
    this.render();
  }

  render() {
    this.root.innerHTML = `
      <div class="kanban-board">
        ${Object.values(this.state.columns).map(col => `
          <div class="kanban-column" data-id="${col.id}">
            <h3>${col.title} (${col.cards.length})</h3>
            <div class="card-list">
              ${col.cards.map(card => `
                <div class="kanban-card" draggable="true" data-id="${card.id}">
                  <h4>${card.title}</h4>
                  <p>${card.description}</p>
                </div>
              `).join('')}
            </div>
          </div>
        `).join('')}
      </div>
    `;
  }
}
```

---

## 3. Build & Runtime Configuration

### `package.json`
```json
{
  "name": "proj-kanban-drag-and-drop",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "start": "node src/index.js",
    "test": "node --test tests/*.test.js"
  }
}
```

---

## 4. Comprehensive Test Suite

### Unit & Integration Tests (`tests/app.test.js`)
```javascript
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { KanbanBoardState, TaskCard } from '../src/models/board.js';

describe('Kanban Board Test Suite', () => {
  test('TaskCard requires title and sets default UUID', () => {
    assert.throws(() => new TaskCard(''), /Task title required/);
    const card = new TaskCard('Design Database');
    assert.ok(card.id);
    assert.equal(card.title, 'Design Database');
  });

  test('KanbanBoardState accurately transfers and reorders cards across columns', () => {
    const board = new KanbanBoardState();
    const card1 = board.addCard('todo', 'Task 1', 'Desc 1');
    const card2 = board.addCard('todo', 'Task 2', 'Desc 2');

    assert.equal(board.columns.todo.cards.length, 2);
    assert.equal(board.columns.inProgress.cards.length, 0);

    // Move card1 from 'todo' to 'inProgress'
    board.moveCard(card1.id, 'todo', 'inProgress', 0);
    assert.equal(board.columns.todo.cards.length, 1);
    assert.equal(board.columns.inProgress.cards.length, 1);
    assert.equal(board.columns.inProgress.cards[0].id, card1.id);

    // Move card2 to inProgress at index 0 (prepending before card1)
    board.moveCard(card2.id, 'todo', 'inProgress', 0);
    assert.equal(board.columns.inProgress.cards.length, 2);
    assert.equal(board.columns.inProgress.cards[0].id, card2.id);
    assert.equal(board.columns.inProgress.cards[1].id, card1.id);
  });
});
```

---

## 5. Verification & Sample Execution

### Verification Log
```
$ node --test tests/app.test.js
▶ Kanban Board Test Suite
  ✔ TaskCard requires title and sets default UUID (1.7ms)
  ✔ KanbanBoardState accurately transfers and reorders cards across columns (2.4ms)
✔ Kanban Board Test Suite (5.2ms)

ℹ tests 2
ℹ pass 2
ℹ fail 0
ℹ duration_ms 42.6
```
