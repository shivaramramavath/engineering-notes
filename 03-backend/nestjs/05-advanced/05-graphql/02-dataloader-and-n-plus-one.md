# DataLoader and Solving the N+1 Query Problem in GraphQL

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 05-graphql
- **Prerequisites**: [01-graphql-fundamentals](./01-graphql-fundamentals.md), [04-intermediate/02-database-foundations/01-database-module-pattern](../../04-intermediate/02-database-foundations/01-database-module-pattern.md)

---

## 1. Context & Purpose

The greatest architectural hazard in GraphQL backends is the **N+1 Query Problem**. Consider a client query requesting 50 posts and their authors:
```graphql
query {
  posts {
    title
    author { name }
  }
}
```
If the `author` field resolver queries the database for each post individually:
1. **1 Query**: Fetches 50 posts (`SELECT * FROM posts`).
2. **N Queries (50)**: Fetches the author for each post (`SELECT * FROM users WHERE id = $1` $\times 50$).
Total: **51 database queries** for a single HTTP request! Under high traffic, this exhausts database connection pools and destroys backend performance.

Facebook's **DataLoader** solves this by coalescing and batching individual field queries into a single batched database query (`SELECT * FROM users WHERE id IN ($1, $2, ...)`), while caching results across the single request lifecycle.

---

## 2. Theoretical Foundations

DataLoader operates via two principles:
1. **Batching via Next Tick**: When a field resolver calls `loader.load(authorId)`, DataLoader does not query immediately. It queues the key and waits for the current Node.js event loop tick to finish. On `process.nextTick()`, it combines all queued keys and executes a single batch loading function.
2. **Request-Scoped Caching**: If Post 1 and Post 5 share the same author, DataLoader fetches that author once and returns the cached result for both posts.
3. **Array Order Contract**: The batch loading function **MUST** return an array of results that corresponds 1:1 in length and order to the input keys array.

---

## 3. Architecture & Internal Mechanics

```
GraphQL Execution Engine resolves 50 Posts
                      │
                      ├─> Post 1: loader.load("user_1")
                      ├─> Post 2: loader.load("user_2")
                      ├─> Post 3: loader.load("user_1") (Cached hit!)
                      │
                      ▼
         process.nextTick() triggers
                      │
                      ▼
         DataLoader Batch Function:
         batchUsers(["user_1", "user_2"])
                      │
                      ▼
         Single Database Query:
         SELECT * FROM users WHERE id IN ('user_1', 'user_2')
                      │
                      ▼
         Distribute results back to Post 1, Post 2, Post 3
```

---

## 4. Concrete Implementation

### 4.1 Installing DataLoader

```bash
npm install --save dataloader
npm install --save-dev @types/dataloader
```

---

### 4.2 Building a Request-Scoped DataLoader Factory

A DataLoader cache must be **request-scoped** so that user data from Request A is never served to Request B. In NestJS, instantiate DataLoaders inside the GraphQL execution context:

```typescript
// src/users/users.loader.ts
import { Injectable, Scope } from '@nestjs/common';
import DataLoader from 'dataloader';
import { In } from 'typeorm';
import { User } from './models/user.model';
import { UsersRepository } from './users.repository';

@Injectable({ scope: Scope.REQUEST })
export class UserDataLoader {
  constructor(private readonly usersRepository: UsersRepository) {}

  public readonly batchAuthors = new DataLoader<string, User>(
    async (authorIds: readonly string[]) => {
      // 1. Execute single batch database query
      const users = await this.usersRepository.findBy({
        id: In([...authorIds]),
      });

      // 2. Map database results to a lookup dictionary
      const userMap = new Map(users.map((u) => [u.id, u]));

      // 3. CRITICAL: Return array matching the exact order and length of authorIds
      return authorIds.map((id) => userMap.get(id) || new Error(`User not found: ${id}`));
    },
  );
}
```

---

### 4.3 Consuming DataLoader in Field Resolvers

```typescript
// src/posts/posts.resolver.ts
import { Resolver, ResolveField, Parent, Query } from '@nestjs/graphql';
import { Post } from './models/post.model';
import { User } from '../users/models/user.model';
import { UserDataLoader } from '../users/users.loader';
import { PostsService } from './posts.service';

@Resolver(() => Post)
export class PostsResolver {
  constructor(
    private readonly postsService: PostsService,
    private readonly userDataLoader: UserDataLoader,
  ) {}

  @Query(() => [Post])
  async posts(): Promise<Post[]> {
    return this.postsService.findAll();
  }

  // Field resolver: Loads author on demand without N+1 queries!
  @ResolveField(() => User)
  async author(@Parent() post: Post): Promise<User> {
    // Calling load() batches all calls into a single query!
    return this.userDataLoader.batchAuthors.load(post.authorId);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Returning Mismatched Result Arrays
If `authorIds` has 5 elements (`['u1', 'u2', 'u3', 'u4', 'u5']`) and the batch function returns only 4 users (because `u3` was deleted), DataLoader throws an unrecoverable runtime error:
`DataLoader must be constructed with a function which accepts Array<key> and returns Promise<Array<value>>, but the function returned an Array of different length.`
- **Handling**: Always map the return array directly over `authorIds`:
```typescript
return authorIds.map((id) => userMap.get(id) || null);
```

### 5.2 Sharing DataLoaders Across Requests (Cross-Tenant Data Leak)
Making a DataLoader singleton (`Scope.DEFAULT`) caches database records across different users and requests. User B could read a cached entity from User A, bypassing tenant security.
- **Rule**: DataLoaders must always be **request-scoped** (`Scope.REQUEST` or attached to `context`).

---

## 6. Performance & Memory Implications

- **Query Reduction**: Reduces 1,000 separate SQL queries into 1 batched `IN (...)` query, cutting database network round-trip time from 1,500ms down to 15ms (100x improvement!).
- **Memory Consumption**: DataLoader's internal cache is scoped to the request and automatically garbage collected when the HTTP request ends.

---

## 7. Anti-Patterns & Common Mistakes

- **Fetching Relations Directly in Every Field Resolver**: Calling `this.usersService.findById(post.authorId)` inside `@ResolveField()`. This is the exact root cause of N+1 query storms.
- **Creating a New DataLoader Inside the Field Resolver**: Writing `const loader = new DataLoader(...)` inside the method. A new loader is created for every post, destroying batching completely.

---

## 8. Production Checklist & Operational Readiness

- [ ] DataLoaders scoped strictly per request.
- [ ] Batch function guarantees 1:1 length and order match with input keys.
- [ ] Database columns used in `WHERE IN (...)` are indexed.
- [ ] SQL query logging enabled in test environments to verify batching.

---

## 9. Migration & Version History

- **DataLoader 2.x**: Added support for TypeScript types and custom cache map implementations.

---

## 10. Verification & Testing

Unit testing a DataLoader batch function:

```typescript
import DataLoader from 'dataloader';

describe('DataLoader Batching', () => {
  it('batches concurrent load calls into a single execution', async () => {
    const batchFn = vi.fn(async (keys: readonly string[]) => {
      return keys.map((k) => `User_${k}`);
    });

    const loader = new DataLoader(batchFn);

    // Call load concurrently
    const [u1, u2, u3] = await Promise.all([
      loader.load('1'),
      loader.load('2'),
      loader.load('1'), // Duplicate should use cache
    ]);

    expect(u1).toBe('User_1');
    expect(u2).toBe('User_2');
    expect(u3).toBe('User_1');

    // Batch function should have been called EXACTLY ONCE with unique keys!
    expect(batchFn).toHaveBeenCalledTimes(1);
    expect(batchFn).toHaveBeenCalledWith(['1', '2']);
  });
});
```

---

## 11. Comprehensive Examples

### NestJS GraphQL Context-Based DataLoader Setup

```typescript
// src/common/graphql/context-dataloader.ts
import { GqlExecutionContext } from '@nestjs/graphql';
import DataLoader from 'dataloader';

export interface GqlContext {
  loaders: {
    userLoader: DataLoader<string, any>;
  };
}

export const createLoaders = (usersService: any) => ({
  userLoader: new DataLoader(async (ids: readonly string[]) => {
    return usersService.findBatchByIds(ids);
  }),
});
```

---

## 12. Cross-References & Further Reading

- [01-graphql-fundamentals.md](./01-graphql-fundamentals.md) - GraphQL setup
- [03-graphql-auth-and-security.md](./03-graphql-auth-and-security.md) - Complexity limiting
- [04-intermediate/02-database-foundations/01-database-module-pattern.md](../../04-intermediate/02-database-foundations/01-database-module-pattern.md) - Database query optimization
- [DataLoader GitHub Repository](https://github.com/graphql/dataloader)
