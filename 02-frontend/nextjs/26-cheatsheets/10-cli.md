# Module 26: Next.js CLI & Tooling Cheatsheet

> **Quick Reference**: Next.js CLI commands, Turbopack flags, build debugging, bundle analysis, and ESLint 9 / Biome configurations.

---

## 1. Core Next.js CLI Commands

| Command | Description | Useful Flags |
| :--- | :--- | :--- |
| `next dev` | Start development server | `--turbo`, `-p 3001`, `-H 0.0.0.0` |
| `next build` | Compile production build | `--profile`, `--no-lint` |
| `next start` | Run production server | `-p 8080`, `-H 0.0.0.0` |
| `next info` | Print environment & dependency info | None |

---

## 2. Turbopack Development & Build

Turbopack is the Rust-based compilation engine in Next.js:

```bash
# Start development with Turbopack (recommended)
npx next dev --turbo

# Run on custom port with network binding
npx next dev --turbo -p 3000 -H 0.0.0.0

# Build with Turbopack (experimental)
npx next build --turbo
```

---

## 3. Linting and Formatting in Next.js 15+ / 16

> **Important**: Next.js 16 decouples `next lint`. Use standalone ESLint 9 Flat Config or Biome:

### Running ESLint 9
```bash
# Run flat config linter
npx eslint .

# Fix auto-fixable issues
npx eslint . --fix
```

### ESLint 9 Flat Config (`eslint.config.mjs`)
```javascript
import nextPlugin from '@next/eslint-plugin-next';
import reactPlugin from 'eslint-plugin-react';
import tsPlugin from '@typescript-eslint/eslint-plugin';
import tsParser from '@typescript-eslint/parser';

export default [
  {
    files: ['**/*.{js,mjs,cjs,ts,jsx,tsx}'],
    plugins: {
      '@next/next': nextPlugin,
      'react': reactPlugin,
      '@typescript-eslint': tsPlugin,
    },
    languageOptions: {
      parser: tsParser,
    },
    rules: {
      ...nextPlugin.configs.recommended.rules,
      ...nextPlugin.configs['core-web-vitals'].rules,
    },
  },
];
```

### Biome Fast Alternative (`biome.json`)
```bash
# Run Biome lint & format in sub-second time
npx @biomejs/biome check --write ./src
```

---

## 4. Useful Production Shell Scripts (`package.json`)

```json
{
  "scripts": {
    "dev": "next dev --turbo",
    "build": "next build",
    "start": "next start",
    "lint": "eslint .",
    "typecheck": "tsc --noEmit",
    "analyze": "ANALYZE=true next build",
    "test": "vitest run",
    "test:e2e": "playwright test"
  }
}
```
