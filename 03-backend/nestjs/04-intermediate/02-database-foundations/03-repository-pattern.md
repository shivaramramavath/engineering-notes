# Repository Pattern

A repository is a class that hides **how data is stored and queried** behind methods that speak your domain's language: `findActiveByEmail()`, `save(order)`. Services call the repository and never see SQL, query builders, or ORM specifics.

Whether you need one depends on your app. This note covers what the pattern buys you, what it costs, and how to build it in Nest without the usual over-engineering.

Prerequisites: [Database architecture](./01-database-architecture.md), [custom providers](../../03-core-concepts/04-modules-and-di/05-custom-providers.md), [injection tokens](../../03-core-concepts/04-modules-and-di/06-injection-tokens-and-optional-dependencies.md).

## Do you need one?

| Situation | Recommendation |
|-----------|----------------|
| Simple CRUD, one ORM, small team | Call the ORM from the service. A repository layer adds ceremony |
| Same non-trivial queries used by several services | Extract into a repository |
| Complex queries you want to name, test, and optimize in one place | Repository |
| You want unit tests of services without mocking ORM internals | Repository with an interface/fake |
| You might swap persistence (SQL ↔ other) or isolate the domain from the ORM | Repository behind an abstraction |

TypeORM already gives you `Repository<T>`, and Prisma's client delegates (`prisma.user`) play a similar role. A "repository" here usually means **your own thin class on top of them**, with intent-revealing methods. Wrapping an ORM repository in a class that just forwards `find`/`save` adds nothing.

## Building one

### 1. Describe what the service needs (the port)

```ts
// users.repository.ts
export abstract class UsersRepository {
  abstract findById(id: string): Promise<User | null>;
  abstract findByEmail(email: string): Promise<User | null>;
  abstract create(data: NewUser): Promise<User>;
  abstract listActive(page: number, limit: number): Promise<{ items: User[]; total: number }>;
}
```

An abstract class works as both the type and the DI token, so no `@Inject()` is needed ([tokens](../../03-core-concepts/04-modules-and-di/06-injection-tokens-and-optional-dependencies.md)). `User` and `NewUser` are your own types, not ORM classes.

### 2. Implement it with your ORM (the adapter)

**TypeORM:**

```ts
@Injectable()
export class TypeOrmUsersRepository extends UsersRepository {
  constructor(@InjectRepository(UserEntity) private readonly repo: Repository<UserEntity>) {
    super();
  }

  async findById(id: string) {
    const row = await this.repo.findOneBy({ id });
    return row ? toUser(row) : null;
  }

  async findByEmail(email: string) {
    const row = await this.repo.findOneBy({ email });
    return row ? toUser(row) : null;
  }

  async create(data: NewUser) {
    return toUser(await this.repo.save(this.repo.create(data)));
  }

  async listActive(page: number, limit: number) {
    const [rows, total] = await this.repo.findAndCount({
      where: { active: true },
      order: { createdAt: 'DESC' },
      skip: (page - 1) * limit,
      take: limit,
    });
    return { items: rows.map(toUser), total };
  }
}
```

**Prisma:**

```ts
@Injectable()
export class PrismaUsersRepository extends UsersRepository {
  constructor(private readonly prisma: PrismaService) {
    super();
  }

  findById(id: string) {
    return this.prisma.user.findUnique({ where: { id } });
  }
  findByEmail(email: string) {
    return this.prisma.user.findUnique({ where: { email } });
  }
  create(data: NewUser) {
    return this.prisma.user.create({ data });
  }
  async listActive(page: number, limit: number) {
    const where = { active: true };
    const [items, total] = await this.prisma.$transaction([
      this.prisma.user.findMany({ where, orderBy: { createdAt: 'desc' }, skip: (page - 1) * limit, take: limit }),
      this.prisma.user.count({ where }),
    ]);
    return { items, total };
  }
}
```

(`findUnique({ where: { email } })` requires `email` to be unique in the Prisma schema.)

### 3. Bind port to adapter in the module

```ts
@Module({
  imports: [TypeOrmModule.forFeature([UserEntity])],
  providers: [
    UsersService,
    { provide: UsersRepository, useClass: TypeOrmUsersRepository },
  ],
  exports: [UsersService],        // the repository stays private to the module
})
export class UsersModule {}
```

The service depends only on the port:

```ts
@Injectable()
export class UsersService {
  constructor(private readonly users: UsersRepository) {}
}
```

## What the repository should (and shouldn't) do

**Do:**

- Name methods by **intent**: `findActiveByEmail`, `listOverdueInvoices`, not `findWhere(filter)`.
- Return **your types** (or entities consistently), not query builders or ORM-specific results.
- **Translate database errors** into domain errors where it makes the service simpler ([database errors](./07-database-errors.md)).
- Own pagination, sorting, and eager-loading decisions for its queries.

**Don't:**

- Put **business rules** here ("a user can't have more than 3 projects"). That belongs in the service/domain.
- Expose `QueryBuilder`, Prisma `where` types, or Mongoose documents to callers; it defeats the abstraction.
- Build a **generic `BaseRepository<T>`** with `find(options)` that forwards arbitrary ORM options. It leaks the ORM through every call and gives you nothing a plain ORM repository doesn't.
- Hide expensive behavior behind innocent names (a method that triggers N+1 queries).

## Testing benefits

Services depend on the abstract class, so unit tests can swap in a fake:

```ts
{ provide: UsersRepository, useClass: InMemoryUsersRepository }
```

The service test asserts on behavior without mocking ORM method chains ([mocking](../01-testing/03-mocking.md)). The repository itself is tested against a **real database** in [integration tests](../01-testing/05-integration-testing.md), since only a real database proves queries and constraints work.

## Repositories and transactions

A multi-step write must run several repository calls inside **one transaction**, so repositories need a way to join it. Common approaches:

- Pass a transaction handle (`tx`/`manager`) as an optional parameter to repository methods.
- Make the repository obtain its executor from a transaction context (for example, request-scoped or `AsyncLocalStorage`-based helpers).

Both are covered in [transactions](./04-transactions.md). Decide this early; retrofitting transaction support into a repository layer designed without it is painful.

## Common mistakes

- **A repository that only forwards ORM calls**, adding indirection with no benefit.
- **Generic base repositories** that leak ORM options everywhere.
- **Business logic in repositories.**
- **Returning ORM entities/documents** that carry lazy relations or internal fields to controllers.
- **Exporting the repository** from the module, letting other modules bypass the service.
- **No plan for transactions**, so multi-repository operations aren't atomic.
- **Mocking the repository in "integration" tests**, so query bugs go unnoticed.
- **Creating the abstraction with only one implementation "just in case"** in a tiny app.

## Quick Summary

- A repository exposes intent-named data-access methods and hides the ORM.
- Skip it for trivial CRUD; add it for shared/complex queries, testability, and isolation.
- Use an abstract class (port) + ORM implementation (adapter), bound with `{ provide: Port, useClass: Adapter }`.
- Keep business rules out of it, don't leak ORM types, avoid generic base repositories.
- Test services with fakes and repositories against a real database; design transaction participation up front.

## Next

[Transactions →](./04-transactions.md)
