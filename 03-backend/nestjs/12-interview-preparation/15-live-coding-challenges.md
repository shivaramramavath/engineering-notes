---
title: "NestJS Interview: Live Coding Challenges"
description: "Practical coding challenges: custom param decorators, dynamic modules, and resilient retry interceptors"
tags: [nestjs, interview, live-coding, coding-challenges]
---

# NestJS Interview: Live Coding Challenges

### Challenge: Implement a Custom `@UserAgent()` Parameter Decorator
```typescript
import { createParamDecorator, ExecutionContext } from '@nestjs/common';

export const UserAgent = createParamDecorator(
  (data: unknown, ctx: ExecutionContext) => {
    const request = ctx.switchToHttp().getRequest();
    return request.headers['user-agent'];
  },
);
```
