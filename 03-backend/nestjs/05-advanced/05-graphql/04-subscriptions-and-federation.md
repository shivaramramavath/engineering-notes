# GraphQL Subscriptions and Apollo Federation

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 05-graphql
- **Prerequisites**: [01-graphql-fundamentals](./01-graphql-fundamentals.md), [05-advanced/04-realtime/01-gateways-and-events](../04-realtime/01-gateways-and-events.md)

---

## 1. Context & Purpose

Enterprise GraphQL architectures expand in two dimensions:
1. **Real-Time Push Subscriptions**: Allowing clients to listen for real-time mutations (e.g. `subscription { commentAdded(postId: "1") { text author } }`) over persistent WebSocket channels (`graphql-ws`).
2. **Distributed Microservices via Apollo Federation**: Splitting a monolithic GraphQL schema across multiple independent microservices (Subgraphs) coordinated by an API Gateway (Supergraph Router).

NestJS provides native support for both: real-time `@Subscription()` decorators using PubSub engines and Apollo Federation v2 subgraphs via `@nestjs/apollo` and `@apollo/subgraph`.

---

## 2. Theoretical Foundations

### 2.1 GraphQL Subscriptions Protocol (`graphql-ws`)
Subscriptions maintain a persistent WebSocket connection. When a mutation occurs, the server publishes an event to an in-memory or Redis PubSub engine. The subscription resolver filters the event stream and pushes updated payload trees down to listening clients.

### 2.2 Apollo Federation v2 Architecture
In Federation, microservices own distinct slices of the unified schema:
- **Subgraph Services**: Expose domain entities (`@key(fields: "id")`) and extend entities owned by other subgraphs (`@extends`).
- **Supergraph Router**: Compiles subgraph schemas into an aggregated schema, accepts client queries, builds an execution query plan, and queries subgraphs in parallel.

---

## 3. Architecture & Internal Mechanics

```
FEDERATION SUPERGRAPH ARCHITECTURE:
                  Client GraphQL Query
                  { user(id: "1") { name orders { total } } }
                            │
                            ▼
              Apollo Gateway / Supergraph Router
                            │
        ┌───────────────────┴───────────────────┐
        ▼                                       ▼
Users Subgraph (NestJS)             Orders Subgraph (NestJS)
@Key('id')                          @ResolveReference()
User { id, name }                   User { id, orders: [Order] }
```

---

## 4. Concrete Implementation

### 4.1 Configuring Subscriptions with `graphql-ws`

Install dependencies:
```bash
npm install --save graphql-ws graphql-subscriptions
```

Configure in `app.module.ts`:

```typescript
// src/app.module.ts
import { Module } from '@nestjs/common';
import { GraphQLModule } from '@nestjs/graphql';
import { ApolloDriver, ApolloDriverConfig } from '@nestjs/apollo';

@Module({
  imports: [
    GraphQLModule.forRoot<ApolloDriverConfig>({
      driver: ApolloDriver,
      autoSchemaFile: true,
      subscriptions: {
        'graphql-ws': {
          path: '/graphql',
          onConnect: (context) => {
            const { connectionParams } = context;
            // Validate JWT token from connectionParams during WS handshake
            return { user: connectionParams?.authorization };
          },
        },
      },
    }),
  ],
})
export class AppModule {}
```

---

### 4.2 Implementing Subscription Resolvers with `PubSub`

```typescript
// src/comments/comments.resolver.ts
import { Resolver, Mutation, Subscription, Args, ID } from '@nestjs/graphql';
import { PubSub } from 'graphql-subscriptions';
import { Comment } from './models/comment.model';

const pubSub = new PubSub();

@Resolver(() => Comment)
export class CommentsResolver {
  // 1. Mutation triggers event publish
  @Mutation(() => Comment)
  async addComment(
    @Args('postId', { type: () => ID }) postId: string,
    @Args('content') content: string,
  ): Promise<Comment> {
    const newComment = { id: 'c_' + Date.now(), postId, content };

    // Publish event to topic
    await pubSub.publish(`commentAdded:${postId}`, { commentAdded: newComment });

    return newComment;
  }

  // 2. Real-Time Subscription filtered by postId
  @Subscription(() => Comment, {
    name: 'commentAdded',
    filter: (payload, variables) => {
      return payload.commentAdded.postId === variables.postId;
    },
  })
  commentAdded(@Args('postId', { type: () => ID }) postId: string) {
    return pubSub.asyncIterator(`commentAdded:${postId}`);
  }
}
```

---

### 4.3 Apollo Federation v2 Subgraph Setup

To transform a NestJS GraphQL application into an Apollo Federation Subgraph:

Install Federation packages:
```bash
npm install --save @apollo/subgraph
```

Configure `ApolloFederationDriver` in `app.module.ts`:

```typescript
// src/app.module.ts (Users Subgraph)
import { Module } from '@nestjs/common';
import { GraphQLModule } from '@nestjs/graphql';
import { ApolloFederationDriver, ApolloFederationDriverConfig } from '@nestjs/apollo';
import { UsersModule } from './users/users.module';

@Module({
  imports: [
    GraphQLModule.forRoot<ApolloFederationDriverConfig>({
      driver: ApolloFederationDriver,
      autoSchemaFile: {
        federation: 2, // Enable Apollo Federation v2!
      },
    }),
    UsersModule,
  ],
})
export class AppModule {}
```

---

### 4.4 Federating Entities with `@Directive()` and `@ResolveReference()`

```typescript
// src/users/models/user.model.ts (Users Subgraph)
import { ObjectType, Field, ID, Directive } from '@nestjs/graphql';

@ObjectType()
@Directive('@key(fields: "id")') // Declares 'id' as the primary federation key
export class User {
  @Field(() => ID)
  id!: string;

  @Field()
  name!: string;
}
```

```typescript
// src/users/users.resolver.ts
import { Resolver, Query, ResolveReference } from '@nestjs/graphql';
import { User } from './models/user.model';
import { UsersService } from './users.service';

@Resolver(() => User)
export class UsersResolver {
  constructor(private readonly usersService: UsersService) {}

  // Gateway calls this when another subgraph needs to resolve a User by ID
  @ResolveReference()
  resolveReference(reference: { __typename: string; id: string }): Promise<User> {
    return this.usersService.findById(reference.id);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Legacy `subscriptions-transport-ws` Protocol
The older `subscriptions-transport-ws` library is unmaintained and insecure.
- **Rule**: Use the modern `graphql-ws` standard protocol. Ensure client libraries (Apollo Client) are configured with `createClient` from `graphql-ws`.

### 5.2 Federation Schema Composition Errors
If two subgraphs define the same type field without `@shareable` or with conflicting scalar types, the Apollo Gateway fails composition and refuses to start.
- **Handling**: Use Apollo Rover CLI in CI pipelines to validate subgraph composition before merging pull requests.

---

## 6. Performance & Memory Implications

- **In-Memory PubSub in Production**: `new PubSub()` from `graphql-subscriptions` is strictly an in-memory EventEmitter. It does not scale across multiple pods! For production, use `graphql-redis-subscriptions`.
- **Federation Query Planning**: Apollo Supergraph Router executes independent subgraph queries in parallel, minimizing overall latency.

---

## 7. Anti-Patterns & Common Mistakes

- **Using In-Memory PubSub Across Multi-Pod Clusters**: Subscribers connected to Pod A never receive mutations executed on Pod B. Always back production subscriptions with Redis.
- **Forgetting `@ResolveReference()` in Subgraphs**: Defining `@key(fields: "id")` without implementing a reference resolver. The Federation Gateway will fail to resolve cross-subgraph relations.

---

## 8. Production Checklist & Operational Readiness

- [ ] `graphql-ws` protocol configured with connection authentication.
- [ ] Redis PubSub (`graphql-redis-subscriptions`) used for multi-pod subscriptions.
- [ ] Subgraph schemas validated via Apollo Rover in CI/CD.
- [ ] Cross-subgraph entity resolution implemented via `@ResolveReference()`.

---

## 9. Migration & Version History

- **Apollo Federation 1 to 2**: Federation 2 removed `@extends` requirements and added `@shareable` directives.
- **NestJS 10/11**: First-class Apollo Federation v2 driver integration.

---

## 10. Verification & Testing

Testing Subgraph reference resolution:

```typescript
import { UsersResolver } from './users.resolver';

describe('UsersResolver (Federation)', () => {
  let resolver: UsersResolver;
  let mockService: any;

  beforeEach(() => {
    mockService = { findById: vi.fn().mockResolvedValue({ id: 'u1', name: 'Alex' }) };
    resolver = new UsersResolver(mockService);
  });

  it('resolves reference by id correctly', async () => {
    const user = await resolver.resolveReference({ __typename: 'User', id: 'u1' });
    expect(user.name).toBe('Alex');
    expect(mockService.findById).toHaveBeenCalledWith('u1');
  });
});
```

---

## 11. Comprehensive Examples

### Redis PubSub Production Configuration

```typescript
// src/common/graphql/redis-pubsub.provider.ts
import { RedisPubSub } from 'graphql-redis-subscriptions';
import Redis from 'ioredis';

export const REDIS_PUBSUB = 'REDIS_PUBSUB';

export const redisPubSubProvider = {
  provide: REDIS_PUBSUB,
  useFactory: () => {
    const options = { host: 'localhost', port: 6379 };
    return new RedisPubSub({
      publisher: new Redis(options),
      subscriber: new Redis(options),
    });
  },
};
```

---

## 12. Cross-References & Further Reading

- [01-graphql-fundamentals.md](./01-graphql-fundamentals.md) - Fundamentals
- [03-graphql-auth-and-security.md](./03-graphql-auth-and-security.md) - Security
- [05-advanced/04-realtime/05-scaling-websockets.md](../04-realtime/05-scaling-websockets.md) - Scaling real-time sockets
- [Apollo Federation Official Guide](https://www.apollographql.com/docs/federation/)
