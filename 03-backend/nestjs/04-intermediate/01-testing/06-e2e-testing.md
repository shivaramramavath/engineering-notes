# E2E Testing

An end-to-end test starts your real Nest application (without opening a network port), sends it real HTTP requests, and asserts on the responses. It exercises everything between the socket and the database: routing, middleware, guards, pipes, interceptors, filters, serialization, and your services.

This is where you prove the **contract** of your API and that the [request pipeline](../../03-core-concepts/01-request-pipeline/README.md) is wired correctly. Keep these tests few and focused on critical flows; they're the slowest level.

Prerequisites: [Testing fundamentals](./01-testing-fundamentals.md), [mocking](./03-mocking.md), [integration testing](./05-integration-testing.md) (database setup and cleanup apply here too).

## Basic setup

The scaffold gives you `test/app.e2e-spec.ts` and `test/jest-e2e.json`.

```ts
// test/app.e2e-spec.ts
import { Test } from '@nestjs/testing';
import { INestApplication } from '@nestjs/common';
import request from 'supertest';
import { AppModule } from '../src/app.module';

describe('App (e2e)', () => {
  let app: INestApplication;

  beforeAll(async () => {
    const moduleRef = await Test.createTestingModule({ imports: [AppModule] }).compile();

    app = moduleRef.createNestApplication();
    await app.init();
  });

  afterAll(async () => {
    await app.close();
  });

  it('GET /health -> 200', () => {
    return request(app.getHttpServer()).get('/health').expect(200).expect({ status: 'ok' });
  });
});
```

Key points:

- `createNestApplication()` builds the app; `app.init()` initializes it (runs lifecycle hooks, registers routes). You do **not** call `listen()`. Supertest binds to an ephemeral port itself, which avoids port conflicts.
- `app.getHttpServer()` is what Supertest talks to.
- `afterAll(app.close())` is essential; otherwise connections stay open and Jest hangs.
- Run with `npm run test:e2e`, which uses `test/jest-e2e.json` (regex `.e2e-spec.ts$`, its own `rootDir`).
- Import style for Supertest depends on your TypeScript config: `import request from 'supertest'` needs `esModuleInterop`; otherwise use `import * as request from 'supertest'`. A `request is not a function` error means the wrong one.

## The biggest E2E gotcha: mirror `main.ts`

`main.ts` is **not executed** in tests. Anything configured there (global pipes, prefix, versioning, filters, CORS, shutdown hooks) is missing from your test app unless you recreate it. The classic symptom: a validation test returns 201 instead of 400 because no `ValidationPipe` was registered.

Fix it once by extracting configuration into a function used by both:

```ts
// src/setup-app.ts
import { INestApplication, ValidationPipe, VersioningType } from '@nestjs/common';

export function setupApp(app: INestApplication) {
  app.setGlobalPrefix('api');
  app.enableVersioning({ type: VersioningType.URI });
  app.useGlobalPipes(new ValidationPipe({ whitelist: true, forbidNonWhitelisted: true, transform: true }));
  app.useGlobalFilters(new AllExceptionsFilter(app.get(HttpAdapterHost)));
  return app;
}
```

```ts
// main.ts
const app = await NestFactory.create(AppModule);
setupApp(app);
await app.listen(3000);

// test
app = moduleRef.createNestApplication();
setupApp(app);
await app.init();
```

Alternative: register enhancers through `APP_PIPE`/`APP_FILTER`/`APP_GUARD` providers in `AppModule`, so they come with the module automatically ([overview](../../03-core-concepts/01-request-pipeline/01-pipeline-overview-and-execution-order.md)). Settings with no provider equivalent (prefix, versioning) still need the shared function.

## Making requests

```ts
const server = app.getHttpServer();

await request(server)
  .post('/users')
  .send({ email: 'a@b.com', password: 'secret123' })
  .expect(201)
  .expect((res) => {
    expect(res.body).toMatchObject({ email: 'a@b.com' });
    expect(res.body).not.toHaveProperty('passwordHash');   // serialization leak check
  });

await request(server).get('/users?page=2&limit=10').expect(200);
await request(server).get('/users/does-not-exist').expect(404);
await request(server).post('/users').send({ email: 'nope' }).expect(400);
```

For a failing assertion, the response body is the most useful thing to look at. Log it while debugging: `.expect((res) => console.log(res.body))`.

Other Supertest features you'll use:

```ts
.set('Authorization', `Bearer ${token}`)      // headers
.query({ page: 2 })                           // query string
.attach('file', Buffer.from('data'), 'a.txt') // file upload (multipart)
.field('title', 'Report')                     // multipart fields
request.agent(server)                         // keeps cookies between requests (session/cookie auth)
```

## Authentication in E2E tests

Two approaches; use the first for auth endpoints and critical flows, the second to keep other tests simple.

**1. Real login, real token** (tests the full auth stack):

```ts
async function login(app: INestApplication, email: string, password: string) {
  const res = await request(app.getHttpServer()).post('/auth/login').send({ email, password }).expect(201);
  return res.body.accessToken as string;
}

it('GET /me with a valid token', async () => {
  await createUser({ email: 'a@b.com', password: 'secret123' });
  const token = await login(app, 'a@b.com', 'secret123');

  await request(app.getHttpServer()).get('/me').set('Authorization', `Bearer ${token}`).expect(200);
});
```

**2. Override the guard** for tests that are about something else:

```ts
const moduleRef = await Test.createTestingModule({ imports: [AppModule] })
  .overrideGuard(JwtAuthGuard)
  .useValue({
    canActivate: (ctx: ExecutionContext) => {
      ctx.switchToHttp().getRequest().user = { id: 'u1', roles: ['admin'] };
      return true;
    },
  })
  .compile();
```

Always keep **some** tests with the real guard, including the negative cases: no token → 401, wrong role → 403, a user accessing another user's resource → 403/404. These are the tests that catch broken authorization ([authorization](../07-authorization/README.md)).

## Database and external services

Follow the integration-test rules ([integration testing](./05-integration-testing.md)): a real test database built from migrations, truncated between tests, one DB per Jest worker if running in parallel, never a shared/production database.

```ts
beforeEach(async () => {
  await resetDatabase(app.get(DataSource));
});
```

Fake only the third-party edges:

```ts
.overrideProvider(MailService).useValue({ send: jest.fn() })
.overrideProvider(StripeClient).useValue(fakeStripe)
```

Other things to neutralize in tests: rate limiting/throttling (override `ThrottlerGuard` or raise limits so unrelated tests don't hit 429), schedulers/cron (don't start them), and queue consumers (override or point at a test Redis).

## Seeding data

Create test data through the same app or repositories, using small builders:

```ts
async function createUser(overrides: Partial<CreateUserDto> = {}) {
  return app.get(UsersService).register({ email: `u${Date.now()}@t.com`, password: 'secret123', ...overrides });
}
```

Or insert through the repository/ORM directly for speed. Avoid long chains of HTTP calls just to set up state when a direct insert does the job; keep HTTP for the behavior under test.

## What to cover (and what not to)

| Cover with E2E | Leave to unit/integration |
|----------------|--------------------------|
| Critical flows: register → login → use token → logout | Every branch of business logic |
| Status codes and response shape per endpoint | Every validation rule variant (unit-test DTOs, one E2E per endpoint for wiring) |
| 400/401/403/404/409 behavior | Query correctness details |
| Serialization (no leaked fields) | Pure functions |
| Pipeline wiring: guards on the right routes, global pipes active | Internals of each pipeline class |
| File uploads, pagination contract, versioning/prefix | |

If a failure in an E2E test could have been caught faster by a unit test, push the detail down a level and keep a single E2E check for the wiring.

## Fastify

With the Fastify adapter, the app must be ready before requests:

```ts
import { NestFastifyApplication, FastifyAdapter } from '@nestjs/platform-fastify';

const app = moduleRef.createNestApplication<NestFastifyApplication>(new FastifyAdapter());
await app.init();
await app.getHttpAdapter().getInstance().ready();

await app.inject({ method: 'GET', url: '/health' }).then((res) => expect(res.statusCode).toBe(200));
```

`app.inject()` is Fastify's built-in test helper; Supertest also works against `app.getHttpServer()` once the instance is ready.

## Noise and logging

Nest logs request-time errors by default, which clutters test output. Reduce it:

```ts
app = moduleRef.createNestApplication({ logger: ['error'] });    // or logger: false
```

Expected 4xx responses aren't logged as errors by default; 5xx unknown errors are, so a noisy log often points at a real bug.

## Common mistakes

- **Not replicating `main.ts` setup**, so pipes/prefix/versioning/filters are missing and expectations fail or pass wrongly.
- **Forgetting `await app.init()`** or `app.close()`.
- **Calling `app.listen()`** in tests, causing port conflicts and open handles.
- **Overriding the auth guard everywhere**, so auth is never really tested.
- **Dirty database between tests**, causing order dependence and conflicts (409s).
- **Parallel workers sharing a database.**
- **Using `.expect(201)` without checking the body**; a 201 with the wrong payload still passes.
- **Hitting real third-party services** from tests.
- **Too many E2E tests**, making the suite slow and flaky.
- **Rate limiting/throttling** causing unrelated 429s.

## Debugging

- **Jest doesn't exit:** `npx jest --config test/jest-e2e.json --detectOpenHandles`. Usually an unclosed app, DB connection, Redis client, or scheduler.
- **Validation tests return 201:** global `ValidationPipe` isn't registered in the test app ([mirror main.ts](#the-biggest-e2e-gotcha-mirror-maints)).
- **404 on a route that exists:** missing global prefix or version in the test path (`/api/v1/...`).
- **401 unexpectedly:** the global guard is active; send a token or override it deliberately.
- **Timeouts:** app init is slow (DB connect, migrations). Raise `testTimeout` in `jest-e2e.json` and check what `init()` is waiting on.
- **Inspect failures:** print `res.body` and `res.headers`; check server logs for the underlying exception.
- **Run one test:** `npx jest --config test/jest-e2e.json -t "GET /me"`.

## Quick Summary

- E2E tests run the real app in-process: `createNestApplication()` + `init()` + Supertest against `getHttpServer()`. No `listen()`.
- `main.ts` doesn't run in tests; share a `setupApp(app)` function (or use `APP_*` providers) so test and production configuration match.
- Use real login for auth tests and guard overrides elsewhere; always keep negative auth cases.
- Real test DB, cleaned between tests; override only third-party edges, throttling, and schedulers.
- Keep E2E tests few and about contracts and wiring; always `close()` the app.

## Next

Section complete. Continue with [Database foundations](../02-database-foundations/README.md), or review the [testing overview](./README.md).

← Back to [Testing overview](./README.md)