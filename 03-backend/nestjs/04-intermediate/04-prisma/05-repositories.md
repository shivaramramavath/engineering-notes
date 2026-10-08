# Repositories

Prisma has no repository concept: the generated client **is** your data-access API (`prisma.user.findMany(...)`). Whether to wrap it in your own repository classes is a design choice, covered generally in [the repository pattern](../02-database-foundations/03-repository-pattern.md). This note covers the Prisma-specific parts: what to expose, how to type things, how to join transactions, and how to test.

Prerequisites: [Prisma Client](./03-prisma-client.md), [repository pattern](../02-database-foundations/03-repository-pattern.md).

## Do you need a repository with Prisma?

| Situation | Suggestion |
|-----------|------------|
| Simple CRUD service | Inject `PrismaService` straight into the service. Prisma's API is already expressive and typed |
| The same non-trivial query is used in several places | Extract it (a repository method or a function) |
| You want services unit-testable without mocking nested Prisma calls | Repository behind an abstraction |
| You want one place for soft-delete filters, default `select`s, error translation | Repository (or a client extension) |

Because Prisma's types are so specific (`Prisma.UserWhereInput`, query-shaped result types), a thin pass-through wrapper adds more friction than value. Add the layer where it earns its keep.

## A Prisma-backed repository

```ts
// users.repository.ts
import { Injectable } from '@nestjs/common';
import { Prisma } from '../generated/prisma/client';
import { PrismaService } from '../prisma/prisma.service';

const publicUser = {
  id: true,
  email: true,
  name: true,
  role: true,
  createdAt: true,
} satisfies Prisma.UserSelect;

export type PublicUser = Prisma.UserGetPayload<{ select: typeof publicUser }>;

@Injectable()
export class UsersRepository {
  constructor(private readonly prisma: PrismaService) {}

  findById(id: string): Promise<PublicUser | null> {
    return this.prisma.user.findUnique({ where: { id }, select: publicUser });
  }

  findCredentialsByEmail(email: string) {
    // the only place that selects passwordHash
    return this.prisma.user.findUnique({
      where: { email },
      select: { id: true, email: true, passwordHash: true, role: true },
    });
  }

  async list(params: { q?: string; page: number; limit: number }) {
    const where: Prisma.UserWhereInput = {
      active: true,
      ...(params.q ? { email: { contains: params.q, mode: 'insensitive' } } : {}),
    };

    const [items, total] = await this.prisma.$transaction([
      this.prisma.user.findMany({
        where,
        select: publicUser,
        orderBy: [{ createdAt: 'desc' }, { id: 'desc' }],
        skip: (params.page - 1) * params.limit,
        take: params.limit,
      }),
      this.prisma.user.count({ where }),
    ]);
    return { items, total };
  }
}
```

Techniques worth copying:

- **A shared `select` object** defined once (`satisfies Prisma.UserSelect`) keeps secrets like `passwordHash` out of every normal read, and `Prisma.UserGetPayload<{ select: typeof publicUser }>` derives the return type automatically, so there's no hand-written DTO that can drift.
- **One explicit method for credentials**, so reads of the password hash are rare and greppable.
- **Dynamic filters built as `Prisma.UserWhereInput`** with conditional spreads. Careful that an `undefined` value means "no filter" ([the undefined trap](./03-prisma-client.md)); here the conditional spread makes the intent explicit.

## Types: Prisma's vs your own

Two reasonable approaches:

| Approach | Pros | Cons |
|----------|------|------|
| **Use Prisma's generated types** (`User`, `Prisma.UserGetPayload<...>`) in services | Zero mapping, always in sync | Services depend on generated types; schema changes ripple |
| **Map to your own domain types** in the repository | Clean boundary, ORM swap possible | Mapping code, more files |

For most apps, using Prisma's types inside the module and **never exposing them to controllers** (map to response DTOs) is a pragmatic middle ground. Types come from your generated folder (the generator `output`), not from `@prisma/client`, in Prisma 7.

If you define a port (abstract class) for the repository, type its methods with **your own types**, not `Prisma.*`, or the abstraction leaks the ORM ([repository pattern](../02-database-foundations/03-repository-pattern.md)).

## Making repositories transaction-aware

Inside an interactive transaction you get a transaction client `tx` that replaces `prisma`. Repositories that should join the transaction need to accept it:

```ts
import { Prisma } from '../generated/prisma/client';

type Db = PrismaService | Prisma.TransactionClient;

@Injectable()
export class AccountsRepository {
  constructor(private readonly prisma: PrismaService) {}

  debit(id: string, amount: number, db: Db = this.prisma) {
    return db.account.updateMany({
      where: { id, balance: { gte: amount } },
      data: { balance: { decrement: amount } },
    });
  }

  credit(id: string, amount: number, db: Db = this.prisma) {
    return db.account.update({ where: { id }, data: { balance: { increment: amount } } });
  }
}

// service
await this.prisma.$transaction(async (tx) => {
  const res = await this.accounts.debit(fromId, amount, tx);
  if (res.count === 0) throw new BadRequestException('Insufficient funds');
  await this.accounts.credit(toId, amount, tx);
});
```

`Prisma.TransactionClient` is the type of `tx`. The default parameter keeps normal calls simple. More in [transactions](./07-transactions.md).

## Soft delete, defaults, and cross-cutting rules

Repositories are one place to enforce "never return deleted rows":

```ts
findActive() {
  return this.prisma.user.findMany({ where: { deletedAt: null } });
}
```

The risk is that someone queries `prisma.user` directly and forgets the filter. A **client extension** can centralize it, intercepting queries so the rule applies everywhere the extended client is used:

```ts
const extended = prisma.$extends({
  query: {
    user: {
      async findMany({ args, query }) {
        args.where = { deletedAt: null, ...args.where };
        return query(args);
      },
    },
  },
});
```

Extensions are powerful but implicit, so test them and confirm they cover the operations you rely on (`findFirst`, `findUnique`, `count`, relation loads, and so on). Consult the Prisma docs for the current extension API. If you adopt one, decide how the extended client reaches Nest DI (for example, a provider that builds it once). Background: [soft delete](../../08-architecture-and-patterns/04-real-world-patterns/02-soft-delete.md).

## Translating Prisma errors

Prisma throws `Prisma.PrismaClientKnownRequestError` with codes (`P2002` unique, `P2003` foreign key, `P2025` record not found). Translate the ones you care about in the repository so services see domain errors:

```ts
async create(data: Prisma.UserCreateInput) {
  try {
    return await this.prisma.user.create({ data });
  } catch (err) {
    if (err instanceof Prisma.PrismaClientKnownRequestError && err.code === 'P2002') {
      throw new EmailAlreadyUsedError();
    }
    throw err;
  }
}
```

Keep a global filter as the safety net for everything else ([database errors](../02-database-foundations/07-database-errors.md), [exception filters](../../03-core-concepts/01-request-pipeline/08-exception-filters.md)). Note the `await` inside `try`: returning the promise without awaiting skips your `catch`.

## Testing

**Unit-testing services** against a repository: provide a fake repository (or an in-memory implementation of your port) ([mocking](../01-testing/03-mocking.md)). **Unit-testing a repository** by mocking `PrismaService` is possible but low value: you'd be asserting that the mock was called with the arguments you wrote. Prefer **integration tests against a real database** for repositories, since only that proves queries, constraints, and nested writes actually work ([integration testing](../01-testing/05-integration-testing.md)):

```ts
beforeEach(async () => {
  await prisma.$executeRaw`TRUNCATE TABLE "users" RESTART IDENTITY CASCADE`;
});
```

For mocking `PrismaService` anyway, a plain object with `jest.fn()`s for the methods you use is enough; community helpers exist for deep mocks, but weigh the "everything returns a truthy mock" caveat from [mocking](../01-testing/03-mocking.md).

## Common mistakes

- **Wrapping Prisma in a generic pass-through repository**, adding indirection without benefit.
- **Returning full models from repositories** (including `passwordHash`) instead of using a shared `select`.
- **Exposing `Prisma.*` types through an abstraction** that was meant to hide the ORM.
- **Forgetting to pass `tx`** into repositories inside a transaction, so work runs outside it.
- **Hand-written DTOs duplicating query result types** that can drift from the query.
- **Relying on soft-delete filtering by convention** with no enforcement.
- **`return this.prisma...` inside `try` without `await`**, so errors skip the `catch`.
- **Mocking Prisma heavily** and skipping integration tests for queries.

## Debugging

- Wrong or missing fields in results: check which `select`/`include` the repository method uses (types follow it).
- Writes happening outside your transaction: a repository call used `this.prisma` instead of the `tx` you passed.
- Error translation not triggering: missing `await` in the `try`, or the error is a different class/code (log `err.code` and `err.meta`).
- Soft-deleted rows appearing: some query path bypasses your filter or extension (relation loads, `count`, raw queries).

## Quick Summary

- Prisma's client is already a typed data-access API; add repositories only where they earn their keep.
- Define shared `select` objects (with `satisfies Prisma.XSelect`) and derive result types via `Prisma.XGetPayload`; isolate credential reads.
- Keep `Prisma.*` types out of abstractions meant to hide the ORM; don't leak models to controllers.
- Accept `Prisma.TransactionClient` in repository methods to join transactions.
- Translate `P2xxx` errors in repositories (awaited inside `try`); test repositories against a real database.

## Next

[Migrations →](./06-migrations.md)
