# Node.js Framework Selection Guide & Benchmarks

> Comprehensive decision matrix comparing Express 5, Fastify 5, Hono, Koa, and NestJS across throughput, developer ergonomics, ecosystem maturity, and deployment targets.

| Metadata | Details |
| :--- | :--- |
| **Frameworks** | Express 5, Fastify 5, Hono, Koa, NestJS |
| **Benchmarking Tool** | Autocannon (HTTP/1.1 load testing) |
| **Level** | Staff Architectural Decision |

---

## 1. Architectural Feature & Performance Comparison

| Metric / Feature | Express 5 | Fastify v5 | Hono | Koa | NestJS |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Throughput (RPS)** | ~25,000–35,000 | **~65,000–85,000** | **~70,000–90,000** | ~40,000–50,000 | ~20,000–30,000 (Express engine) |
| **Cold Start Latency** | Fast (~50ms) | Fast (~60ms) | **Instant (~10ms)**| Fast (~45ms) | Slower (~300–600ms due to DI) |
| **Built-in Validation**| No (Zod / Joi) | **Native AJV schemas**| No (Zod / Valibot) | No | Class-validator / Zod |
| **Ecosystem & Community**| **Massive (15+ yrs)**| Very Large | Rapidly Growing | Moderate | Enterprise Large |
| **Edge Portability** | Node.js only | Node.js only | **Full (Edge, CF, Deno, Bun)** | Node.js only | Node.js only |
| **Architecture Style** | Linear Middleware | Encapsulated Plugins | Web Standards `c` | Onion Cascade | Modular OOP / Angular DI |

---

## 2. Decision Tree: Which Framework Should You Choose?

```
                        What is your primary constraint?
                                      │
        ┌─────────────────────────────┼─────────────────────────────┐
        ▼                             ▼                             ▼
  Maximum Throughput            Multi-Runtime Portability     Enterprise DI & Structure
  & Low-Latency APIs            (Cloudflare / Lambda / Node)  (Large teams, Java/C# devs)
        │                             │                             │
        ▼                             ▼                             ▼
   Fastify v5                       Hono                         NestJS
   (AJV Schemas)            (@hono/node-server)          (03-backend/nestjs)
        │
        ▼
   (If team already knows Express or migrating legacy codebase)
        │
        ▼
   Express 5.0
```

---

## 3. Indicative Benchmark Methodology

Running indicative benchmarks using `autocannon`:
```bash
# Test command: 100 concurrent connections for 10 seconds
npx autocannon -c 100 -d 10 http://localhost:3000/api/users
```
- In I/O-bound microservices where database queries dominate latency (e.g. 20ms SQL query), framework overhead accounts for $< 5\%$ of total latency.
- In CPU-intensive, high-throughput gateway proxies, Fastify and Hono achieve noticeably higher requests per second with lower memory footprints.
