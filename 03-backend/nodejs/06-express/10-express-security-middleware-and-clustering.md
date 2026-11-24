# Express Security Middleware & Production Clustering

> Hardening Express 5 applications: Helmet headers, rate limiting, CORS configuration, clustering, and worker process managers.

| Metadata | Details |
| :--- | :--- |
| **Framework Baseline** | Express v5.0+ |
| **Packages** | `helmet`, `express-rate-limit`, `cors`, `node:cluster` |
| **Level** | Production Security |

---

## 1. Hardening Security Headers with Helmet

`helmet` automatically sets 15+ HTTP response headers that protect against common web vulnerabilities (XSS, clickjacking, MIME sniffing):

```javascript
import express from 'express';
import helmet from 'helmet';

const app = express();

app.use(helmet({
  contentSecurityPolicy: {
    directives: {
      defaultSrc: ["'self'"],
      scriptSrc: ["'self'", "'trusted-scripts.com'"],
      objectSrc: ["'none'"],
      upgradeInsecureRequests: [],
    },
  },
  crossOriginEmbedderPolicy: false,
}));
```

---

## 2. Rate Limiting & Brute-Force Defense

```javascript
import rateLimit from 'express-rate-limit';

export const apiLimiter = rateLimit({
  windowMs: 15 * 60 * 1000, // 15 minutes
  max: 100,                  // Limit each IP to 100 requests per windowMs
  standardHeaders: true,     // Return standard RateLimit-* headers
  legacyHeaders: false,
  message: { error: 'Too many requests, please try again later.' },
});

// Stricter limiter for authentication endpoints:
export const loginLimiter = rateLimit({
  windowMs: 60 * 1000, // 1 minute
  max: 5,              // Max 5 attempts per minute
  message: { error: 'Too many login attempts. Please wait 1 minute.' },
});

app.use('/api/', apiLimiter);
app.post('/api/auth/login', loginLimiter, handleLogin);
```

---

## 3. Production Clustering with `node:cluster`

Scale across multi-core CPUs by spawning worker processes:

```javascript
import cluster from 'node:cluster';
import os from 'node:os';
import { app } from './app.js';

const numCPUs = os.cpus().length;

if (cluster.isPrimary) {
  console.log(`Primary cluster process ${process.pid} is running`);

  // Fork workers matching available CPU cores
  for (let i = 0; i < numCPUs; i++) {
    cluster.fork();
  }

  cluster.on('exit', (worker, code, signal) => {
    console.warn(`Worker ${worker.process.pid} died. Forking replacement...`);
    cluster.fork();
  });
} else {
  // Workers share the same TCP port
  app.listen(3000, () => {
    console.log(`Worker process ${process.pid} started on port 3000`);
  });
}
```
*(In containerized Kubernetes environments, run 1 single process per container and scale via replica pods rather than in-process cluster).*
