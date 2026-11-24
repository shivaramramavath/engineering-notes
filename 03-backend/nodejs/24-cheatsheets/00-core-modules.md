# Cheatsheet 00: Node.js Core Modules Reference

> Quick-reference syntax and common methods for Node.js 22 LTS / 24+ core modules (`node:*`).

---

### 1. `node:fs/promises` (File System)
```javascript
import fs from 'node:fs/promises';

// Read / Write
const text = await fs.readFile('data.txt', 'utf8');
const json = JSON.parse(await fs.readFile('config.json', 'utf8'));
await fs.writeFile('output.txt', 'Hello Node.js\n', 'utf8');
await fs.appendFile('log.txt', 'New entry\n');

// Directory & Stats
await fs.mkdir('nested/folder', { recursive: true });
const entries = await fs.readdir('dir', { withFileTypes: true });
const stats = await fs.stat('file.txt'); // stats.isFile(), stats.size
await fs.rm('dist', { recursive: true, force: true });
```

---

### 2. `node:path` (Path Resolution)
```javascript
import path from 'node:path';

// ESM Directory Resolution (Node.js 20.11+ / 22+)
const currentDir = import.meta.dirname;
const currentFile = import.meta.filename;

// Path Operations
path.join('/app', 'src', 'index.js');          // '/app/src/index.js'
path.resolve('src', 'index.js');              // Absolute path from cwd
path.extname('photo.png');                    // '.png'
path.basename('/data/file.txt');              // 'file.txt'
path.basename('/data/file.txt', '.txt');      // 'file'
path.dirname('/app/src/index.js');            // '/app/src'
```

---

### 3. `node:crypto` (Hashing & Random)
```javascript
import crypto from 'node:crypto';

// Fast SHA-256 Hash
const hash = crypto.createHash('sha256').update('password123').digest('hex');

// HMAC-SHA256 Signature
const hmac = crypto.createHmac('sha256', 'secret-key').update('payload').digest('hex');

// Timing-safe Equality (Anti-Timing Attack)
const match = crypto.timingSafeEqual(Buffer.from(sigA, 'hex'), Buffer.from(sigB, 'hex'));

// Cryptographic Randomness
const uuid = crypto.randomUUID();             // Native UUIDv4
const token = crypto.randomBytes(32).toString('hex');
const num = crypto.randomInt(1, 100);          // Inclusive 1, exclusive 100
```

---

### 4. `node:sqlite` (Native SQLite in Node 22+)
```javascript
import { DatabaseSync } from 'node:sqlite';

const db = new DatabaseSync(':memory:'); // or path to .db file

// Execute DDL
db.exec(`CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT)`);

// Prepared Statements
const insert = db.prepare('INSERT INTO users (name, email) VALUES (?, ?)');
insert.run('Grace Hopper', 'grace@navy.mil');

// Queries
const query = db.prepare('SELECT * FROM users WHERE id = ?');
const user = query.get(1); // Returns object
const allUsers = db.prepare('SELECT * FROM users').all(); // Returns array of objects
```

---

### 5. `node:util` (`parseArgs` CLI Flags)
```javascript
import { parseArgs } from 'node:util';

const { values, positionals } = parseArgs({
  options: {
    port: { type: 'string', short: 'p', default: '3000' },
    verbose: { type: 'boolean', short: 'v', default: false },
  },
  allowPositionals: true,
});
// values.port, values.verbose, positionals[0]
```

---

### 6. `node:async_hooks` (`AsyncLocalStorage`)
```javascript
import { AsyncLocalStorage } from 'node:async_hooks';

const als = new AsyncLocalStorage();

// Wrap request execution:
als.run({ requestId: 'req_123' }, () => {
  doWork();
});

function doWork() {
  const store = als.getStore(); // { requestId: 'req_123' }
}
```
