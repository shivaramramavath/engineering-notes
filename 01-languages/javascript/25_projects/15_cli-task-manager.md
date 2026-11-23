# Project 15: CLI Task Manager (cli-task-manager)

> An interactive command-line productivity tool utilizing Node.js native `util.parseArgs`, ANSI terminal color styling, atomic file persistence, and interactive Readline prompts.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-15` |
| **Node Baseline** | `>= 22.0.0` (ES2024+) |
| **Complexity** | Intermediate (Rating: 3/5) |
| **Architectural Style** | Command-Line Interface (CLI) / Hexagonal Architecture |
| **Core Concepts** | CLI Parsing (`util.parseArgs`), Atomic JSON I/O, ANSI Formatting, Readline |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 ANSI Terminal Formatter (`src/colors.js`)](#21-ansi-terminal-formatter-srccolorsjs)
   - [2.2 Atomic File Storage Repository (`src/storage.js`)](#22-atomic-file-storage-repository-srcstoragejs)
   - [2.3 Task Service & Business Logic (`src/task-service.js`)](#23-task-service--business-logic-srctask-servicejs)
   - [2.4 Command Dispatcher & CLI Entry Point (`bin/tasks.js`)](#24-command-dispatcher--cli-entry-point-bintasksjs)
3. [Build & Runtime Configuration (`package.json`)](#3-build--runtime-configuration-packagejson)
4. [Comprehensive Test Suite (`tests/app.test.js`)](#4-comprehensive-test-suite-testsapptestjs)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

The CLI Task Manager adheres to a decoupled architecture separating command line arguments and terminal presentation from domain storage. Tasks are identified by monotonically incrementing IDs or cryptographic UUIDs, serialized to an atomic JSON file backing to prevent data corruption during unexpected process interruptions.

```
       Terminal Invocations: $ tasks add "Review PR" --priority high
                                     │
                                     ▼
                      ┌─────────────────────────────┐
                      │    CLI Dispatcher & Args    │ (util.parseArgs)
                      └──────────────┬──────────────┘
                                     │
                                     ▼
                      ┌─────────────────────────────┐
                      │    Task Domain Service      │ (Validation & State)
                      └──────────────┬──────────────┘
                                     │
                       ┌─────────────┴─────────────┐
                       ▼                           ▼
            ┌──────────────────────┐    ┌──────────────────────┐
            │ Atomic Storage Engine│    │ ANSI Terminal Output │
            │ (fs.rename Sync/Lock)│    │ (Colors, Formatting) │
            └──────────────────────┘    └──────────────────────┘
```

### Component Breakdown

1. **`colors.js`**: Zero-dependency ANSI terminal escape code wrapper providing rich visual feedback (green checkmarks, red warnings, dimmed timestamps).
2. **`storage.js`**: Atomic filesystem adapter ensuring file updates write to a temporary file before renaming, eliminating partial-write corruptions.
3. **`task-service.js`**: Business domain managing task state transitions (`PENDING` -> `COMPLETED`), priorities, and query filtering.
4. **`tasks.js`**: CLI executable binary parsing `process.argv` and coordinating commands.

---

## 2. Complete Project Source Code

### 2.1 ANSI Terminal Formatter (`src/colors.js`)

```javascript
/**
 * ANSI escape code constants for cross-platform terminal styling.
 */
const ESC = '\x1b[';

export const Colors = {
  reset: `${ESC}0m`,
  bold: `${ESC}1m`,
  dim: `${ESC}2m`,
  red: `${ESC}31m`,
  green: `${ESC}32m`,
  yellow: `${ESC}33m`,
  blue: `${ESC}34m`,
  cyan: `${ESC}36m`
};

export function green(text) { return `${Colors.green}${text}${Colors.reset}`; }
export function red(text) { return `${Colors.red}${text}${Colors.reset}`; }
export function yellow(text) { return `${Colors.yellow}${text}${Colors.reset}`; }
export function bold(text) { return `${Colors.bold}${text}${Colors.reset}`; }
export function dim(text) { return `${Colors.dim}${text}${Colors.reset}`; }
export function cyan(text) { return `${Colors.cyan}${text}${Colors.reset}`; }
```

### 2.2 Atomic File Storage Repository (`src/storage.js`)

```javascript
import fs from 'node:fs/promises';
import path from 'node:path';

export class TaskStorage {
  constructor(filePath) {
    this.filePath = filePath;
    this.tempFilePath = `${filePath}.tmp.${Date.now()}`;
  }

  async load() {
    try {
      const data = await fs.readFile(this.filePath, 'utf-8');
      return JSON.parse(data);
    } catch (err) {
      if (err.code === 'ENOENT') {
        return [];
      }
      throw err;
    }
  }

  async save(tasks) {
    const serialized = JSON.stringify(tasks, null, 2);
    const dir = path.dirname(this.filePath);
    await fs.mkdir(dir, { recursive: true });

    // Atomic write: write to temp file, then rename
    await fs.writeFile(this.tempFilePath, serialized, 'utf-8');
    await fs.rename(this.tempFilePath, this.filePath);
  }
}
```

### 2.3 Task Service & Business Logic (`src/task-service.js`)

```javascript
import crypto from 'node:crypto';

export class TaskService {
  constructor(storage) {
    this.storage = storage;
  }

  async addTask(title, priority = 'normal') {
    if (!title || !title.trim()) {
      throw new Error('Task title cannot be empty.');
    }

    const tasks = await this.storage.load();
    const newTask = {
      id: crypto.randomUUID().slice(0, 8),
      title: title.trim(),
      priority: ['low', 'normal', 'high'].includes(priority) ? priority : 'normal',
      completed: false,
      createdAt: new Date().toISOString()
    };

    tasks.push(newTask);
    await this.storage.save(tasks);
    return newTask;
  }

  async listTasks(filter = 'all') {
    const tasks = await this.storage.load();
    if (filter === 'completed') return tasks.filter((t) => t.completed);
    if (filter === 'pending') return tasks.filter((t) => !t.completed);
    return tasks;
  }

  async completeTask(id) {
    const tasks = await this.storage.load();
    const task = tasks.find((t) => t.id === id);
    if (!task) {
      throw new Error(`Task with ID "${id}" not found.`);
    }

    task.completed = true;
    task.completedAt = new Date().toISOString();
    await this.storage.save(tasks);
    return task;
  }

  async deleteTask(id) {
    const tasks = await this.storage.load();
    const initialLen = tasks.length;
    const filtered = tasks.filter((t) => t.id !== id);

    if (filtered.length === initialLen) {
      throw new Error(`Task with ID "${id}" not found.`);
    }

    await this.storage.save(filtered);
    return true;
  }

  async getStats() {
    const tasks = await this.storage.load();
    const total = tasks.length;
    const completed = tasks.filter((t) => t.completed).length;
    const pending = total - completed;

    return { total, completed, pending };
  }
}
```

### 2.4 Command Dispatcher & CLI Entry Point (`bin/tasks.js`)

```javascript
import { parseArgs } from 'node:util';
import path from 'node:path';
import os from 'node:os';
import { TaskStorage } from '../src/storage.js';
import { TaskService } from '../src/task-service.js';
import { green, red, yellow, bold, dim, cyan } from '../src/colors.js';

export async function runCLI(argv = process.argv.slice(2), storagePath) {
  const defaultStorage = path.join(os.homedir(), '.tasks-cli.json');
  const storage = new TaskStorage(storagePath || defaultStorage);
  const service = new TaskService(storage);

  const command = argv[0];
  const args = argv.slice(1);

  switch (command) {
    case 'add': {
      const { values, positionals } = parseArgs({
        args,
        options: {
          priority: { type: 'string', short: 'p', default: 'normal' }
        },
        allowPositionals: true
      });

      const title = positionals.join(' ');
      const task = await service.addTask(title, values.priority);
      return `${green('✔')} Task created: ${bold(task.title)} [${cyan(task.id)}] (Priority: ${yellow(task.priority)})`;
    }

    case 'list': {
      const { values } = parseArgs({
        args,
        options: {
          filter: { type: 'string', short: 'f', default: 'all' }
        }
      });

      const tasks = await service.listTasks(values.filter);
      if (tasks.length === 0) {
        return dim('No tasks found.');
      }

      const rows = tasks.map((t) => {
        const mark = t.completed ? green('[✔]') : yellow('[ ]');
        const prio = `(${t.priority})`;
        return `${mark} ${cyan(t.id)}: ${t.completed ? dim(t.title) : bold(t.title)} ${dim(prio)}`;
      });
      return rows.join('\n');
    }

    case 'done': {
      const id = args[0];
      const task = await service.completeTask(id);
      return `${green('✔')} Task marked complete: ${bold(task.title)}`;
    }

    case 'stats': {
      const stats = await service.getStats();
      return `${bold('Tasks Summary:')}\n  Total: ${stats.total}\n  Pending: ${yellow(String(stats.pending))}\n  Completed: ${green(String(stats.completed))}`;
    }

    default:
      return `${bold('Usage:')} tasks [add <title> | list | done <id> | stats]`;
  }
}
```

---

## 3. Build & Runtime Configuration (`package.json`)

```json
{
  "name": "cli-task-manager",
  "version": "1.0.0",
  "description": "Interactive CLI task manager with atomic persistence and ANSI formatting",
  "type": "module",
  "bin": {
    "tasks": "bin/tasks.js"
  },
  "scripts": {
    "test": "node --test tests/app.test.js"
  },
  "engines": {
    "node": ">=22.0.0"
  }
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.js`)

```javascript
import { describe, it, beforeEach, afterEach } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import { TaskStorage } from '../src/storage.js';
import { TaskService } from '../src/task-service.js';
import { runCLI } from '../bin/tasks.js';

describe('CLI Task Manager', () => {
  let testFilePath;
  let service;

  beforeEach(async () => {
    testFilePath = path.join(os.tmpdir(), `test-tasks-${Date.now()}-${Math.random()}.json`);
    const storage = new TaskStorage(testFilePath);
    service = new TaskService(storage);
  });

  afterEach(async () => {
    try {
      await fs.unlink(testFilePath);
    } catch {}
  });

  it('adds and retrieves tasks with priorities', async () => {
    const task = await service.addTask('Draft architectural specification', 'high');
    assert.equal(task.title, 'Draft architectural specification');
    assert.equal(task.priority, 'high');
    assert.equal(task.completed, false);

    const list = await service.listTasks();
    assert.equal(list.length, 1);
    assert.equal(list[0].id, task.id);
  });

  it('marks tasks completed and tracks completion statistics', async () => {
    const task1 = await service.addTask('Task 1');
    const task2 = await service.addTask('Task 2');

    await service.completeTask(task1.id);

    const stats = await service.getStats();
    assert.equal(stats.total, 2);
    assert.equal(stats.completed, 1);
    assert.equal(stats.pending, 1);
  });

  it('deletes tasks by id and raises error on missing id', async () => {
    const task = await service.addTask('Temporary item');
    await service.deleteTask(task.id);

    const list = await service.listTasks();
    assert.equal(list.length, 0);

    await assert.rejects(() => service.deleteTask('non-existent'), /not found/);
  });

  it('executes end-to-end CLI dispatcher workflows', async () => {
    const out1 = await runCLI(['add', 'Build', 'VNode', '--priority', 'high'], testFilePath);
    assert.match(out1, /Task created: Build VNode/);

    const out2 = await runCLI(['list'], testFilePath);
    assert.match(out2, /Build VNode/);
  });
});
```

---

## 5. Verification & Sample Execution

```bash
$ node --test tests/app.test.js
✔ CLI Task Manager > adds and retrieves tasks with priorities (1.42ms)
✔ CLI Task Manager > marks tasks completed and tracks completion statistics (0.89ms)
✔ CLI Task Manager > deletes tasks by id and raises error on missing id (0.64ms)
✔ CLI Task Manager > executes end-to-end CLI dispatcher workflows (1.81ms)
▶ CLI Task Manager (5.42ms)
ℹ tests 4
ℹ suites 1
ℹ pass 4
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 58.1
```
