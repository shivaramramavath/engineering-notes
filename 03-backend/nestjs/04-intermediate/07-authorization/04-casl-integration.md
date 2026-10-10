# CASL Integration

[CASL](https://casl.js.org) is an isomorphic authorization library for JavaScript/TypeScript. You define **rules** (`can` / `cannot`, with optional conditions and field lists) once per user, then ask questions of the resulting **ability**: *can this user update this article? which fields? which records can they list?* It formalizes the ABAC patterns from [the previous note](./03-abac-and-policies.md) and, with adapters, can turn the same rules into **database query filters**.

Use it when rules are numerous or shared with the frontend. For a handful of simple checks, plain [policy functions](./03-abac-and-policies.md) are less machinery.

> Targets `@casl/ability` v6 (`createMongoAbility`). Older code uses the `Ability` class directly; adapter packages (`@casl/prisma`, `@casl/mongoose`) have their own version requirements. Verify API details against the CASL documentation for the versions you install.

Prerequisites: [Authorization fundamentals](./01-authorization-fundamentals.md), [RBAC](./02-rbac.md), [ABAC and policies](./03-abac-and-policies.md).

## Install

```bash
npm i @casl/ability
# optional adapters
npm i @casl/prisma        # translate rules into Prisma `where` filters
npm i @casl/mongoose      # translate rules into Mongoose queries
```

## Defining actions, subjects, and the ability type

```ts
// casl/casl.types.ts
import { InferSubjects, MongoAbility } from '@casl/ability';

export enum Action {
  Manage = 'manage',        // CASL special: any action
  Create = 'create',
  Read = 'read',
  Update = 'update',
  Delete = 'delete',
}

export type Subjects = InferSubjects<typeof Article | typeof User> | 'all';    // 'all' = any subject
export type AppAbility = MongoAbility<[Action, Subjects]>;
```

- **Action:** what is attempted. `manage` is a wildcard meaning every action.
- **Subject:** the thing acted on: a class (`Article`), a string type (`'Article'`), or `'all'`.
- The ability type gives you compile-time checking of actions and subjects.

## The ability factory

Build an ability **per request/user** from their roles and attributes:

```ts
// casl/casl-ability.factory.ts
import { AbilityBuilder, createMongoAbility, ExtractSubjectType } from '@casl/ability';

@Injectable()
export class CaslAbilityFactory {
  createForUser(user: AuthUser): AppAbility {
    const { can, cannot, build } = new AbilityBuilder<AppAbility>(createMongoAbility);

    if (user.roles.includes(Role.Admin)) {
      can(Action.Manage, 'all');                                   // admins can do everything
    } else {
      can(Action.Read, Article, { status: 'published' });         // anyone: published articles
      can(Action.Read, Article, { authorId: user.id });           // plus their own
      can(Action.Create, Article);
      can(Action.Update, Article, { authorId: user.id });         // update only their own
      can(Action.Delete, Article, { authorId: user.id });
      cannot(Action.Delete, Article, { status: 'published' });    // ...but never published ones
      can(Action.Update, User, ['name', 'avatar'], { id: user.id });   // fields: only these, only themselves
    }

    return build({
      detectSubjectType: (item) => item.constructor as ExtractSubjectType<Subjects>,   // class instances → subject type
    });
  }
}
```

Rule semantics you must know:

- **Order matters; later rules override earlier ones.** A `cannot` after a `can` removes that permission for matching subjects; a `can` after a `cannot` re-grants. Put broad `can`s first and specific `cannot`s after.
- **Conditions** use a MongoDB-style query object (`{ authorId: user.id }`, `{ status: { $in: ['draft'] } }`), evaluated in JavaScript against the subject instance.
- **Fields** (an array as the third argument) limit a rule to those properties.
- **`manage` + `all`** is the superuser rule.
- `detectSubjectType` tells CASL how to find the subject type from an instance. For **class instances** use `item.constructor` as above; for plain objects tag them with the `subject()` helper (below).

Register the factory as a provider (and export it) in a `CaslModule`:

```ts
@Module({ providers: [CaslAbilityFactory], exports: [CaslAbilityFactory] })
export class CaslModule {}
```

## Checking permissions

```ts
const ability = this.abilityFactory.createForUser(user);

ability.can(Action.Update, article);          // article is an Article INSTANCE → conditions are evaluated
ability.cannot(Action.Delete, article);

ability.can(Action.Create, Article);          // check against a TYPE → "is it possible at all?"
```

### Gotcha: checking a type ignores conditions

`ability.can('update', Article)` (the **class**, not an instance) answers "can the user update **some** article?", which is `true` if **any** `update` rule exists, even one conditioned on ownership. Always check against the **actual loaded instance** for record-level decisions. Use the type form only for "may I show a Create button / call this route at all".

### Plain objects: the `subject()` helper

Records from Prisma, `lean()` Mongoose queries, or raw SQL are plain objects, not class instances. Tag them:

```ts
import { subject } from '@casl/ability';

ability.can(Action.Update, subject('Article', articleRow));
```

This pairs with string subject types (`'Article'`) in the rules. Choose class-based or string-based subjects and use one style consistently.

### Throwing: `ForbiddenError`

```ts
import { ForbiddenError } from '@casl/ability';

ForbiddenError.from(ability).throwUnlessCan(Action.Update, article);   // throws ForbiddenError (not an HttpException)
```

CASL's `ForbiddenError` isn't a Nest `HttpException`, so map it to a 403 with an [exception filter](../../03-core-concepts/01-request-pipeline/08-exception-filters.md):

```ts
@Catch(ForbiddenError)
export class CaslForbiddenFilter implements ExceptionFilter {
  catch(_err: ForbiddenError<AppAbility>, host: ArgumentsHost) {
    const res = host.switchToHttp().getResponse();
    res.status(403).json({ statusCode: 403, message: 'Forbidden' });
  }
}
```

(Or catch it and rethrow Nest's `ForbiddenException`/`NotFoundException` yourself.)

## Route-level: a policies guard

The Nest docs' pattern: declare policy handlers with a decorator, run them in a guard.

```ts
type PolicyHandler = (ability: AppAbility) => boolean;

export const CHECK_POLICIES_KEY = 'check_policy';
export const CheckPolicies = (...handlers: PolicyHandler[]) => SetMetadata(CHECK_POLICIES_KEY, handlers);

@Injectable()
export class PoliciesGuard implements CanActivate {
  constructor(private readonly reflector: Reflector, private readonly abilities: CaslAbilityFactory) {}

  canActivate(ctx: ExecutionContext) {
    const handlers = this.reflector.get<PolicyHandler[]>(CHECK_POLICIES_KEY, ctx.getHandler()) ?? [];
    const { user } = ctx.switchToHttp().getRequest();
    if (!user) return false;

    const ability = this.abilities.createForUser(user);
    return handlers.every((h) => h(ability));
  }
}
```

```ts
@UseGuards(PoliciesGuard)
@CheckPolicies((ability) => ability.can(Action.Create, Article))     // type-level check: fine for "may create at all"
@Post()
create(@Body() dto: CreateArticleDto) {}
```

This guard has no record loaded, so it is only suitable for **type-level** checks. Record-level checks happen in the service:

```ts
async update(id: string, dto: UpdateArticleDto, user: AuthUser) {
  const article = await this.repo.findById(id);
  if (!article) throw new NotFoundException();

  const ability = this.abilities.createForUser(user);
  ForbiddenError.from(ability).throwUnlessCan(Action.Update, subject('Article', article));
  return this.repo.update(id, dto);
}
```

To avoid rebuilding the ability repeatedly in one request, build it **once** (in the authentication guard, an interceptor, or lazily via a request-scoped/cached helper) and attach it to the request, for example `req.ability`, exposed with a `@CurrentAbility()` [param decorator](../../03-core-concepts/01-request-pipeline/10-custom-decorators.md).

## Field-level permissions

Rules can restrict which fields an action covers (`can('update', 'User', ['name','avatar'], { id })`). To enforce on writes, filter the incoming data to the permitted fields:

```ts
import { permittedFieldsOf } from '@casl/ability/extra';
import { pick } from 'lodash';

const fields = permittedFieldsOf(ability, Action.Update, subject('User', userRow), {
  fieldsFrom: (rule) => rule.fields || ALL_USER_FIELDS,     // rules without a field list allow every field
});
const changes = pick(dto, fields);                          // ignore anything not permitted
```

Or check individually: `ability.can('update', subject('User', row), 'role')` returns whether that field may be updated. For reads, shape the response using the permitted `read` fields ([serialization](../../03-core-concepts/02-validation-and-serialization/07-serialization.md)). Remember to also whitelist DTO fields so unknown properties can't slip through ([mass assignment](./01-authorization-fundamentals.md)).

## Query-level: authorizing lists

The standout CASL feature: the **same rules** that answer "can I update this article?" can be converted into a **database filter** so list endpoints return only what the user may read, avoiding the read-predicate/filter divergence problem from [ABAC](./03-abac-and-policies.md).

### Prisma

```ts
import { createPrismaAbility, PrismaQuery, Subjects } from '@casl/prisma';
import { PureAbility, AbilityBuilder } from '@casl/ability';
import { accessibleBy } from '@casl/prisma';
import type { Article, User } from '../generated/prisma/client';       // your generated models

type AppPrismaAbility = PureAbility<[string, Subjects<{ Article: Article; User: User }> | 'all'], PrismaQuery>;

// build with: new AbilityBuilder<AppPrismaAbility>(createPrismaAbility)
// rules use Prisma-style conditions: can('read', 'Article', { authorId: user.id })

const where = accessibleBy(ability, 'read').Article;                    // a Prisma `ArticleWhereInput`
const articles = await this.prisma.article.findMany({ where: { AND: [where, userFilters] } });
```

`accessibleBy(ability, action).<Model>` returns a `where` object representing the rules for that action, so combine it with your own filters using `AND`. If the user has **no** applicable rule it produces a filter that matches nothing (verify in your version).

### Mongoose

```ts
import { accessibleRecordsPlugin } from '@casl/mongoose';

mongoose.plugin(accessibleRecordsPlugin);              // once, before models are compiled

const articles = await this.articleModel.accessibleBy(ability, Action.Read).lean();
```

### TypeORM and others

There's no first-party TypeORM adapter. Options: use CASL's lower-level utilities (`rulesToQuery`, `rulesToFields` from `@casl/ability/extra`) to translate rules into your ORM's conditions, use a community package, or keep the list filter hand-written next to the policy (and test them against each other). Check the CASL docs for the current adapter list.

Rule-to-query translation supports a subset of query operators (conditions must be expressible in the target database), and **inverted (`cannot`) rules** with complex conditions can be harder to translate. Test list endpoints with a real database ([integration testing](../01-testing/05-integration-testing.md)).

## Sharing rules with the frontend

CASL abilities work in browsers, so you can hide buttons the user can't use. Send the serialized rules:

```ts
import { packRules } from '@casl/ability/extra';

@Get('me/abilities')
abilities(@CurrentUser() user: AuthUser) {
  return packRules(this.abilityFactory.createForUser(user).rules);
}
```

The client rebuilds the ability (`unpackRules` + `createMongoAbility`). Keep in mind: this is **UX only**. The server must still enforce every check. Rules containing server-only data (user ids in conditions) are visible to the client; don't encode secrets in them.

## Practical guidance

- **Build abilities from fresh user data** (roles, teams) per request; don't cache them across users. Cache per request if it's costly.
- **Keep rule definitions readable**: group by role, comment intent, prefer a few clear rules over many clever ones.
- **Combine with RBAC**: use roles to decide which rule sets to load (as in the factory), and permissions/policies for fine-grained checks.
- **Deny by default** still applies: no matching `can` means denied.
- **Version your rules in tests**: the ability factory is the most important thing to cover with table-driven tests.

## Testing

```ts
describe('CaslAbilityFactory', () => {
  const factory = new CaslAbilityFactory();
  const author = { id: 'u1', roles: [Role.User] } as AuthUser;
  const admin = { id: 'u9', roles: [Role.Admin] } as AuthUser;

  const own = Object.assign(new Article(), { authorId: 'u1', status: 'draft' });
  const publishedOwn = Object.assign(new Article(), { authorId: 'u1', status: 'published' });
  const others = Object.assign(new Article(), { authorId: 'u2', status: 'draft' });

  it.each([
    ['author updates own draft',       author, Action.Update, own,          true],
    ['author updates others',          author, Action.Update, others,       false],
    ['author deletes own published',   author, Action.Delete, publishedOwn, false],
    ['admin deletes anything',         admin,  Action.Delete, others,       true],
  ])('%s', (_n, user, action, article, expected) => {
    expect(factory.createForUser(user).can(action, article)).toBe(expected);
  });
});
```

Then E2E tests per endpoint for the enforcement and the status codes (403 vs 404), plus list-endpoint tests that verify other users' records never appear ([E2E testing](../01-testing/06-e2e-testing.md)).

## When to use CASL (and when not)

| Use CASL when | Prefer plain policy functions when |
|---------------|-------------------------------------|
| Many resource types and rules, with field-level and conditional access | A few resources and rules |
| You want the same rules for checks **and** list filtering | Lists are simple to scope by hand |
| You want to share rules with a frontend | Backend-only checks |
| Rules come from roles/data and need a uniform engine | Team prefers minimal dependencies |

CASL adds concepts (subjects, `detectSubjectType`, adapters) to learn. If your rules fit in a few policy functions, don't adopt it prematurely.

## Common mistakes

- **Checking against a type/class** for record-level decisions (conditions ignored).
- **Passing plain objects without `subject()`** (or without `detectSubjectType`), so rules for the subject don't match and everything is denied (or wrongly allowed via `all`).
- **Rule order mistakes**: a later broad `can` overriding an earlier `cannot`.
- **Forgetting `ForbiddenError` isn't an `HttpException`**, resulting in 500s instead of 403s.
- **Checking only single-record endpoints** and forgetting list queries (not using `accessibleBy` or an equivalent filter).
- **Building abilities from stale or request-supplied data.**
- **Relying on frontend ability checks** for security.
- **Giving `can('manage', 'all')` too broadly** (for example through a mapping bug).
- **Not whitelisting fields** on writes after granting field-limited rules.
- **Rebuilding the ability repeatedly** in one request.

## Debugging

- Unexpected deny: print `ability.rules` and use `ability.relevantRuleFor(action, subject)` to see which rule matched (or didn't). Check the subject type detection (`subject('Article', obj)` / `constructor`).
- Unexpected allow: look for a broad `can`/`manage all` rule, or a type-level check that ignored conditions.
- `Cannot read properties ... constructor`: `detectSubjectType` received `undefined`/a string; guard for missing records before checking.
- Conditions never match: field names/values differ in type (`ObjectId` vs string) between the rule and the subject; compare with `String(...)` or normalize.
- Filter from `accessibleBy` returns too much/little: print it and test against real data; verify inverted rules and operator support for your adapter version.

## Quick Summary

- CASL expresses authorization as rules (`can`/`cannot`, conditions, fields) compiled into an **ability** you query; build one per user from fresh data.
- Check **instances** for record-level decisions (type checks ignore conditions); use `subject('Type', obj)` for plain objects; later rules override earlier ones.
- Use a policies guard for type-level route checks, and `ForbiddenError.from(ability).throwUnlessCan(...)` (mapped to 403 by a filter) in services for record-level checks.
- Field rules (`permittedFieldsOf`) and query-level helpers (`accessibleBy` for Prisma/Mongoose) keep writes, reads, and lists consistent with the same rules.
- Rules sent to the frontend are UX only; test the ability factory with tables and verify enforcement and list scoping end to end.

## Next

Section complete. Continue with [API design](../08-api-design/README.md), or revisit [Authentication](../06-authentication/README.md) to see how `req.user` is established.

← Back to [Authorization overview](./README.md)