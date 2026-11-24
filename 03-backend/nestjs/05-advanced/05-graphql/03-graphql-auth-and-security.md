# GraphQL Authentication, Authorization, and Security Hardening

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 05-graphql
- **Prerequisites**: [01-graphql-fundamentals](./01-graphql-fundamentals.md), [04-intermediate/06-authentication/02-jwt-authentication](../../04-intermediate/06-authentication/02-jwt-authentication.md), [04-intermediate/07-authorization/02-role-guards](../../04-intermediate/07-authorization/02-role-guards.md)

---

## 1. Context & Purpose

GraphQL introduces unique security vulnerabilities that do not exist in standard REST APIs:
1. **Context-Mismatch**: Standard NestJS HTTP guards read `context.switchToHttp()`, which fails inside GraphQL resolvers.
2. **Denial of Service (DoS) via Nested Queries**: A malicious client can submit deeply nested circular queries (`author { posts { author { posts { ... } } } }`), exhausting server CPU and memory.
3. **Batch Attack / Query Complexity**: Submitting 1,000 aliased queries in a single HTTP POST request.
4. **Schema Introspection Exposure**: Leaking internal queries and data structures to unauthorized public clients.

Hardening a GraphQL API requires custom `GqlExecutionContext` guards, **Query Depth Limiting**, **Query Complexity Validation**, rate limiting, and disabling public introspection.

---

## 2. Theoretical Foundations

### 2.1 The Execution Context Adaptation
In NestJS, standard HTTP interceptors and guards expect an Express request (`req`). In GraphQL, the incoming request is wrapped inside a GraphQL root context:
$$\text{ExecutionContext} \xrightarrow{\text{GqlExecutionContext.create(context)}} \text{GqlContext} \longrightarrow \{ \text{req}, \text{res}, \text{connection} \}$$
Guards must extract `ctx.getContext().req` to evaluate JWT headers.

### 2.2 Query Complexity Analysis
Every field in a GraphQL query is assigned a complexity score:
- Scalar field (`name`): 1 point.
- Relational list (`posts(limit: 50)`): 50 points.
If a query's total computed score exceeds a configured threshold (e.g. 500 points), the query is rejected before execution begins.

---

## 3. Architecture & Internal Mechanics

```
Incoming GraphQL Request
            │
            ▼
   Query Validation Rules
   - graphql-depth-limit (Max Depth: 5)
   - graphql-query-complexity (Max Score: 500)
            │
      ┌─────┴─────┐
      ▼           ▼
   [Exceeded]  [Within Limits]
      │           │
 400 Bad Request  ▼
               GqlAuthGuard
               Extracts user from context.req
                  │
          ┌───────┴───────┐
          ▼               ▼
      [Valid]         [Invalid]
          │               │
          ▼          401 Unauthorized
   Execute Resolver
```

---

## 4. Concrete Implementation

### 4.1 Building a Universal `GqlAuthGuard`

Extend Passport's `AuthGuard('jwt')` to support GraphQL execution contexts:

```typescript
// src/common/guards/gql-auth.guard.ts
import { ExecutionContext, Injectable } from '@nestjs/common';
import { AuthGuard } from '@nestjs/passport';
import { GqlExecutionContext } from '@nestjs/graphql';

@Injectable()
export class GqlAuthGuard extends AuthGuard('jwt') {
  // Override getRequest to pull Express req from GraphQL context
  getRequest(context: ExecutionContext) {
    const ctx = GqlExecutionContext.create(context);
    return ctx.getContext().req;
  }
}
```

---

### 4.2 Custom Current User Decorator: `@CurrentUser()`

```typescript
// src/common/decorators/current-user.decorator.ts
import { createParamDecorator, ExecutionContext } from '@nestjs/common';
import { GqlExecutionContext } from '@nestjs/graphql';

export const CurrentUser = createParamDecorator(
  (data: unknown, context: ExecutionContext) => {
    const ctx = GqlExecutionContext.create(context);
    return ctx.getContext().req.user;
  },
);
```

---

### 4.3 Applying Guards in Resolvers

```typescript
// src/users/users.resolver.ts
import { Resolver, Query, UseGuards } from '@nestjs/graphql';
import { GqlAuthGuard } from '../common/guards/gql-auth.guard';
import { CurrentUser } from '../common/decorators/current-user.decorator';
import { User } from './models/user.model';
import { UsersService } from './users.service';

@Resolver(() => User)
export class UsersResolver {
  constructor(private readonly usersService: UsersService) {}

  @Query(() => User)
  @UseGuards(GqlAuthGuard)
  async me(@CurrentUser() user: any): Promise<User> {
    return this.usersService.findById(user.id);
  }
}
```

---

### 4.4 Hardening with Depth and Complexity Limits

Install security plugins:
```bash
npm install --save graphql-depth-limit
```

Configure in `GraphQLModule`:

```typescript
// src/app.module.ts
import { Module } from '@nestjs/common';
import { GraphQLModule } from '@nestjs/graphql';
import { ApolloDriver, ApolloDriverConfig } from '@nestjs/apollo';
import depthLimit from 'graphql-depth-limit';

@Module({
  imports: [
    GraphQLModule.forRoot<ApolloDriverConfig>({
      driver: ApolloDriver,
      autoSchemaFile: true,
      // 1. Disable Introspection & Playground in Production
      introspection: process.env.NODE_ENV !== 'production',
      playground: process.env.NODE_ENV !== 'production',
      // 2. Reject queries deeper than 5 levels
      validationRules: [depthLimit(5)],
      // 3. Attach incoming Express request to GraphQL context
      context: ({ req, res }) => ({ req, res }),
    }),
  ],
})
export class AppModule {}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Missing `context: ({ req }) => ({ req })`
If the `context` function is omitted in `GraphQLModule.forRoot()`, `ctx.getContext().req` is undefined. The `GqlAuthGuard` crashes with `TypeError: Cannot read properties of undefined (reading 'headers')`.

### 5.2 Circular Query Denial of Service (DoS)
Without depth limiting, an attacker sends:
```graphql
query Malicious {
  user(id: "1") {
    posts {
      author {
        posts {
          author { posts { author { ... } } }
        }
      }
    }
  }
}
```
The server attempts millions of recursive resolver executions, freezing the Node.js event loop.
- **Remedy**: Always configure `depthLimit(5)` in production.

---

## 6. Performance & Memory Implications

- **Validation Timing**: Depth and complexity validation rules execute during AST analysis **before** any resolver runs, consuming zero database or business logic resources on rejected queries.
- **Context Allocation**: Constructing `GqlExecutionContext` adds negligible microsecond overhead.

---

## 7. Anti-Patterns & Common Mistakes

- **Using Standard `@UseGuards(AuthGuard('jwt'))`**: Forgetting to extend `getRequest()` for GraphQL. Standard Passport guards look at `context.switchToHttp()` and fail immediately.
- **Allowing Unlimited Pagination Limits**: Letting clients query `posts(limit: 1000000)`. Always enforce strict pagination caps (`Math.min(limit, 50)`).
- **Leaving Introspection Open to the Public**: Allowing automated scanners to map your entire database schema and detect unpatched entities.

---

## 8. Production Checklist & Operational Readiness

- [ ] `GqlAuthGuard` extracts request via `GqlExecutionContext`.
- [ ] `introspection: false` in production builds.
- [ ] Query depth limit configured (`depthLimit(5)`).
- [ ] Pagination limits bounded with strict upper limits.
- [ ] Field-level authorization applied to sensitive fields (`@Field({ middleware: [...] })`).

---

## 9. Migration & Version History

- **Apollo Server 4**: Redesigned context functions and removed bundled Apollo plugins in favor of standalone validation rules.

---

## 10. Verification & Testing

Testing `GqlAuthGuard` execution:

```typescript
import { GqlAuthGuard } from './gql-auth.guard';

describe('GqlAuthGuard', () => {
  it('extracts request cleanly from GraphQL context', () => {
    const guard = new GqlAuthGuard();
    const mockReq = { headers: { authorization: 'Bearer token' } };

    const mockGqlContext: any = {
      getContext: () => ({ req: mockReq }),
      getArgs: () => ({}),
      getRoot: () => ({}),
      getInfo: () => ({}),
    };

    const mockContext: any = {
      getType: () => 'graphql',
      getHandler: () => {},
      getClass: () => {},
      getArgs: () => [{}, {}, { req: mockReq }, {}],
    };

    const req = guard.getRequest(mockContext);
    expect(req).toBeDefined();
    expect(req.headers.authorization).toContain('Bearer');
  });
});
```

---

## 11. Comprehensive Examples

### Field-Level Authorization Middleware

```typescript
// src/common/graphql/field-auth.middleware.ts
import { FieldMiddleware, MiddlewareContext, NextFn } from '@nestjs/graphql';
import { UnauthorizedException } from '@nestjs/common';

export const checkAdminFieldMiddleware: FieldMiddleware = async (
  ctx: MiddlewareContext,
  next: NextFn,
) => {
  const { req } = ctx.context;
  const user = req?.user;

  if (!user || !user.roles?.includes('ADMIN')) {
    throw new UnauthorizedException('Admin privileges required to read this field');
  }

  return next();
};
```

Applied to sensitive fields:
```typescript
@Field({ middleware: [checkAdminFieldMiddleware] })
internalAuditId: string;
```

---

## 12. Cross-References & Further Reading

- [01-graphql-fundamentals.md](./01-graphql-fundamentals.md) - GraphQL setup
- [04-intermediate/06-authentication/02-jwt-authentication.md](../../04-intermediate/06-authentication/02-jwt-authentication.md) - JWT authentication
- [04-intermediate/07-authorization/02-role-guards.md](../../04-intermediate/07-authorization/02-role-guards.md) - RBAC guards
- [Apollo Server Security Best Practices](https://www.apollographql.com/docs/apollo-server/security/terminating-ssl/)
