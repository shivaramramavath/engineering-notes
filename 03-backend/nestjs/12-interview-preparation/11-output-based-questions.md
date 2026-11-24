---
title: "NestJS Interview: Output-Based and Code Analysis Questions"
description: "Predicting runtime output, finding subtle bugs, and diagnosing execution order in tricky NestJS code snippets"
tags: [nestjs, interview, code-analysis, output-questions, debugging]
---

# NestJS Interview: Output-Based and Code Analysis Questions

### Code Snippet 1: The Accidental Request Scope
```typescript
@Injectable({ scope: Scope.REQUEST })
export class RequestContextService {}

@Injectable()
export class CacheService {
  constructor(private ctx: RequestContextService) {}
}

@Controller()
export class AppController {
  constructor(private cache: CacheService) {}
}
```
**Question**: What is the scope of `CacheService` and `AppController`?
**Answer**: Both become **`Scope.REQUEST`** due to scope bubble-up. A new instance of `CacheService` and `AppController` will be instantiated on every incoming HTTP request.
