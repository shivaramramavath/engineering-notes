# API Keys & HMAC Request Signing in Node.js

> Machine-to-machine authentication using cryptographically secure hashed API keys and HMAC-SHA256 signature verification for webhooks.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module** | `node:crypto` (`timingSafeEqual`, `createHmac`) |
| **Security Standards** | Constant-Time String Comparison, Replay Attack Defense |

---

## 1. Verifying HMAC Signatures with Timing-Attack Safety

When receiving webhooks (Stripe, GitHub, Shopify) or machine-to-machine requests signed with HMAC-SHA256:

> **Critical Vulnerability**: Never use `signature === expectedSignature` because string comparison short-circuits on the first mismatched byte, allowing timing attacks. Always use `crypto.timingSafeEqual()`.

```javascript
import crypto from 'node:crypto';

export function verifyWebhookSignature(payloadRawBody, signatureHeader, secret) {
  // 1. Calculate expected signature
  const hmac = crypto.createHmac('sha256', secret);
  hmac.update(payloadRawBody);
  const expectedSignature = hmac.digest('hex');

  const providedBuffer = Buffer.from(signatureHeader, 'hex');
  const expectedBuffer = Buffer.from(expectedSignature, 'hex');

  // Buffers must have identical length for timingSafeEqual
  if (providedBuffer.length !== expectedBuffer.length) {
    return false;
  }

  // 2. Constant-time comparison
  return crypto.timingSafeEqual(providedBuffer, expectedBuffer);
}
```

---

## 2. Preventing Replay Attacks (Timestamp Validation)

Always include a Unix timestamp in the signature calculation:
```
Signature = HMAC_SHA256(secret, `${timestamp}.${payload}`)
```
Reject requests if `Math.abs(Date.now() / 1000 - timestamp) > 300` (older than 5 minutes).
