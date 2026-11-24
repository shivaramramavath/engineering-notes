# Single Executable Applications (SEA) in Node.js

> Building standalone native binaries from Node.js applications using the native `--build-sea` command and `node:sea` asset embedding.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Tooling** | `node --build-sea`, `node:sea` |
| **Target Output** | Standalone OS binary (Linux ELF, macOS Mach-O, Windows EXE) |

---

## 1. What Are Single Executable Applications?

Single Executable Applications (SEA) package your JavaScript/TypeScript application, its bundled dependencies, and runtime assets directly into the official `node` binary.
- **Self-Contained**: The user does not need Node.js or `npm` installed on their machine.
- **Fast Startup**: Zero module-resolution overhead or file lookup on disk.
- **Official Alternative**: Replaces legacy third-party tools like `pkg` and `nexe`.

---

## 2. Step-by-Step SEA Generation Workflow

### Step 1: Bundle Application Code
Use `esbuild` or `tsup` to bundle all dependencies into a single CommonJS or ESM file:
```bash
npx esbuild src/index.ts --bundle --platform=node --outfile=dist/bundle.js
```

### Step 2: Create Configuration (`sea-config.json`)
```json
{
  "main": "dist/bundle.js",
  "output": "dist/my-app",
  "disableExperimentalSEAWarning": true
}
```

### Step 3: Compile Binary via Node Native Flag
```bash
node --build-sea sea-config.json
```
This generates a compiled executable binary `dist/my-app` ready for distribution.

---

## 3. Embedding and Reading Assets (`node:sea`)

You can embed configuration files, templates, or HTML assets directly into the binary:

```json
{
  "main": "dist/bundle.js",
  "output": "dist/my-app",
  "assets": {
    "default-config": "assets/default-config.json"
  }
}
```

Inside your application code, read the embedded asset synchronously without touching the disk:
```javascript
import { getAsset } from 'node:sea';

const configBuffer = getAsset('default-config');
const configJson = JSON.parse(configBuffer.toString('utf8'));
console.log('Embedded config:', configJson);
```
