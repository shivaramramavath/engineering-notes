# The Native Node.js Test Runner (`node:test`)

> Writing zero-dependency unit and integration tests using Node's built-in `node:test` and `node:assert/strict`.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Modules** | `node:test`, `node:assert/strict` |
| **Features** | Subtests, Mocks, Snapshot testing, Code coverage |

---

## 1. Why Adopt the Built-in Test Runner?

Historically, Node.js applications relied on heavy third-party testing frameworks (Mocha, Jest).
- **Fast Startup**: Zero `node_modules` overhead; runs directly in the V8 engine.
- **Native ESM & TypeScript**: Runs directly with modern Node.js type stripping without Babel or ts-jest transpilers.
- **CLI Runner**: `node --test` discovers and executes test files out-of-the-box.

---

## 2. Writing Comprehensive Tests

```javascript
import { describe, it, before, after, mock } from 'node:test';
import assert from 'node:assert/strict';

describe('Order Service Tests', () => {
  it('should calculate discount and subtotal correctly', () => {
    const item = { price: 100, quantity: 2 };
    const subtotal = item.price * item.quantity;
    
    assert.equal(subtotal, 200);
    assert.deepEqual(item, { price: 100, quantity: 2 });
  });

  it('should verify mocked dependencies', (t) => {
    const emailService = {
      sendEmail: () => true,
    };

    t.mock.method(emailService, 'sendEmail');
    emailService.sendEmail('alice@example.com');

    assert.equal(emailService.sendEmail.mock.callCount(), 1);
  });
});
```
