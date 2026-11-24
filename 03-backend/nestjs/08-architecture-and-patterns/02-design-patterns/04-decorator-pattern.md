---
title: "The Decorator Pattern in NestJS"
description: "Structural Decorator pattern vs TypeScript metadata decorators: wrapping services for caching, retries, and timing"
tags: [nestjs, design-patterns, decorator-pattern, dynamic-wrapping, interception]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# The Decorator Pattern in NestJS

## Conceptual Overview
Do not confuse the **Gang of Four (GoF) Structural Decorator Pattern** with TypeScript syntactic annotations (`@Injectable()`). The GoF pattern attaches new behaviors to an object dynamically by wrapping it without modifying its class.

In NestJS, the Decorator pattern is used to wrap services with cross-cutting behaviors (caching, logging, retries) transparently.

---

## Implementation: Caching Decorator Wrapper

```typescript
export interface DataService {
  getData(id: string): Promise<string>;
}

export class RealDataService implements DataService {
  async getData(id: string): Promise<string> {
    return `Fresh data from DB for ${id}`;
  }
}

// Decorator wrapping RealDataService with caching
export class CachedDataServiceDecorator implements DataService {
  private cache = new Map<string, string>();

  constructor(private readonly inner: DataService) {}

  async getData(id: string): Promise<string> {
    if (this.cache.has(id)) {
      return this.cache.get(id)!;
    }
    const result = await this.inner.getData(id);
    this.cache.set(id, result);
    return result;
  }
}
```

---

## Key Takeaways & Cheat Sheet

- GoF Decorators wrap instances to augment behavior while adhering to the same interface.
- NestJS Interceptors provide declarative Aspect-Oriented decoration.

---

## Cross-References
- [03-metadata-and-reflection.md](../../06-internals/03-metadata-and-reflection.md) - TypeScript decorators
- [05-facade.md](./05-facade.md) - Facade pattern
