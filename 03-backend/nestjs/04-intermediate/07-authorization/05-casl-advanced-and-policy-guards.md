# CASL Advanced and Policy Guards

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 04-intermediate / 07-authorization
- **Prerequisites**: [01-rbac-vs-abac](./01-rbac-vs-abac.md), [02-role-guards](./02-role-guards.md), [03-core-concepts/01-request-pipeline/02-guards](../../03-core-concepts/01-request-pipeline/02-guards.md)

---

## 1. Context & Purpose

Simple Role-Based Access Control (RBAC)—such as checking `@Roles('ADMIN')`—breaks down quickly in enterprise applications. Real-world authorization rules depend on dynamic conditions, object ownership, and context-dependent attributes:
- *"A project manager can update a project only if it belongs to their department and has not been archived."*
- *"A writer can edit their own blog posts, but only an editor can publish or delete them."*

This is **Attribute-Based Access Control (ABAC)**. **CASL** (pronounced "Castle") is an isomorphic JavaScript/TypeScript authorization library that allows defining user abilities in pure code or loading them dynamically from a database. Integrating CASL with NestJS Policy Guards provides fine-grained, declarative, type-safe authorization across all controllers and domain services.

---

## 2. Theoretical Foundations

CASL organizes authorization around **Actions**, **Subjects**, and **Conditions**:
- **Action**: What the user wants to do (`create`, `read`, `update`, `delete`, or `manage` for all).
- **Subject**: The domain entity being acted upon (`Article`, `User`, `Project`, or `'all'`).
- **Conditions**: Object attribute constraints (e.g. `{ authorId: user.id, isPublished: false }`).
- **Fields**: Optional field-level permissions (e.g. user can update `title` and `content`, but not `status`).

---

## 3. Architecture & Internal Mechanics

```
Incoming Request (PATCH /articles/42)
                  │
                  ▼
          PoliciesGuard
                  │
    Reads @CheckPolicies(...) Metadata
                  │
                  ▼
         CaslAbilityFactory
    .createForUser(req.user)
                  │
    Builds Pure Ability Instance
    - can('update', Article, { authorId: user.id })
                  │
                  ▼
    PolicyHandler Evaluates Ability:
    ability.can(Action.Update, currentArticle)
                  │
          ┌───────┴───────┐
          ▼               ▼
       [True]          [False]
          │               │
          ▼               ▼
      Execute        403 Forbidden
      Handler     (ForbiddenException)
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save @casl/ability
```

### 4.2 Defining Domain Subjects and Actions

```typescript
// src/casl/action.enum.ts
export enum Action {
  Manage = 'manage', // Wildcard for all actions
  Create = 'create',
  Read = 'read',
  Update = 'update',
  Delete = 'delete',
}
```

```typescript
// src/articles/entities/article.entity.ts
export class Article {
  id!: string;
  title!: string;
  authorId!: string;
  isPublished!: boolean;
}
```

---

### 4.3 Building the `CaslAbilityFactory`

```typescript
// src/casl/casl-ability.factory.ts
import {
  AbilityBuilder,
  PureAbility,
  AbilityClass,
  ExtractSubjectType,
  InferSubjects,
} from '@casl/ability';
import { Injectable } from '@nestjs/common';
import { Action } from './action.enum';
import { Article } from '../articles/entities/article.entity';
import { User } from '../users/entities/user.entity';

export type Subjects = InferSubjects<typeof Article | typeof User> | 'all';
export type AppAbility = PureAbility<[Action, Subjects]>;

@Injectable()
export class CaslAbilityFactory {
  createForUser(user: User): AppAbility {
    const { can, cannot, build } = new AbilityBuilder<PureAbility<[Action, Subjects]>>(
      PureAbility as AbilityClass<AppAbility>,
    );

    if (user.isAdmin) {
      can(Action.Manage, 'all'); // Admin has full access
    } else {
      can(Action.Read, Article); // Everyone can read articles

      // Author can update only their own unpublished articles
      can(Action.Update, Article, { authorId: user.id, isPublished: false });

      // No regular user can delete articles
      cannot(Action.Delete, Article).because('Only admins can delete articles');
    }

    return build({
      detectSubjectType: (item) =>
        item.constructor as ExtractSubjectType<Subjects>,
    });
  }
}
```

---

### 4.4 Declarative Policy Handlers and `@CheckPolicies()` Decorator

```typescript
// src/casl/policy-handler.interface.ts
import { AppAbility } from './casl-ability.factory';

interface IPolicyHandler {
  handle(ability: AppAbility): boolean;
}

type PolicyHandlerCallback = (ability: AppAbility) => boolean;

export type PolicyHandler = IPolicyHandler | PolicyHandlerCallback;
```

```typescript
// src/casl/check-policies.decorator.ts
import { SetMetadata } from '@nestjs/common';
import { PolicyHandler } from './policy-handler.interface';

export const CHECK_POLICIES_KEY = 'CHECK_POLICIES_METADATA';

export const CheckPolicies = (...handlers: PolicyHandler[]) =>
  SetMetadata(CHECK_POLICIES_KEY, handlers);
```

---

### 4.5 The Unified `PoliciesGuard`

```typescript
// src/casl/policies.guard.ts
import {
  Injectable,
  CanActivate,
  ExecutionContext,
  ForbiddenException,
} from '@nestjs/common';
import { Reflector } from '@nestjs/core';
import { CaslAbilityFactory, AppAbility } from './casl-ability.factory';
import { CHECK_POLICIES_KEY } from './check-policies.decorator';
import { PolicyHandler } from './policy-handler.interface';

@Injectable()
export class PoliciesGuard implements CanActivate {
  constructor(
    private readonly reflector: Reflector,
    private readonly caslAbilityFactory: CaslAbilityFactory,
  ) {}

  async canActivate(context: ExecutionContext): Promise<boolean> {
    const policyHandlers =
      this.reflector.get<PolicyHandler[]>(
        CHECK_POLICIES_KEY,
        context.getHandler(),
      ) || [];

    if (policyHandlers.length === 0) {
      return true; // No policies attached, permit access
    }

    const { user } = context.switchToHttp().getRequest();
    if (!user) {
      throw new ForbiddenException('User context required for policy verification');
    }

    const ability = this.caslAbilityFactory.createForUser(user);

    for (const handler of policyHandlers) {
      const allowed = this.execPolicyHandler(handler, ability);
      if (!allowed) {
        throw new ForbiddenException('You do not have permission to perform this action');
      }
    }

    return true;
  }

  private execPolicyHandler(handler: PolicyHandler, ability: AppAbility): boolean {
    if (typeof handler === 'function') {
      return handler(ability);
    }
    return handler.handle(ability);
  }
}
```

---

### 4.6 Controller Integration

```typescript
// src/articles/articles.controller.ts
import {
  Controller,
  Get,
  Post,
  Patch,
  Delete,
  Param,
  Body,
  UseGuards,
  ForbiddenException,
} from '@nestjs/common';
import { PoliciesGuard } from '../casl/policies.guard';
import { CheckPolicies } from '../casl/check-policies.decorator';
import { Action } from '../casl/action.enum';
import { Article } from './entities/article.entity';
import { AppAbility, CaslAbilityFactory } from '../casl/casl-ability.factory';
import { ArticlesService } from './articles.service';

@Controller('articles')
@UseGuards(PoliciesGuard)
export class ArticlesController {
  constructor(
    private readonly articlesService: ArticlesService,
    private readonly caslAbilityFactory: CaslAbilityFactory,
  ) {}

  // Class-level policy check
  @Post()
  @CheckPolicies((ability: AppAbility) => ability.can(Action.Create, Article))
  createArticle(@Body() dto: any) {
    return this.articlesService.create(dto);
  }

  // Instance-level policy check
  @Patch(':id')
  async updateArticle(@Param('id') id: string, @Body() dto: any, @Req() req: any) {
    const article = await this.articlesService.findById(id);
    const ability = this.caslAbilityFactory.createForUser(req.user);

    // Verify conditions against the actual entity instance
    if (!ability.can(Action.Update, article)) {
      throw new ForbiddenException('Cannot edit another author or published article');
    }

    return this.articlesService.update(id, dto);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Static Class Checking vs Instance Checking
Checking `ability.can(Action.Update, Article)` only tests whether the user has general permission to update *any* article. It cannot evaluate object condition fields (like `authorId === user.id`).
- **Rule**: Route guards evaluate general subject permissions. Instance-level condition checks must be evaluated against the fetched database entity inside the service or controller.

### 5.2 Circular Subject Prototypes
Using plain JavaScript object literals `{ authorId: '123' }` as subjects fails because CASL cannot detect subject types via `item.constructor`.
- **Handling**: Ensure entities are instantiated via class instances (`Object.assign(new Article(), data)`).

---

## 6. Performance & Memory Implications

- **Rule Evaluation Speed**: CASL compiles conditions into fast in-memory boolean functions, evaluating rules in microseconds (< 0.05ms).
- **Zero Database Overhead**: Abilities are derived in-memory from user role and permission arrays already present on `req.user`.

---

## 7. Anti-Patterns & Common Mistakes

- **Hardcoding Role Logic Across Services**: Checking `if (user.role === 'ADMIN' || user.id === post.authorId)` inside dozens of controller routes. CASL centralizes authorization logic in a single factory.
- **Forgetting `detectSubjectType`**: Omitting `detectSubjectType` in `build()`, preventing CASL from distinguishing between different entity classes.

---

## 8. Production Checklist & Operational Readiness

- [ ] All sensitive mutation routes protected with `PoliciesGuard`.
- [ ] Instance-level checks performed after entity retrieval.
- [ ] Custom denial reasons configured with `.because('...')`.
- [ ] Unit tests cover both positive and negative permission matrix branches.

---

## 9. Migration & Version History

- **CASL 5/6**: Unified TypeScript API with `PureAbility` and `AbilityBuilder`.
- **NestJS 11.x**: Validated with reflector metadata caching and Express 5 execution contexts.

---

## 10. Verification & Testing

Unit testing CASL ability rules:

```typescript
import { CaslAbilityFactory } from './casl-ability.factory';
import { Action } from './action.enum';
import { Article } from '../articles/entities/article.entity';

describe('CaslAbilityFactory', () => {
  let factory: CaslAbilityFactory;

  beforeEach(() => {
    factory = new CaslAbilityFactory();
  });

  it('allows author to edit their own unpublished article', () => {
    const user = { id: 'user_1', isAdmin: false } as any;
    const ability = factory.createForUser(user);

    const ownArticle = Object.assign(new Article(), {
      id: 'art_1',
      authorId: 'user_1',
      isPublished: false,
    });

    expect(ability.can(Action.Update, ownArticle)).toBe(true);
  });

  it('forbids author from editing published article', () => {
    const user = { id: 'user_1', isAdmin: false } as any;
    const ability = factory.createForUser(user);

    const publishedArticle = Object.assign(new Article(), {
      id: 'art_2',
      authorId: 'user_1',
      isPublished: true,
    });

    expect(ability.can(Action.Update, publishedArticle)).toBe(false);
  });
});
```

---

## 11. Comprehensive Examples

### Loading Permissions Dynamically from Database

```typescript
// src/casl/dynamic-ability.factory.ts
import { AbilityBuilder, PureAbility, AbilityClass } from '@casl/ability';
import { Injectable } from '@nestjs/common';
import { Action } from './action.enum';
import { AppAbility, Subjects } from './casl-ability.factory';

@Injectable()
export class DynamicAbilityFactory {
  createFromDbPermissions(permissions: Array<{ action: Action; subject: string; conditions?: any }>) {
    const { can, build } = new AbilityBuilder<PureAbility<[Action, Subjects]>>(
      PureAbility as AbilityClass<AppAbility>,
    );

    for (const perm of permissions) {
      if (perm.conditions) {
        can(perm.action, perm.subject as any, perm.conditions);
      } else {
        can(perm.action, perm.subject as any);
      }
    }

    return build();
  }
}
```

---

## 12. Cross-References & Further Reading

- [01-rbac-vs-abac.md](./01-rbac-vs-abac.md) - RBAC vs ABAC theory
- [02-role-guards.md](./02-role-guards.md) - Standard role guards
- [03-core-concepts/01-request-pipeline/02-guards.md](../../03-core-concepts/01-request-pipeline/02-guards.md) - NestJS Guard pipeline
- [CASL Official Documentation](https://casl.js.org)
