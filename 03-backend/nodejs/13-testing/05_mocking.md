# Mocking

Real dependencies (networks, databases, clocks, random numbers, third-party services) make tests slow, flaky, and hard to control. A **test double** is a stand-in that replaces a real dependency so a test can run fast and deterministically. This file explains the kinds of doubles, how to create them in JavaScript, and, just as importantly, **when not to**.

See also: [Unit Testing](./02_unit-testing.md), [Integration Testing](./03_integration-testing.md), [Vitest and Jest](./04_vitest-and-jest.md), [Dependency Injection](../20_design-patterns/09_dependency-injection.md).

## Test doubles: the vocabulary

People say "mock" for everything, but the types differ:

| Double | Purpose | Example |
|--------|---------|---------|
| **Dummy** | Fills a required parameter; never actually used | `new Service(null, logger)` |
| **Stub** | Returns canned answers so the code under test can proceed | `getUser: () => ({ id: 1 })` |
| **Spy** | Wraps (or replaces) a function and **records** how it was called | `vi.spyOn(obj, 'method')` |
| **Mock** | A double with **expectations** about calls built in (strictly: verifies interactions) | `expect(mailer.send).toHaveBeenCalledWith(...)` |
| **Fake** | A **working, simplified implementation** | An in-memory repository, a local fake HTTP server |

In day-to-day JavaScript, `vi.fn()` / `jest.fn()` gives you a stub, spy, and mock in one object. The distinction matters for **design**: stubs and fakes help the code run; spies and mocks check **interactions**.

## Two styles of verification

| Style | Question | Example |
|-------|----------|---------|
| **State verification** | "What was the result or final state?" | `expect(cart.total()).toBe(30)` |
| **Interaction (behavior) verification** | "Which calls were made to collaborators?" | `expect(mailer.send).toHaveBeenCalledOnce()` |

Prefer **state verification** whenever the outcome is observable. Reserve interaction checks for effects that **leave the system** and cannot be observed otherwise: an email was sent, a payment was charged, an event was published.

## Creating doubles by hand (often the best way)

Plain objects and functions need no library, and dependency injection makes them easy to supply.

```js
// Stub: canned data
const userRepo = {
  find: async (id) => (id === 1 ? { id: 1, name: 'Ada' } : null),
};

// Hand-rolled spy: records calls
function createSpyMailer() {
  const sent = [];
  return {
    sent,
    send: async (to, body) => { sent.push({ to, body }); },
  };
}

it('emails the user after signup', async () => {
  const mailer = createSpyMailer();
  const signup = createSignup({ userRepo, mailer });

  await signup({ email: 'ada@example.com' });

  expect(mailer.sent).toEqual([{ to: 'ada@example.com', body: expect.stringContaining('Welcome') }]);
});
```

### Fakes: working simplified implementations

```js
function createInMemoryUserRepo() {
  const users = new Map();
  let nextId = 1;

  return {
    async create(data) {
      const user = { id: nextId++, ...data };
      users.set(user.id, user);
      return user;
    },
    async find(id) { return users.get(id) ?? null; },
    async findByEmail(email) { return [...users.values()].find((u) => u.email === email) ?? null; },
  };
}
```

Tests using a fake read like real usage (create, then find) and survive internal refactors, because they assert on results instead of on which methods were called. To keep fakes honest, run the **same contract tests** against both the fake and the real implementation (see [Testing Patterns](./06_testing-patterns.md)).

## Mock functions with Vitest and Jest

(`vi` in Vitest, `jest` in Jest; the API matches.)

```js
import { vi, expect, it } from 'vitest';

const notify = vi.fn();
notify('a@x.com', 'hello');

expect(notify).toHaveBeenCalledOnce();
expect(notify).toHaveBeenCalledWith('a@x.com', 'hello');
expect(notify.mock.calls[0][1]).toBe('hello');
```

### Controlling return values

```js
const getPrice = vi.fn().mockReturnValue(100);                    // always 100
const getPrice2 = vi.fn()
  .mockReturnValueOnce(100)
  .mockReturnValueOnce(120);                                       // then undefined

const fetchUser = vi.fn().mockResolvedValue({ id: 1 });            // Promise.resolve
const failing = vi.fn().mockRejectedValue(new Error('timeout'));   // Promise.reject
const impl = vi.fn((a, b) => a + b);                               // real logic
```

Simulate flaky dependencies by chaining behaviors:

```js
const flaky = vi.fn()
  .mockRejectedValueOnce(new Error('503'))
  .mockRejectedValueOnce(new Error('503'))
  .mockResolvedValue({ ok: true });

await expect(retry(flaky, { retries: 3 })).resolves.toEqual({ ok: true });
expect(flaky).toHaveBeenCalledTimes(3);
```

### Spying on existing methods

```js
const spy = vi.spyOn(Math, 'random').mockReturnValue(0.5);
expect(rollDie()).toBe(4);
spy.mockRestore();                                                  // always restore

const errorSpy = vi.spyOn(console, 'error').mockImplementation(() => {});
```

`spyOn` **replaces a property on a shared object**, so restore it. Configure `restoreMocks: true` or call `vi.restoreAllMocks()` in `afterEach`, otherwise mocks leak into other tests.

## Mocking modules

Replace an imported module for the whole test file:

```js
import { vi, it, expect } from 'vitest';
import { sendEmail } from './mailer.js';
import { registerUser } from './register.js';

vi.mock('./mailer.js', () => ({
  sendEmail: vi.fn().mockResolvedValue(undefined),
}));

it('sends a welcome email', async () => {
  await registerUser('ada@example.com');
  expect(sendEmail).toHaveBeenCalledWith('ada@example.com', expect.any(String));
});
```

Notes:

- `vi.mock` / `jest.mock` are **hoisted** above imports; factories cannot reference variables declared in the test file unless wrapped in `vi.hoisted()` (Vitest) or named with a `mock` prefix (Jest)
- Mock only the **boundary** module (the mailer, the database client), not your own internal modules
- To mock part of a module: spread `await vi.importActual('./utils.js')` and override one export
- Reset between tests (`vi.resetModules()`, `mockClear`) if state leaks

Module mocking is a convenient escape hatch for code **not designed for injection**. For new code, prefer passing dependencies in; you will need far fewer module mocks.

## Mocking time

Never wait for real time in tests.

```js
import { vi, beforeEach, afterEach, it, expect } from 'vitest';

beforeEach(() => vi.useFakeTimers());
afterEach(() => vi.useRealTimers());

it('expires a session after 30 minutes', () => {
  vi.setSystemTime(new Date('2026-01-01T12:00:00Z'));
  const session = createSession();

  vi.advanceTimersByTime(29 * 60 * 1000);
  expect(session.isValid()).toBe(true);

  vi.advanceTimersByTime(2 * 60 * 1000);
  expect(session.isValid()).toBe(false);
});

it('debounces a search', async () => {
  const search = vi.fn();
  const onInput = debounce(search, 300);

  onInput('a'); onInput('ab'); onInput('abc');
  await vi.advanceTimersByTimeAsync(300);              // advances timers and flushes microtasks

  expect(search).toHaveBeenCalledOnce();
  expect(search).toHaveBeenCalledWith('abc');
});
```

| Helper | Use |
|--------|-----|
| `vi.advanceTimersByTime(ms)` | Move time forward, running timers due in that window |
| `await vi.advanceTimersByTimeAsync(ms)` | Same, and lets promise callbacks run in between |
| `vi.runOnlyPendingTimers()` | Run timers currently scheduled (safe with intervals) |
| `vi.runAllTimers()` | Run until none are left (infinite loop with recurring intervals) |
| `vi.setSystemTime(date)` | Control `Date` and `Date.now()` |
| `vi.getTimerCount()` | Debug how many timers are pending |

Often a **cleaner** approach is an injected clock (`now = Date.now` as a parameter), which needs no fake-timer machinery. See [Dependency Injection](../20_design-patterns/09_dependency-injection.md).

`node:test` offers `mock.timers.enable({ apis: ['setTimeout', 'Date'] })` and `mock.timers.tick(ms)`.

## Mocking randomness and IDs

```js
// Injection (preferred)
const createOrder = ({ generateId = () => crypto.randomUUID() } = {}) => ({ id: generateId() });
expect(createOrder({ generateId: () => 'fixed-id' }).id).toBe('fixed-id');

// Spying
vi.spyOn(crypto, 'randomUUID').mockReturnValue('00000000-0000-0000-0000-000000000000');
vi.spyOn(Math, 'random').mockReturnValue(0.42);
```

For random data in many tests, a **seeded** generator gives reproducible variety.

## Mocking the network

Your code calls `fetch`, an HTTP client, or an SDK. Options from the least to the most realistic:

### 1. Inject the client

```js
async function getWeather(city, { fetchImpl = fetch } = {}) {
  const res = await fetchImpl(`https://api.weather.test/v1/${encodeURIComponent(city)}`);
  if (!res.ok) throw new Error(`Weather API ${res.status}`);
  return res.json();
}

it('returns the forecast', async () => {
  const fetchImpl = vi.fn().mockResolvedValue(Response.json({ tempC: 21 }));
  await expect(getWeather('Pune', { fetchImpl })).resolves.toEqual({ tempC: 21 });
  expect(fetchImpl).toHaveBeenCalledWith('https://api.weather.test/v1/Pune');
});

it('throws on API errors', async () => {
  const fetchImpl = vi.fn().mockResolvedValue(new Response('boom', { status: 500 }));
  await expect(getWeather('Pune', { fetchImpl })).rejects.toThrow('500');
});
```

### 2. Stub the global `fetch`

```js
beforeEach(() => {
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue(Response.json({ ok: true })));
});
afterEach(() => {
  vi.unstubAllGlobals();
});
```

### 3. Intercept at the network layer (MSW, nock, undici MockAgent)

**MSW (Mock Service Worker)** intercepts real HTTP requests in Node and the browser, so your code runs unchanged:

```js
import { setupServer } from 'msw/node';
import { http, HttpResponse } from 'msw';

const server = setupServer(
  http.get('https://api.weather.test/v1/:city', ({ params }) =>
    HttpResponse.json({ city: params.city, tempC: 21 })),
  http.post('https://api.weather.test/v1/alerts', () =>
    HttpResponse.json({ error: 'quota exceeded' }, { status: 429 })),
);

beforeAll(() => server.listen({ onUnhandledRequest: 'error' }));
afterEach(() => server.resetHandlers());
afterAll(() => server.close());

it('handles rate limiting', async () => {
  await expect(createAlert({ city: 'Pune' })).rejects.toThrow(/quota/i);
});

it('handles a server error for one test only', async () => {
  server.use(http.get('https://api.weather.test/v1/:city', () => new HttpResponse(null, { status: 500 })));
  await expect(getWeather('Pune')).rejects.toThrow();
});
```

Network-level mocking tests more of your real code (URL building, headers, parsing, error mapping) and survives swapping `fetch` for `axios`. `onUnhandledRequest: 'error'` prevents accidental real calls.

### 4. A local fake server

For client libraries, start a tiny `http` server on port `0` that plays the role of the service. Most realistic, more setup.

Remember to test **failure modes**: slow responses, timeouts, `4xx`/`5xx`, malformed JSON, dropped connections.

## Mocking the file system and environment

```js
// Prefer real temporary directories over mocking fs
import { mkdtemp, writeFile, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

let dir;
beforeEach(async () => { dir = await mkdtemp(join(tmpdir(), 'test-')); });
afterEach(async () => { await rm(dir, { recursive: true, force: true }); });

it('writes the report', async () => {
  await saveReport(join(dir, 'report.json'), { total: 3 });
  expect(JSON.parse(await readFile(join(dir, 'report.json'), 'utf8'))).toEqual({ total: 3 });
});
```

Real temp files are fast, realistic, and avoid brittle `fs` mocks. For environment variables:

```js
beforeEach(() => { vi.stubEnv('API_URL', 'https://test.example'); });
afterEach(() => { vi.unstubAllEnvs(); });
```

## Asserting on interactions well

```js
// Brittle: tied to call order and incidental details
expect(db.query).toHaveBeenNthCalledWith(1, 'SELECT * FROM users WHERE id = ?', [1]);
expect(db.query).toHaveBeenNthCalledWith(2, 'UPDATE users SET last_seen = ? WHERE id = ?', [expect.any(Date), 1]);

// Better: assert the observable result
expect(await repo.find(1)).toMatchObject({ lastSeen: expect.any(Date) });
```

When you do verify a call:

- Check the **meaningful arguments** with asymmetric matchers (`expect.any`, `expect.objectContaining`), not every incidental detail
- Check `not.toHaveBeenCalled()` for **negative** cases (an email must **not** be sent on validation failure)
- Avoid asserting exact **call counts** or **order** unless they are part of the contract

## When not to mock

| Do not mock | Why | Alternative |
|-------------|-----|-------------|
| Code you own that is fast and deterministic | The test stops testing real behavior | Use the real thing |
| Pure functions and value objects | No reason to | Call them |
| The module under test | You would be testing the mock | Mock its collaborators only |
| Everything around a function (full isolation) | Tests mirror the implementation; refactors break them | Test a slightly larger unit with real collaborators |
| The database in tests meant to verify queries | You never test the SQL | Real database (see [Integration Testing](./03_integration-testing.md)) |
| Types you do not own (a third-party client's full API) | Your mock may not match reality | Wrap it in an adapter you own, then fake that |

Signs of over-mocking:

- Setup is longer than the test itself
- Refactoring internals breaks many tests while behavior is unchanged
- All tests pass, but the feature is broken in production
- Mocks return data shapes that the real dependency never produces

**"Don't mock what you don't own":** write a thin [adapter](../20_design-patterns/07_adapter-pattern.md) around third-party code, mock **your adapter** in unit tests, and test the adapter itself against the real library (or a faithful fake) in an integration test.

## Keeping doubles honest

A mock encodes your **assumption** about a dependency. If the assumption is wrong, tests pass while production fails.

| Technique | How |
|-----------|-----|
| **Contract tests** | Run the same test suite against the real implementation and the fake |
| **Integration tests** | A few tests with real dependencies cover the seams that doubles hide |
| **Realistic stubs** | Copy real response payloads (recorded or from API docs) |
| **Type checking** | Types (TypeScript) catch fakes that drift from the real interface |
| **Strict mocks** | `onUnhandledRequest: 'error'`; fail on unexpected calls |
| **Recorded interactions** | Record real responses once, replay in tests, re-record periodically |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Not restoring spies, stubs, and timers | Leaks into other tests; order-dependent failures | `restoreMocks: true`; `afterEach` cleanup |
| Mocking everything | Tests prove only that mocks were called | Mock only boundaries; use fakes and real collaborators |
| Asserting call counts and order everywhere | Brittle against refactors | Assert results and outcomes |
| Mocks that return unrealistic data | Production breaks on real payloads | Use real-shaped fixtures |
| Mocking what you do not own, directly | Mock drifts from the real API | Wrap in an adapter; mock the adapter |
| Module mocks where injection is possible | Hidden coupling, hoisting surprises | Pass dependencies in |
| Forgetting `await` with `mockResolvedValue` results | Assertions run too early | `await` async calls |
| Real timers with `await sleep()` | Slow and flaky | Fake timers or an injected clock |
| `runAllTimers()` with an interval | Infinite loop | `runOnlyPendingTimers()` or `advanceTimersByTime` |
| Letting real network calls happen accidentally | Flaky, slow, may hit production | `onUnhandledRequest: 'error'`; block outbound network in CI |
| Mocking in a shared setup file for all tests | Surprising behavior elsewhere | Mock per test or per file |
| Mocking the same function in many places | Maintenance burden | One shared fake or helper |
| Testing that the mock was called, when nothing else is asserted | Zero behavior checked | Assert outcomes as well |

## Key takeaways

- Test doubles replace slow or non-deterministic dependencies: **stubs** return data, **spies** record calls, **mocks** verify interactions, **fakes** are working simplified versions
- Prefer **state verification** (results) over **interaction verification** (calls); verify calls only for effects that leave the system
- Hand-written doubles plus dependency injection are often simpler than mocking libraries; use `vi.fn`/`jest.fn` and `spyOn` for the rest
- Use fake timers or an injected clock for time; seeded or injected generators for randomness
- Mock the network at the **boundary** (injection or MSW), include failure cases, and fail on unexpected requests
- Use real temporary directories instead of mocking the file system
- Do not mock what you do not own: wrap it in an adapter, mock the adapter, and integration-test the adapter
- Keep doubles honest with contract tests, realistic data, and a few real-integration tests
- Always restore mocks, timers, and stubs after each test

**Next:** [Testing Patterns](./06_testing-patterns.md)
