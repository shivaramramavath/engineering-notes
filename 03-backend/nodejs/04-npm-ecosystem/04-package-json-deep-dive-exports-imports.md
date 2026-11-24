# `package.json` Deep Dive: `exports`, `imports` & `engines`

> Mastering modern package definitions, conditional subpath exports, private subpath imports (`#`), and strict engine enforcement in Node.js.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Fields** | `"exports"`, `"imports"`, `"engines"`, `"type": "module"` |
| **Level** | Modern Tooling |

---

## 1. Subpath Exports (`"exports"`)

The `"exports"` field supersedes `"main"`. It provides:
1. **Encapsulation**: Files not explicitly listed in `"exports"` **cannot be imported by external consumers** (preventing illegal deep imports into internal files like `./lib/internal-helper.js`).
2. **Conditional Exports**: Mapping different build targets based on the consumer's module system (`import` vs `require`) or environment (`types`, `node`, `default`).

```json
{
  "name": "my-shared-library",
  "version": "1.0.0",
  "type": "module",
  "exports": {
    ".": {
      "types": "./dist/index.d.ts",
      "import": "./dist/index.js",
      "require": "./dist/index.cjs"
    },
    "./utils": {
      "types": "./dist/utils.d.ts",
      "default": "./dist/utils.js"
    }
  }
}
```

---

## 2. Subpath Imports (`"imports"` with `#`)

Node.js supports internal package-private aliases without requiring TypeScript path mapping or Babel plugins. All private imports start with `#`:

```json
{
  "name": "backend-api",
  "type": "module",
  "imports": {
    "#db": "./src/database/client.js",
    "#config": "./src/config/index.js",
    "#utils/*": "./src/utils/*.js"
  }
}
```

Now, anywhere in your application:
```javascript
// Clean alias without ../../../ relative path nesting!
import { db } from '#db';
import { formatDate } from '#utils/date.js';
```

---

## 3. Strict Engines & Provenance

```json
{
  "engines": {
    "node": ">=22.0.0",
    "npm": ">=10.0.0"
  },
  "publishConfig": {
    "access": "public",
    "provenance": true
  }
}
```
- **`provenance: true`**: Links your published npm package directly to the GitHub Actions build run via OpenID Connect (OIDC) and cryptographic Sigstore signatures, proving package authenticity.
