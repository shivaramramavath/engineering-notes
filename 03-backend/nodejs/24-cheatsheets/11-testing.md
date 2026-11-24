# Cheatsheet 11: Testing with `node:test` & Vitest Reference Card

> Production syntax reference for automated testing using the built-in Node.js test runner (`node:test`) and modern Vitest.

---

### 1. Built-in Node.js Test Runner (`node:test` & `node:assert`)
Available natively with zero external dependencies:
```javascript
import { describe, it, before, after, beforeEach } from 'node:test';
import assert from 'node:assert/strict'; // Always use strict mode

describe('Calculator Service', () => {
  before(() => {
    // Run once before all tests in suite
  });

  beforeEach(() => {
    // Run before each individual test
  });

  it('adds two numbers correctly', () => {
    const sum = 2 + 3;
    assert.equal(sum, 5);
  });

  it('validates deep object equality', () => {
    const actual = { user: 'Ada', roles: ['admin'] };
    assert.deepEqual(actual, { user: 'Ada', roles: ['admin'] });
  });

  it('asserts async promise rejection', async () => {
    await assert.rejects(
      async () => { throw new Error('DB_ERROR'); },
      { name: 'Error', message: 'DB_ERROR' }
    );
  });
});
```

Run tests with: `node --test` or `node --test --watch`.

---

### 2. Built-in Mocking with `node:test`
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

describe('Function Spies', (t) => {
  it('tracks function calls', () => {
    const fn = t.mock.fn((x) => x * 2);

    assert.equal(fn(4), 8);
    assert.equal(fn.mock.callCount(), 1);
    assert.deepEqual(fn.mock.calls[0].arguments, [4]);
  });

  it('mocks timers', () => {
    t.mock.timers.enable({ apis: ['setTimeout', 'Date'] });

    let executed = false;
    setTimeout(() => { executed = true; }, 10000);

    t.mock.timers.tick(10000); // Advance virtual clock by 10 seconds!
    assert.equal(executed, true);
  });
});
```

---

### 3. Vitest Syntax & Module Mocking
```typescript
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { UserService } from './user.service.js';
import { db } from './db.js';

// Mock entire external module:
vi.mock('./db.js', () => ({
  db: {
    findUser: vi.fn(),
  },
}));

describe('UserService', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('returns user when found', async () => {
    vi.mocked(db.findUser).mockResolvedValue({ id: '1', name: 'Grace' });

    const service = new UserService();
    const result = await service.getUser('1');

    expect(db.findUser).toHaveBeenCalledWith('1');
    expect(result.name).toBe('Grace');
  });
});
```

---

### 4. HTTP API Testing with Supertest
```javascript
import { describe, it, expect } from 'vitest';
import request from 'supertest';
import { app } from './app.js';

describe('API Endpoints', () => {
  it('POST /users creates new user', async () => {
    const res = await request(app)
      .post('/api/v1/users')
      .send({ name: 'Alan Turing', email: 'alan@bletchley.org' })
      .set('Authorization', 'Bearer valid_token');

    expect(res.status).toBe(201);
    expect(res.headers['content-type']).toMatch(/json/);
    expect(res.body.data.name).toBe('Alan Turing');
  });
});
```
