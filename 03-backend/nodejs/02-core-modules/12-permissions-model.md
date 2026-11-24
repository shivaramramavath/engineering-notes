# The Node.js Permission Model

> Comprehensive guide to configuring and querying the built-in Node.js security permission model to restrict filesystem, process, and worker access.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22.13.0+ / 23.5.0+ (Stable) |
| **CLI Flags** | `--permission`, `--allow-fs-read`, `--allow-fs-write`, `--allow-child-process` |
| **Runtime API** | `process.permission.has()` |

---

## 1. Architectural Intent: Seat Belt Security

The Node.js Permission Model is an authorization mechanism that acts as a seat belt for your application:
- **Blast Radius Reduction**: In modern backend applications with hundreds of transitive npm dependencies, a compromised package could attempt to read `/etc/passwd`, steal `.env` credentials, or spawn reverse shells.
- **Explicit Grant Principle**: When started with `--permission`, all sensitive system calls (file I/O, child processes, worker threads, native addons) are blocked by default. You must explicitly whitelist only the resources your application legitimately needs.
- **Important Boundary Rule**: The permission model is designed to contain trusted code and defend against supply-chain exploits; it is **NOT** a sandbox for running arbitrary malicious user code.

---

## 2. CLI Flags Reference

| Flag | Purpose | Example |
| :--- | :--- | :--- |
| `--permission` | Enables the permission model (blocks all unauthorized access) | `node --permission app.js` |
| `--allow-fs-read=<path>` | Whitelists paths for file reading (wildcards supported) | `node --permission --allow-fs-read=./config,./public app.js` |
| `--allow-fs-write=<path>` | Whitelists paths for file writing | `node --permission --allow-fs-write=./logs app.js` |
| `--allow-child-process` | Permits spawning child processes (`child_process.spawn`) | `node --permission --allow-child-process app.js` |
| `--allow-worker` | Permits spawning worker threads (`worker_threads`) | `node --permission --allow-worker app.js` |
| `--allow-addons` | Permits loading native C++ addons | `node --permission --allow-addons app.js` |

---

## 3. Querying Permissions at Runtime

You can inspect granted capabilities programmatically using `process.permission`:

```javascript
// Check read permission before attempting file access
if (process.permission.has('fs.read', './data/cache.json')) {
  console.log('Read permission granted');
} else {
  console.warn('Read permission denied. Falling back to in-memory cache.');
}

// Check child process permission
if (process.permission.has('child')) {
  // Safe to spawn external utility
}
```

---

## 4. Production Deployment Example

Run an Express service with permission restrictions in Docker:
```dockerfile
CMD ["node", "--permission", "--allow-fs-read=./dist,./node_modules", "--allow-fs-write=./tmp", "dist/server.js"]
```
If any dependency attempts to access files outside `./dist` or spawn child processes, Node.js throws an immediate `ERR_ACCESS_DENIED` and prevents data exfiltration.
