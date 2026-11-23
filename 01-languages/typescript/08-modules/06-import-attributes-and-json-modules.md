# Import Attributes and JSON Modules

> Standardized in ECMAScript and TypeScript 5.3+, import attributes (`with { type: "json" }`) guarantee secure, type-checked asset loading without MIME-type confusion or arbitrary code execution vulnerabilities.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-imports-and-exports.md](file:///E:/Learning/engineering-notes/01-languages/typescript/08-modules/00-imports-and-exports.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 5.3 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `module: NodeNext`, `resolveJsonModule: true` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-Common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
JavaScript engines originally executed imported files based on server MIME types. If a developer imported a JSON configuration:
```javascript
import config from "./config.json";
```
An attacker who manipulated server headers could serve executable JavaScript under the `.json` URL, causing arbitrary script execution.

To fix this security vulnerability, TC39 standardized **Import Attributes** (formerly Import Assertions). By appending an attribute clause:
```typescript
import config from "./config.json" with { type: "json" };
```
The runtime JavaScript engine (V8, Node.js, Deno, browsers) strictly verifies that the response is JSON and refuses to execute it as JavaScript code.

TypeScript 5.3 updated its syntax from the obsolete `assert { type: "json" }` to the official ECMAScript Stage 3 standard: **`with { type: "json" }`**.

---

## 2. Core Concepts

### Static Import Attributes Syntax
```typescript
import packageConfig from "./package.json" with { type: "json" };

console.log(packageConfig.version); // Strictly typed from JSON structure!
```

### Dynamic Import Attributes Syntax
```typescript
async function loadTranslations(lang: string) {
  const data = await import(`./locales/${lang}.json`, {
    with: { type: "json" }
  });
  return data.default;
}
```

### Migration from `assert` to `with`
TypeScript 4.5 supported the older proposal syntax `assert { type: "json" }`. In TypeScript 5.3, this syntax was deprecated in favor of `with`. Modern Node.js runtimes (Node 22 LTS) warn or reject the legacy `assert` keyword.

---

## 3. How It Works Internally

```
Source: import pkg from "./pkg.json" with { type: "json" };
                          |
                          v
         Is resolveJsonModule: true enabled?
                   /             \
                 Yes              No (TS2732: Cannot find module)
                 /
  Parse JSON AST & Intern Inferred Object Type:
  {
    readonly name: "app",
    readonly version: "1.0.0",
    readonly private: true
  }
                 |
                 v
  Emit to JavaScript with preserved 'with { type: "json" }'
```

When `resolveJsonModule: true` is configured:
1. The TypeScript parser reads the JSON file from disk into a `JsonSourceFile` AST.
2. The type checker infers literal types for strings, numbers, booleans, and arrays inside the JSON document.
3. The emitter outputs the exact import statement with `with { type: "json" }` intact.

---

## 4. Syntax & API Reference

### `tsconfig.json` Requirements
```json
{
  "compilerOptions": {
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "resolveJsonModule": true
  }
}
```

---

## 5. Practical Examples

### 5.1 Reading Package Metadata
```typescript
import pkg from "../package.json" with { type: "json" };

export function getAppVersion(): string {
  return pkg.version;
}
```

### 5.2 Real-World Application: Dynamic Feature Flag Loader
```typescript
interface FeatureFlags {
  enableBetaUi: boolean;
  maxFileUploadMb: number;
}

async function loadFlags(): Promise<FeatureFlags> {
  const module = await import("./flags.json", {
    with: { type: "json" }
  });
  return module.default;
}
```

### 5.3 Edge Case: Re-Exporting JSON with Attributes
```typescript
export { default as configData } from "./config.json" with { type: "json" };
```

### 5.4 Failure Case: Omitting `with` in NodeNext
```typescript
// Under "module": "NodeNext" in modern Node runtimes:
// @ts-expect-error TS2821: Import assertion or attribute of type 'json' is required to import JSON files when 'moduleResolution' is 'node16' or 'nodenext'.
import data from "./data.json";
```

### 5.5 Idiomatic Pattern: Type Assertion on Generic JSON
```typescript
import rawSchema from "./schema.json" with { type: "json" };

interface ValidatedSchema {
  id: string;
  fields: string[];
}

const schema = rawSchema as ValidatedSchema;
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Mandatory in NodeNext**: When compiling for `NodeNext` or `Node16`, omitting `with { type: "json" }` on JSON imports triggers compiler error `TS2821`.
2. **Readonly Inferences**: Property types inferred from JSON modules are treated as literal constants if the JSON file is part of the build root.
3. **JSON Files in OutDir**: When TypeScript compiles a project containing JSON imports, the JSON files are copied directly into the `outDir` preserving directory hierarchy.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Omitting `with { type: "json" }` | Rejected by modern Node.js ESM loaders | Add `with { type: "json" }` | `TS2821: Import assertion or attribute required...` |
| Using deprecated `assert` keyword | Fails in newer Node.js releases | Use standardized `with` keyword | Deprecation warning / Runtime SyntaxError |
| Forgetting `resolveJsonModule: true` | TypeScript refuses to parse `.json` files | Enable `"resolveJsonModule": true` in tsconfig | `TS2732: Cannot find module '*.json'...` |
| Importing massive JSON files | Explodes compiler memory and output bundle | Fetch asynchronously or read with `fs.readFile` | Out of memory / slow type checks |
| Mutating imported JSON object properties | Inferred JSON types are immutable and cached | Clone before modifying: `{ ...data }` | `TS2540: Cannot assign to read-only property` |
| Using import attributes for non-standard types | Runtime engines currently only support `"json"` | Keep custom asset loading to bundlers | Runtime `TypeError: Unsupported attribute` |

---

## 8. Performance & Type-Checker Complexity
- Large JSON files (e.g. 10MB GeoJSON or database dumps) create massive AST symbol tables.
- Never import multi-megabyte JSON files with static imports. Read them with streaming file I/O (`fs.createReadStream`).

---

## 9. Compiler Internals
- `ImportAttributes` AST node (`kind: SyntaxKind.ImportAttributes`) contains a list of `ImportAttribute` elements.
- In `src/compiler/parser.ts`, the parser accepts both `assert` and `with` tokens.
- In `src/compiler/checker.ts`, `resolveJsonModule` checks that any `.json` import has an attribute `type: "json"` when required by the target module flag.

---

## 10. Best Practices & Design Guidelines
- Always use `with { type: "json" }` for importing configuration or schema files in ESM.
- Keep static JSON imports small (< 50KB).
- Enable both `resolveJsonModule: true` and `verbatimModuleSyntax: true` in modern projects.

---

## 11. Real-World Case Study
### Secure Dynamic Locale Loader in High-Traffic SSR Application
A global e-commerce application dynamically loaded translation strings:
```typescript
export async function getTranslation(locale: string): Promise<Record<string, string>> {
  try {
    const localeModule = await import(`../locales/${locale}.json`, {
      with: { type: "json" }
    });
    return localeModule.default;
  } catch (err) {
    console.error(`Failed to load locale ${locale}`, err);
    const fallback = await import("../locales/en-US.json", {
      with: { type: "json" }
    });
    return fallback.default;
  }
}
```
By using import attributes, the SSR server strictly eliminated the possibility of remote code execution via compromised locale files.

---

## 12. Interview Questions

<details>
<summary>1. What problem do Import Attributes solve in modern JavaScript and TypeScript?</summary>

They prevent security vulnerabilities (such as MIME-type confusion attacks) when importing non-JavaScript resources like JSON. The attribute `with { type: "json" }` forces the host engine to verify that the target is strictly JSON before parsing, preventing malicious code execution.
</details>

<details>
<summary>2. Why was <code>assert</code> replaced with <code>with</code> in TypeScript 5.3?</summary>

TC39 updated the ECMAScript specification from the earlier "Import Assertions" (`assert`) proposal to "Import Attributes" (`with`) because attributes can influence how a module is loaded, not merely assert its shape. TypeScript 5.3 implemented this final Stage 3 standard.
</details>

<details>
<summary>3. Which compiler flag must be enabled in <code>tsconfig.json</code> to import JSON files?</summary>

`"resolveJsonModule": true`.
</details>

<details>
<summary>4. What error occurs if you import a JSON file without attributes in <code>module: NodeNext</code>?</summary>

`TS2821: Import assertion or attribute of type 'json' is required to import JSON files when 'moduleResolution' is 'node16' or 'nodenext'.`
</details>

<details>
<summary>5. How does TypeScript type the contents of an imported JSON file?</summary>

TypeScript parses the JSON document into literal types and object shapes, inferring exact keys and primitive types matching the JSON structure.
</details>

<details>
<summary>6. How do you use import attributes with dynamic <code>import()</code>?</summary>

Pass a second options argument: `import("./data.json", { with: { type: "json" } })`.
</details>

<details>
<summary>7. What happens when compiling JSON files to an output directory (<code>outDir</code>)?</summary>

TypeScript copies the referenced `.json` files directly into the `outDir`, maintaining relative directory structures alongside the emitted `.js` files.
</details>

<details>
<summary>8. Can import attributes be used for CSS or WebAssembly?</summary>

The ECMAScript specification supports arbitrary attribute keys, and web specifications define `with { type: "css" }` for CSS module scripts. In Node.js, `"json"` is currently the only supported type.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Import App Version (Easy)
Import the version field from a local `version.json` using valid TS 5.3+ syntax.
<details>
<summary>Solution</summary>

```typescript
import versionData from "./version.json" with { type: "json" };
console.log(versionData.version);
```
</details>

### Exercise 2: Migrate Assert to With (Easy)
Refactor this legacy import:
```typescript
import config from "./config.json" assert { type: "json" };
```
<details>
<summary>Solution</summary>

```typescript
import config from "./config.json" with { type: "json" };
```
</details>

### Exercise 3: Dynamic Config Loader (Medium)
Write an async function that loads an environment-specific JSON configuration file with dynamic import attributes.
<details>
<summary>Solution</summary>

```typescript
async function loadEnvConfig(env: "dev" | "prod") {
  const config = await import(`./config.${env}.json`, {
    with: { type: "json" }
  });
  return config.default;
}
```
</details>

### Exercise 4: Re-Export JSON Module (Medium)
Re-export a JSON schema definition as a named export from a library index file.
<details>
<summary>Solution</summary>

```typescript
export { default as AppSchema } from "./schema.json" with { type: "json" };
```
</details>

### Exercise 5: JSON Schema Type Validator (Hard)
Write a TypeScript type guard that checks if an imported JSON object matches an expected interface contract.
<details>
<summary>Solution</summary>

```typescript
import rawConfig from "./config.json" with { type: "json" };

interface AppConfig {
  port: number;
  host: string;
}

function isValidConfig(obj: any): obj is AppConfig {
  return typeof obj === "object" && obj !== null &&
         typeof obj.port === "number" &&
         typeof obj.host === "string";
}

if (!isValidConfig(rawConfig)) {
  throw new Error("Invalid configuration structure");
}
const config: AppConfig = rawConfig;
```
</details>

### Exercise 6: Parse Import Attribute AST (Hard)
Write a helper that validates whether an AST node representing an import statement possesses a valid `with { type: "json" }` attribute.
<details>
<summary>Solution</summary>

```typescript
interface MockImportNode {
  path: string;
  attributes?: Record<string, string>;
}

function requiresJsonAttribute(node: MockImportNode): boolean {
  if (node.path.endsWith(".json")) {
    return node.attributes?.["type"] === "json";
  }
  return true;
}

console.log(requiresJsonAttribute({ path: "./data.json", attributes: { type: "json" } })); // true
console.log(requiresJsonAttribute({ path: "./data.json" })); // false
```
</details>
