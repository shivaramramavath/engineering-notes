# Testing Pipeline Components

Middleware, guards, pipes, interceptors, and exception filters are ordinary classes with one method each, so most of them can be unit-tested by **calling that method directly** with small fakes for what Nest would normally pass in. The wiring (that they actually run, in the right order, on the right routes) is verified with a few [E2E tests](./06-e2e-testing.md).

Prerequisites: [Request pipeline](../../03-core-concepts/01-request-pipeline/README.md), [execution context](../../03-core-concepts/01-request-pipeline/09-execution-context.md), [mocking](./03-mocking.md).

## The two-level approach

| Level | Proves | Cost |
|-------|--------|------|
| Direct unit test of the class | The component's **logic** is right | Cheap, fast, many cases |
| E2E through a real route | The component is **bound and ordered** correctly | Slower, a few cases |

A guard that is perfectly implemented but never attached to the route protects nothing. Test both.

## A reusable fake `ExecutionContext`

Guards and interceptors receive an `ExecutionContext`. Build a minimal one once and reuse it.

```ts
// test/utils/execution-context.ts
import { ExecutionContext } from '@nestjs/common';

export function createExecutionContext(options: {
  request?: Record<string, unknown>;
  response?: Record<string, unknown>;
  handler?: Function;
  cls?: Function;
  type?: string;
} = {}): ExecutionContext {
  const { request = {}, response = {}, handler = () => {}, cls = class {}, type = 'http' } = options;
  return {
    switchToHttp: () => ({
      getRequest: () => request,
      getResponse: () => response,
      getNext: () => undefined,
    }),
    getHandler: () => handler,
    getClass: () => cls,
    getType: () => type,
    getArgs: () => [request, response],
    getArgByIndex: (i: number) => [request, response][i],
  } as unknown as ExecutionContext;
}
```

Cast through `unknown` because you're implementing only the parts the code under test uses. If a component touches another method, extend the helper. (The community `@golevelup/ts-jest` `createMock<ExecutionContext>()` is an alternative that auto-mocks everything; see the caveat in [mocking](./03-mocking.md).)

## Guards

Test the decision, including how it reads metadata. Using a **real** `Reflector` with a real decorator is more faithful than mocking `reflector.get`:

```ts
// roles.guard.spec.ts
import { Reflector } from '@nestjs/core';

describe('RolesGuard', () => {
  const reflector = new Reflector();
  const guard = new RolesGuard(reflector);

  class Controller {
    @Roles('admin')
    adminOnly() {}

    open() {}
  }

  const ctxFor = (handler: Function, user?: { roles: string[] }) =>
    createExecutionContext({ request: { user }, handler, cls: Controller });

  it('allows a user with a required role', () => {
    expect(guard.canActivate(ctxFor(Controller.prototype.adminOnly, { roles: ['admin'] }))).toBe(true);
  });

  it('denies a user without the role', () => {
    expect(guard.canActivate(ctxFor(Controller.prototype.adminOnly, { roles: ['user'] }))).toBe(false);
  });

  it('allows any caller when no roles are declared', () => {
    expect(guard.canActivate(ctxFor(Controller.prototype.open))).toBe(true);
  });
});
```

For guards that throw (`UnauthorizedException`) or are async:

```ts
await expect(guard.canActivate(ctx)).rejects.toThrow(UnauthorizedException);   // async guard
expect(() => guard.canActivate(ctx)).toThrow(UnauthorizedException);           // sync guard
```

Cases worth covering: no credentials, malformed credentials, expired/invalid token, valid token, public route bypass, class-level vs handler-level metadata. Auth design context: [guards](../../03-core-concepts/01-request-pipeline/04-guards.md).

## Pipes

A pipe's `transform(value, metadata)` is a plain function.

```ts
describe('TrimPipe', () => {
  const pipe = new TrimPipe();
  const meta = { type: 'body', metatype: String } as const;

  it('trims strings', () => {
    expect(pipe.transform('  hi  ', meta)).toBe('hi');
  });

  it('leaves non-strings untouched', () => {
    expect(pipe.transform(42 as any, meta)).toBe(42);
  });
});
```

Built-in pipes with options behave the same way:

```ts
await expect(new ParseIntPipe().transform('abc', { type: 'param' })).rejects.toThrow(BadRequestException);
await expect(new ParseIntPipe().transform('42', { type: 'param' })).resolves.toBe(42);
```

(`ParseIntPipe.transform` is async, so use `await`/`resolves`.)

### Testing a DTO's validation rules

Run the real `ValidationPipe` against the DTO, with the **same options as production**:

```ts
const pipe = new ValidationPipe({ whitelist: true, forbidNonWhitelisted: true, transform: true });
const meta = { type: 'body', metatype: CreateUserDto } as const;

it('rejects an invalid email', async () => {
  await expect(pipe.transform({ email: 'nope', password: 'secret123' }, meta))
    .rejects.toThrow(BadRequestException);
});

it('strips unknown properties', async () => {
  await expect(pipe.transform({ email: 'a@b.com', password: 'secret123', role: 'admin' }, meta))
    .rejects.toThrow(BadRequestException);   // forbidNonWhitelisted
});

it('returns a DTO instance for valid input', async () => {
  const dto = await pipe.transform({ email: 'a@b.com', password: 'secret123' }, meta);
  expect(dto).toBeInstanceOf(CreateUserDto);
});
```

Test the **boundaries** of every rule (too short, too long, wrong type, missing, extra). See [ValidationPipe](../../03-core-concepts/02-validation-and-serialization/02-validation-pipe.md). To avoid duplicating production options, export them from one place (`validationPipeOptions`) and import in both `main.ts` and tests.

## Interceptors

Provide a fake `CallHandler` returning an Observable and convert the result with `lastValueFrom`.

```ts
import { of, throwError, lastValueFrom } from 'rxjs';

describe('ResponseEnvelopeInterceptor', () => {
  const reflector = new Reflector();
  const interceptor = new ResponseEnvelopeInterceptor(reflector);
  const ctx = createExecutionContext();

  it('wraps the handler result', async () => {
    const next = { handle: () => of({ id: 1 }) };
    await expect(lastValueFrom(interceptor.intercept(ctx, next))).resolves.toEqual({ data: { id: 1 } });
  });

  it('propagates errors', async () => {
    const next = { handle: () => throwError(() => new Error('boom')) };
    await expect(lastValueFrom(interceptor.intercept(ctx, next))).rejects.toThrow('boom');
  });
});
```

Interceptors that don't call `next.handle()` (caches) should be tested with a `handle` mock to verify it was **not** called:

```ts
const handle = jest.fn();
await lastValueFrom(cacheInterceptor.intercept(ctx, { handle }));
expect(handle).not.toHaveBeenCalled();
```

If `intercept` is `async`, `await` it first to get the Observable. For timeouts, use RxJS `TestScheduler`, or fake timers, with a `handle` that never emits. See [interceptors](../../03-core-concepts/01-request-pipeline/05-interceptors.md) and [recipes](../../03-core-concepts/01-request-pipeline/06-interceptor-recipes.md).

## Exception filters

Fake the `ArgumentsHost` with a response that records `status`/`json`:

```ts
describe('HttpExceptionFilter', () => {
  const filter = new HttpExceptionFilter();

  it('formats an HttpException', () => {
    const json = jest.fn();
    const status = jest.fn().mockReturnValue({ json });
    const host = {
      switchToHttp: () => ({
        getResponse: () => ({ status }),
        getRequest: () => ({ url: '/cats/1' }),
      }),
    } as unknown as ArgumentsHost;

    filter.catch(new NotFoundException('Cat not found'), host);

    expect(status).toHaveBeenCalledWith(404);
    expect(json).toHaveBeenCalledWith(expect.objectContaining({ statusCode: 404, path: '/cats/1' }));
  });
});
```

Filters that use `HttpAdapterHost` need a fake adapter with `reply` and `getRequestUrl` (`jest.fn()`s). For unknown errors, assert the response is a generic 500 and that **nothing sensitive** (stack, raw message) is sent. See [exception filters](../../03-core-concepts/01-request-pipeline/08-exception-filters.md).

## Middleware

Call `use` with fake `req`, `res`, and a `next` mock:

```ts
it('adds a request id and calls next', () => {
  const req: any = { headers: {} };
  const res: any = { setHeader: jest.fn() };
  const next = jest.fn();

  requestId(req, res, next);

  expect(req.headers['x-request-id']).toBeDefined();
  expect(res.setHeader).toHaveBeenCalledWith('x-request-id', req.headers['x-request-id']);
  expect(next).toHaveBeenCalledTimes(1);
});
```

Also test short-circuit cases: responding without calling `next` (`expect(next).not.toHaveBeenCalled()`). Route binding (`forRoutes`, `exclude`) can only be verified through a real app ([E2E](./06-e2e-testing.md)).

## Custom decorators

- **Metadata decorators** (`@Roles`, `@Public`) are covered indirectly by testing the guard with a real `Reflector` (above).
- **Param decorators** from `createParamDecorator` are awkward to call directly. The practical options: extract the logic into a plain function and unit-test that, then cover the decorator itself with one E2E assertion.

```ts
export const extractUser = (req: any, key?: string) => (key ? req.user?.[key] : req.user);
export const CurrentUser = createParamDecorator((key: string | undefined, ctx: ExecutionContext) =>
  extractUser(ctx.switchToHttp().getRequest(), key));
```

See [custom decorators](../../03-core-concepts/01-request-pipeline/10-custom-decorators.md).

## Verifying the wiring (small E2E)

To prove a guard/pipe/filter is actually applied, build a tiny app with a throwaway controller, or use your real module with provider overrides:

```ts
const moduleRef = await Test.createTestingModule({ controllers: [ProbeController] })
  .overrideProvider(AuthService).useValue({ verify: jest.fn().mockResolvedValue({ id: '1' }) })
  .compile();
const app = moduleRef.createNestApplication();
app.useGlobalFilters(new HttpExceptionFilter());
await app.init();

await request(app.getHttpServer()).get('/probe').expect(401);   // guard wired
```

More in [E2E testing](./06-e2e-testing.md).

## Common mistakes

- **Mocking `Reflector`** so thoroughly that the test never exercises real metadata lookup.
- **Forgetting handler *and* class metadata cases.**
- **Not awaiting async guards/pipes** (`ParseIntPipe.transform` returns a Promise).
- **Testing DTO validation with different `ValidationPipe` options than production.**
- **Asserting only the happy path** for guards. Denials are the point.
- **Assuming a unit-tested guard is attached.** Verify wiring in E2E.
- **Calling `controller.method()` directly** and thinking pipes/guards ran.
- **Forgetting that `intercept` may return a Promise of an Observable.**

## Debugging

- Guard test always returns `undefined` metadata: the decorator wasn't applied to the handler you passed (`Controller.prototype.adminOnly`), or you passed a different class/handler to the context.
- `TypeError: ctx.switchToHttp is not a function`: your fake context is missing a method the code uses.
- Interceptor test hangs: your fake `handle` returns an Observable that never completes (use `of(...)`), or you didn't subscribe (`lastValueFrom`).
- Filter assertions fail on `status`: the code may use `httpAdapter.reply` rather than `res.status().json()`; fake the matching API.

## Quick Summary

- Unit-test components by calling their single method with small fakes; verify wiring with a few E2E checks.
- Build one reusable fake `ExecutionContext`; use a real `Reflector` and real decorators for metadata tests.
- Test pipes and DTO validation with production `ValidationPipe` options, including boundaries.
- Interceptors: fake `CallHandler` with `of()`/`throwError()`, assert with `lastValueFrom`; verify cache short-circuits.
- Filters: fake the host/response and check status, body, and no leaked internals.

## Next

[Integration testing →](./05-integration-testing.md)