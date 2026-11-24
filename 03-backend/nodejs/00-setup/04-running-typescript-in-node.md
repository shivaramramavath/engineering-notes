# Running TypeScript in Node.js: Native Type Stripping vs Transpilation

> Comprehensive guide to executing TypeScript directly in Node.js 22 LTS / 24+ via native type stripping, comparison with `tsx` and `ts-node`, and production compilation strategies.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22.6.0+ (Experimental) / Node.js 22.18.0+ (Enabled by Default) |
| **Flags** | `--experimental-strip-types`, `--strip-types`, `--experimental-transform-types` |
| **Target tsconfig** | `"moduleResolution": "NodeNext"`, `"target": "ES2022"` |

---

## 1. Native Type Stripping Architecture

Node.js introduced native support for executing `.ts` files without requiring an external transpiler:
- **How it Works**: At runtime, Node.js parses TypeScript syntax and removes type annotations, interfaces, and type aliases by replacing their character spans with **whitespace**.
- **Preserved Source Maps**: Because type annotations are replaced with exact whitespace, line numbers and column offsets in runtime stack traces match the original `.ts` source file identically without external source map mappers.
- **No Type Checking**: Node.js **does NOT typecheck** code at runtime. It is purely an execution mechanism. Type checking must be performed separately in CI/CD via `tsc --noEmit`.

```
TypeScript Source (.ts)
       │
       ▼
[ Node.js Internal Parser (amaro) ]
       │
       ├── Strips types -> replaces with whitespace
       │
       ▼
JavaScript Engine (V8 Execution)
```

---

## 2. Supported vs. Unsupported Syntax

### Supported Syntax (Erasable Syntax)
- Type annotations: `const x: number = 42;`
- Function return types: `function greet(): string { ... }`
- Interfaces and Type Aliases: `interface User { id: string; }`
- Type Assertions: `val as string`
- `import type { Foo } from './foo.ts'`

### Unsupported Syntax (Code-Generating Features)
- `enum` declarations
- Namespaces (`namespace Foo { ... }`)
- Class parameter properties (`constructor(public id: string)`)
- Legacy experimental decorators

> **Note**: To run code containing `enum`s or namespaces, pass `--experimental-transform-types` to enable on-the-fly code generation.

---

## 3. Running TypeScript Across Node Versions

### Node.js 22.18.0+ / Node.js 24+ (Default Mode)
```bash
# Direct execution (No flags required!)
node server.ts

# Watch mode
node --watch server.ts
```

### Node.js 22.6.0 – 22.17.x
```bash
node --experimental-strip-types server.ts
```

### Alternative Runner: `tsx` (When Transform is Needed)
`tsx` uses `esbuild` under the hood and supports full JSX and non-erasable TypeScript:
```bash
npx tsx watch src/server.ts
```

---

## 4. Production Build vs Direct Execution Strategy

| Feature | Direct Node Execution (`node app.ts`) | Precompiled Build (`tsc` / `tsup`) |
| :--- | :--- | :--- |
| **Development Experience** | Instant startup, zero build step | Requires build watch daemon |
| **Production Deployment** | Requires Node 22+, slightly higher startup parse | Fast cold start, runs on any Node runtime |
| **Type Verification** | None (requires `tsc --noEmit` in CI) | Verified during compile step |
| **Best Used For** | Local dev, CLI tools, microservices | Enterprise production container images |
