# Editor, Debugger & Inspector Setup for Node.js

> Comprehensive guide to configuring VS Code debugging, Chrome DevTools profiling, `--inspect` flags, and source map resolution in modern Node.js backends.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Tools** | VS Code, Chrome DevTools (`chrome://inspect`), Node Inspector Protocol |
| **Flags** | `--inspect`, `--inspect-brk`, `--trace-warnings` |

---

## 1. The V8 Inspector Protocol

Node.js exposes an HTTP server and WebSocket endpoint conforming to the Chrome DevTools Protocol when launched with inspection flags:
- `--inspect`: Opens a debugger port (default `127.0.0.1:9229`) without interrupting execution.
- `--inspect-brk`: Opens the debugger port and **breaks on the very first line of user code**, allowing inspection before any asynchronous bootstrapping occurs.

```bash
# Start server with inspector attached
node --inspect server.js

# Break immediately at startup
node --inspect-brk server.js
```

Connecting via browser:
1. Open Google Chrome.
2. Navigate to `chrome://inspect`.
3. Under **Remote Target**, click **inspect** next to your Node.js process to open full Chrome DevTools (Console, Sources, Memory Profiler, CPU Profiler).

---

## 2. VS Code `launch.json` Configurations

Create `.vscode/launch.json` in your project root:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "node",
      "request": "launch",
      "name": "Launch Server (Node.js 22+ TypeScript)",
      "skipFiles": ["<node_internals>/**"],
      "program": "${workspaceFolder}/src/server.ts",
      "runtimeArgs": ["--experimental-strip-types", "--env-file=.env"],
      "console": "integratedTerminal"
    },
    {
      "type": "node",
      "request": "attach",
      "name": "Attach to Running Node Process",
      "port": 9229,
      "restart": true,
      "skipFiles": ["<node_internals>/**"]
    },
    {
      "type": "node",
      "request": "launch",
      "name": "Run Current Test File (node:test)",
      "program": "${file}",
      "runtimeArgs": ["--test"],
      "console": "integratedTerminal"
    }
  ]
}
```

---

## 3. Useful Diagnostic Flags in Development

```bash
# Print stack traces for process deprecation warnings
node --trace-deprecation server.js

# Trace unhandled promise rejections with full stack trace
node --trace-uncaught server.js

# Monitor event loop blockages and print warnings
node --trace-warnings server.js
```
