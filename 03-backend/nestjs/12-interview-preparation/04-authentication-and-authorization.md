---
title: "NestJS Interview: Authentication and Authorization"
description: "Interview questions covering Passport JWT, Argon2id, RBAC, ABAC, and CASL policy guards"
tags: [nestjs, interview, auth, jwt, rbac, casl]
---

# NestJS Interview: Authentication and Authorization

### Q1: How do you implement global authentication while exempting public endpoints?
**Answer**: Register the authentication guard globally using `{ provide: APP_GUARD, useClass: JwtAuthGuard }` in `AppModule`. Create a custom decorator `@Public()` that sets metadata via `SetMetadata('isPublic', true)`. In the guard, use `reflector.getAllAndOverride<boolean>('isPublic', [context.getHandler(), context.getClass()])` to bypass validation if true.
