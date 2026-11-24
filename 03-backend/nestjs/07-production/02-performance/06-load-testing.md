---
title: "Load Testing and Benchmarking NestJS Applications"
description: "Designing realistic load, stress, and soak tests for NestJS APIs using Autocannon, k6, and Artillery"
tags: [nestjs, performance, load-testing, k6, autocannon, benchmarking]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Load Testing and Benchmarking NestJS Applications

## Conceptual Overview
Load testing verifies that an application satisfies throughput and latency SLAs under projected production traffic. Benchmarking identifies the precise breaking point where resource saturation causes catastrophic latency degradation or connection dropping.

Testing methodologies include:
- **Load Testing**: Simulating normal expected traffic to measure baseline P95/P99 latencies.
- **Stress Testing**: Progressively scaling concurrency beyond capacity to determine the breaking point and failure modes.
- **Soak Testing**: Running continuous sustained load for 24-48 hours to uncover slow memory leaks and connection leaks.
- **Spike Testing**: Simulating sudden $10	imes$ traffic surges to test auto-scaling responsiveness.

---

## Implementation Details & Tooling

### 1. Rapid Benchmarking with Autocannon

```bash
# Run 100 concurrent connections for 30 seconds against NestJS endpoint
npx autocannon -c 100 -d 30 -p 10 http://localhost:3000/v1/health
```

### 2. Comprehensive k6 Script

```javascript
// test/k6-load-test.js
import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  stages: [
    { duration: '30s', target: 50 },  // Ramp-up to 50 users
    { duration: '1m', target: 200 },   // Sustained peak load
    { duration: '20s', target: 0 },    // Ramp-down
  ],
  thresholds: {
    http_req_duration: ['p(95)<150', 'p(99)<300'], // 95% under 150ms
    http_req_failed: ['rate<0.01'],                 // Error rate < 1%
  },
};

export default function () {
  const res = http.get('http://localhost:3000/v1/orders');
  check(res, {
    'status is 200': (r) => r.status === 200,
  });
  sleep(1);
}
```

---

## Key Takeaways & Cheat Sheet

- Always establish performance baselines with automated k6 or Autocannon tests before major releases.
- Monitor event loop lag, CPU usage, and database connection pool saturation during load tests.
- Execute soak tests to catch memory leaks that only materialize after millions of requests.

---

## Cross-References
- [01-performance-fundamentals.md](./01-performance-fundamentals.md) - Golden signals
- [03-database-performance.md](./03-database-performance.md) - Database tuning
