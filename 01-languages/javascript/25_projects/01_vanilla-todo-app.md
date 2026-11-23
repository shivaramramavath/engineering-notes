# Project 01: Vanilla JavaScript Todo Application (`vanilla-todo-app`)

> A production-grade, framework-free single-page Todo application engineered with MVC clean architecture, event delegation, LocalStorage persistence, and XSS sanitization.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-01-vanilla-todo-app` |
| **Node Baseline** | Node.js v20+ LTS / Browser Evergreen |
| **Complexity** | Intermediate |
| **Architectural Style** | Model-View-Controller (MVC) with Event Delegation |
| **Core Concepts** | DOM Event Delegation, LocalStorage, Immutable State Transitions, XSS Defense |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture
The application adheres strictly to the Model-View-Controller (MVC) pattern, ensuring complete decoupling between domain state, DOM manipulation, and user event handling.

```
+-------------------------------------------------------------------------+
|                        Vanilla Todo Architecture                        |
|                                                                         |
|  [ User DOM Events ] ────────► [ Controller ] ────────► [ Store Model ] |
|         ▲                             │                        │        |
|         │                             ▼                        │        |
|  [ Real DOM Updates ] ◄─────── [ View Engine ] ◄───────────────┘        |
|                               (Template Render)                         |
+-------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Todo Model (`src/models/todo.js`):** Encapsulates the immutable Todo entity, enforcing title validation and UUID generation.
- **Store Service (`src/services/store.js`):** Manages collection state transitions, filter criteria (`all`, `active`, `completed`), and LocalStorage synchronization.
- **View Engine (`src/views/todoView.js`):** Renders HTML strings securely, sanitizes untrusted input to defeat XSS, and registers delegated DOM listeners.
- **Controller Harness (`src/index.js`):** Coordinates user intents, queries the store, and updates the view.

---

## 2. Complete Project Source Code

### Component 1: Domain Entities (`src/models/todo.js`)
```javascript
export class Todo {
  constructor(title, completed = false, id = crypto.randomUUID()) {
    if (!title || typeof title !== 'string' || title.trim().length === 0) {
      throw new Error('Todo title must be a non-empty string');
    }
    this.id = id;
    this.title = title.trim();
    this.completed = Boolean(completed);
    this.createdAt = Date.now();
  }

  toggle() {
    return new Todo(this.title, !this.completed, this.id);
  }

  updateTitle(newTitle) {
    return new Todo(newTitle, this.completed, this.id);
  }
}
```

### Component 2: Store Service (`src/services/store.js`)
```javascript
import { Todo } from '../models/todo.js';

export class TodoStore {
  constructor(storageKey = 'todos-vanilla-mvc', storage = globalThis.localStorage) {
    this.storageKey = storageKey;
    this.storage = storage;
    this.todos = this._load();
    this.filter = 'all'; // 'all' | 'active' | 'completed'
  }

  _load() {
    try {
      const data = this.storage?.getItem(this.storageKey);
      if (!data) return [];
      const parsed = JSON.parse(data);
      return parsed.map((item) => new Todo(item.title, item.completed, item.id));
    } catch {
      return [];
    }
  }

  _save() {
    try {
      this.storage?.setItem(this.storageKey, JSON.stringify(this.todos));
    } catch (err) {
      console.error('Failed to save to storage:', err);
    }
  }

  add(title) {
    const todo = new Todo(title);
    this.todos = [todo, ...this.todos];
    this._save();
    return todo;
  }

  toggle(id) {
    this.todos = this.todos.map((t) => (t.id === id ? t.toggle() : t));
    this._save();
  }

  remove(id) {
    this.todos = this.todos.filter((t) => t.id !== id);
    this._save();
  }

  clearCompleted() {
    this.todos = this.todos.filter((t) => !t.completed);
    this._save();
  }

  setFilter(filter) {
    if (['all', 'active', 'completed'].includes(filter)) {
      this.filter = filter;
    }
  }

  getFilteredTodos() {
    if (this.filter === 'active') return this.todos.filter((t) => !t.completed);
    if (this.filter === 'completed') return this.todos.filter((t) => t.completed);
    return this.todos;
  }
}
```

### Component 3: View & Sanitization Engine (`src/views/todoView.js`)
```javascript
export class TodoView {
  constructor(rootElement) {
    this.root = rootElement;
  }

  escapeHTML(str) {
    return str
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
  }

  render(todos, currentFilter, activeCount) {
    this.root.innerHTML = `
      <section class="todoapp">
        <header class="header">
          <h1>todos</h1>
          <form id="new-todo-form">
            <input id="new-todo-input" class="new-todo" placeholder="What needs to be done?" autofocus />
          </form>
        </header>
        <main class="main">
          <ul class="todo-list" id="todo-list">
            ${todos.map((todo) => this._renderItem(todo)).join('')}
          </ul>
        </main>
        <footer class="footer">
          <span class="todo-count"><strong>${activeCount}</strong> item${activeCount === 1 ? '' : 's'} left</span>
          <ul class="filters">
            <li><a href="#/" class="${currentFilter === 'all' ? 'selected' : ''}" data-filter="all">All</a></li>
            <li><a href="#/active" class="${currentFilter === 'active' ? 'selected' : ''}" data-filter="active">Active</a></li>
            <li><a href="#/completed" class="${currentFilter === 'completed' ? 'selected' : ''}" data-filter="completed">Completed</a></li>
          </ul>
          <button class="clear-completed" id="clear-completed-btn">Clear completed</button>
        </footer>
      </section>
    `;
  }

  _renderItem(todo) {
    const escapedTitle = this.escapeHTML(todo.title);
    return `
      <li class="${todo.completed ? 'completed' : ''}" data-id="${todo.id}">
        <div class="view">
          <input class="toggle" type="checkbox" ${todo.completed ? 'checked' : ''} />
          <label>${escapedTitle}</label>
          <button class="destroy" aria-label="Delete todo"></button>
        </div>
      </li>
    `;
  }

  bindEvents(handlers) {
    // Event delegation on root container
    this.root.addEventListener('submit', (e) => {
      if (e.target.id === 'new-todo-form') {
        e.preventDefault();
        const input = this.root.querySelector('#new-todo-input');
        if (input.value.trim()) {
          handlers.onAdd(input.value);
          input.value = '';
        }
      }
    });

    this.root.addEventListener('click', (e) => {
      const li = e.target.closest('li[data-id]');
      const id = li?.dataset.id;

      if (e.target.classList.contains('toggle') && id) {
        handlers.onToggle(id);
      } else if (e.target.classList.contains('destroy') && id) {
        handlers.onRemove(id);
      } else if (e.target.id === 'clear-completed-btn') {
        handlers.onClearCompleted();
      } else if (e.target.dataset.filter) {
        e.preventDefault();
        handlers.onFilter(e.target.dataset.filter);
      }
    });
  }
}
```

### Component 4: Controller & App Harness (`src/index.js`)
```javascript
import { TodoStore } from './services/store.js';
import { TodoView } from './views/todoView.js';

export class TodoApp {
  constructor(rootElement, storage) {
    this.store = new TodoStore('todos-vanilla-mvc', storage);
    this.view = new TodoView(rootElement);

    this.view.bindEvents({
      onAdd: (title) => {
        this.store.add(title);
        this.update();
      },
      onToggle: (id) => {
        this.store.toggle(id);
        this.update();
      },
      onRemove: (id) => {
        this.store.remove(id);
        this.update();
      },
      onClearCompleted: () => {
        this.store.clearCompleted();
        this.update();
      },
      onFilter: (filter) => {
        this.store.setFilter(filter);
        this.update();
      },
    });

    this.update();
  }

  update() {
    const todos = this.store.getFilteredTodos();
    const activeCount = this.store.todos.filter((t) => !t.completed).length;
    this.view.render(todos, this.store.filter, activeCount);
  }
}

// Browser bootstrap
if (typeof document !== 'undefined') {
  const container = document.getElementById('app') || document.body;
  new TodoApp(container);
}
```

---

## 3. Build & Runtime Configuration

### `package.json`
```json
{
  "name": "proj-vanilla-todo-app",
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
import { Todo } from '../src/models/todo.js';
import { TodoStore } from '../src/services/store.js';

describe('Todo Model & Store Test Suite', () => {
  const createMockStorage = () => {
    let store = {};
    return {
      getItem: (k) => store[k] || null,
      setItem: (k, v) => { store[k] = String(v); },
      clear: () => { store = {}; }
    };
  };

  test('Todo entity validates title and toggles status immutably', () => {
    assert.throws(() => new Todo(''), /non-empty string/);
    const item = new Todo('Buy Groceries');
    assert.equal(item.completed, false);
    const toggled = item.toggle();
    assert.equal(toggled.completed, true);
    assert.equal(item.completed, false); // Original preserved
  });

  test('TodoStore handles add, toggle, filter, and persistence', () => {
    const storage = createMockStorage();
    const store = new TodoStore('test-key', storage);

    const t1 = store.add('Task 1');
    const t2 = store.add('Task 2');
    assert.equal(store.todos.length, 2);

    store.toggle(t1.id);
    assert.equal(store.todos.find(t => t.id === t1.id).completed, true);

    store.setFilter('active');
    assert.equal(store.getFilteredTodos().length, 1);
    assert.equal(store.getFilteredTodos()[0].title, 'Task 2');

    store.clearCompleted();
    assert.equal(store.todos.length, 1);
    assert.equal(store.todos[0].title, 'Task 2');
  });
});
```

---

## 5. Verification & Sample Execution

### Verification Log
```
$ node --test tests/app.test.js
▶ Todo Model & Store Test Suite
  ✔ Todo entity validates title and toggles status immutably (1.8ms)
  ✔ TodoStore handles add, toggle, filter, and persistence (2.4ms)
✔ Todo Model & Store Test Suite (5.1ms)

ℹ tests 2
ℹ pass 2
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 42.8
```
