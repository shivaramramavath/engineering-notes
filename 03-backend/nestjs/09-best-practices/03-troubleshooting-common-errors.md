---
title: "Troubleshooting Common NestJS Errors and Exceptions"
description: "Resolving 'Nest can't resolve dependencies', circular reference undefined, and common NestJS runtime exceptions"
tags: [nestjs, troubleshooting, error-resolution, nest-cant-resolve-dependencies, debug]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Troubleshooting Common NestJS Errors and Exceptions

## 1. "Nest can't resolve dependencies of the TargetService (?)"
- **Symptoms**: Application crashes at startup with question marks for parameter types.
- **Root Causes**:
  1. The dependency is not registered in the current module's `providers` array.
  2. The dependency belongs to another module that is not imported in `imports: [...]`.
  3. The exporting module forgot to include the provider in its `exports: [...]` array.
  4. Circular dependency where one class evaluates to `undefined` during module evaluation.
- **Resolution**:
  - Verify module imports/exports.
  - Wrap both sides in `forwardRef(() => TargetModule)`.

## 2. "TypeError: Cannot read properties of undefined (reading 'prototype')"
- **Root Cause**: Circular import between files at the TypeScript module level.
- **Resolution**: Avoid importing types from index barrels (`index.ts`) within the same module; import directly from the specific file.

## 3. "Error: listen EADDRINUSE: address already in use :::3000"
- **Root Cause**: Another process is already bound to port 3000.
- **Resolution**: Identify and kill the process:
  ```bash
  # Windows
  netstat -ano | findstr :3000
  taskkill /PID <PID> /F
  ```

---

## Key Takeaways & Cheat Sheet

- "Nest can't resolve dependencies (?)" almost always means a missing `imports` or `exports` entry.
- Beware of barrel file (`index.ts`) circular imports.
- Use `app.enableShutdownHooks()` to release sockets cleanly.
