# GraphQL Fundamentals in NestJS (Code-First vs Schema-First)

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 05-graphql
- **Prerequisites**: [02-fundamentals/03-controllers](../../02-fundamentals/03-controllers.md), [03-core-concepts/04-modules-and-di/01-dependency-injection](../../03-core-concepts/04-modules-and-di/01-dependency-injection.md)

---

## 1. Context & Purpose

REST APIs suffer from two chronic data-fetching problems in modern frontend architectures:
- **Over-fetching**: The client needs only a user's name, but the REST endpoint returns 50 database columns (avatar, addresses, billing history, audit logs).
- **Under-fetching**: A screen requires a user, their recent orders, and the shipping tracking status, requiring 3 sequential, waterfall HTTP calls.

**GraphQL** is a query language and runtime for APIs that lets clients request *exactly* what they need and nothing more in a single network round-trip. NestJS provides first-class GraphQL support via `@nestjs/graphql`, supporting both **Code-First** (generating the GraphQL SDL from TypeScript classes) and **Schema-First** (generating TypeScript types from `.graphql` files) development approaches on Apollo Server or Yoga.

---

## 2. Theoretical Foundations

### 2.1 The GraphQL Type System
GraphQL defines a strictly typed schema (Schema Definition Language, SDL):
- **Types & Fields**: Data models and their scalar types (`String`, `Int`, `Float`, `Boolean`, `ID`).
- **Queries**: Read-only requests for data.
- **Mutations**: Requests that modify data on the server.
- **Subscriptions**: Real-time push streams (typically over WebSockets).

### 2.2 Code-First vs Schema-First Approaches
| Feature | Code-First Approach (Recommended) | Schema-First Approach |
|---|---|---|
| Single Source of Truth | TypeScript classes with decorators (`@ObjectType()`) | GraphQL SDL files (`.graphql`) |
| Workflow | Write TypeScript; NestJS generates `schema.gql` | Write `.graphql` schema; CLI generates TS definitions |
| Type Syncing | 100% automated via compiler reflection | Requires running codegen watchers |
| Ecosystem | Idiomatic NestJS (mirrors DTO and TypeORM patterns) | Language-agnostic (shared across teams) |

---

## 3. Architecture & Internal Mechanics

```
Browser Client (GraphQL Query)
POST /graphql
{ user(id: "1") { name email } }
               │
               ▼
GraphQLModule (Apollo / Yoga Engine)
               │
               ├─> Validates query against compiled GraphQL Schema
               ├─> Compiles execution AST
               │
               ▼
Resolvers Layer (@Resolver(of => User))
               │
               ├─> @Query(() => User) getUser()
               │
               ▼
Field Resolvers (@ResolveField(() => [Post]))
(Executed ONLY if client requests 'posts' field!)
               │
               ▼
Database Query ──> Return exact JSON shape requested
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save @nestjs/graphql @apollo/server @nestjs/apollo graphql
```

### 4.2 Configuring `GraphQLModule` (Code-First)

```typescript
// src/app.module.ts
import { Module } from '@nestjs/common';
import { GraphQLModule } from '@nestjs/graphql';
import { ApolloDriver, ApolloDriverConfig } from '@nestjs/apollo';
import { join } from 'path';
import { UsersModule } from './users/users.module';

@Module({
  imports: [
    GraphQLModule.forRoot<ApolloDriverConfig>({
      driver: ApolloDriver,
      autoSchemaFile: join(process.cwd(), 'src/schema.gql'), // Code-First SDL generation
      sortSchema: true,
      playground: process.env.NODE_ENV !== 'production',    // Apollo Sandbox / Playground
      introspection: process.env.NODE_ENV !== 'production',
    }),
    UsersModule,
  ],
})
export class AppModule {}
```

---

### 4.3 Defining ObjectTypes and InputTypes

```typescript
// src/users/models/user.model.ts
import { ObjectType, Field, ID } from '@nestjs/graphql';

@ObjectType({ description: 'User account model' })
export class User {
  @Field(() => ID)
  id!: string;

  @Field()
  email!: string;

  @Field()
  name!: string;

  @Field({ defaultValue: true })
  isActive!: boolean;

  @Field(() => Date)
  createdAt!: Date;
}
```

```typescript
// src/users/dto/create-user.input.ts
import { InputType, Field } from '@nestjs/graphql';
import { IsEmail, MinLength } from 'class-validator';

@InputType()
export class CreateUserInput {
  @Field()
  @IsEmail()
  email!: string;

  @Field()
  @MinLength(2)
  name!: string;
}
```

---

### 4.4 Implementing Resolvers

```typescript
// src/users/users.resolver.ts
import { Resolver, Query, Mutation, Args, ID, ResolveField, Parent } from '@nestjs/graphql';
import { User } from './models/user.model';
import { CreateUserInput } from './dto/create-user.input';
import { UsersService } from './users.service';

@Resolver(() => User)
export class UsersResolver {
  constructor(private readonly usersService: UsersService) {}

  // 1. Query: Fetch single user by ID
  @Query(() => User, { name: 'user', nullable: true })
  async getUser(@Args('id', { type: () => ID }) id: string): Promise<User | null> {
    return this.usersService.findById(id);
  }

  // 2. Query: List all users
  @Query(() => [User], { name: 'users' })
  async getUsers(): Promise<User[]> {
    return this.usersService.findAll();
  }

  // 3. Mutation: Create a new user
  @Mutation(() => User)
  async createUser(@Args('input') input: CreateUserInput): Promise<User> {
    return this.usersService.create(input);
  }

  // 4. Field Resolver: Computed or relation field
  @ResolveField(() => String)
  displayName(@Parent() user: User): string {
    return `${user.name} (${user.email})`;
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Circular Type References
When defining bidirectional relations (e.g., `User` has many `Posts`, and `Post` belongs to `User`), circular TypeScript imports cause runtime errors:
`ReferenceError: Cannot access 'Post' before initialization`.
- **Handling**: Use lazy arrow functions in decorator type definitions:
```typescript
@Field(() => [Post])
posts: Post[];
```

### 5.2 Exposing Sensitive Fields
If a database entity includes `passwordHash`, exposing that entity directly in `@ObjectType()` makes the hash selectable by GraphQL clients.
- **Rule**: Never decorate database password fields with `@Field()`. Create dedicated GraphQL model classes separate from database entities.

---

## 6. Performance & Memory Implications

- **Schema Compilation**: In the Code-First approach, NestJS compiles the SDL once at startup via metadata reflection, adding ~10–20ms to cold start time and zero overhead during requests.
- **Field Selection**: Field resolvers execute **only** when the client explicitly requests that field in their query document, preventing unnecessary database joins.

---

## 7. Anti-Patterns & Common Mistakes

- **Treating Resolvers as Database Entities**: Adding TypeORM `@Entity()` and GraphQL `@ObjectType()` to the same class across large projects. This creates tight coupling between your database schema and your public API contract.
- **Leaving Introspection Enabled in Production**: Exposing your entire API schema to the public, allowing attackers to discover internal entities and fields.

---

## 8. Production Checklist & Operational Readiness

- [ ] `autoSchemaFile` configured for Code-First schema generation.
- [ ] `introspection: false` and `playground: false` configured for production environments.
- [ ] Input types validated using `ValidationPipe`.
- [ ] Sensitive fields (passwords, salts) excluded from `@Field()` declarations.

---

## 9. Migration & Version History

- **NestJS 9/10**: Migrated to Apollo Server 4 (`@apollo/server`), deprecating older Apollo v3 packages.
- **NestJS 11.x**: Validated with Apollo Server 4.11 and modern GraphQL 16.x AST execution.

---

## 10. Verification & Testing

Unit testing a GraphQL resolver:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { UsersResolver } from './users.resolver';
import { UsersService } from './users.service';

describe('UsersResolver', () => {
  let resolver: UsersResolver;
  let mockService: any;

  beforeEach(async () => {
    mockService = {
      findById: vi.fn().mockResolvedValue({ id: '1', name: 'Alice', email: 'a@b.com' }),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        UsersResolver,
        { provide: UsersService, useValue: mockService },
      ],
    }).compile();

    resolver = module.get<UsersResolver>(UsersResolver);
  });

  it('queries user by id', async () => {
    const user = await resolver.getUser('1');
    expect(user).toBeDefined();
    expect(user?.name).toBe('Alice');
    expect(mockService.findById).toHaveBeenCalledWith('1');
  });
});
```

---

## 11. Comprehensive Examples

### Generated Schema Sample (`src/schema.gql`)

```graphql
# ------------------------------------------------------
# THIS FILE WAS AUTOMATICALLY GENERATED (DO NOT MODIFY)
# ------------------------------------------------------

input CreateUserInput {
  email: String!
  name: String!
}

type Mutation {
  createUser(input: CreateUserInput!): User!
}

type Query {
  user(id: ID!): User
  users: [User!]!
}

"""User account model"""
type User {
  createdAt: DateTime!
  displayName: String!
  email: String!
  id: ID!
  isActive: Boolean!
  name: String!
}

"""
A date-time string at UTC, such as 2019-12-03T09:54:33Z, compliant with the date-time format.
"""
scalar DateTime
```

---

## 12. Cross-References & Further Reading

- [02-dataloader-and-n-plus-one.md](./02-dataloader-and-n-plus-one.md) - Solving N+1 query problems
- [03-graphql-auth-and-security.md](./03-graphql-auth-and-security.md) - Securing GraphQL APIs
- [04-subscriptions-and-federation.md](./04-subscriptions-and-federation.md) - Realtime subscriptions
- [NestJS GraphQL Documentation](https://docs.nestjs.com/graphql/quick-start)
