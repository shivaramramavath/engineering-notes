<!--
================================================================================
Metadata:
  Title: Reference Project 06 - Full-Stack Dev Environment with Docker Compose Watch
  Track: Docker / DevOps
  Difficulty: Intermediate
  Estimated Time: 55 minutes
  Prerequisites: 05-compose/07-compose-watch-and-dev-workflows.md, 04-volumes-and-networking/01-volumes-and-bind-mounts.md
  Key Terminology: Compose Watch, Hot Reloading, File Sync, Sync + Restart, Rebuild Action, React Vite, Node Express
================================================================================
-->

# Reference Project 06: Full-Stack Dev Environment with Docker Compose Watch

[<- 05-multi-stage-minimal-go-image.md](./05-multi-stage-minimal-go-image.md) | [Back to Projects Overview](./README.md) | [Next Project: 07-secure-hardened-image-pipeline.md ->](./07-secure-hardened-image-pipeline.md)

---

## 1. Overview & Architecture

Traditional containerized development workflows relied heavily on crude host bind mounts (`./src:/app/src`). This caused significant developer friction:
- Massive performance degradation on macOS and Windows due to filesystem virtualization (VirtioFS / gRPC-FUSE).
- `node_modules` pollution when host operating system binaries overwritten container Linux binaries.
- Needing to run manual `docker compose restart` commands whenever backend files changed.

**Docker Compose Watch** introduces native, high-performance file synchronization and event-driven automation directly into the Compose CLI. This project provides a full-stack local development environment (React Vite frontend + Node.js Express backend + PostgreSQL) leveraging:
1. `action: sync` for instant browser Hot Module Replacement (HMR).
2. `action: sync+restart` for backend server process reloads without tearing down the container.
3. `action: rebuild` to trigger automated container image rebuilds when `package.json` dependencies change.

### Architectural Diagram

```
+-----------------------------------------------------------------------------------+
|                                 DEVELOPER HOST                                    |
|                                                                                   |
|  [Developer edits code in IDE]                                                    |
|  - frontend/src/App.tsx                                                           |
|  - backend/src/server.js                                                          |
|  - package.json                                                                   |
|                                                                                   |
|  DOCKER COMPOSE WATCH CLI ENGINE                                                  |
|  +-----------------------------------------------------------------------------+  |
|  | Event: frontend/src/* changes  ---> ACTION: sync (Instantly inject into FS) |  |
|  | Event: backend/src/* changes   ---> ACTION: sync+restart (Restart server)   |  |
|  | Event: package.json changes    ---> ACTION: rebuild (Rebuild layer cache)   |  |
|  +-----------------------------------------------------------------------------+  |
|                                       |                                           |
|                                       v                                           |
+-----------------------------------------------------------------------------------+
|                            DOCKER COMPOSE RUNTIME                                 |
|                                                                                   |
|  +---------------------------------+     +-------------------------------------+  |
|  |       SERVICE: client-ui        |     |         SERVICE: api-server         |  |
|  |  React + Vite Dev Server        |     |  Node.js Express + Nodemon          |  |
|  |  Port: 5173 (HMR WebSocket)     |     |  Port: 4000                         |  |
|  |  Synced: /app/src               |     |  Synced: /app/src                   |  |
|  +----------------+----------------+     +------------------+------------------+  |
|                   |                                         |                     |
|                   +-------------------+---------------------+                     |
|                                       |                                           |
|                                       v                                           |
|                          +--------------------------+                             |
|                          |   SERVICE: postgres-db   |                             |
|                          |   PostgreSQL 16          |                             |
|                          |   Volume: dev_db_data    |                             |
|                          +--------------------------+                             |
+-----------------------------------------------------------------------------------+
```

---

## 2. Directory Structure & File Listing

```
fullstack-compose-watch/
├── compose.yaml
├── backend/
│   ├── Dockerfile
│   ├── package.json
│   └── src/
│       └── server.js
├── frontend/
│   ├── Dockerfile
│   ├── index.html
│   ├── package.json
│   ├── vite.config.js
│   └── src/
│       ├── App.jsx
│       └── main.jsx
└── init-db/
    └── 01-init.sql
```

---

## 3. Complete Implementation

### File: `backend/package.json`

```json
{
  "name": "watch-backend",
  "version": "1.0.0",
  "main": "src/server.js",
  "scripts": {
    "start": "node src/server.js"
  },
  "dependencies": {
    "express": "^4.19.2",
    "pg": "^8.11.5"
  }
}
```

### File: `backend/src/server.js`

```javascript
const express = require('express');
const { Pool } = require('pg');

const app = express();
const port = process.env.PORT || 4000;

const pool = new Pool({
  host: process.env.DB_HOST || 'postgres-db',
  port: 5432,
  user: process.env.DB_USER || 'devuser',
  password: process.env.DB_PASSWORD || 'devsecret',
  database: process.env.DB_NAME || 'devdb',
});

app.use(express.json());

// Enable CORS for local dev
app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Headers', 'Origin, X-Requested-With, Content-Type, Accept');
  next();
});

app.get('/api/message', async (req, res) => {
  try {
    const result = await pool.query('SELECT message FROM greetings ORDER BY id DESC LIMIT 1');
    const dbMsg = result.rows[0]?.message || 'No DB message found';
    res.json({ message: 'Hello from Node.js Express Backend!', db_message: dbMsg });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

app.listen(port, '0.0.0.0', () => {
  console.log(`Backend server listening at http://0.0.0.0:${port}`);
});
```

### File: `backend/Dockerfile`

```dockerfile
FROM node:20-alpine

WORKDIR /app

COPY package*.json ./
RUN npm install

COPY src/ ./src/

EXPOSE 4000
CMD ["npm", "start"]
```

### File: `frontend/package.json`

```json
{
  "name": "watch-frontend",
  "version": "1.0.0",
  "scripts": {
    "dev": "vite --host 0.0.0.0"
  },
  "dependencies": {
    "react": "^18.3.1",
    "react-dom": "^18.3.1"
  },
  "devDependencies": {
    "@vitejs/plugin-react": "^4.2.1",
    "vite": "^5.2.11"
  }
}
```

### File: `frontend/vite.config.js`

```javascript
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    host: '0.0.0.0',
    watch: {
      usePolling: true, // Guarantees file change detection inside container
    },
  },
});
```

### File: `frontend/index.html`

```html
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <title>Compose Watch Dev Demo</title>
  </head>
  <body>
    <div id="root"></div>
    <script type="module" src="/src/main.jsx"></script>
  </body>
</html>
```

### File: `frontend/src/main.jsx`

```javascript
import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';

ReactDOM.createRoot(document.getElementById('root')).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);
```

### File: `frontend/src/App.jsx`

```javascript
import React, { useState, useEffect } from 'react';

export default function App() {
  const [data, setData] = useState({ message: 'Loading...', db_message: '' });

  useEffect(() => {
    fetch('http://localhost:4000/api/message')
      .then((res) => res.json())
      .then((json) => setData(json))
      .catch((err) => setData({ message: 'Error: ' + err.message }));
  }, []);

  return (
    <div style={{ fontFamily: 'sans-serif', textAlign: 'center', marginTop: '60px' }}>
      <h1>Docker Compose Watch Live Demo</h1>
      <p style={{ color: '#0284c7', fontSize: '18px' }}>{data.message}</p>
      <p style={{ color: '#16a34a' }}>Database Message: {data.db_message}</p>
      <div style={{ padding: '20px', border: '1px solid #ccc', display: 'inline-block' }}>
        <em>Edit <code>frontend/src/App.jsx</code> to test instant browser Hot Reloading!</em>
      </div>
    </div>
  );
}
```

### File: `frontend/Dockerfile`

```dockerfile
FROM node:20-alpine

WORKDIR /app

COPY package*.json ./
RUN npm install

COPY . .

EXPOSE 5173
CMD ["npm", "run", "dev"]
```

### File: `init-db/01-init.sql`

```sql
CREATE TABLE IF NOT EXISTS greetings (
    id SERIAL PRIMARY KEY,
    message VARCHAR(255) NOT NULL
);

INSERT INTO greetings (message) VALUES ('Greetings from PostgreSQL inside Docker!');
```

### File: `compose.yaml`

```yaml
version: "3.8"

services:
  client-ui:
    build:
      context: ./frontend
      dockerfile: Dockerfile
    ports:
      - "5173:5173"
    develop:
      watch:
        # Instant frontend sync (HMR)
        - action: sync
          path: ./frontend/src
          target: /app/src
          ignore:
            - node_modules/
        # Rebuild container if package dependencies change
        - action: rebuild
          path: ./frontend/package.json

  api-server:
    build:
      context: ./backend
      dockerfile: Dockerfile
    ports:
      - "4000:4000"
    environment:
      - PORT=4000
      - DB_HOST=postgres-db
      - DB_USER=devuser
      - DB_PASSWORD=devsecret
      - DB_NAME=devdb
    depends_on:
      postgres-db:
        condition: service_healthy
    develop:
      watch:
        # Sync backend files and automatically restart Node.js server
        - action: sync+restart
          path: ./backend/src
          target: /app/src
        # Rebuild if package.json updates
        - action: rebuild
          path: ./backend/package.json

  postgres-db:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: devuser
      POSTGRES_PASSWORD: devsecret
      POSTGRES_DB: devdb
    volumes:
      - dev_db_data:/var/lib/postgresql/data
      - ./init-db:/docker-entrypoint-initdb.d:ro
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U devuser -d devdb"]
      interval: 5s
      timeout: 3s
      retries: 5
    ports:
      - "5432:5432"

volumes:
  dev_db_data:
```

---

## 4. Step-by-Step Verification & Testing

### Step 1: Launch Stack with Compose Watch

```bash
mkdir -p fullstack-compose-watch/backend/src fullstack-compose-watch/frontend/src fullstack-compose-watch/init-db
cd fullstack-compose-watch

# Start containers and attach file watcher
docker compose watch
```

*Expected Terminal Log*:
```
[+] Building 2.1s (18/18) FINISHED
[+] Running 4/4
 ✔ Network fullstack-compose-watch_default          Created
 ✔ Container fullstack-compose-watch-postgres-db-1  Healthy
 ✔ Container fullstack-compose-watch-api-server-1   Started
 ✔ Container fullstack-compose-watch-client-ui-1    Started
Watch enabled
```

### Step 2: Verify Initial Response

Open browser at `http://localhost:5173` or run:
```bash
curl -s http://localhost:4000/api/message
# Output: {"message":"Hello from Node.js Express Backend!","db_message":"Greetings from PostgreSQL inside Docker!"}
```

### Step 3: Test Real-Time Frontend Sync

Modify `frontend/src/App.jsx` on the host:
Change `<h1>Docker Compose Watch Live Demo</h1>` to `<h1>Docker Compose Watch Live Demo - UPDATED!</h1>`.

Look at the `docker compose watch` terminal:
```
Watch action: sync applied for frontend/src/App.jsx
```
*The browser immediately updates via Vite HMR without a page reload!*

### Step 4: Test Backend Sync + Restart

Modify `backend/src/server.js`:
Change `"Hello from Node.js Express Backend!"` to `"Hello from HOT RELOADED Express Backend!"`.

Terminal output:
```
Watch action: sync+restart applied for backend/src/server.js
Container fullstack-compose-watch-api-server-1 restarted
```

Query API again:
```bash
curl -s http://localhost:4000/api/message
# Output: {"message":"Hello from HOT RELOADED Express Backend!","db_message":"..."}
```

### Step 5: Teardown

```bash
# In another terminal window:
docker compose down -v
```

---

## 5. Production Considerations

1. **Development Only Feature**: Never use `compose watch` in staging or production. In production, images should be immutable and deployed via standard tags.
2. **Exclusion of `node_modules`**: Notice `ignore: ["node_modules/"]` in `compose.yaml`. This ensures host filesystem dependencies never overwrite container-installed packages.
3. **Rebuild Efficiency**: When `package.json` triggers `action: rebuild`, BuildKit utilizes its layer cache, reinstalling only newly added packages in seconds.

---

[<- 05-multi-stage-minimal-go-image.md](./05-multi-stage-minimal-go-image.md) | [Back to Projects Overview](./README.md) | [Next Project: 07-secure-hardened-image-pipeline.md ->](./07-secure-hardened-image-pipeline.md)
