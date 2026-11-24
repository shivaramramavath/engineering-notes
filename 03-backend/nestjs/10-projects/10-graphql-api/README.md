# Project 10: Code-First GraphQL Enterprise API (`10-graphql-api`)

> Production GraphQL API with Apollo Server 4, NestJS Code-First schema decorators, request-scoped DataLoader to eliminate N+1 queries, and query complexity analysis.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-10-graphql-api` |
| **GraphQL Engine** | Apollo Server 4 (`@nestjs/apollo`, `@nestjs/graphql`) |
| **Schema Paradigm** | Code-First (`@ObjectType`, `@Field`, `@Resolver`) |
| **Optimization** | Request-scoped DataLoader batching |
| **Complexity Level** | Advanced Enterprise Reference |

---

## 1. Code-First Resolver with DataLoader

```typescript
// src/authors/authors.resolver.ts
import { Resolver, Query, ResolveField, Parent, Args } from '@nestjs/graphql';
import { AuthorType } from './types/author.type';
import { PostType } from '../posts/types/post.type';
import { PostsLoader } from '../posts/posts.loader';

@Resolver(() => AuthorType)
export class AuthorsResolver {
  constructor(private readonly postsLoader: PostsLoader) {}

  @Query(() => AuthorType)
  async author(@Args('id') id: string) {
    return { id, name: 'Martin Fowler' };
  }

  @ResolveField(() => [PostType])
  async posts(@Parent() author: AuthorType) {
    // Batches requests across multiple authors into a single SQL IN query
    return this.postsLoader.load(author.id);
  }
}
```
