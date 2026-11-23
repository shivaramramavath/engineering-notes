# Module 28: tsconfig.json Options Cheatsheet

> **Quick Reference**: Authoritative breakdown of crucial `tsconfig.json` compiler options, recommended baselines for modern Node.js and Bundler projects, and strict flags.

---

## 1. Recommended Base tsconfig (Vite / Bundler - TS 5.0+)

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022", "DOM", "DOM.Iterable"],
    "module": "ESNext",
    "moduleResolution": "Bundler",
    "strict": true,
    "noEmit": true,
    "isolatedModules": true,
    "verbatimModuleSyntax": true,
    "skipLibCheck": true,
    "esModuleInterop": true,
    "forceConsistentCasingInFileNames": true,
    "exactOptionalPropertyTypes": true,
    "noUncheckedIndexedAccess": true
  },
  "include": ["src"]
}
```

---

## 2. Recommended Base tsconfig (Node.js ESM - TS 5.0+)

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022"],
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "outDir": "./dist",
    "rootDir": "./src",
    "strict": true,
    "declaration": true,
    "declarationMap": true,
    "sourceMap": true,
    "verbatimModuleSyntax": true,
    "skipLibCheck": true,
    "esModuleInterop": true,
    "exactOptionalPropertyTypes": true,
    "noUncheckedIndexedAccess": true
  },
  "include": ["src"]
}
```

---

## 3. Strict Flag Suite Reference

| Flag | Impact | Why Enable? |
|---|---|---|
| `"strict": true` | Umbrella flag turning on all core strict checks | Baseline for all modern TypeScript |
| `"noImplicitAny"` | Errors on un-typed variables inferred as `any` | Prevents sneaky type leaks |
| `"strictNullChecks"` | Treats `null` and `undefined` as independent types | Eliminates billion-dollar null reference crashes |
| `"strictFunctionTypes"` | Enforces contravariant parameter assignment | Prevents unsound function subtyping |
| `"noUncheckedIndexedAccess"` | Appends `undefined` to dictionary lookup results (`T[K] | undefined`) | Prevents indexing into non-existent array/object keys |
| `"exactOptionalPropertyTypes"` | Differentiates `{ prop?: string }` from `{ prop: string | undefined }` | Prevents passing `{ prop: undefined }` when omitting is required |\n